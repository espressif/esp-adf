/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"

#include "esp_fourcc.h"
#include "esp_media_track.h"
#include "esp_player_service_playback.h"
#include "esp_playlist.h"

#include "feed_source.h"
#include "link_source.h"
#include "mix_sources.h"
#include "settings.h"

typedef struct {
    esp_player_service_t *service;
    volatile bool         stop;
    TaskHandle_t          task;
} pcm_producer_t;

typedef enum {
    SLOT0_IDLE  = 0,
    SLOT0_MOVIE = 1,
    SLOT0_ES    = 2,
    SLOT0_LINK  = 3,
} slot0_mode_t;

static const char *TAG = "VPM_MIX";

static const esp_media_stream_id_t s_movie_stream = ESP_MEDIA_DEFAULT_STREAM;
static const esp_media_stream_id_t s_tts_stream = (esp_media_stream_id_t)1;

static const char *s_playlist_json =
    "{\"playlist_name\":\"demo\",\"items\":["
    "{\"name\":\"" EXAMPLE_TRACK0_NAME "\",\"url\":\"" EXAMPLE_TRACK0_URL "\"},"
    "{\"name\":\"" EXAMPLE_TRACK1_NAME "\",\"url\":\"" EXAMPLE_TRACK1_URL "\"},"
    "{\"name\":\"" EXAMPLE_TRACK2_NAME "\",\"url\":\"" EXAMPLE_TRACK2_URL "\"}"
    "]}";

static esp_player_service_t *s_service;
static esp_playlist_handle_t s_playlist;
static bool s_playlist_attached;
static slot0_mode_t s_slot0;
static SemaphoreHandle_t s_lock;
static pcm_producer_t s_tts;

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

static esp_err_t apply_slot0_mix(bool url_source)
{
    esp_player_mix_cfg_t movie_cfg = {
        .active_gain = gain_percent_to_float(EXAMPLE_MOVIE_GAIN_PERCENT),
        .duck_gain = gain_percent_to_float(EXAMPLE_MOVIE_DUCK_GAIN_PERCENT),
        .transition_ms = EXAMPLE_MIXER_TRANSITION_MS,
        .priority = ESP_PLAYER_PRIO_BACKGROUND,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = url_source ? ESP_PLAYER_ON_PREEMPT_PAUSE : ESP_PLAYER_ON_PREEMPT_DROP,
    };
    return esp_player_service_set_mix_cfg(s_service, s_movie_stream, &movie_cfg);
}

static esp_err_t configure_mix(esp_player_service_t *service)
{
    ESP_RETURN_ON_ERROR(apply_slot0_mix(true), TAG, "Set movie mix cfg");

    esp_player_mix_cfg_t tts_cfg = {
        .active_gain = gain_percent_to_float(EXAMPLE_TTS_GAIN_PERCENT),
        .duck_gain = gain_percent_to_float(EXAMPLE_MOVIE_DUCK_GAIN_PERCENT),
        .transition_ms = EXAMPLE_MIXER_TRANSITION_MS,
        .priority = ESP_PLAYER_PRIO_NOTIFY,
        .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
        .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,
    };
    ESP_RETURN_ON_ERROR(esp_player_service_set_mix_cfg(service, s_tts_stream, &tts_cfg), TAG,
                        "Set TTS mix cfg");

    ESP_LOGI(TAG, "Mix: stream0=BACKGROUND (movie URL / ES / link), tts=NOTIFY");
    return ESP_OK;
}

static esp_err_t attach_playlist(void)
{
    if (s_playlist == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_playlist_attached) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_player_service_set_playlist(s_service, s_movie_stream, s_playlist), TAG,
                        "Attach playlist");
    s_playlist_attached = true;
    return ESP_OK;
}

static void detach_playlist(void)
{
    if (s_service != NULL) {
        (void)esp_player_service_set_playlist(s_service, s_movie_stream, NULL);
    }
    s_playlist_attached = false;
}

static esp_err_t create_playlist(esp_player_service_t *service)
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

    ret = esp_player_service_set_playlist(service, s_movie_stream, pl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set playlist failed: %s", esp_err_to_name(ret));
        esp_playlist_del(pl);
        return ret;
    }
    ret = esp_player_service_set_repeat_mode(service, s_movie_stream, ESP_PLAYLIST_REPEAT_ALL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set repeat mode failed: %s", esp_err_to_name(ret));
        (void)esp_player_service_set_playlist(service, s_movie_stream, NULL);
        esp_playlist_del(pl);
        return ret;
    }

    s_playlist = pl;
    s_playlist_attached = true;
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

static void tts_task(void *arg)
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

    ESP_LOGI(TAG, "TTS started (%s, frame=%u bytes)", pcm_path, (unsigned)frame_bytes);

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
        esp_err_t ret = esp_player_service_write_frame(ctx->service, s_tts_stream, &frame);
        if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "TTS write_frame failed: %s", esp_err_to_name(ret));
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
        (void)esp_player_service_write_frame(ctx->service, s_tts_stream, &eos);
    }

    free(frame_buf);
    fclose(fp);

done:
    if (!ctx->stop) {
        wait_player_terminal(ctx->service, s_tts_stream, 60000, &ctx->stop);
    }
    if (ctx->stop) {
        (void)esp_player_service_stop(ctx->service, s_tts_stream);
        ESP_LOGI(TAG, "TTS stopped");
    } else {
        ESP_LOGI(TAG, "TTS finished");
    }
    ctx->task = NULL;
    vTaskDelete(NULL);
}

static void stop_slot0(void)
{
    if (link_source_active() || s_slot0 == SLOT0_LINK) {
        (void)link_source_stop();
    } else if (feed_source_active() || s_slot0 == SLOT0_ES) {
        (void)feed_source_stop();
    } else {
        (void)esp_player_service_stop(s_service, s_movie_stream);
    }
    s_slot0 = SLOT0_IDLE;
    (void)apply_slot0_mix(true);
    (void)attach_playlist();
}

static int current_playlist_index(void)
{
    esp_playlist_info_t info = {0};
    if (s_playlist == NULL || esp_playlist_curr(s_playlist, &info) != ESP_OK) {
        return 0;
    }
    return info.index;
}

static esp_err_t start_movie(void)
{
    if (s_slot0 == SLOT0_ES || s_slot0 == SLOT0_LINK || feed_source_active() || link_source_active()) {
        ESP_LOGI(TAG, "Stopping stream 0 before movie URL");
        stop_slot0();
    }

    esp_player_state_t st = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(s_service, s_movie_stream, &st);
    if (state_is_active(st)) {
        ESP_LOGW(TAG, "Movie already active");
        s_slot0 = SLOT0_MOVIE;
        return ESP_OK;
    }
    if (st == ESP_PLAYER_STATE_PAUSED) {
        ESP_RETURN_ON_ERROR(esp_player_service_resume(s_service, s_movie_stream), TAG, "Resume movie");
        s_slot0 = SLOT0_MOVIE;
        ESP_LOGI(TAG, "Movie resumed");
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(attach_playlist(), TAG, "Attach playlist");
    int index = current_playlist_index();
    ESP_RETURN_ON_ERROR(esp_player_service_play_index(s_service, s_movie_stream, index), TAG,
                        "Play playlist");
    s_slot0 = SLOT0_MOVIE;

    esp_playlist_info_t info = {0};
    if (esp_playlist_curr(s_playlist, &info) == ESP_OK) {
        ESP_LOGI(TAG, "Movie started: [%d] %s", info.index, info.media_name);
    } else {
        ESP_LOGI(TAG, "Movie started");
    }
    return ESP_OK;
}

static esp_err_t start_es(void)
{
    if (feed_source_active()) {
        ESP_LOGW(TAG, "ES already active");
        s_slot0 = SLOT0_ES;
        return ESP_OK;
    }
    if (s_slot0 != SLOT0_IDLE) {
        ESP_LOGI(TAG, "Stopping stream 0 before ES");
        stop_slot0();
    }

    detach_playlist();
    ESP_RETURN_ON_ERROR(apply_slot0_mix(false), TAG, "Set ES mix cfg");
    esp_err_t ret = feed_source_start();
    if (ret != ESP_OK) {
        (void)apply_slot0_mix(true);
        (void)attach_playlist();
        return ret;
    }
    s_slot0 = SLOT0_ES;
    ESP_LOGI(TAG, "ES started");
    return ESP_OK;
}

static esp_err_t start_link(void)
{
    if (link_source_active() || s_slot0 == SLOT0_LINK) {
        ESP_LOGW(TAG, "LINK already active");
        s_slot0 = SLOT0_LINK;
        return ESP_OK;
    }
    if (s_slot0 != SLOT0_IDLE || feed_source_active()) {
        ESP_LOGI(TAG, "Stopping stream 0 before LINK");
        stop_slot0();
    }

    detach_playlist();
    ESP_RETURN_ON_ERROR(apply_slot0_mix(false), TAG, "Set LINK mix cfg");
    esp_err_t ret = link_source_start();
    if (ret != ESP_OK) {
        (void)apply_slot0_mix(true);
        (void)attach_playlist();
        return ret;
    }
    s_slot0 = SLOT0_LINK;
    return ESP_OK;
}

static esp_err_t start_tts(void)
{
    if (s_tts.task != NULL) {
        ESP_LOGW(TAG, "TTS already active");
        return ESP_OK;
    }

    char pcm_path[96];
    int n = snprintf(pcm_path, sizeof(pcm_path), "/sdcard/%s", EXAMPLE_PCM_FILENAME);
    if (n <= 0 || (size_t)n >= sizeof(pcm_path)) {
        return ESP_ERR_INVALID_ARG;
    }
    FILE *fp = fopen(pcm_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open PCM: %s (%s)", pcm_path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    fclose(fp);

    ESP_RETURN_ON_ERROR(configure_pcm_track(s_service, s_tts_stream, EXAMPLE_FEED_TRACK_ID), TAG,
                        "Set TTS track");
    s_tts.service = s_service;
    s_tts.stop = false;
    BaseType_t ok = xTaskCreate(tts_task, "vpm_tts", 6144, &s_tts, 5, &s_tts.task);
    if (ok != pdPASS) {
        s_tts.task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t stop_tts(void)
{
    if (s_tts.task == NULL) {
        (void)esp_player_service_stop(s_service, s_tts_stream);
        ESP_LOGI(TAG, "TTS stopped");
        return ESP_OK;
    }

    s_tts.stop = true;
    for (int i = 0; i < 200 && s_tts.task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (s_tts.task != NULL) {
        ESP_LOGW(TAG, "TTS task did not exit in time");
    }
    ESP_LOGI(TAG, "TTS stopped");
    return ESP_OK;
}

esp_err_t mix_sources_init(esp_player_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_service = service;
    memset(&s_tts, 0, sizeof(s_tts));
    s_slot0 = SLOT0_IDLE;
    ESP_RETURN_ON_ERROR(configure_mix(service), TAG, "Configure mix");
    ESP_RETURN_ON_ERROR(create_playlist(service), TAG, "Create playlist");
    ESP_RETURN_ON_ERROR(link_source_init(service), TAG, "Init link source");
    return feed_source_init(service);
}

esp_player_service_t *mix_sources_service(void)
{
    return s_service;
}

esp_playlist_handle_t mix_sources_playlist(void)
{
    return s_playlist;
}

bool mix_sources_es_active(void)
{
    return s_slot0 == SLOT0_ES || feed_source_active();
}

esp_err_t mix_sources_require_movie(void)
{
    if (s_slot0 == SLOT0_ES || s_slot0 == SLOT0_LINK || feed_source_active() || link_source_active()) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

void mix_sources_mark_movie(void)
{
    s_slot0 = SLOT0_MOVIE;
}

esp_err_t mix_sources_start(const char *name)
{
    if (name == NULL || s_service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(15000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (strcmp(name, "movie") == 0) {
        ret = start_movie();
    } else if (strcmp(name, "es") == 0) {
        ret = start_es();
    } else if (strcmp(name, "link") == 0) {
        ret = start_link();
    } else if (strcmp(name, "tts") == 0) {
        ret = start_tts();
    } else {
        ESP_LOGW(TAG, "Unknown stream '%s' (use movie|es|link|tts)", name);
    }

    xSemaphoreGive(s_lock);
    return ret;
}

esp_err_t mix_sources_stop(const char *name)
{
    if (name == NULL || s_service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(15000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (strcmp(name, "movie") == 0 || strcmp(name, "es") == 0 || strcmp(name, "link") == 0) {
        stop_slot0();
        const char *stopped = "Movie";
        if (strcmp(name, "es") == 0) {
            stopped = "ES";
        } else if (strcmp(name, "link") == 0) {
            stopped = "LINK";
        }
        ESP_LOGI(TAG, "%s stopped", stopped);
        ret = ESP_OK;
    } else if (strcmp(name, "tts") == 0) {
        ret = stop_tts();
    } else {
        ESP_LOGW(TAG, "Unknown stream '%s' (use movie|es|link|tts)", name);
    }

    xSemaphoreGive(s_lock);
    return ret;
}

void mix_sources_status(void)
{
    if (s_service == NULL) {
        printf("player not ready\n");
        return;
    }

    esp_player_state_t movie_st = ESP_PLAYER_STATE_IDLE;
    esp_player_state_t tts_st = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(s_service, s_movie_stream, &movie_st);
    (void)esp_player_service_get_state(s_service, s_tts_stream, &tts_st);

    bool es = mix_sources_es_active();
    bool link = (s_slot0 == SLOT0_LINK || link_source_active());
    const char *role = "movie";
    const char *path = "URL ";
    if (es) {
        role = "es   ";
        path = "ES  ";
    } else if (link) {
        role = "link ";
        path = "LINK";
    }
    printf("stream  role   priority    path  state\n");
    printf("0       %s  BACKGROUND  %s    %s%s\n", role, path, state_name(movie_st),
           (es || link || state_is_active(movie_st)) ? " (joined)" : "");
    printf("1       tts    NOTIFY      FEED  %s%s\n", state_name(tts_st),
           s_tts.task != NULL ? " (joined)" : "");

    if (es) {
        feed_source_status();
        return;
    }
    if (link) {
        printf("link dummy src bouncing-ball H264+AAC\n");
        return;
    }

    esp_playlist_info_t info = {0};
    if (s_playlist != NULL && esp_playlist_curr(s_playlist, &info) == ESP_OK) {
        printf("movie playlist index=%d name=%s\n", info.index, info.media_name);
        printf("url=%s\n", info.media_url);
    }
}
