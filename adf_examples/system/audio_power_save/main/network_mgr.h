/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"

#include "power_save_app.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Connect Wi-Fi using the example connection helper
 *
 * @param[in,out]  app_ctx  Shared example context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - Others               Errors from Wi-Fi/NVS initialization or connection
 */
esp_err_t network_mgr_connect(power_save_context_t *app_ctx);

/**
 * @brief  Start MQTT client and wait until it is connected
 *
 * @param[in,out]  app_ctx  Shared example context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - ESP_ERR_TIMEOUT      MQTT broker connection timeout
 *       - Others               Errors from MQTT client initialization or start
 */
esp_err_t network_mgr_start_mqtt(power_save_context_t *app_ctx);

/**
 * @brief  Set Wi-Fi power save mode and log the transition reason
 *
 * @param[in]  ps_mode  Wi-Fi power save mode
 * @param[in]  reason   Human-readable reason for the mode change
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Errors from esp_wifi_set_ps()
 */
esp_err_t network_mgr_set_wifi_power_save_mode(wifi_ps_type_t ps_mode, const char *reason);

/**
 * @brief  Stop MQTT and Wi-Fi resources owned by the network manager
 */
void network_mgr_deinit(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
