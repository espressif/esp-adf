/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Initialize WiFi (STA required), block until connected or retries exhausted.
 *
 * @return
 *       - ESP_OK                 On STA got IP
 *       - ESP_ERR_INVALID_STATE  If STA SSID not configured
 *       - ESP_FAIL               If association failed after max retries
 */
esp_err_t app_wifi_main(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
