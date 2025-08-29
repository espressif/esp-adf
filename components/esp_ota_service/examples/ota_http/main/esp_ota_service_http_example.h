/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_ota_ops.h"

/** Example-specific return aliases (documented on the functions below). */
#define ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP  ESP_ERR_NO_MEM
#define ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_TIMEOUT    ESP_ERR_TIMEOUT
#define ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_FAILED     ESP_FAIL
#define ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_GENERIC     ESP_FAIL
#define ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_TIMEOUT     ESP_ERR_TIMEOUT

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Connect to the configured WiFi access point (STA).
 *
 * @return
 *       - ESP_OK                                      On success
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP  If the WiFi event group could not be created
 *       - esp_err_t                                   Pass-through from WiFi or TCP/IP setup calls
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_TIMEOUT    If association or IP did not succeed in time
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_FAILED     If connection failed after retries (see WiFi event logs)
 */
esp_err_t esp_ota_service_http_example_wifi_connect(void);

/**
 * @brief  If the running image is PENDING_VERIFY, confirm OTA validity or roll back.
 *
 * @param[in]  img_state  OTA state of the running partition from esp_ota_get_state_partition()
 *
 * @return
 *       - ESP_OK     If the image is not pending verification
 *       - ESP_OK     If confirmation succeeded
 *       - esp_err_t  Pass-through from esp_ota_service_confirm_update() on failure
 */
esp_err_t esp_ota_service_http_example_confirm_if_pending_verify(esp_ota_img_states_t img_state);

/**
 * @brief  Run the HTTP OTA service session (version check inside start).
 *
 * @note  On full success the example reboots the device with esp_restart() and does not return.
 *
 * @return
 *       - ESP_OK                                      On success or if the session was skipped as not newer
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP  If the OTA event group could not be created
 *       - esp_err_t                                   Pass-through from OTA setup (create, subscribe, factories, set list, start)
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_GENERIC     If the OTA session failed
 *       - ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_TIMEOUT     If waiting for session completion timed out
 */
esp_err_t esp_ota_service_http_example_run_ota_session(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
