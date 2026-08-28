/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_rtmp_service_priv.h"

static const char *TAG = "rtmp_src";

#define RTMP_DEFAULT_ALIGNMENT (64)

esp_err_t rtmp_src_ensure_mngr(esp_rtmp_service_t *service)
{
    if (service->src.mngr != NULL) {
        return ESP_OK;
    }
    esp_media_track_mngr_cfg_t cfg = {
        .max_track_num = RTMP_MAX_TRACKS,
        .use_global_cache = service->src.need_global_cache,
        .global_cache = {
            .cache_size = service->src.cache_size,
        },
    };
    esp_err_t ret = esp_media_track_mngr_create(&cfg, &service->src.mngr);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->src.mngr, &service->src.provider);
}

static int src_stream_cb(rtmp_src_stream_info_t *stream_info, void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    if (service == NULL || service->src.mngr == NULL || stream_info == NULL) {
        return -1;
    }
    esp_media_track_info_t info = {0};
    if (stream_info->stream_type == RTMP_STREAM_TYPE_AUDIO) {
        info.id = RTMP_TRACK_AUDIO_ID;
        info.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
        info.info.audio.codec = rtmp_from_audio_codec(stream_info->audio_info.codec);
        info.info.audio.sample_rate = stream_info->audio_info.sample_rate;
        info.info.audio.bits_per_sample = stream_info->audio_info.bits_per_sample;
        info.info.audio.channel = stream_info->audio_info.channel;
    } else if (stream_info->stream_type == RTMP_STREAM_TYPE_VIDEO) {
        info.id = RTMP_TRACK_VIDEO_ID;
        info.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
        info.info.video.codec = rtmp_from_video_codec(stream_info->video_info.codec);
        info.info.video.width = stream_info->video_info.width;
        info.info.video.height = stream_info->video_info.height;
        info.info.video.fps = stream_info->video_info.fps;
    } else {
        return -1;
    }

    uint16_t track_num = 0;
    esp_media_provider_get_track_num(&service->src.provider, &track_num);
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t old = {0};
        if (esp_media_provider_get_track_info(&service->src.provider, i, &old) == ESP_OK && old.id == info.id) {
            return esp_media_track_mngr_update_track(service->src.mngr, i, &info) == ESP_OK ? 0 : -1;
        }
    }

    size_t cache_size = (info.type == ESP_MEDIA_TRACK_TYPE_AUDIO) ?
                        service->src.audio_cache_size : service->src.video_cache_size;
    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = info,
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
            .track_cache = {
                .cache_size = cache_size,
                .addr_align = RTMP_DEFAULT_ALIGNMENT,
            },
        },
    };
    esp_err_t ret = esp_media_track_mngr_add_track(service->src.mngr, &track_cfg);
    return ret == ESP_OK ? 0 : -1;
}

static int src_data_cb(rtmp_src_stream_data_t *stream_data, void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    if (service == NULL || service->src.mngr == NULL || stream_data == NULL) {
        return -1;
    }

    esp_media_frame_t frame = {0};
    if (stream_data->stream_type == RTMP_STREAM_TYPE_AUDIO) {
        frame.track_id = RTMP_TRACK_AUDIO_ID;
        frame.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
        frame.data = stream_data->audio_data.data;
        frame.size = stream_data->audio_data.size;
        frame.pts = stream_data->audio_data.pts;
        frame.dts = frame.pts;
    } else if (stream_data->stream_type == RTMP_STREAM_TYPE_VIDEO) {
        frame.track_id = RTMP_TRACK_VIDEO_ID;
        frame.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
        frame.data = stream_data->video_data.data;
        frame.size = stream_data->video_data.size;
        frame.pts = stream_data->video_data.pts;
        frame.dts = frame.pts;
        frame.flags = stream_data->video_data.key_frame ? ESP_MEDIA_FRAME_FLAG_KEY : 0;
    } else {
        return -1;
    }

    /* EOS / empty payload from lib during teardown — ignore. */
    if (frame.size == 0 || frame.data == NULL) {
        return 0;
    }

    esp_err_t wret = esp_media_track_write_frame(service->src.mngr, &frame, 0);
    if (wret != ESP_OK) {
        ESP_LOGD(TAG, "Drop frame type:%d size:%u ret:%s",
                 (int)frame.type, (unsigned)frame.size, esp_err_to_name(wret));
    }
    return 0;
}

esp_err_t rtmp_src_on_start(esp_rtmp_service_t *service)
{
    if (service->url == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = rtmp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    (void)esp_media_track_clear_abort(service->src.mngr);
    media_lib_thread_cfg_t thread_cfg = {0};
    rtmp_get_media_thread_cfg(service, ESP_RTMP_SERVICE_SRC_TASK_NAME, &thread_cfg);
    rtmp_src_cfg_t src_cfg = {
        .url = service->url,
        .chunk_size = service->chunk_size == 0 ? RTMP_DEFAULT_CHUNK_SIZE : service->chunk_size,
        .thread_cfg = thread_cfg,
        .ssl_cfg = rtmp_url_is_secure(service->url) ? &service->ssl_cfg.client : NULL,
        .stream_cb = src_stream_cb,
        .data_cb = src_data_cb,
        .event_cb = rtmp_service_protocol_event,
        .ctx = service,
    };
    service->src.handle = esp_rtmp_src_open(&src_cfg);
    if (service->src.handle == NULL) {
        return ESP_FAIL;
    }
    ret = rtmp_media_err_to_esp(esp_rtmp_src_connect(service->src.handle));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RTMP src start failed url:%s ret:%s", service->url, esp_err_to_name(ret));
        esp_rtmp_src_close(service->src.handle);
        service->src.handle = NULL;
    }
    return ret;
}

esp_err_t rtmp_src_on_stop(esp_rtmp_service_t *service)
{
    /* Abort first so teardown EOS callbacks do not block on write. */
    if (service->src.mngr != NULL) {
        esp_media_track_write_abort(service->src.mngr);
    }
    if (service->src.handle != NULL) {
        esp_rtmp_src_close(service->src.handle);
        service->src.handle = NULL;
    }
    return ESP_OK;
}

esp_err_t rtmp_src_get_provider(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_provider_t *out_provider)
{
    if (service == NULL || out_provider == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = rtmp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_provider = service->src.provider;
    return ESP_OK;
}

esp_err_t rtmp_src_set_request(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                               const esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    service->src.need_global_cache = request->need_global_cache;
    esp_err_t ret = rtmp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_media_track_mngr_set_global_cache(service->src.mngr, request->need_global_cache,
                                                service->src.cache_size);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->src.mngr, &service->src.provider);
}
