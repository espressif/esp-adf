/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"

#include "esp_audio_player_service.h"
#include "esp_fourcc.h"
#include "esp_player_service_playback.h"
#include "esp_playlist.h"

#include "link_source.h"
#include "settings.h"
#include "stream_sources.h"

typedef struct {
    esp_player_service_t *service;
    volatile bool         stop;
    TaskHandle_t          task;
} pcm_producer_t;

static const char *TAG = "MIX_STREAMS";

static const esp_media_stream_id_t s_url_stream = ESP_MEDIA_DEFAULT_STREAM;
static const esp_media_stream_id_t s_link_stream = (esp_media_stream_id_t)1;
static const esp_media_stream_id_t s_feed_stream = (esp_media_stream_id_t)2;

/* JSON matches esp_playlist_import_ram() format used by unit tests. */
static const char *s_playlist_json =
    "{\"playlist_name\":\"demo\",\"items\":["
    "{\"name\":\"" EXAMPLE_TRACK0_NAME "\",\"url\":\"" EXAMPLE_TRACK0_URL "\"},"
    "{\"name\":\"" EXAMPLE_TRACK1_NAME "\",\"url\":\"" EXAMPLE_TRACK1_URL "\"},"
    "{\"name\":\"" EXAMPLE_TRACK2_NAME "\",\"url\":\"" EXAMPLE_TRACK2_URL "\"},"
    "{\"name\":\"" EXAMPLE_TRACK3_NAME "\",\"url\":\"" EXAMPLE_TRACK3_URL "\"}"
    "]}";

static esp_player_service_t *s_service;
static esp_playlist_handle_t s_playlist;
static SemaphoreHandle_t s_lock;
static pcm_producer_t s_feed;

static float gain_percent_to_float(int percent)
{
    if (percent < 0) {
        return 0.0f;
    }
    if (percent > 100) {
        return 1.0f;
    }
    return (float)percent / 100.0f;
}

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

static bool state_is_active(esp_player_state_t state)
{
    return state == ESP_PLAYER_STATE_PREPARING ||
           state == ESP_PLAYER_STATE_PLAYING ||
           state == ESP_PLAYER_STATE_PAUSED;
}

static bool state_is_terminal(esp_player_state_t state)
{
    return state == ESP_PLAYER_STATE_FINISHED ||
           state == ESP_PLAYER_STATE_STOPPED ||
           state == ESP_PLAYER_STATE_ERROR ||
           state == ESP_PLAYER_STATE_IDLE;
}

static void wait_player_terminal(esp_player_service_t *service, esp_media_stream_id_t stream,
                                 int timeout_ms, volatile bool *stop)
{
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += 50) {
        if (stop != NULL && *stop) {
            return;
        }
        esp_player_state_t st = ESP_PLAYER_STATE_IDLE;
        if (esp_player_service_get_state(service, stream, &st) == ESP_OK && state_is_terminal(st)) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGW(TAG, "Wait player terminal timed out on stream %u", (unsigned)stream);
}

static esp_err_t configure_mix(esp_player_service_t *service)
{
    esp_player_mix_cfg_t url_cfg = {
        .active_gain = gain_percent_to_float(EXAMPLE_URL_GAIN_PERCENT),
        .duck_gain = gain_percent_to_float(EXAMPLE_URL_DUCK_GAIN_PERCENT),
        .transition_ms = EXAMPLE_MIXER_TRANSITION_MS,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,
    };
    ESP_RETURN_ON_ERROR(esp_player_service_set_mix_cfg(service, s_url_stream, &url_cfg), TAG,
                        "Set URL mix cfg");

    esp_player_mix_cfg_t link_cfg = {
        .active_gain = gain_percent_to_float(EXAMPLE_LINK_GAIN_PERCENT),
        .duck_gain = gain_percent_to_float(EXAMPLE_URL_DUCK_GAIN_PERCENT),
        .transition_ms = EXAMPLE_MIXER_TRANSITION_MS,
        .priority = ESP_PLAYER_PRIO_NOTIFY,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    ESP_RETURN_ON_ERROR(esp_player_service_set_mix_cfg(service, s_link_stream, &link_cfg), TAG,
                        "Set LINK mix cfg");

    esp_player_mix_cfg_t feed_cfg = {
        .active_gain = gain_percent_to_float(EXAMPLE_FEED_GAIN_PERCENT),
        .duck_gain = 0.0f,
        .transition_ms = EXAMPLE_MIXER_TRANSITION_MS,
        .priority = ESP_PLAYER_PRIO_URGENT,
        .preempt_mode = ESP_PLAYER_PREEMPT_EXCLUSIVE,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    ESP_RETURN_ON_ERROR(esp_player_service_set_mix_cfg(service, s_feed_stream, &feed_cfg), TAG,
                        "Set FEED mix cfg");

    ESP_LOGI(TAG,
             "Mix fixed: URL=BACKGROUND(%d%%/%d%% duck), LINK=NOTIFY(%d%%), FEED=URGENT(%d%% EXCLUSIVE)",
             EXAMPLE_URL_GAIN_PERCENT, EXAMPLE_URL_DUCK_GAIN_PERCENT, EXAMPLE_LINK_GAIN_PERCENT,
             EXAMPLE_FEED_GAIN_PERCENT);
    return ESP_OK;
}

static esp_err_t configure_pcm_track(esp_player_service_t *service, esp_media_stream_id_t stream,
                                     uint16_t track_id)
{
    esp_media_track_info_t track = {
        .id = track_id,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    };
    track.info.audio.codec = ESP_FOURCC_PCM;
    track.info.audio.sample_rate = EXAMPLE_PCM_SAMPLE_RATE;
    track.info.audio.bits_per_sample = EXAMPLE_PCM_BITS_PER_SAMPLE;
    track.info.audio.channel = (uint8_t)EXAMPLE_PCM_CHANNEL;
    return esp_player_service_set_track(service, stream, &track);
}

static void feed_task(void *arg)
{
    pcm_producer_t *ctx = (pcm_producer_t *)arg;
    char pcm_path[96];
    snprintf(pcm_path, sizeof(pcm_path), "/sdcard/%s", EXAMPLE_PCM_FILENAME);

    FILE *fp = fopen(pcm_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open PCM: %s (%s)", pcm_path, strerror(errno));
        goto done;
    }

    const uint32_t frame_period_ms = EXAMPLE_PCM_FEED_PERIOD_MS;
    const size_t frame_bytes = (size_t)EXAMPLE_PCM_SAMPLE_RATE * EXAMPLE_PCM_CHANNEL
                               * (EXAMPLE_PCM_BITS_PER_SAMPLE / 8) * frame_period_ms / 1000;
    uint8_t *frame_buf = malloc(frame_bytes);
    if (frame_buf == NULL) {
        fclose(fp);
        goto done;
    }

    ESP_LOGI(TAG, "FEED started (%s, frame=%u bytes)", pcm_path, (unsigned)frame_bytes);

    /* feof() only trips after a read runs past the end, so a file whose size is an
       exact multiple of frame_bytes needs the length to recognize the last frame. */
    long total_bytes = 0;
    if (fseek(fp, 0, SEEK_END) == 0) {
        total_bytes = ftell(fp);
    }
    rewind(fp);

    int64_t pts_ms = 0;
    size_t read_bytes = 0;
    size_t consumed = 0;
    bool sent_any = false;

    while (!ctx->stop && (read_bytes = fread(frame_buf, 1, frame_bytes, fp)) > 0) {
        consumed += read_bytes;
        bool is_last = ctx->stop || read_bytes < frame_bytes || feof(fp) || (total_bytes > 0 && consumed >= (size_t)total_bytes);
        esp_media_frame_t frame = {
            .track_id = EXAMPLE_FEED_TRACK_ID,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = frame_buf,
            .size = read_bytes,
            .pts = pts_ms,
            .flags = is_last ? ESP_MEDIA_FRAME_FLAG_EOS : 0,
        };
        esp_err_t ret = esp_player_service_write_frame(ctx->service, s_feed_stream, &frame);
        if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "FEED write_frame failed: %s", esp_err_to_name(ret));
            break;
        }
        sent_any = true;
        pts_ms += frame_period_ms;
        if (is_last) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(frame_period_ms));
    }

    if (!sent_any && !ctx->stop) {
        memset(frame_buf, 0, 1);
        esp_media_frame_t eos = {
            .track_id = EXAMPLE_FEED_TRACK_ID,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .data = frame_buf,
            .size = 1,
            .pts = pts_ms,
            .flags = ESP_MEDIA_FRAME_FLAG_EOS,
        };
        (void)esp_player_service_write_frame(ctx->service, s_feed_stream, &eos);
    }

    free(frame_buf);
    fclose(fp);

done:
    if (!ctx->stop) {
        /* File EOF only means frames were submitted; wait until the player
           actually reaches FINISHED before considering the stream done. */
        wait_player_terminal(ctx->service, s_feed_stream, 60000, &ctx->stop);
    }
    if (ctx->stop) {
        (void)esp_player_service_stop(ctx->service, s_feed_stream);
        ESP_LOGI(TAG, "FEED stopped");
    } else {
        ESP_LOGI(TAG, "FEED finished");
    }
    ctx->task = NULL;
    vTaskDelete(NULL);
}

static esp_err_t attach_url_playlist(esp_player_service_t *service)
{
    esp_playlist_handle_t pl = NULL;
    esp_playlist_cfg_t pl_cfg = {
        .playlist_name = "demo",
    };
    ESP_RETURN_ON_ERROR(esp_playlist_new(&pl_cfg, &pl), TAG, "Playlist new failed");
    esp_err_t ret = esp_playlist_import_ram(pl, s_playlist_json, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Import playlist failed: %s", esp_err_to_name(ret));
        esp_playlist_del(pl);
        return ret;
    }

    int count = 0;
    (void)esp_playlist_get_count(pl, &count);
    ESP_LOGI(TAG, "Playlist ready: %d tracks", count);

    ret = esp_player_service_set_playlist(service, s_url_stream, pl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set playlist failed: %s", esp_err_to_name(ret));
        esp_playlist_del(pl);
        return ret;
    }
    ret = esp_player_service_set_repeat_mode(service, s_url_stream, ESP_PLAYLIST_REPEAT_ALL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set repeat mode failed: %s", esp_err_to_name(ret));
        (void)esp_player_service_set_playlist(service, s_url_stream, NULL);
        esp_playlist_del(pl);
        return ret;
    }

    s_playlist = pl;
    return ESP_OK;
}

static int current_playlist_index(void)
{
    esp_playlist_info_t info = {0};
    if (s_playlist == NULL || esp_playlist_curr(s_playlist, &info) != ESP_OK) {
        return 0;
    }
    return info.index;
}

static esp_err_t start_url(void)
{
    esp_player_state_t st = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(s_service, s_url_stream, &st);
    if (state_is_active(st)) {
        ESP_LOGW(TAG, "URL already active");
        return ESP_OK;
    }
    if (st == ESP_PLAYER_STATE_PAUSED) {
        ESP_RETURN_ON_ERROR(esp_player_service_resume(s_service, s_url_stream), TAG, "Resume URL");
        ESP_LOGI(TAG, "URL resumed");
        return ESP_OK;
    }

    int index = current_playlist_index();
    ESP_RETURN_ON_ERROR(esp_player_service_play_index(s_service, s_url_stream, index), TAG,
                        "Play playlist");
    esp_playlist_info_t info = {0};
    if (esp_playlist_curr(s_playlist, &info) == ESP_OK) {
        ESP_LOGI(TAG, "URL started: [%d] %s", info.index, info.media_name);
    } else {
        ESP_LOGI(TAG, "URL started");
    }
    return ESP_OK;
}

static esp_err_t stop_url(void)
{
    (void)esp_player_service_stop(s_service, s_url_stream);
    ESP_LOGI(TAG, "URL stopped");
    return ESP_OK;
}

static esp_err_t start_feed(void)
{
    if (s_feed.task != NULL) {
        ESP_LOGW(TAG, "FEED already active");
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(configure_pcm_track(s_service, s_feed_stream, EXAMPLE_FEED_TRACK_ID), TAG,
                        "Set FEED track");
    s_feed.service = s_service;
    s_feed.stop = false;
    BaseType_t ok = xTaskCreate(feed_task, "mix_feed", 6144, &s_feed, 5, &s_feed.task);
    if (ok != pdPASS) {
        s_feed.task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t stop_feed(void)
{
    if (s_feed.task == NULL) {
        (void)esp_player_service_stop(s_service, s_feed_stream);
        ESP_LOGI(TAG, "FEED stopped");
        return ESP_OK;
    }

    s_feed.stop = true;
    for (int i = 0; i < 200 && s_feed.task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_feed.task != NULL) {
        ESP_LOGW(TAG, "FEED task did not exit in time");
    }
    ESP_LOGI(TAG, "FEED stopped");
    return ESP_OK;
}

static esp_err_t start_link(void)
{
    if (link_source_active()) {
        ESP_LOGW(TAG, "LINK already active");
        return ESP_OK;
    }
    return link_source_start();
}

static esp_err_t stop_link(void)
{
    esp_err_t ret = link_source_stop();
    ESP_LOGI(TAG, "LINK stopped");
    return ret;
}

esp_err_t stream_sources_init(esp_player_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_service = service;
    memset(&s_feed, 0, sizeof(s_feed));
    ESP_RETURN_ON_ERROR(configure_mix(service), TAG, "Configure mix");
    ESP_RETURN_ON_ERROR(link_source_init(service), TAG, "Init link source");
    return attach_url_playlist(service);
}

esp_player_service_t *stream_sources_service(void)
{
    return s_service;
}

esp_playlist_handle_t stream_sources_playlist(void)
{
    return s_playlist;
}

esp_err_t stream_sources_start(const char *name)
{
    if (name == NULL || s_service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (strcmp(name, "url") == 0) {
        ret = start_url();
    } else if (strcmp(name, "link") == 0) {
        ret = start_link();
    } else if (strcmp(name, "feed") == 0) {
        ret = start_feed();
    } else {
        ESP_LOGW(TAG, "Unknown stream '%s' (use url|link|feed)", name);
    }

    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t stream_sources_stop(const char *name)
{
    if (name == NULL || s_service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (strcmp(name, "url") == 0) {
        ret = stop_url();
    } else if (strcmp(name, "link") == 0) {
        ret = stop_link();
    } else if (strcmp(name, "feed") == 0) {
        ret = stop_feed();
    } else {
        ESP_LOGW(TAG, "Unknown stream '%s' (use url|link|feed)", name);
    }

    xSemaphoreGive(s_lock);
    return ret;
}

void stream_sources_status(void)
{
    if (s_service == NULL) {
        printf("player not ready\n");
        return;
    }

    esp_player_state_t url_st = ESP_PLAYER_STATE_IDLE;
    esp_player_state_t link_st = ESP_PLAYER_STATE_IDLE;
    esp_player_state_t feed_st = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(s_service, s_url_stream, &url_st);
    (void)esp_player_service_get_state(s_service, s_link_stream, &link_st);
    (void)esp_player_service_get_state(s_service, s_feed_stream, &feed_st);

    printf("stream  role       priority    path  state\n");
    printf("0       url        BACKGROUND  URL   %s%s\n", state_name(url_st),
           state_is_active(url_st) ? " (joined)" : "");
    printf("1       link       NOTIFY      LINK  %s%s\n", state_name(link_st),
           link_source_active() ? " (joined)" : "");
    printf("2       feed       URGENT      FEED  %s%s\n", state_name(feed_st),
           s_feed.task != NULL ? " (joined)" : "");

    esp_playlist_info_t info = {0};
    if (s_playlist != NULL && esp_playlist_curr(s_playlist, &info) == ESP_OK) {
        printf("url playlist index=%d name=%s\n", info.index, info.media_name);
        printf("url=%s\n", info.media_url);
    }
}
