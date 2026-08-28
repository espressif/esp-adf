/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_media_dummy_service.h"
#include "esp_rtmp_scheduler.h"
#include "esp_service_scheduler.h"

#include "rtmp_scheduler.h"

#define SERVICE_NAME_STARTS_WITH(request, prefix)                                  \
    ((request)->service_name != NULL &&                                            \
     strncmp((request)->service_name, (prefix), strlen(prefix)) == 0)

static esp_err_t service_scheduler_cb(const esp_service_thread_request_t *request,
                                      esp_service_thread_cfg_t *cfg,
                                      void *ctx)
{
    (void)ctx;
    if (request == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (SERVICE_NAME_STARTS_WITH(request, ESP_RTMP_SERVICE_NAME) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_RTMP_SERVICE_PUSH_TASK_NAME) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_RTMP_SERVICE_SRC_TASK_NAME) ||
        ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(request, ESP_RTMP_SERVICE_SERVER_TASK_NAME)) {
        cfg->stack_size = 8 * 1024;
        cfg->priority = 10;
        cfg->core_id = 0;
        return ESP_OK;
    }
    if (SERVICE_NAME_STARTS_WITH(request, ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SRC_NAME) ||
        SERVICE_NAME_STARTS_WITH(request, ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SINK_NAME)) {
        cfg->stack_size = 6 * 1024;
        cfg->priority = 8;
        cfg->core_id = 0;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t rtmp_scheduler_install(void)
{
    return esp_service_scheduler_set_cb(service_scheduler_cb, NULL);
}
