/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_audio_capture_scheduler.h"
#include "esp_audio_capture_service.h"
#include "esp_service_scheduler.h"

#include "audio_record_scheduler.h"
#include "esp_media_dummy_service.h"

static esp_err_t service_scheduler_cb(const esp_service_thread_request_t *request,
                                      esp_service_thread_cfg_t *cfg,
                                      void *ctx)
{
    (void)ctx;
    if (request == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    printf("sch for %s:%s\n", request->service_name, request->thread_name);

    /* Capture pipeline + AI tasks use the audio capture service name. */
    if (ESP_SERVICE_SCHEDULER_SERVICE_NAME_IS(request, ESP_AUDIO_CAPTURE_SERVICE_NAME)) {
        if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AUD_SRC)) {
            cfg->priority = 15;
            cfg->core_id = 0;
            return ESP_OK;
        }
        if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_0) ||
            ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AENC_1)) {
            cfg->stack_size = 40 * 1024;
            cfg->priority = 5;
            cfg->core_id = 1;
            return ESP_OK;
        }
        if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FEED) ||
            ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH) ||
            ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AI_PIPE)) {
            cfg->stack_size = 8 * 1024;
            if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FEED)) {
                cfg->core_id = 1;
                cfg->priority = 15;
            } else {
                cfg->core_id = 0;
                cfg->priority = 10;
            }
            if (ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH)) {
                cfg->priority = 16;
            }
            return ESP_OK;
        }
        return ESP_ERR_NOT_FOUND;
    }

    if (ESP_SERVICE_SCHEDULER_SERVICE_NAME_IS(request, ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SINK_NAME) &&
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, "media_dummy_sink")) {
        cfg->stack_size = 4 * 1024;
        cfg->priority = 8;
        cfg->core_id = 1;
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t audio_record_scheduler_install(void)
{
    return esp_service_scheduler_set_cb(service_scheduler_cb, NULL);
}
