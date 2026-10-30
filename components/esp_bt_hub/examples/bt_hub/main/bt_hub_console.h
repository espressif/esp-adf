/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize the Bluetooth hub and start the interactive console
 *
 * @return
 *       - ESP_OK                 Console started
 *       - ESP_ERR_NO_MEM         Context allocation failed
 *       - ESP_ERR_NOT_SUPPORTED  No supported console transport is enabled
 *       - Others                 Error returned by an ESP-IDF or esp_bt_hub operation
 */
esp_err_t bt_hub_console_run(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
