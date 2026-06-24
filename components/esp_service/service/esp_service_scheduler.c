/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_service_scheduler.h"

#define SERVICE_DEFAULT_SCHEDULER()                      \
    (esp_service_thread_cfg_t)                           \
    {                                                    \
        .stack_size = 4 * 1024,  \
        .priority = 5,  \
        .core_id = ESP_SERVICE_THREAD_CORE_NO_AFFINITY,  \
        .is_ext = false,                                 \
    }

static const char *TAG = "SERVICE_SCHEDULER";

static esp_service_scheduler_cb_t s_scheduler_cb;
static void *s_scheduler_ctx;

esp_err_t esp_service_scheduler_set_cb(esp_service_scheduler_cb_t cb, void *ctx)
{
    s_scheduler_cb = cb;
    s_scheduler_ctx = ctx;
    return ESP_OK;
}

esp_err_t esp_service_scheduler_get_thread_cfg(const esp_service_thread_request_t *request,
                                               const esp_service_thread_cfg_t *default_cfg,
                                               esp_service_thread_cfg_t *out_cfg)
{
    if (request == NULL || request->service_name == NULL || request->thread_name == NULL || out_cfg == NULL) {
        ESP_LOGE(TAG, "Invalid get thread cfg args request:%p out_cfg:%p", request, out_cfg);
        return ESP_ERR_INVALID_ARG;
    }

    if (default_cfg != NULL) {
        *out_cfg = *default_cfg;
    } else {
        *out_cfg = SERVICE_DEFAULT_SCHEDULER();
    }
    esp_service_scheduler_cb_t cb = s_scheduler_cb;
    void *ctx = s_scheduler_ctx;
    if (cb == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = cb(request, out_cfg, ctx);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Scheduler callback failed service:%s thread:%s ret:%s",
                 request->service_name, request->thread_name, esp_err_to_name(ret));
    }
    return ret == ESP_ERR_NOT_FOUND ? ESP_OK : ret;
}
