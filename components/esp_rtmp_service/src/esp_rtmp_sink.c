/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_log.h"
#include "esp_rtmp_service_priv.h"

static const char *TAG = "RTMP_SINK";

#define RTMP_PUSH_TASK_EXIT_BIT  (1U << 0)

static esp_err_t configure_push_track_info(esp_rtmp_service_t *service, const esp_media_track_info_t *info)
{
    if (service->sink.handle == NULL || info == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO && !service->sink.audio_info_set) {
        esp_rtmp_audio_info_t audio_info = {
            .codec = rtmp_to_audio_codec(info->info.audio.codec),
            .channel = info->info.audio.channel,
            .bits_per_sample = info->info.audio.bits_per_sample,
            .sample_rate = info->info.audio.sample_rate,
        };
        ret = rtmp_media_err_to_esp(esp_rtmp_push_set_audio_info(service->sink.handle, &audio_info));
        if (ret == ESP_OK) {
            service->sink.audio_info_set = true;
        }
    } else if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO && !service->sink.video_info_set) {
        esp_rtmp_video_info_t video_info = {
            .codec = rtmp_to_video_codec(info->info.video.codec),
            .width = info->info.video.width,
            .height = info->info.video.height,
            .fps = info->info.video.fps,
        };
        ret = rtmp_media_err_to_esp(esp_rtmp_push_set_video_info(service->sink.handle, &video_info));
        if (ret == ESP_OK) {
            service->sink.video_info_set = true;
        }
    }
    return ret;
}

static void clear_track_info(esp_rtmp_service_t *service, const esp_media_track_info_t *info)
{
    if (service == NULL || info == NULL) {
        return;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        service->sink.audio_info_set = false;
    } else if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        service->sink.video_info_set = false;
    }
}

static void provider_event_handler(esp_media_provider_event_t event, const esp_media_track_info_t *info, void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    if (service == NULL) {
        return;
    }
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT) {
        service->sink.task_stop = true;
        return;
    }
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED) {
        clear_track_info(service, info);
        return;
    }
    if (info == NULL) {
        return;
    }
    if (event != ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED && event != ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED) {
        return;
    }
    if (service->sink.handle != NULL) {
        (void)configure_push_track_info(service, info);
    }
}

static esp_err_t configure_push_tracks(esp_rtmp_service_t *service)
{
    if (service->sink.handle == NULL || service->sink.provider.ops == NULL) {
        return ESP_OK;
    }
    uint16_t track_num = 0;
    esp_err_t ret = esp_media_provider_get_track_num(&service->sink.provider, &track_num);
    if (ret != ESP_OK) {
        return ret;
    }
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        ret = esp_media_provider_get_track_info(&service->sink.provider, i, &info);
        if (ret != ESP_OK) {
            continue;
        }
        ret = configure_push_track_info(service, &info);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static esp_err_t push_frame(esp_rtmp_service_t *service, const esp_media_frame_t *frame)
{
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        esp_rtmp_audio_data_t audio_data = {
            .pts = (uint32_t)(frame->pts < 0 ? 0 : frame->pts),
            .data = frame->data,
            .size = frame->size,
        };
        return rtmp_media_err_to_esp(esp_rtmp_push_audio(service->sink.handle, &audio_data));
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        /* H264 push converts Annex-B start codes in place. Copy first so the
         * provider queue payload (and release pointer match) stays intact. */
        uint8_t *copy = NULL;
        esp_rtmp_video_data_t video_data = {
            .pts = (uint32_t)(frame->pts < 0 ? 0 : frame->pts),
            .key_frame = (frame->flags & ESP_MEDIA_FRAME_FLAG_KEY) != 0,
            .data = frame->data,
            .size = frame->size,
        };
        if (frame->data != NULL && frame->size > 0) {
            copy = media_lib_malloc(frame->size);
            if (copy == NULL) {
                return ESP_ERR_NO_MEM;
            }
            memcpy(copy, frame->data, frame->size);
            video_data.data = copy;
        }
        esp_err_t ret = rtmp_media_err_to_esp(esp_rtmp_push_video(service->sink.handle, &video_data));
        media_lib_free(copy);
        return ret;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static void push_task(void *arg)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)arg;
    while (!service->sink.task_stop) {
        esp_media_frame_t frame = {0};
        esp_err_t ret = esp_media_provider_acquire_frame(&service->sink.provider, &frame, 20);
        if (ret != ESP_OK) {
            /* Avoid a busy-loop if a previous release left the track stuck. */
            if (ret != ESP_ERR_TIMEOUT) {
                media_lib_thread_sleep(10);
            }
            continue;
        }
        if (!service->sink.task_stop && service->sink.handle != NULL) {
            ret = push_frame(service, &frame);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Push frame failed: %s", esp_err_to_name(ret));
            }
        }
        ret = esp_media_provider_release_frame(&service->sink.provider, &frame);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Release frame failed: %s", esp_err_to_name(ret));
            media_lib_thread_sleep(10);
        }
    }
    if (service->sink.task_event != NULL) {
        media_lib_event_group_set_bits(service->sink.task_event, RTMP_PUSH_TASK_EXIT_BIT);
    }
    media_lib_thread_destroy(NULL);
}

esp_err_t rtmp_sink_get_request(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(request, 0, sizeof(*request));
    request->need_global_cache = true;
    return ESP_OK;
}

esp_err_t rtmp_sink_set_provider(esp_rtmp_service_t *service, esp_media_stream_id_t stream,
                                 const esp_media_provider_t *provider)
{
    if (service == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    if (service->sink.provider.ops != NULL) {
        esp_media_provider_set_event_cb(&service->sink.provider, NULL, NULL);
    }
    if (provider == NULL || provider->ops == NULL) {
        memset(&service->sink.provider, 0, sizeof(service->sink.provider));
        return ESP_OK;
    }
    service->sink.provider = *provider;
    return esp_media_provider_set_event_cb(&service->sink.provider, provider_event_handler, service);
}

esp_err_t rtmp_sink_on_start(esp_rtmp_service_t *service)
{
    if (service->url == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->sink.provider.ops == NULL) {
        return ESP_OK;
    }
    service->sink.audio_info_set = false;
    service->sink.video_info_set = false;
    media_lib_thread_cfg_t thread_cfg = {0};
    rtmp_get_media_thread_cfg(service, ESP_RTMP_SERVICE_PUSH_TASK_NAME, &thread_cfg);
    rtmp_push_cfg_t push_cfg = {
        .url = service->url,
        .chunk_size = service->chunk_size == 0 ? RTMP_DEFAULT_CHUNK_SIZE : service->chunk_size,
        .thread_cfg = thread_cfg,
        .ssl_cfg = rtmp_url_is_secure(service->url) ? &service->ssl_cfg.client : NULL,
        .event_cb = rtmp_service_protocol_event,
        .ctx = service,
    };
    service->sink.handle = esp_rtmp_push_open(&push_cfg);
    if (service->sink.handle == NULL) {
        return ESP_FAIL;
    }
    esp_err_t ret = configure_push_tracks(service);
    if (ret == ESP_OK) {
        ret = rtmp_media_err_to_esp(esp_rtmp_push_connect(service->sink.handle));
    }
    if (ret != ESP_OK) {
        esp_rtmp_push_close(service->sink.handle);
        service->sink.handle = NULL;
        return ret;
    }

    media_lib_thread_cfg_t task_cfg = {0};
    rtmp_get_media_thread_cfg(service, ESP_RTMP_SERVICE_PUSH_TASK_NAME, &task_cfg);
    if (media_lib_event_group_create(&service->sink.task_event) != ESP_OK) {
        esp_rtmp_push_close(service->sink.handle);
        service->sink.handle = NULL;
        return ESP_ERR_NO_MEM;
    }
    service->sink.task_stop = false;
    if (media_lib_thread_create(&service->sink.task, ESP_RTMP_SERVICE_PUSH_TASK_NAME, push_task, service,
                                task_cfg.stack_size, task_cfg.priority, task_cfg.core_id) != ESP_OK) {
        media_lib_event_group_destroy(service->sink.task_event);
        service->sink.task_event = NULL;
        esp_rtmp_push_close(service->sink.handle);
        service->sink.handle = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t rtmp_sink_on_stop(esp_rtmp_service_t *service)
{
    service->sink.task_stop = true;
    if (service->sink.provider.ops != NULL) {
        esp_media_provider_abort(&service->sink.provider);
    }
    if (service->sink.task != NULL) {
        if (service->sink.task_event != NULL) {
            media_lib_event_group_wait_bits(service->sink.task_event, RTMP_PUSH_TASK_EXIT_BIT,
                                            0xFFFFFFFF);
        }
        service->sink.task = NULL;
    }
    if (service->sink.task_event != NULL) {
        media_lib_event_group_destroy(service->sink.task_event);
        service->sink.task_event = NULL;
    }
    if (service->sink.handle != NULL) {
        esp_rtmp_push_close(service->sink.handle);
        service->sink.handle = NULL;
    }
    return ESP_OK;
}
