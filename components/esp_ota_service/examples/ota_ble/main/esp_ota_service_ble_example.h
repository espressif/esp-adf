/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_ble_example_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Confirm the running image if it was left in PENDING_VERIFY after OTA.
 *
 * @return
 *       - ESP_OK     If the image was pending verify and confirmation succeeded
 *       - ESP_OK     If the image is not pending verify (ESP_ERR_INVALID_STATE from API)
 *       - esp_err_t  Pass-through from esp_ota_service_confirm_update() for other failures
 */
esp_err_t esp_ota_service_ble_example_confirm_running_image(void);

/**
 * @brief  Start the BLE OTA service (APP protocol) and wait for one session outcome.
 *
 * @note  On full success the example reboots the device with esp_restart() and does not return.
 *
 * @return
 *       - ESP_OK                                       On success or if the firmware was skipped as not newer
 *       - ESP_OTA_SERVICE_BLE_EX_ERR_NO_EVENT_GROUP    If the OTA event group could not be created
 *       - esp_err_t                                    Pass-through from OTA setup (create, subscribe, factories, set list, start)
 *       - ESP_OTA_SERVICE_BLE_EX_ERR_OTA_GENERIC       If the OTA session failed
 *       - ESP_OTA_SERVICE_BLE_EX_ERR_OTA_WAIT_TIMEOUT  If waiting for session completion timed out
 */
esp_err_t esp_ota_service_ble_example_run_ota(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
