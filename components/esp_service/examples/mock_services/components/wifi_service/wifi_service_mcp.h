/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

/**
 * @file wifi_service_mcp.h
 * @brief  MCP tool support for wifi_service — only compiled when CONFIG_ESP_MCP_ENABLE=y
 */

#include "esp_err.h"
#include "esp_service_manager.h"
#include "wifi_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Return the MCP tool description JSON for the WiFi service
 *
 * @return
 *       - Pointer  to a null-terminated JSON array string (statically embedded)
 */
const char *wifi_service_mcp_schema(void);

/**
 * @brief  MCP tool invocation handler for wifi_service
 *
 *         Pass this as esp_service_registration_t::tool_invoke when registering
 *         the WiFi service with tool support enabled.
 *
 *         Supported tool names:
 *   - wifi_service_get_status   — returns connection status string
 *   - wifi_service_get_ssid     — returns configured/connected SSID
 *   - wifi_service_get_rssi     — returns signal strength in dBm
 *   - wifi_service_reconnect    — reconnects to a specified SSID
 */
esp_err_t wifi_service_tool_invoke(esp_service_t *service,
                                   const esp_service_tool_t *tool,
                                   const char *args,
                                   char *result,
                                   size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
