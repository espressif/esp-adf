/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "esp_fourcc.h"
#include "esp_media_service_types.h"
#include "esp_player_service_playback.h"

#include "feed_source.h"
#include "settings.h"

#define ADTS_HEADER_LEN    (7)
#define AAC_FRAME_SAMPLES  (1024)

#define JPEG_MARKER_SOI   (0xD8)
#define JPEG_MARKER_EOI   (0xD9)
#define JPEG_READ_CHUNK   (4096)
/* SOF0 sits after APP0 and the quantization tables, well inside this window. */
#define JPEG_PROBE_BYTES  (2048)

typedef struct {
    const char             *path;
    esp_media_track_type_t  type;
    uint16_t                track_id;
    const char             *name;
    volatile bool           stop;
    TaskHandle_t            task;
    uint32_t                frames;
    uint64_t                bytes;
    int64_t                 pts_ms;
    int64_t                 start_us;
} feed_track_t;

typedef struct {
    FILE    *fp;
    uint8_t  chunk[JPEG_READ_CHUNK];
    size_t   len;
    size_t   pos;
} jpeg_reader_t;

static const char *TAG = "VPF_SOURCE";

static const uint32_t s_adts_sample_rates[] = {96000, 88200, 64000, 48000, 44100, 32000,
                                               24000, 22050, 16000, 12000, 11025, 8000, 7350};

static esp_player_service_t *s_service;
static feed_track_t s_audio = {
    .path     = EXAMPLE_AUDIO_ES_PATH,
    .type     = ESP_MEDIA_TRACK_TYPE_AUDIO,
    .track_id = EXAMPLE_AUDIO_TRACK_ID,
    .name     = "audio",
};
static feed_track_t s_video = {
    .path     = EXAMPLE_VIDEO_ES_PATH,
    .type     = ESP_MEDIA_TRACK_TYPE_VIDEO,
    .track_id = EXAMPLE_VIDEO_TRACK_ID,
    .name     = "video",
};
static uint32_t s_audio_sample_rate;
static uint8_t s_audio_channel;
static uint16_t s_video_width;
static uint16_t s_video_height;

static const char *state_name(esp_player_state_t state)
{
    switch (state) {
        case ESP_PLAYER_STATE_IDLE:
            return "idle";
        case ESP_PLAYER_STATE_PREPARING:
            return "preparing";
        case ESP_PLAYER_STATE_PLAYING:
            return "playing";
        case ESP_PLAYER_STATE_PAUSED:
            return "paused";
        case ESP_PLAYER_STATE_STOPPED:
            return "stopped";
        case ESP_PLAYER_STATE_FINISHED:
            return "finished";
        case ESP_PLAYER_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static esp_err_t event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    (void)ctx;
    if (msg->type == ESP_PLAYER_SERVICE_EVENT_FINISHED) {
        ESP_LOGI(TAG, "Playback finished (both tracks reached EOS)");
    } else if (msg->type == ESP_PLAYER_SERVICE_EVENT_ERROR) {
        ESP_LOGE(TAG, "Playback error");
    }
    return ESP_OK;
}

/* Frame length including the header, or 0 when hdr is not an ADTS sync word. */
static size_t adts_frame_len(const uint8_t *hdr)
{
    if (hdr[0] != 0xFF || (hdr[1] & 0xF0) != 0xF0) {
        return 0;
    }
    return ((size_t)(hdr[3] & 0x03) << 11) | ((size_t)hdr[4] << 3) | (size_t)(hdr[5] >> 5);
}

static esp_err_t adts_parse_format(const uint8_t *hdr, uint32_t *out_rate, uint8_t *out_channel)
{
    uint8_t rate_idx = (uint8_t)((hdr[2] >> 2) & 0x0F);
    uint8_t channel_cfg = (uint8_t)(((hdr[2] & 0x01) << 2) | (hdr[3] >> 6));
    if (rate_idx >= sizeof(s_adts_sample_rates) / sizeof(s_adts_sample_rates[0]) || channel_cfg == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    *out_rate = s_adts_sample_rates[rate_idx];
    *out_channel = channel_cfg;
    return ESP_OK;
}

static esp_err_t probe_audio_format(void)
{
    FILE *fp = fopen(EXAMPLE_AUDIO_ES_PATH, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Open %s failed: %s", EXAMPLE_AUDIO_ES_PATH, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t hdr[ADTS_HEADER_LEN];
    size_t got = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);
    if (got != sizeof(hdr) || adts_frame_len(hdr) == 0) {
        ESP_LOGE(TAG, "%s is not an ADTS stream", EXAMPLE_AUDIO_ES_PATH);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return adts_parse_format(hdr, &s_audio_sample_rate, &s_audio_channel);
}

/* Baseline / progressive SOF marker: FF Cx, length, precision, height, width. */
static esp_err_t jpeg_parse_size(const uint8_t *buf, size_t len, uint16_t *out_w, uint16_t *out_h)
{
    for (size_t i = 0; i + 8 < len; i++) {
        if (buf[i] != 0xFF) {
            continue;
        }
        uint8_t marker = buf[i + 1];
        if (marker == 0xC0 || marker == 0xC1 || marker == 0xC2) {
            *out_h = (uint16_t)((buf[i + 5] << 8) | buf[i + 6]);
            *out_w = (uint16_t)((buf[i + 7] << 8) | buf[i + 8]);
            return (*out_w != 0 && *out_h != 0) ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t probe_video_format(void)
{
    FILE *fp = fopen(EXAMPLE_VIDEO_ES_PATH, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Open %s failed: %s", EXAMPLE_VIDEO_ES_PATH, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    uint8_t *head = malloc(JPEG_PROBE_BYTES);
    if (head == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    size_t got = fread(head, 1, JPEG_PROBE_BYTES, fp);
    fclose(fp);
    esp_err_t ret = jpeg_parse_size(head, got, &s_video_width, &s_video_height);
    free(head);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s has no JPEG frame header", EXAMPLE_VIDEO_ES_PATH);
    }
    return ret;
}

static int reader_byte(jpeg_reader_t *r)
{
    if (r->pos >= r->len) {
        r->len = fread(r->chunk, 1, sizeof(r->chunk), r->fp);
        r->pos = 0;
        if (r->len == 0) {
            return -1;
        }
    }
    return r->chunk[r->pos++];
}

static bool reader_at_end(jpeg_reader_t *r)
{
    int b = reader_byte(r);
    if (b < 0) {
        return true;
    }
    r->pos--;
    return false;
}

/* Copies one SOI..EOI frame into buf. Returns its size, or 0 at end of stream.
   JPEG byte stuffing keeps FF D9 out of the entropy-coded data, so scanning for
   the marker pair is enough to find the frame boundary. */
static size_t jpeg_next_frame(jpeg_reader_t *r, uint8_t *buf, size_t cap)
{
    int b = -1;
    int prev = -1;
    while ((b = reader_byte(r)) >= 0) {
        if (prev == 0xFF && b == JPEG_MARKER_SOI) {
            break;
        }
        prev = b;
    }
    if (b < 0 || cap < 2) {
        return 0;
    }

    size_t len = 0;
    buf[len++] = 0xFF;
    buf[len++] = JPEG_MARKER_SOI;
    prev = -1;
    while ((b = reader_byte(r)) >= 0) {
        if (len >= cap) {
            ESP_LOGE(TAG, "JPEG frame exceeds %d bytes", EXAMPLE_VIDEO_FRAME_MAX_SIZE);
            return 0;
        }
        buf[len++] = (uint8_t)b;
        if (prev == 0xFF && b == JPEG_MARKER_EOI) {
            return len;
        }
        prev = b;
    }
    return 0;
}

/* Retries on ESP_ERR_TIMEOUT when the submit queue stays full. */
static esp_err_t feed_submit(feed_track_t *tr, void *data, size_t size, bool eos)
{
    esp_media_frame_t frame = {
        .track_id = tr->track_id,
        .type = tr->type,
        .data = data,
        .size = size,
        .pts = tr->pts_ms,
        .flags = eos ? ESP_MEDIA_FRAME_FLAG_EOS : 0,
    };
    while (!tr->stop) {
        esp_err_t ret = esp_player_service_write_frame(s_service, ESP_MEDIA_DEFAULT_STREAM, &frame);
        if (ret == ESP_OK) {
            tr->frames++;
            tr->bytes += size;
            return ESP_OK;
        }
        if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "%s write_frame failed: %s", tr->name, esp_err_to_name(ret));
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_FEED_RETRY_MS));
    }
    return ESP_ERR_INVALID_STATE;
}

/* Keeps the submit clock a fixed lead ahead of the presentation clock. */
static void pace_to_pts(const feed_track_t *tr)
{
    int64_t target_us = tr->start_us + (tr->pts_ms - EXAMPLE_FEED_LEAD_MS) * 1000;
    int64_t ahead_us = target_us - esp_timer_get_time();
    if (ahead_us > 0) {
        vTaskDelay(pdMS_TO_TICKS(ahead_us / 1000));
    }
}

static void audio_feed_task(void *arg)
{
    feed_track_t *tr = (feed_track_t *)arg;
    uint8_t *buf = malloc(EXAMPLE_AUDIO_FRAME_MAX_SIZE);
    FILE *fp = fopen(tr->path, "rb");
    uint64_t samples = 0;

    if (buf == NULL || fp == NULL) {
        ESP_LOGE(TAG, "Audio feeder cannot start: %s", strerror(errno));
    } else {
        ESP_LOGI(TAG, "Audio feeder started: %s (%" PRIu32 " Hz, %u ch)", tr->path, s_audio_sample_rate,
                 (unsigned)s_audio_channel);
    }

    while (buf != NULL && fp != NULL && !tr->stop) {
        if (fread(buf, 1, ADTS_HEADER_LEN, fp) != ADTS_HEADER_LEN) {
            break;
        }
        size_t frame_len = adts_frame_len(buf);
        if (frame_len <= ADTS_HEADER_LEN || frame_len > EXAMPLE_AUDIO_FRAME_MAX_SIZE) {
            ESP_LOGE(TAG, "Lost ADTS sync after %" PRIu32 " frames", tr->frames);
            break;
        }
        size_t payload = frame_len - ADTS_HEADER_LEN;
        if (fread(buf + ADTS_HEADER_LEN, 1, payload, fp) != payload) {
            break;
        }

        /* One byte of lookahead: EOS has to ride on the last frame, since the
           player needs a payload to carry it. */
        bool is_last = (fgetc(fp) == EOF);
        if (!is_last) {
            (void)fseek(fp, -1, SEEK_CUR);
        }
        pace_to_pts(tr);
        if (feed_submit(tr, buf, frame_len, is_last) != ESP_OK || is_last) {
            break;
        }
        samples += AAC_FRAME_SAMPLES;
        tr->pts_ms = (int64_t)(samples * 1000 / s_audio_sample_rate);
    }

    if (fp != NULL) {
        fclose(fp);
    }
    free(buf);
    ESP_LOGI(TAG, "Audio feeder %s after %" PRIu32 " frames", tr->stop ? "stopped" : "done", tr->frames);
    tr->task = NULL;
    vTaskDelete(NULL);
}

static void video_feed_task(void *arg)
{
    feed_track_t *tr = (feed_track_t *)arg;
    uint8_t *buf = malloc(EXAMPLE_VIDEO_FRAME_MAX_SIZE);
    jpeg_reader_t *reader = calloc(1, sizeof(jpeg_reader_t));
    uint32_t index = 0;

    if (buf == NULL || reader == NULL) {
        ESP_LOGE(TAG, "Video feeder cannot allocate %d bytes", EXAMPLE_VIDEO_FRAME_MAX_SIZE);
    } else {
        reader->fp = fopen(tr->path, "rb");
        if (reader->fp == NULL) {
            ESP_LOGE(TAG, "Video feeder cannot start: %s", strerror(errno));
        } else {
            ESP_LOGI(TAG, "Video feeder started: %s (%ux%u @%d fps)", tr->path, (unsigned)s_video_width,
                     (unsigned)s_video_height, EXAMPLE_VIDEO_FPS);
        }
    }

    while (buf != NULL && reader != NULL && reader->fp != NULL && !tr->stop) {
        size_t size = jpeg_next_frame(reader, buf, EXAMPLE_VIDEO_FRAME_MAX_SIZE);
        if (size == 0) {
            break;
        }
        bool is_last = reader_at_end(reader);
        pace_to_pts(tr);
        if (feed_submit(tr, buf, size, is_last) != ESP_OK || is_last) {
            break;
        }
        index++;
        tr->pts_ms = (int64_t)index * 1000 / EXAMPLE_VIDEO_FPS;
    }

    if (reader != NULL) {
        if (reader->fp != NULL) {
            fclose(reader->fp);
        }
        free(reader);
    }
    free(buf);
    ESP_LOGI(TAG, "Video feeder %s after %" PRIu32 " frames", tr->stop ? "stopped" : "done", tr->frames);
    tr->task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t declare_audio_track(void)
{
    esp_media_track_info_t track = {
        .id = EXAMPLE_AUDIO_TRACK_ID,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_AAC;
    track.info.audio.sample_rate = s_audio_sample_rate;
    track.info.audio.channel = s_audio_channel;
    track.info.audio.bits_per_sample = EXAMPLE_AUDIO_BITS_PER_SAMPLE;
    return esp_player_service_set_track(s_service, ESP_MEDIA_DEFAULT_STREAM, &track);
}

static esp_err_t declare_video_track(void)
{
    esp_media_track_info_t track = {
        .id = EXAMPLE_VIDEO_TRACK_ID,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    };
    track.info.video.codec = ESP_FOURCC_MJPG;
    track.info.video.width = s_video_width;
    track.info.video.height = s_video_height;
    track.info.video.fps = EXAMPLE_VIDEO_FPS;
    return esp_player_service_set_track(s_service, ESP_MEDIA_DEFAULT_STREAM, &track);
}

static void reset_counters(feed_track_t *tr, int64_t start_us)
{
    tr->stop = false;
    tr->frames = 0;
    tr->bytes = 0;
    tr->pts_ms = 0;
    tr->start_us = start_us;
}

static esp_err_t join_task(feed_track_t *tr)
{
    tr->stop = true;
    for (int i = 0; i < EXAMPLE_FEED_JOIN_RETRIES && tr->task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_FEED_JOIN_POLL_MS));
    }
    if (tr->task != NULL) {
        ESP_LOGW(TAG, "%s feeder did not exit in time", tr->name);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t feed_source_init(esp_player_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Init failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    s_service = service;
    return esp_player_service_set_event_cb(service, event_cb, NULL);
}

esp_err_t feed_source_start(void)
{
    if (s_service == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (feed_source_active()) {
        ESP_LOGW(TAG, "Feeding already active");
        return ESP_ERR_INVALID_STATE;
    }

    bool has_audio = false;
    if (probe_audio_format() == ESP_OK) {
        esp_err_t ret = declare_audio_track();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Declare audio track failed: %s", esp_err_to_name(ret));
            return ret;
        }
        has_audio = true;
    }
    bool has_video = false;
    if (probe_video_format() == ESP_OK) {
        esp_err_t ret = declare_video_track();
        if (ret == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "Stream has no video output; feeding audio only");
        } else if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Declare video track failed: %s", esp_err_to_name(ret));
            return ret;
        }
        has_video = (ret == ESP_OK);
    }
    if (!has_audio && !has_video) {
        ESP_LOGE(TAG, "No usable elementary stream on the SD card");
        return ESP_ERR_NOT_FOUND;
    }

    /* One shared origin so both tracks pace against the same wall clock. */
    int64_t start_us = esp_timer_get_time();
    if (has_audio) {
        reset_counters(&s_audio, start_us);
        if (xTaskCreate(audio_feed_task, "vpf_aud", EXAMPLE_FEED_TASK_STACK, &s_audio,
                        EXAMPLE_FEED_TASK_PRIO, &s_audio.task) != pdPASS) {
            s_audio.task = NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    if (has_video) {
        reset_counters(&s_video, start_us);
        if (xTaskCreate(video_feed_task, "vpf_vid", EXAMPLE_FEED_TASK_STACK, &s_video,
                        EXAMPLE_FEED_TASK_PRIO, &s_video.task) != pdPASS) {
            s_video.task = NULL;
            (void)join_task(&s_audio);
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "Feeding started (%s%s%s)", has_audio ? "audio" : "", (has_audio && has_video) ? " + " : "",
             has_video ? "video" : "");
    return ESP_OK;
}

esp_err_t feed_source_stop(void)
{
    if (s_service == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t audio_ret = join_task(&s_audio);
    esp_err_t video_ret = join_task(&s_video);
    (void)esp_player_service_stop(s_service, ESP_MEDIA_DEFAULT_STREAM);
    ESP_LOGI(TAG, "Feeding stopped");
    return (audio_ret != ESP_OK) ? audio_ret : video_ret;
}

bool feed_source_active(void)
{
    return s_audio.task != NULL || s_video.task != NULL;
}

void feed_source_status(void)
{
    if (s_service == NULL) {
        printf("player not ready\n");
        return;
    }

    esp_player_state_t state = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(s_service, ESP_MEDIA_DEFAULT_STREAM, &state);
    uint64_t pos_ms = 0;
    (void)esp_player_service_get_position(s_service, ESP_MEDIA_DEFAULT_STREAM, &pos_ms);

    printf("state=%s pos=%" PRIu64 "ms\n", state_name(state), pos_ms);
    printf("track  codec  format         frames  bytes      feeding\n");
    printf("audio  AAC    %" PRIu32 "Hz %uch    %-6" PRIu32 "  %-9" PRIu64 "  %s\n", s_audio_sample_rate,
           (unsigned)s_audio_channel, s_audio.frames, s_audio.bytes, s_audio.task != NULL ? "yes" : "no");
    printf("video  MJPEG  %ux%u @%dfps  %-6" PRIu32 "  %-9" PRIu64 "  %s\n", (unsigned)s_video_width,
           (unsigned)s_video_height, EXAMPLE_VIDEO_FPS, s_video.frames, s_video.bytes,
           s_video.task != NULL ? "yes" : "no");
}
