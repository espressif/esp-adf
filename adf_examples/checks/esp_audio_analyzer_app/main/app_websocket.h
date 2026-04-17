/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  WebSocket server configuration
 */
typedef struct {
    uint8_t  max_clients;       /*!< Maximum concurrent WebSocket clients */
    uint8_t  recv_timeout_sec;  /*!< Receive timeout in seconds */
    uint8_t  send_timeout_sec;  /*!< Send timeout in seconds */
} app_websocket_config_t;

/**
 * @brief  Default WebSocket server configuration
 */
#define APP_WEBSOCKET_CONFIG_DEFAULT()  {  \
    .max_clients      = 1,                 \
    .recv_timeout_sec = 20,                \
    .send_timeout_sec = 20,                \
}

/**
 * @brief  Initialize and start WebSocket server
 *
 * @param[in]  config  Server configuration, NULL for default
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to initialize WebSocket server
 */
esp_err_t app_websocket_init(app_websocket_config_t *config);

/**
 * @brief  Stop and deinitialize WebSocket server
 */
void app_websocket_deinit(void);

/**
 * @brief  Get WebSocket server handle
 *
 * @return
 *       - Handle  On success
 *       - NULL    If server not initialized
 */
httpd_handle_t app_websocket_get_handle(void);

/**
 * @brief  Check if WebSocket server is running
 *
 * @return
 *       - true   On success
 *       - false  If server not running
 */
bool app_websocket_is_running(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
