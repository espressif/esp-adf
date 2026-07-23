/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_media_provider.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"

#include "media_dummy_priv.h"

static const char *TAG = "DUMMY_SINK";

static esp_err_t drain_track(esp_media_dummy_service_t *svc, uint16_t stream, esp_media_track_type_t type)
{
    esp_media_provider_t *provider = &svc->sink_streams[stream].provider;
    esp_media_dummy_stream_stats_t *stats = &svc->sink_streams[stream].stats;
    esp_media_frame_t frame = {
        .type = type,
    };
    /**
     * Take at most one frame per wake. Dummy src tops the queue back up inside
     * acquire/release (feed_stream), so a "drain until empty" loop never exits
     * and starves the rest of the system.
     */
    esp_err_t ret = esp_media_provider_acquire_frame(provider, &frame, 0);
    if (ret != ESP_OK) {
        return ret;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        stats->video_frame_count++;
        stats->video_byte_count += frame.size;
    } else if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        stats->audio_frame_count++;
        stats->audio_byte_count += frame.size;
    }
    esp_media_provider_release_frame(provider, &frame);
    return ESP_OK;
}

static esp_err_t drain_stream(esp_media_dummy_service_t *svc, uint16_t stream)
{
    esp_media_provider_t *provider = &svc->sink_streams[stream].provider;
    uint16_t track_num = 0;
    // Waiting for tracks to be added
    if (esp_media_provider_get_track_num(provider, &track_num) != ESP_OK) {
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(provider, i, &info) != ESP_OK) {
            continue;
        }
        ret = drain_track(svc, stream, info.type);
        if (ret != ESP_OK) {
            if (ret == ESP_ERR_TIMEOUT) {
                continue;
            }
            return ret;
        }
    }
    return ret;
}

static void sink_consume_task(void *ctx)
{
    esp_media_dummy_service_t *svc = ctx;
    while (svc->running) {
        bool linked = false;
        uint16_t err_stream = 0;
        for (uint16_t i = 0; i < svc->max_stream_num; i++) {
            if (svc->sink_streams[i].provider.ops == NULL) {
                continue;
            }
            linked = true;
            esp_err_t ret = drain_stream(svc, i);
            if (ret != ESP_OK) {
                if (ret == ESP_ERR_TIMEOUT) {
                    continue;
                }
                err_stream++;
            }
        }
        if (err_stream == svc->max_stream_num) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(linked ? DUMMY_SINK_BUSY_DELAY_MS : DUMMY_SINK_IDLE_DELAY_MS));
    }
    svc->sink_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t media_dummy_sink_init(esp_media_dummy_service_t *svc)
{
    svc->sink_streams = calloc(svc->max_stream_num, sizeof(*svc->sink_streams));
    return svc->sink_streams ? ESP_OK : ESP_ERR_NO_MEM;
}

void media_dummy_sink_deinit(esp_media_dummy_service_t *svc)
{
    if (svc == NULL) {
        return;
    }
    free(svc->sink_streams);
    svc->sink_streams = NULL;
    svc->sink_task = NULL;
}

esp_err_t media_dummy_sink_set_provider(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                        const esp_media_provider_t *provider)
{
    if (svc == NULL || stream >= svc->max_stream_num || svc->sink_streams == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (provider == NULL) {
        memset(&svc->sink_streams[stream].provider, 0, sizeof(svc->sink_streams[stream].provider));
    } else {
        svc->sink_streams[stream].provider = *provider;
    }
    return ESP_OK;
}

void media_dummy_sink_reset_stats(esp_media_dummy_service_t *svc)
{
    if (svc == NULL || svc->sink_streams == NULL) {
        return;
    }
    for (uint16_t i = 0; i < svc->max_stream_num; i++) {
        memset(&svc->sink_streams[i].stats, 0, sizeof(svc->sink_streams[i].stats));
    }
}

esp_err_t media_dummy_sink_get_stats(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                     esp_media_dummy_stream_stats_t *stats)
{
    if (svc == NULL || stats == NULL || svc->sink_streams == NULL || stream >= svc->max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    *stats = svc->sink_streams[stream].stats;
    return ESP_OK;
}

esp_err_t media_dummy_sink_on_start(esp_media_dummy_service_t *svc)
{
    media_dummy_sink_reset_stats(svc);

    const char *service_name = NULL;
    (void)esp_service_get_name(ESP_SERVICE_BASE(&svc->media), &service_name);

    esp_service_thread_cfg_t default_cfg = {
        .stack_size = DUMMY_SINK_TASK_STACK,
        .priority = DUMMY_SINK_TASK_PRIORITY,
        .core_id = ESP_SERVICE_THREAD_CORE_NO_AFFINITY,
    };
    esp_service_thread_cfg_t task_cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service_name ? service_name : ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SINK_NAME,
        .thread_name = "media_dummy_sink",
    };
    ESP_RETURN_ON_ERROR(esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &task_cfg),
                        TAG, "scheduler");

    svc->running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(sink_consume_task,
                                            "media_dummy_sink",
                                            task_cfg.stack_size,
                                            svc,
                                            task_cfg.priority,
                                            &svc->sink_task,
                                            task_cfg.core_id < 0 ? tskNO_AFFINITY : task_cfg.core_id);
    if (ok != pdPASS) {
        svc->running = false;
        svc->sink_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t media_dummy_sink_on_stop(esp_media_dummy_service_t *svc)
{
    svc->running = false;
    for (uint16_t i = 0; i < svc->max_stream_num; i++) {
        if (svc->sink_streams[i].provider.ops != NULL) {
            (void)esp_media_provider_abort(&svc->sink_streams[i].provider);
        }
    }
    while (svc->sink_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}
