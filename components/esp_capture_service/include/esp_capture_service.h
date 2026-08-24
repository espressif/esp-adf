/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Capture service handle.
 *
 *         Capture service is a media source service. It keeps the service and
 *         auto-link surface small. Common runtime operations live in
 *         esp_capture_service_ops.h, and setup/bundle APIs live in
 *         esp_capture_service_setup.h.
 */
typedef struct esp_capture_service esp_capture_service_t;

/**
 * @brief  Callback invoked when the capture service is deinitialized
 *
 * @param[in]  service    Capture service handle
 * @param[in]  user_data  User context from esp_capture_service_set_deinit_cb()
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Error returned by the callback implementation
 */
typedef esp_err_t (*esp_capture_service_deinit_cb_t)(esp_capture_service_t *service, void *user_data);

/**
 * @brief  Capture service configuration.
 */
typedef struct {
    const char *name;            /*!< Service name for esp_service/service manager */
    uint16_t    max_stream_num;  /*!< Maximum output streams */
} esp_capture_service_cfg_t;

#define ESP_CAPTURE_SERVICE_NAME  "capture_service"

#define ESP_CAPTURE_SERVICE_CFG_DEFAULT() {      \
    .name           = ESP_CAPTURE_SERVICE_NAME,  \
    .max_stream_num = 1,                         \
}

/**
 * @brief  Service name used when forwarding esp_capture threads to esp_service_scheduler.
 *
 *         Capture pipeline threads (e.g. `venc_0`, `aenc_0`, `AUD_SRC`, `VID_SRC`)
 *         are requested under this name. Applications should tune them through
 *         `esp_service_scheduler_set_cb()` instead of `esp_capture_set_thread_scheduler()`.
 *         The bridge is installed automatically by `esp_capture_service_create()`.
 */
#define ESP_CAPTURE_SERVICE_SCHEDULER_NAME  "esp_capture"

/**
 * @brief  Create capture service.
 *
 * @param[in]   cfg          Capture service configuration
 * @param[out]  out_service  Created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t esp_capture_service_create(const esp_capture_service_cfg_t *cfg,
                                     esp_capture_service_t **out_service);

/**
 * @brief  Destroy capture service.
 *
 * @param[in]  service  Capture service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_capture_service_destroy(esp_capture_service_t *service);

/**
 * @brief  Set the callback invoked when the service is deinitialized
 *
 * @param[in]  service    Capture service handle
 * @param[in]  deinit_cb  Callback to invoke, or NULL to clear it
 * @param[in]  user_data  User context passed to deinit_cb
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service is NULL
 */
esp_err_t esp_capture_service_set_deinit_cb(esp_capture_service_t *service,
                                            esp_capture_service_deinit_cb_t deinit_cb,
                                            void *user_data);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
