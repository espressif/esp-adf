/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_capture.h"
#include "esp_log.h"
#include "esp_service_scheduler.h"
#include "esp_capture_service_priv.h"
#include "esp_capture_service_setup.h"
#include "capture_service_err.h"
#include "esp_muxer_default.h"

#define ESP_CAPTURE_SERVICE_STORAGE_SLICE_MS  600000
#define ESP_CAPTURE_SERVICE_POLL_STEP_MS      10
#define ESP_CAPTURE_SERVICE_WAIT_FOREVER      UINT32_MAX

static const char *TAG = "CAPTURE_SERVICE";

static esp_err_t capture_service_restore_aborted_streams(esp_capture_service_t *service);
static esp_err_t capture_service_start_writers(esp_capture_service_t *service);
static esp_err_t capture_service_setup_pending_storage(esp_capture_service_t *service);
static void destroy_capture(esp_capture_service_t *service);

static const char *scheduler_names = NULL;

static void capture_service_scheduler(const char *thread_name,
                                      esp_capture_thread_schedule_cfg_t *schedule_cfg)
{
    if (schedule_cfg == NULL) {
        return;
    }
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = schedule_cfg->stack_size,
        .priority = schedule_cfg->priority,
        .core_id = schedule_cfg->core_id,
        .is_ext = schedule_cfg->stack_in_ext,
    };
    esp_service_thread_cfg_t out_cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = scheduler_names ? scheduler_names : ESP_CAPTURE_SERVICE_SCHEDULER_NAME,
        .thread_name = thread_name,
    };
    esp_err_t ret = esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &out_cfg);
    if (ret != ESP_OK) {
        return;
    }
    schedule_cfg->stack_size = out_cfg.stack_size;
    schedule_cfg->priority = out_cfg.priority;
    schedule_cfg->core_id = (out_cfg.core_id < 0) ? 0 : (uint8_t)out_cfg.core_id;
    schedule_cfg->stack_in_ext = out_cfg.is_ext;
}

static esp_err_t media_get_provider(esp_service_t *media, esp_media_stream_id_t stream,
                                    esp_media_provider_t *out_provider)
{
    return esp_capture_service_get_provider((esp_capture_service_t *)media, stream, out_provider);
}

static esp_err_t capture_service_start_writers(esp_capture_service_t *service)
{
    esp_err_t ret = capture_service_restore_aborted_streams(service);
    if (ret != ESP_OK) {
        return ret;
    }
    /* Sink muxers are added lazily right before the capture starts. This lets the
       application change muxer type / storage URL any number of times while the
       service is stopped, because `esp_capture_sink_add_muxer()` can only run
       before `esp_capture_start()`. */
    ret = capture_service_setup_pending_storage(service);
    if (ret != ESP_OK) {
        return ret;
    }
    return capture_err_to_esp(esp_capture_start(service->capture));
}

static esp_err_t media_set_request(esp_service_t *base, esp_media_stream_id_t stream,
                                   const esp_media_service_request_t *request)
{
    esp_capture_service_t *service = (esp_capture_service_t *)base;
    if (service == NULL || request == NULL || stream >= service->max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    if (base->state == ESP_SERVICE_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }
    service->use_global_cache = request->need_global_cache;
    return ESP_OK;
}

static esp_err_t media_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    esp_capture_service_t *service = (esp_capture_service_t *)base;
    if (out_role == NULL || service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!service->configured) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_role = ESP_MEDIA_ROLE_SRC;
    return ESP_OK;
}

static esp_capture_stream_type_t media_type_to_capture_stream(esp_media_track_type_t type)
{
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_CAPTURE_STREAM_TYPE_AUDIO;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return ESP_CAPTURE_STREAM_TYPE_VIDEO;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_MUXER) {
        return ESP_CAPTURE_STREAM_TYPE_MUXER;
    }
    return ESP_CAPTURE_STREAM_TYPE_NONE;
}

static const char *muxer_extension(esp_muxer_type_t muxer_type)
{
    switch (muxer_type) {
        default:
        case ESP_MUXER_TYPE_MP4:
            return "mp4";
        case ESP_MUXER_TYPE_TS:
            return "ts";
        case ESP_MUXER_TYPE_FLV:
            return "flv";
        case ESP_MUXER_TYPE_WAV:
            return "wav";
        case ESP_MUXER_TYPE_CAF:
            return "caf";
        case ESP_MUXER_TYPE_OGG:
            return "ogg";
    }
}

static int storage_url_pattern(esp_muxer_slice_info_t *info, void *ctx)
{
    capture_stream_t *stream = (capture_stream_t *)ctx;
    if (stream->storage_url != NULL) {
        if (info->len == 0) {
            return -1;
        }
        strncpy(info->file_path, stream->storage_url, info->len);
        info->file_path[info->len - 1] = '\0';
    } else {
        if (stream->storage_dir == NULL) {
            return -1;
        }
        struct timeval tv;
        gettimeofday(&tv, NULL);
        struct tm timeinfo = {0};
        localtime_r(&tv.tv_sec, &timeinfo);
        char time_str[32];
        strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", &timeinfo);
        snprintf(info->file_path, info->len, "%s/%s.%s", stream->storage_dir, time_str,
                 muxer_extension(stream->muxer.muxer_type));
    }
    char *url = strdup(info->file_path);
    if (url != NULL) {
        free(stream->last_storage_url);
        stream->last_storage_url = url;
    }
    return 0;
}

static bool has_ext(const char *url, const char *ext)
{
    if (url == NULL || ext == NULL) {
        return false;
    }
    const char *dot = strrchr(url, '.');
    if (dot == NULL) {
        return false;
    }
    while (*dot != '\0' && *ext != '\0') {
        char a = (*dot >= 'A' && *dot <= 'Z') ? *dot - 'A' + 'a' : *dot;
        char b = (*ext >= 'A' && *ext <= 'Z') ? *ext - 'A' + 'a' : *ext;
        if (a != b) {
            return false;
        }
        dot++;
        ext++;
    }
    return *dot == '\0' && *ext == '\0';
}

static esp_err_t guess_muxer_type(const char *url, esp_muxer_type_t *out_type)
{
    if (out_type == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (has_ext(url, ".mp4") || url == NULL) {
        *out_type = ESP_MUXER_TYPE_MP4;
    } else if (has_ext(url, ".ts")) {
        *out_type = ESP_MUXER_TYPE_TS;
    } else if (has_ext(url, ".flv")) {
        *out_type = ESP_MUXER_TYPE_FLV;
    } else if (has_ext(url, ".wav")) {
        *out_type = ESP_MUXER_TYPE_WAV;
    } else if (has_ext(url, ".caf")) {
        *out_type = ESP_MUXER_TYPE_CAF;
    } else if (has_ext(url, ".ogg")) {
        *out_type = ESP_MUXER_TYPE_OGG;
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static bool support_streaming(esp_muxer_type_t muxer_type)
{
    switch (muxer_type) {
        case ESP_MUXER_TYPE_TS:
        case ESP_MUXER_TYPE_FLV:
            return true;
        default:
            return false;
    }
}

/**
 * @brief  Add and bind the sink muxer for a stream using the current muxer config.
 *
 *         Resolution order for the container type:
 *         1. Use `stream->muxer.muxer_type` when it is already a valid type.
 *         2. Otherwise guess it from `stream->storage_url` extension.
 *         3. Otherwise fall back to the default storage URL extension (mp4).
 *
 *         After the muxer is added:
 *           - `auto_record` enables the muxer immediately (start recording).
 *           - `streaming == false` (or a non-streaming container) disables the
 *             muxer stream output so muxed frames are not exposed to providers.
 *
 *         This must run before `esp_capture_start()`. It is therefore invoked
 *         lazily from `capture_service_start_writers()` for streams marked
 *         `storage_pending`, and directly from `set_storage_url()` only while the
 *         instance is already running and the muxer was not added yet.
 */
static esp_err_t capture_service_setup_storage(capture_stream_t *stream)
{
    if (stream->storage_configured) {
        return ESP_OK;
    }
    esp_muxer_type_t muxer_type = stream->muxer.muxer_type;
    if (muxer_type == ESP_CAPTURE_SERVICE_MUXER_NONE) {
        esp_err_t ret = guess_muxer_type(stream->storage_url, &muxer_type);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    union {
        mp4_muxer_config_t  mp4;
        ts_muxer_config_t  ts;
        flv_muxer_config_t  flv;
        wav_muxer_config_t  wav;
        caf_muxer_config_t  caf;
        ogg_muxer_config_t  ogg;
    } muxer_cfg_data = {0};

    esp_capture_muxer_cfg_t muxer_cfg = {
        .base_config = NULL,
        .cfg_size = 0,
    };
    switch (muxer_type) {
        case ESP_MUXER_TYPE_MP4:
            muxer_cfg.base_config = &muxer_cfg_data.mp4.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.mp4);
            break;
        case ESP_MUXER_TYPE_TS:
            muxer_cfg.base_config = &muxer_cfg_data.ts.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.ts);
            break;
        case ESP_MUXER_TYPE_FLV:
            muxer_cfg.base_config = &muxer_cfg_data.flv.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.flv);
            break;
        case ESP_MUXER_TYPE_WAV:
            muxer_cfg.base_config = &muxer_cfg_data.wav.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.wav);
            break;
        case ESP_MUXER_TYPE_CAF:
            muxer_cfg.base_config = &muxer_cfg_data.caf.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.caf);
            break;
        case ESP_MUXER_TYPE_OGG:
            muxer_cfg.base_config = &muxer_cfg_data.ogg.base_config;
            muxer_cfg.cfg_size = sizeof(muxer_cfg_data.ogg);
            break;
        default:
            ESP_LOGE(TAG, "Unsupported muxer type: %x", (int)muxer_type);
            return ESP_ERR_NOT_SUPPORTED;
    }
    muxer_cfg.base_config->muxer_type = muxer_type;
    if (stream->storage_dir != NULL || stream->storage_url != NULL) {
        muxer_cfg.base_config->url_pattern_ex = storage_url_pattern;
    }
    muxer_cfg.base_config->ctx = stream;
    muxer_cfg.base_config->slice_duration = stream->muxer.slice_duration ?
                                            stream->muxer.slice_duration :
                                            ESP_CAPTURE_SERVICE_STORAGE_SLICE_MS;
    muxer_cfg.base_config->ram_cache_size = stream->muxer.ram_cache_size;

    esp_err_t ret = capture_err_to_esp(esp_capture_sink_add_muxer(stream->sink, &muxer_cfg));
    if (ret != ESP_OK) {
        return ret;
    }
    stream->storage_configured = true;
    stream->storage_pending = false;
    stream->muxer.muxer_type = muxer_type;
    if (stream->muxer.auto_record) {
        esp_capture_sink_enable_muxer(stream->sink, true);
    }
    if (stream->muxer.streaming == false || support_streaming(muxer_type) == false) {
        esp_capture_sink_disable_stream(stream->sink, ESP_CAPTURE_STREAM_TYPE_MUXER);
    }
    return ret;
}

static esp_err_t capture_service_setup_pending_storage(esp_capture_service_t *service)
{
    if (service == NULL || service->streams == NULL) {
        return ESP_OK;
    }
    for (uint16_t i = 0; i < service->stream_num; i++) {
        capture_stream_t *stream = &service->streams[i];
        if (!stream->storage_pending || stream->storage_configured || stream->sink == NULL) {
            continue;
        }
        esp_err_t ret = capture_service_setup_storage(stream);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_capture_stream_type_t track_type_to_stream_type(esp_media_track_type_t type)
{
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_CAPTURE_STREAM_TYPE_AUDIO;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return ESP_CAPTURE_STREAM_TYPE_VIDEO;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_MUXER) {
        return ESP_CAPTURE_STREAM_TYPE_MUXER;
    }
    return ESP_CAPTURE_STREAM_TYPE_NONE;
}

static int find_track_index_by_stream(const capture_stream_t *stream, esp_capture_stream_type_t stream_type)
{
    for (uint8_t i = 0; i < stream->track_num; i++) {
        if (track_type_to_stream_type(stream->tracks[i].type) == stream_type) {
            return i;
        }
    }
    return -1;
}

static esp_err_t capture_frame_to_media_frame(const capture_stream_t *stream,
                                              const esp_capture_stream_frame_t *capture_frame,
                                              esp_media_frame_t *media_frame)
{
    int track_idx = find_track_index_by_stream(stream, capture_frame->stream_type);
    if (track_idx < 0) {
        return ESP_ERR_NOT_FOUND;
    }
    memset(media_frame, 0, sizeof(*media_frame));
    media_frame->track_id = stream->tracks[track_idx].id;
    media_frame->type = stream->tracks[track_idx].type;
    media_frame->data = capture_frame->data;
    media_frame->size = capture_frame->size;
    media_frame->pts = (int64_t)capture_frame->pts;
    media_frame->dts = media_frame->pts;
    return ESP_OK;
}

static esp_capture_stream_type_t first_stream_type(const capture_stream_t *stream)
{
    if (stream->track_num == 0) {
        return ESP_CAPTURE_STREAM_TYPE_NONE;
    }
    return track_type_to_stream_type(stream->tracks[0].type);
}

static esp_err_t provider_get_track_num(void *ctx, uint16_t *out_num)
{
    if (ctx == NULL || out_num == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_num = ((capture_stream_t *)ctx)->track_num;
    return ESP_OK;
}

static esp_err_t provider_get_track_info(void *ctx, uint16_t index, esp_media_track_info_t *out_info)
{
    capture_stream_t *stream = (capture_stream_t *)ctx;
    if (stream == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= stream->track_num) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = stream->tracks[index];
    return ESP_OK;
}

static esp_err_t provider_set_event_cb(void *ctx, esp_media_provider_event_cb_t cb, void *event_ctx)
{
    capture_stream_t *stream = (capture_stream_t *)ctx;
    if (stream == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    stream->event_cb = cb;
    stream->event_ctx = event_ctx;
    return ESP_OK;
}

static void capture_stream_notify_tracks_abort(capture_stream_t *stream)
{
    if (stream == NULL || stream->provider_aborted) {
        return;
    }
    stream->provider_aborted = true;
    if (stream->event_cb != NULL) {
        stream->event_cb(ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT, NULL, stream->event_ctx);
    }
}

static esp_err_t capture_stream_abort_provider(capture_stream_t *stream, bool disable_sink)
{
    if (stream == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (stream->provider_aborted) {
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    if (disable_sink) {
        if (stream->sink == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        ret = capture_err_to_esp(esp_capture_sink_enable(stream->sink, ESP_CAPTURE_RUN_MODE_DISABLE));
    }
    capture_stream_notify_tracks_abort(stream);
    return ret;
}

static bool global_cache_try_acquire(capture_stream_t *stream, esp_capture_stream_frame_t *capture_frame)
{
    for (uint8_t i = 0; i < stream->track_num; i++) {
        esp_capture_stream_type_t stream_type = track_type_to_stream_type(stream->tracks[i].type);
        if (stream_type == ESP_CAPTURE_STREAM_TYPE_NONE) {
            continue;
        }
        capture_frame->stream_type = stream_type;
        if (esp_capture_sink_acquire_frame(stream->sink, capture_frame, true) == ESP_CAPTURE_ERR_OK) {
            return true;
        }
    }
    return false;
}

/**
 * @brief  Acquire one frame, honoring `timeout_ms` semantics.
 *
 *         The underlying sink API only supports blocking / non-blocking, so a
 *         finite timeout is emulated with a poll loop:
 *           - `timeout_ms == 0`: single non-blocking attempt.
 *           - `timeout_ms == ESP_CAPTURE_SERVICE_WAIT_FOREVER`: block until a
 *             frame is available or the provider is aborted.
 *           - otherwise: poll every ESP_CAPTURE_SERVICE_POLL_STEP_MS until a
 *             frame arrives, the timeout elapses, or the provider is aborted.
 *
 *         When the linked sink requested a global cache (`need_global_cache`)
 *         and the caller does not pin a track type, all tracks are scanned in
 *         arrival order. The loop always bails out once `provider_aborted` is set
 *         so a stopping provider can never dead-loop.
 */
static esp_err_t provider_acquire_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    capture_stream_t *stream = (capture_stream_t *)ctx;
    if (stream == NULL || out_frame == NULL || stream->sink == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_capture_service_t *service = stream->service;
    bool global = (service != NULL && service->use_global_cache && out_frame->type == ESP_MEDIA_TRACK_TYPE_UNKNOWN);
    esp_capture_stream_frame_t capture_frame = {0};

    if (!global) {
        capture_frame.stream_type = track_type_to_stream_type(out_frame->type);
        if (capture_frame.stream_type == ESP_CAPTURE_STREAM_TYPE_NONE) {
            capture_frame.stream_type = first_stream_type(stream);
        }
        if (capture_frame.stream_type == ESP_CAPTURE_STREAM_TYPE_NONE) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    esp_capture_err_t ret = ESP_CAPTURE_ERR_NOT_FOUND;
    uint32_t waited = 0;
    while (true) {
        bool found;
        if (global) {
            found = global_cache_try_acquire(stream, &capture_frame);
            ret = found ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_NOT_FOUND;
        } else {
            /* Only let the sink block when the caller asked to wait forever; a
               finite timeout must stay responsive to abort. */
            bool no_wait = (timeout_ms != ESP_CAPTURE_SERVICE_WAIT_FOREVER);
            ret = esp_capture_sink_acquire_frame(stream->sink, &capture_frame, no_wait);
            found = (ret == ESP_CAPTURE_ERR_OK);
        }
        if (found) {
            return capture_frame_to_media_frame(stream, &capture_frame, out_frame);
        }
        if (timeout_ms == 0 || stream->provider_aborted) {
            ret = (stream->provider_aborted ? ESP_CAPTURE_ERR_INVALID_STATE : ESP_CAPTURE_ERR_TIMEOUT);
            break;
        }
        if (timeout_ms != ESP_CAPTURE_SERVICE_WAIT_FOREVER && waited >= timeout_ms) {
            ret = ESP_CAPTURE_ERR_TIMEOUT;
            break;
        }
        /* The forever-blocking non-global path already slept inside the sink. */
        if (global || timeout_ms != ESP_CAPTURE_SERVICE_WAIT_FOREVER) {
            uint32_t step = ESP_CAPTURE_SERVICE_POLL_STEP_MS;
            if (timeout_ms != ESP_CAPTURE_SERVICE_WAIT_FOREVER && (timeout_ms - waited) < step) {
                step = pdMS_TO_TICKS(timeout_ms - waited);
            }
            if (step == 0) {
                step = 1;
            }
            vTaskDelay(step);
            waited += step;
        }
    }
    return capture_err_to_esp(ret);
}

static esp_err_t provider_release_frame(void *ctx, esp_media_frame_t *frame)
{
    capture_stream_t *stream = (capture_stream_t *)ctx;
    if (stream == NULL || frame == NULL || stream->sink == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_capture_stream_frame_t capture_frame = {
        .stream_type = track_type_to_stream_type(frame->type),
        .pts = frame->pts,
        .data = frame->data,
        .size = frame->size,
    };
    return capture_err_to_esp(esp_capture_sink_release_frame(stream->sink, &capture_frame));
}

static esp_err_t capture_stream_restore_after_abort(capture_stream_t *stream)
{
    if (stream == NULL || !stream->provider_aborted || !stream->enabled || stream->sink == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = capture_err_to_esp(
        esp_capture_sink_enable(stream->sink, ESP_CAPTURE_RUN_MODE_ALWAYS));
    if (ret == ESP_OK) {
        stream->provider_aborted = false;
    }
    return ret;
}

static esp_err_t capture_service_restore_aborted_streams(esp_capture_service_t *service)
{
    if (service == NULL || service->streams == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    for (uint16_t j = 0; j < service->stream_num; j++) {
        esp_err_t restore_ret = capture_stream_restore_after_abort(&service->streams[j]);
        if (restore_ret != ESP_OK && ret == ESP_OK) {
            ret = restore_ret;
        }
    }
    return ret;
}

static esp_err_t provider_abort(void *ctx)
{
    return capture_stream_abort_provider((capture_stream_t *)ctx, true);
}

static esp_err_t provider_read_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    if (out_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    void *buffer = out_frame->data;
    size_t buffer_size = out_frame->size;
    esp_err_t ret = provider_acquire_frame(ctx, out_frame, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    do {
        if (out_frame->size > buffer_size) {
            ret = ESP_ERR_INVALID_SIZE;
            break;
        }
        if (out_frame->size > 0 && buffer == NULL) {
            ret = ESP_ERR_INVALID_ARG;
            break;
        }
        size_t capture_size = out_frame->size;
        if (capture_size > 0) {
            memcpy(buffer, out_frame->data, capture_size);
        }
        ret = provider_release_frame(ctx, out_frame);
        /* Always hand the caller-owned buffer back, even when release fails, so
           we never leak a capture-owned pointer out of the service. */
        out_frame->data = buffer;
        out_frame->size = capture_size;
        return ret;
    } while (0);
    provider_release_frame(ctx, out_frame);
    out_frame->data = buffer;
    out_frame->size = buffer_size;
    return ret;
}

static esp_err_t capture_service_on_start(esp_service_t *base)
{
    esp_capture_service_t *service = (esp_capture_service_t *)base;
    if (!service->configured || !service->enabled || service->capture == NULL) {
        return ESP_OK;
    }
    return capture_service_start_writers(service);
}

static esp_err_t capture_service_on_stop(esp_service_t *base)
{
    esp_capture_service_t *service = (esp_capture_service_t *)base;
    if (!service->configured || !service->enabled || service->capture == NULL) {
        return ESP_OK;
    }
    if (service->streams != NULL) {
        for (uint16_t j = 0; j < service->stream_num; j++) {
            (void)capture_stream_abort_provider(&service->streams[j], false);
        }
    }
    return capture_err_to_esp(esp_capture_stop(service->capture));
}

static esp_err_t capture_service_on_deinit(esp_service_t *base)
{
    esp_capture_service_t *service = (esp_capture_service_t *)base;
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    destroy_capture(service);
    if (service->deinit_cb != NULL) {
        esp_err_t ret = service->deinit_cb(service, service->user_data);
        service->user_data = NULL;
        service->deinit_cb = NULL;
        return ret;
    }
    return ESP_OK;
}

static esp_err_t capture_service_find_stream(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                             capture_stream_t **out_stream)
{
    if (service == NULL || out_stream == NULL || stream >= service->max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    if (service->streams == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    capture_stream_t *stream_state = &service->streams[stream];
    if (!stream_state->configured) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_stream = stream_state;
    return ESP_OK;
}

static void destroy_stream(capture_stream_t *stream)
{
    free(stream->storage_url);
    free(stream->storage_dir);
    free(stream->last_storage_url);
    memset(stream, 0, sizeof(*stream));
}

static void destroy_capture(esp_capture_service_t *service)
{
    if (service->streams != NULL) {
        if (service->capture != NULL) {
            esp_capture_stop(service->capture);
        }
        for (uint16_t i = 0; i < service->stream_num; i++) {
            destroy_stream(&service->streams[i]);
        }
        free(service->streams);
        service->streams = NULL;
    }
    if (service->capture != NULL) {
        esp_capture_close(service->capture);
        service->capture = NULL;
    }
    service->configured = false;
    service->enabled = false;
    service->use_global_cache = false;
    service->stream_num = 0;
    service->audio_src = NULL;
    service->video_src = NULL;
}

esp_err_t capture_service_teardown(esp_capture_service_t *service)
{
    if (service == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (state == ESP_SERVICE_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!service->configured) {
        return ESP_OK;
    }
    destroy_capture(service);
    return ESP_OK;
}

esp_err_t esp_capture_service_create(const esp_capture_service_cfg_t *cfg, esp_capture_service_t **out_service)
{
    if (cfg == NULL || out_service == NULL || cfg->max_stream_num == 0) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid create arguments");
    }
    esp_capture_service_t *service = calloc(1, sizeof(*service));
    if (service == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Allocate service failed");
    }
    service->max_stream_num = cfg->max_stream_num;
    static const esp_media_service_ops_t s_media_ops = {
        .get_role     = media_get_role,
        .get_provider = media_get_provider,
        .set_provider = NULL,
        .set_request  = media_set_request,
    };
    static const esp_service_ops_t s_service_ops = {
        .on_deinit = capture_service_on_deinit,
        .on_start  = capture_service_on_start,
        .on_stop   = capture_service_on_stop,
    };

    esp_media_service_config_t media_cfg = {
        .name = cfg->name ? cfg->name : ESP_CAPTURE_SERVICE_NAME,
        .service_ops = &s_service_ops,
        .media_ops = &s_media_ops,
    };
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        free(service);
        RET_FOR(ret, "Initialize media service failed");
    }
    // TODO only support one service currently
    scheduler_names = service->media.base.name;
    esp_capture_err_t capt_ret = esp_capture_set_thread_scheduler(capture_service_scheduler);
    if (capt_ret != ESP_CAPTURE_ERR_OK) {
        esp_media_service_deinit(&service->media.base);
        free(service);
        RET_FOR(capture_err_to_esp(capt_ret), "Set scheduler failed");
    }
    *out_service = service;
    return ESP_OK;
}

esp_err_t esp_capture_service_destroy(esp_capture_service_t *service)
{
    if (service == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid service");
    }
    /* Destroy is best-effort: the service memory is always released, but the
       deinit/callback result is propagated so callers can log a failure. */
    esp_err_t ret = esp_media_service_deinit(&service->media.base);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s:%d Deinitialize media service failed", __func__, __LINE__);
    }
    free(service);
    return ret;
}

esp_err_t esp_capture_service_set_deinit_cb(esp_capture_service_t *service,
                                            esp_capture_service_deinit_cb_t deinit_cb,
                                            void *user_data)
{
    if (service == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid service");
    }
    service->deinit_cb = deinit_cb;
    service->user_data = user_data;
    return ESP_OK;
}

static bool service_is_running(esp_capture_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK) {
        return false;
    }
    return state == ESP_SERVICE_STATE_RUNNING;
}

esp_err_t esp_capture_service_set_audio_src_fixed_caps(esp_capture_service_t *service,
                                                       const esp_capture_audio_info_t *fixed_caps)
{
    if (service == NULL || fixed_caps == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid fixed caps");
    }
    /* Fixed caps shape source negotiation, so they must be set before any capture
       instance opens/negotiates. Reject changes while the service is running. */
    if (service_is_running(service)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Service is running");
    }
    if (service->audio_src == NULL || service->audio_src->set_fixed_caps == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Audio source unavailable");
    }
    RET_CHK(capture_err_to_esp(service->audio_src->set_fixed_caps(service->audio_src, fixed_caps)),
            "Set fixed caps failed");
    return ESP_OK;
}

esp_err_t esp_capture_service_get_capture_handle(esp_capture_service_t *service,
                                                 esp_capture_handle_t *out_capture)
{
    if (service == NULL || out_capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid capture handle arguments");
    }
    if (service->capture == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_capture = service->capture;
    return ESP_OK;
}

esp_err_t esp_capture_service_get_sink_handle(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                              esp_capture_sink_handle_t *out_sink)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (out_sink == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid sink output");
    }
    *out_sink = stream_state->sink;
    return ESP_OK;
}

esp_err_t esp_capture_service_get_provider(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                           esp_media_provider_t *out_provider)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (out_provider == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid provider output");
    }
    static const esp_media_provider_ops_t s_provider_ops = {
        .get_track_num  = provider_get_track_num,
        .get_track_info = provider_get_track_info,
        .set_event_cb   = provider_set_event_cb,
        .acquire_frame  = provider_acquire_frame,
        .read_frame     = provider_read_frame,
        .release_frame  = provider_release_frame,
        .abort          = provider_abort,
    };
    out_provider->ops = &s_provider_ops;
    out_provider->ctx = stream_state;
    return ESP_OK;
}

esp_err_t esp_capture_service_set_storage_url(esp_capture_service_t *service, esp_media_stream_id_t stream, const char *url)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    ret = capture_service_ensure_storage_url_parent(url);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Prepare storage directory failed");
    }
    /* Resolve the container type for the new URL:
         - infer from the URL extension when recognizable;
         - otherwise keep the muxer type already configured by setup;
         - otherwise fall back to the default container.
       This lets callers pass signed/extension-less URLs once a muxer type is
       known, instead of being rejected for an unknown extension. */
    esp_muxer_type_t muxer_type = stream_state->muxer.muxer_type;
    if (url != NULL) {
        esp_muxer_type_t guessed = ESP_CAPTURE_SERVICE_MUXER_NONE;
        if (guess_muxer_type(url, &guessed) == ESP_OK) {
            muxer_type = guessed;
        } else if (muxer_type == ESP_CAPTURE_SERVICE_MUXER_NONE) {
            ret = guess_muxer_type(NULL, &muxer_type);
            if (ret != ESP_OK) {
                RET_FOR(ret, "Resolve muxer type failed");
            }
        }
        /* A muxer that is already bound to a container cannot switch type. The
           caller must stop recording and re-apply setup to change container. */
        if (stream_state->storage_configured && stream_state->muxer.muxer_type != muxer_type) {
            RET_FOR(ESP_ERR_INVALID_STATE, "Muxer type cannot change");
        }
    }

    char *copy = NULL;
    if (url != NULL) {
        copy = strdup(url);
        if (copy == NULL) {
            RET_FOR(ESP_ERR_NO_MEM, "Copy storage URL failed");
        }
    }
    free(stream_state->storage_url);
    stream_state->storage_url = copy;
    if (url != NULL) {
        stream_state->muxer.muxer_type = muxer_type;
    }

    /* When the muxer is already added, the slice url_pattern callback reads
       storage_url lazily, so a new URL automatically applies to the next record
       (stop_record -> set_storage_url -> start_record) without rebinding. */
    if (stream_state->storage_configured) {
        return ESP_OK;
    }
    /* Defer adding the muxer until start so the URL/type can change repeatedly
       while stopped. If the service is already running, add it now (only valid
       when the underlying capture has not started yet). */
    stream_state->storage_pending = true;
    if (service_is_running(service)) {
        RET_CHK(capture_service_setup_storage(stream_state), "Configure storage failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_get_last_storage_url(esp_capture_service_t *service,
                                                   esp_media_stream_id_t stream,
                                                   const char **out_url)
{
    if (out_url == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid URL output");
    }
    *out_url = NULL;
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (stream_state->last_storage_url == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_url = stream_state->last_storage_url;
    return ESP_OK;
}

esp_err_t esp_capture_service_get_storage_file_info(esp_capture_service_t *service,
                                                    esp_media_stream_id_t stream,
                                                    const char *url,
                                                    esp_capture_service_storage_file_info_t *out_info)
{
    if (out_info == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid file info output");
    }
    memset(out_info, 0, sizeof(*out_info));

    const char *path = (url != NULL && url[0] != '\0') ? url : NULL;
    if (path == NULL) {
        esp_err_t ret = esp_capture_service_get_last_storage_url(service, stream, &path);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Resolve storage URL failed");
        }
    }
    if (path == NULL || path[0] == '\0') {
        RET_FOR(ESP_ERR_INVALID_ARG, "Storage path is empty");
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            RET_FOR(ESP_ERR_NOT_FOUND, "Storage file not found");
        }
        ESP_LOGE(TAG, "stat(%s) failed: errno %d (%s)", path, errno, strerror(errno));
        return ESP_FAIL;
    }
    if (!S_ISREG(st.st_mode)) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Storage path is not a regular file");
    }

    out_info->size = (int64_t)st.st_size;
    out_info->mtime = (int64_t)st.st_mtime;
    out_info->ctime = (int64_t)st.st_ctime;
    return ESP_OK;
}

esp_err_t esp_capture_service_start_record(esp_capture_service_t *service, esp_media_stream_id_t stream)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (stream_state->storage_pending) {
        stream_state->muxer.auto_record = true;
        return ESP_OK;
    }
    if (!stream_state->storage_configured) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Storage is not configured");
    }
    RET_CHK(capture_err_to_esp(esp_capture_sink_enable_muxer(stream_state->sink, true)),
            "Start record failed");
    return ESP_OK;
}

esp_err_t esp_capture_service_stop_record(esp_capture_service_t *service, esp_media_stream_id_t stream)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    RET_CHK(capture_err_to_esp(esp_capture_sink_enable_muxer(stream_state->sink, false)),
            "Stop record failed");
    return ESP_OK;
}

esp_err_t esp_capture_service_enable_stream(esp_capture_service_t *service, esp_media_stream_id_t stream, bool enable)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    stream_state->enabled = enable;
    ret = capture_err_to_esp(esp_capture_sink_enable(
        stream_state->sink, enable ? ESP_CAPTURE_RUN_MODE_ALWAYS : ESP_CAPTURE_RUN_MODE_DISABLE));
    if (ret == ESP_OK && enable) {
        stream_state->provider_aborted = false;
    }
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set stream state failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_enable_track(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                           esp_media_track_type_t track_type, bool enable)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (enable) {
        ESP_LOGW(TAG, "Capture stream disable is static; enable requires sink re-setup");
        return ESP_ERR_NOT_SUPPORTED;
    }
    RET_CHK(capture_err_to_esp(
                esp_capture_sink_disable_stream(stream_state->sink, media_type_to_capture_stream(track_type))),
            "Disable track failed");
    return ESP_OK;
}

esp_err_t esp_capture_service_one_shot(esp_capture_service_t *service, esp_media_stream_id_t stream)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    RET_CHK(capture_err_to_esp(
                esp_capture_sink_enable(stream_state->sink, ESP_CAPTURE_RUN_MODE_ONESHOT)),
            "Trigger one-shot failed");
    return ESP_OK;
}

esp_err_t esp_capture_service_acquire_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                            esp_media_frame_t *frame, uint32_t timeout_ms)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid frame");
    }
    ret = provider_acquire_frame(stream_state, frame, timeout_ms);
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
        RET_FOR(ret, "Acquire frame failed");
    }
    return ret;
}

esp_err_t esp_capture_service_read_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                         esp_media_frame_t *frame, uint32_t timeout_ms)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    ret = provider_read_frame(stream_state, frame, timeout_ms);
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
        RET_FOR(ret, "Read frame failed");
    }
    return ret;
}

esp_err_t esp_capture_service_release_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                            esp_media_frame_t *frame)
{
    capture_stream_t *stream_state = NULL;
    esp_err_t ret = capture_service_find_stream(service, stream, &stream_state);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Find stream failed");
    }
    if (frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid frame");
    }
    RET_CHK(provider_release_frame(stream_state, frame), "Release frame failed");
    return ESP_OK;
}
