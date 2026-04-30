/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

/**
 * @file led_service_mcp.h
 * @brief  MCP tool support for led_service — only compiled when CONFIG_ESP_MCP_ENABLE=y
 *
 *         Include this header and provide led_service_mcp_schema() / led_service_tool_invoke
 *         in esp_service_registration_t when MCP or tool-dispatch support is needed.
 *         Do NOT include this header in firmware builds that have no MCP requirement.
 */

#include "esp_err.h"
#include "esp_service_manager.h"
#include "led_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Return the MCP tool description JSON for the LED service
 *
 * @return
 *       - Pointer  to a null-terminated JSON array string (statically embedded)
 */
const char *led_service_mcp_schema(void);

/**
 * @brief  MCP tool invocation handler for led_service
 *
 *         Pass this as esp_service_registration_t::tool_invoke when registering
 *         the LED service with tool support enabled.
 */
esp_err_t led_service_tool_invoke(esp_service_t *service,
                                  const esp_service_tool_t *tool,
                                  const char *args,
                                  char *result,
                                  size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
