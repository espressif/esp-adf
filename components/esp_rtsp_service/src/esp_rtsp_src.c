/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rtsp_service_priv.h"

static const char *TAG = "RTSP_SRC";

static int src_state_handler(esp_rtsp_state_t event, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    if (service == NULL) {
        return -1;
    }
    if (event == RTSP_STATE_TEARDOWN) {
        if (service->src.mngr != NULL) {
            esp_media_track_write_abort(service->src.mngr);
        }
    }
    return rtsp_service_state_handler(event, ctx);
}

static uint32_t src_audio_frame_duration_ms(const esp_rtsp_aud_info_t *aud_info)
{
    if (aud_info == NULL) {
        return 0;
    }
    if (aud_info->aud_codec == RTSP_ACODEC_AAC) {
        uint32_t sample_rate = aud_info->sample_rate ? aud_info->sample_rate : 16000;
        uint32_t duration_ms = (1000U * 1024U) / sample_rate;
        return duration_ms > 0 ? duration_ms : 64;
    }
    /* G.711 is commonly packetized as 20 ms. */
    return 20;
}

static uint32_t src_video_frame_duration_ms(const esp_rtsp_video_info_t *vid_info)
{
    if (vid_info == NULL || vid_info->fps <= 0) {
        return 0;
    }
    return 1000U / (uint32_t)vid_info->fps;
}

static uint32_t src_next_pts(esp_rtsp_service_t *service, uint32_t *track_pts, uint32_t frame_duration_ms)
{
    if (frame_duration_ms > 0) {
        uint32_t pts = *track_pts;
        *track_pts += frame_duration_ms;
        return pts;
    }
    int64_t elapsed_us = esp_timer_get_time() - service->src.start_time_us;
    if (elapsed_us < 0) {
        elapsed_us = 0;
    }
    return (uint32_t)(elapsed_us / 1000);
}

esp_err_t rtsp_src_ensure_mngr(esp_rtsp_service_t *service)
{
    if (service->src.mngr != NULL) {
        return ESP_OK;
    }
    if (service->src.audio_cache_size == 0) {
        service->src.audio_cache_size = RTSP_DEFAULT_AUDIO_CACHE_SIZE;
    }
    if (service->src.video_cache_size == 0) {
        service->src.video_cache_size = RTSP_DEFAULT_CACHE_SIZE;
    }
    size_t cache_size = service->src.video_cache_size + service->src.audio_cache_size;
    esp_media_track_mngr_cfg_t cfg = {
        .max_track_num = RTSP_MAX_TRACKS,
        .use_global_cache = service->src.need_global_cache,
        .global_cache = {
            .cache_size = cache_size,
        },
    };
    esp_err_t ret = esp_media_track_mngr_create(&cfg, &service->src.mngr);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->src.mngr, &service->src.provider);
}

static int add_or_update_track(esp_rtsp_service_t *service, const esp_media_track_info_t *info)
{
    uint16_t track_num = 0;
    esp_media_provider_get_track_num(&service->src.provider, &track_num);
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t old = {0};
        if (esp_media_provider_get_track_info(&service->src.provider, i, &old) == ESP_OK && old.id == info->id) {
            return esp_media_track_mngr_update_track(service->src.mngr, i, info) == ESP_OK ? 0 : -1;
        }
    }

    size_t cache_size = (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) ?
                        service->src.audio_cache_size : service->src.video_cache_size;
    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = *info,
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
            .track_cache = {
                .cache_size = cache_size,
                .addr_align = RTSP_DEFAULT_ALIGNMENT,
            },
        },
    };
    return esp_media_track_mngr_add_track(service->src.mngr, &track_cfg) == ESP_OK ? 0 : -1;
}

static int src_stream_codec(esp_rtsp_aud_info_t *aud_info, esp_rtsp_video_info_t *vid_info, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    if (service == NULL || service->src.mngr == NULL) {
        return -1;
    }
    if (aud_info != NULL && service->setup.audio_enable) {
        esp_media_track_info_t info = {
            .id = RTSP_TRACK_AUDIO_ID,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .info.audio = {
                .codec = rtsp_from_codec(aud_info->aud_codec),
                .sample_rate = aud_info->sample_rate,
                .channel = aud_info->channel,
                .bits_per_sample = 16,
            },
        };
        if (info.info.audio.codec == ESP_MEDIA_CODEC_G711A || info.info.audio.codec == ESP_MEDIA_CODEC_G711U) {
            info.info.audio.sample_rate = 8000;
            info.info.audio.channel = 1;
        }
        if (aud_info->sample_rate == 0) {
            info.info.audio.sample_rate = 16000;
        }
        service->src.audio_frame_duration_ms = src_audio_frame_duration_ms(aud_info);
        if (add_or_update_track(service, &info) != 0) {
            return -1;
        }
    }
    if (vid_info != NULL && service->setup.video_enable) {
        esp_media_track_info_t info = {
            .id = RTSP_TRACK_VIDEO_ID,
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
            .info.video = {
                .codec = rtsp_from_codec(vid_info->vcodec),
                .width = vid_info->width,
                .height = vid_info->height,
                .fps = vid_info->fps,
            },
        };
        if (vid_info->fps == 0) {
            info.info.video.fps = RTSP_DEFAULT_VID_FPS;
        }
        service->src.video_frame_duration_ms = src_video_frame_duration_ms(vid_info);
        if (add_or_update_track(service, &info) != 0) {
            return -1;
        }
    }
    return 0;
}

static int src_receive_audio(unsigned char *data, int len, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    if (service == NULL || service->src.mngr == NULL || data == NULL || len <= 0) {
        return -1;
    }
    uint32_t pts = src_next_pts(service, &service->src.audio_pts, service->src.audio_frame_duration_ms);
    esp_media_frame_t frame = {
        .track_id = RTSP_TRACK_AUDIO_ID,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = data,
        .size = (size_t)len,
        .pts = pts,
        .dts = pts,
    };
    int ret = esp_media_track_write_frame(service->src.mngr, &frame, 0) == ESP_OK ? 0 : -1;
    if (ret != 0) {
        ESP_LOGD(TAG, "Write audio stopped");
    }
    return ret;
}

static int src_receive_video(unsigned char *data, int len, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    if (service == NULL || service->src.mngr == NULL || data == NULL || len <= 0) {
        return -1;
    }
    uint32_t pts = src_next_pts(service, &service->src.video_pts, service->src.video_frame_duration_ms);
    esp_media_frame_t frame = {
        .track_id = RTSP_TRACK_VIDEO_ID,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = data,
        .size = (size_t)len,
        .pts = pts,
        .dts = pts,
    };
    int ret = esp_media_track_write_frame(service->src.mngr, &frame, 0) == ESP_OK ? 0 : -1;
    if (ret != 0) {
        ESP_LOGD(TAG, "Write video stopped");
    }
    return ret;
}

static esp_rtsp_data_cb_t s_src_cbs = {
    .receive_audio = src_receive_audio,
    .receive_video = src_receive_video,
    .stream_codec = src_stream_codec,
};

esp_rtsp_data_cb_t *rtsp_src_data_cb(void)
{
    return &s_src_cbs;
}

esp_err_t rtsp_src_on_start(esp_rtsp_service_t *service)
{
    if (service->url == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = rtsp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    service->src.audio_pts = 0;
    service->src.video_pts = 0;
    service->src.audio_frame_duration_ms = 0;
    service->src.video_frame_duration_ms = 0;
    service->src.start_time_us = esp_timer_get_time();
    (void)esp_media_track_clear_abort(service->src.mngr);

    media_lib_thread_cfg_t thread_cfg = {0};
    rtsp_get_media_thread_cfg(service, ESP_RTSP_SERVICE_SRC_TASK_NAME, &thread_cfg);
    esp_rtsp_video_info_t video_info = {
        .vcodec = RTSP_VCODEC_H264,  /* Satisfy client start; PLAY uses SDP */
        .len = rtsp_setup_vid_frame_size(&service->setup),
    };
    esp_rtsp_config_t cfg = {
        .ctx = service,
        .video_enable = service->setup.video_enable,
        .audio_enable = service->setup.audio_enable,
        .uri = service->url,
        .stack_size = thread_cfg.stack_size,
        .task_prio = thread_cfg.priority,
        .mode = RTSP_CLIENT_PLAY,
        .aud_frame_size = rtsp_setup_aud_frame_size(&service->setup),
        .video_info = service->setup.video_enable ? &video_info : NULL,
        .data_cb = rtsp_src_data_cb(),
        .state = src_state_handler,
        .trans = service->setup.transport,
        .local_addr = service->local_addr,
    };
    service->src.handle = esp_rtsp_client_start(&cfg);
    return service->src.handle == NULL ? ESP_FAIL : ESP_OK;
}

esp_err_t rtsp_src_on_stop(esp_rtsp_service_t *service)
{
    if (service->src.mngr != NULL) {
        esp_media_track_write_abort(service->src.mngr);
    }
    if (service->src.handle != NULL) {
        esp_rtsp_client_stop(service->src.handle);
        service->src.handle = NULL;
    }
    return ESP_OK;
}

esp_err_t rtsp_src_get_provider(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_provider_t *out_provider)
{
    if (service == NULL || out_provider == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = rtsp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_provider = service->src.provider;
    return ESP_OK;
}

esp_err_t rtsp_src_set_request(esp_rtsp_service_t *service, esp_media_stream_id_t stream,
                               const esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    service->src.need_global_cache = request->need_global_cache;
    esp_err_t ret = rtsp_src_ensure_mngr(service);
    if (ret != ESP_OK) {
        return ret;
    }
    size_t cache_size = service->src.video_cache_size + service->src.audio_cache_size;
    ret = esp_media_track_mngr_set_global_cache(service->src.mngr, request->need_global_cache,
                                                cache_size);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_track_mngr_get_provider(service->src.mngr, &service->src.provider);
}
