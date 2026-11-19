/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** CPU core selection value for no RTOS task core affinity */
#define ESP_SERVICE_THREAD_CORE_NO_AFFINITY  (-1)

/**
 * @brief  Compare scheduler request service name with a service name macro.
 *
 * @param[in]  _request       Pointer to esp_service_thread_request_t.
 * @param[in]  _service_name  Service name string or macro.
 *
 * @return
 *       - true   Request service name matches _service_name
 *       - false  Request is NULL, service name is NULL, or names differ
 */
#define ESP_SERVICE_SCHEDULER_SERVICE_NAME_IS(_request, _service_name)  \
    ((_request) != NULL && (_request)->service_name != NULL && strcmp((_request)->service_name, (_service_name)) == 0)

/**
 * @brief  Compare scheduler request thread name with a task name macro.
 *
 * @param[in]  _request      Pointer to esp_service_thread_request_t.
 * @param[in]  _thread_name  Thread name string or macro.
 *
 * @return
 *       - true   Request thread name matches _thread_name
 *       - false  Request is NULL, thread name is NULL, or names differ
 */
#define ESP_SERVICE_SCHEDULER_THREAD_NAME_IS(_request, _thread_name)  \
    ((_request) != NULL && (_request)->thread_name != NULL && strcmp((_request)->thread_name, (_thread_name)) == 0)

/**
 * @brief  Thread configuration shared by service implementations.
 */
typedef struct {
    uint32_t  stack_size;  /*!< Stack size in bytes */
    uint8_t   priority;    /*!< RTOS task priority */
    int8_t    core_id;     /*!< CPU core id, or ESP_SERVICE_THREAD_CORE_NO_AFFINITY */
    bool      is_ext;      /*!< Whether put task stack onto SPIRAM or not */
} esp_service_thread_cfg_t;

/**
 * @brief  Thread scheduling request.
 */
typedef struct {
    const char *service_name;      /*!< Service name, for example "esp_rtmp_service" */
    uint16_t    service_inst_idx;  /*!< Service-local instance index */
    const char *thread_name;       /*!< Implementation-specific thread name */
} esp_service_thread_request_t;

/**
 * @brief  Callback used to override service thread settings.
 *
 * @param[in]      request  Thread request identity.
 * @param[in,out]  cfg      Input default config, output final config.
 * @param[in]      ctx      User context registered with esp_service_scheduler_set_cb().
 *
 * @return
 *       - ESP_OK             Configuration overridden successfully
 *       - ESP_ERR_NOT_FOUND  Keep defaults unchanged
 *       - others             Failed to get configuration
 */
typedef esp_err_t (*esp_service_scheduler_cb_t)(const esp_service_thread_request_t *request,
                                                esp_service_thread_cfg_t *cfg,
                                                void *ctx);

/**
 * @brief  Set the scheduler callback.
 *
 * @note  Callback and ctx are published as a pair. Callbacks execute outside
 *        the internal lock. Replacing or clearing the pair does not wait for
 *        earlier callbacks: retain their ctx until all concurrent get calls
 *        finish. Prefer configuring before services start. Not ISR-safe.
 * @param[in]  cb   Callback used to override thread configuration, or NULL to clear.
 * @param[in]  ctx  User context passed to cb.
 *
 * @return
 *       - ESP_OK  On success
 */
esp_err_t esp_service_scheduler_set_cb(esp_service_scheduler_cb_t cb, void *ctx);

/**
 * @brief  Get scheduler configuration for a thread request.
 *
 *         Copies default_cfg into out_cfg, or the built-in defaults (4 KiB
 *         stack, priority 5, ESP_SERVICE_THREAD_CORE_NO_AFFINITY, internal
 *         RAM) when default_cfg is NULL. If a scheduler callback is
 *         registered, its edits are applied to out_cfg only when it returns
 *         ESP_OK; on ESP_ERR_NOT_FOUND or any error out_cfg keeps the defaults.
 *
 * @note  Each call takes a consistent callback/ctx snapshot. Stack size must
 *        be nonzero, priority below configMAX_PRIORITIES, and core_id either
 *        NO_AFFINITY or a configured CPU index. Invalid callback output leaves
 *        defaults unchanged; invalid default_cfg leaves out_cfg unchanged.
 *
 * @param[in]   request      Thread request identity.
 * @param[in]   default_cfg  Default configuration, or NULL to use built-in defaults.
 * @param[out]  out_cfg      Output scheduler configuration.
 *
 * @return
 *       - ESP_OK               On success (including callback returning ESP_ERR_NOT_FOUND)
 *       - ESP_ERR_INVALID_ARG  Required pointer is NULL or thread settings are invalid
 *       - others               Error returned by the scheduler callback; out_cfg holds the defaults
 */
esp_err_t esp_service_scheduler_get_thread_cfg(const esp_service_thread_request_t *request,
                                               const esp_service_thread_cfg_t *default_cfg,
                                               esp_service_thread_cfg_t *out_cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
