/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_media_dummy_service.h"
#include "esp_muxer_scheduler.h"
#include "esp_muxer_service.h"
#include "esp_service_scheduler.h"

#include "muxer_scheduler.h"

static esp_err_t service_scheduler_cb(const esp_service_thread_request_t *request,
                                      esp_service_thread_cfg_t *cfg,
                                      void *ctx)
{
    (void)ctx;
    if (request == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Match default name and suffixes such as esp_muxer_service_mcp. */
    if (request->service_name != NULL &&
        strncmp(request->service_name, ESP_MUXER_SERVICE_NAME, sizeof(ESP_MUXER_SERVICE_NAME) - 1) == 0 &&
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_MUXER_SCHED_TASK)) {
        cfg->stack_size = 8 * 1024;
        cfg->priority = 10;
        cfg->core_id = 0;
        return ESP_OK;
    }
    if (ESP_SERVICE_SCHEDULER_SERVICE_NAME_IS(request, ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SRC_NAME)) {
        cfg->stack_size = 6 * 1024;
        cfg->priority = 8;
        cfg->core_id = 0;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t muxer_scheduler_install(void)
{
    return esp_service_scheduler_set_cb(service_scheduler_cb, NULL);
}
