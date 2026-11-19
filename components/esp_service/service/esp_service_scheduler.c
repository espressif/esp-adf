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
#ifndef ESP_PLATFORM
#include <pthread.h>
#endif  /* ESP_PLATFORM */

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
#ifdef ESP_PLATFORM
static portMUX_TYPE s_scheduler_lock = portMUX_INITIALIZER_UNLOCKED;
#else
static pthread_mutex_t s_scheduler_lock = PTHREAD_MUTEX_INITIALIZER;
#endif  /* ESP_PLATFORM */

static void scheduler_lock(void)
{
#ifdef ESP_PLATFORM
    portENTER_CRITICAL(&s_scheduler_lock);
#else
    pthread_mutex_lock(&s_scheduler_lock);
#endif  /* ESP_PLATFORM */
}

static void scheduler_unlock(void)
{
#ifdef ESP_PLATFORM
    portEXIT_CRITICAL(&s_scheduler_lock);
#else
    pthread_mutex_unlock(&s_scheduler_lock);
#endif  /* ESP_PLATFORM */
}

static bool thread_cfg_valid(const esp_service_thread_cfg_t *cfg)
{
#ifdef configNUMBER_OF_CORES
    const int core_count = configNUMBER_OF_CORES;
#else
    const int core_count = portNUM_PROCESSORS;
#endif  /* configNUMBER_OF_CORES */
    return cfg->stack_size > 0 && cfg->priority < configMAX_PRIORITIES &&
           (cfg->core_id == ESP_SERVICE_THREAD_CORE_NO_AFFINITY ||
            (cfg->core_id >= 0 && cfg->core_id < core_count));
}

esp_err_t esp_service_scheduler_set_cb(esp_service_scheduler_cb_t cb, void *ctx)
{
    scheduler_lock();
    s_scheduler_cb = cb;
    s_scheduler_ctx = cb ? ctx : NULL;
    scheduler_unlock();
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

    if (default_cfg && !thread_cfg_valid(default_cfg)) {
        ESP_LOGE(TAG, "Get thread cfg failed: invalid default stack, priority or core");
        return ESP_ERR_INVALID_ARG;
    }
    if (default_cfg != NULL) {
        *out_cfg = *default_cfg;
    } else {
        *out_cfg = SERVICE_DEFAULT_SCHEDULER();
    }
    scheduler_lock();
    esp_service_scheduler_cb_t cb = s_scheduler_cb;
    void *ctx = s_scheduler_ctx;
    scheduler_unlock();
    if (cb == NULL) {
        return ESP_OK;
    }
    /* The callback edits a scratch copy so out_cfg keeps the defaults unless
     * the callback reports success. */
    esp_service_thread_cfg_t cfg = *out_cfg;
    esp_err_t ret = cb(request, &cfg, ctx);
    if (ret == ESP_OK && !thread_cfg_valid(&cfg)) {
        ESP_LOGE(TAG, "Get thread cfg failed: callback returned invalid stack, priority or core");
        return ESP_ERR_INVALID_ARG;
    }
    if (ret == ESP_OK) {
        *out_cfg = cfg;
    } else if (ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "Scheduler callback failed service:%s thread:%s ret:%s",
                 request->service_name, request->thread_name, esp_err_to_name(ret));
    }
    return ret == ESP_ERR_NOT_FOUND ? ESP_OK : ret;
}
