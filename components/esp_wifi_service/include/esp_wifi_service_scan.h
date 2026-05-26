/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Wi-Fi scan agent handle
 */
typedef struct esp_wifi_service_scan_agent *esp_wifi_service_scan_handle_t;

/**
 * @brief  Wi-Fi scan completion callback
 *
 *         The AP record list is valid only during synchronous callback delivery.
 *         Callers must copy records that need to outlive the callback.
 *
 * @param[in]  scan_err    Scan result error code
 * @param[in]  ap_count    Number of AP records in @p ap_records
 * @param[in]  ap_records  AP record list, or NULL when @p ap_count is 0
 * @param[in]  ctx         User context from esp_wifi_service_scan_request()
 */
typedef void (*esp_wifi_service_scan_cb_t)(esp_err_t scan_err, uint16_t ap_count,
                                           const wifi_ap_record_t *ap_records, void *ctx);

/**
 * @brief  Create a Wi-Fi scan agent
 *
 * @param[out]  out_handle  Created scan agent handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_NO_MEM       Out of memory
 *       - Others               Event handler registration error
 */
esp_err_t esp_wifi_service_scan_init(esp_wifi_service_scan_handle_t *out_handle);

/**
 * @brief  Destroy a Wi-Fi scan agent
 *
 * @param[in]  handle  Scan agent handle
 */
void esp_wifi_service_scan_deinit(esp_wifi_service_scan_handle_t handle);

/**
 * @brief  Request one non-blocking Wi-Fi scan
 *
 *         Requests using the same scan configuration share one driver scan. Requests with
 *         different configurations are kept for a later scan cycle.
 *
 * @param[in]  handle       Scan agent handle
 * @param[in]  scan_config  Optional Wi-Fi scan config; NULL uses Wi-Fi driver defaults
 * @param[in]  cb           Completion callback
 * @param[in]  ctx          Callback context
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument
 *       - ESP_ERR_INVALID_STATE  Scan agent is not initialized
 *       - ESP_ERR_NO_MEM         Out of memory
 *       - Others                 Wi-Fi scan start error
 */
esp_err_t esp_wifi_service_scan_request(esp_wifi_service_scan_handle_t handle,
                                        const wifi_scan_config_t *scan_config,
                                        esp_wifi_service_scan_cb_t cb,
                                        void *ctx);

/**
 * @brief  Remove pending callbacks matching @p cb
 *
 * @param[in]  handle  Scan agent handle
 * @param[in]  cb      Callback function to remove
 */
void esp_wifi_service_scan_remove_cb(esp_wifi_service_scan_handle_t handle, esp_wifi_service_scan_cb_t cb);

/**
 * @brief  Cancel all pending scan callbacks and stop the current scan if any
 *
 * @param[in]  handle  Scan agent handle
 */
void esp_wifi_service_scan_cancel_all(esp_wifi_service_scan_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
