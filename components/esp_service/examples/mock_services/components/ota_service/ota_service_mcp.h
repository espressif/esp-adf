/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

/**
 * @file ota_service_mcp.h
 * @brief  MCP tool support for ota_service (mock) — only compiled when CONFIG_ESP_MCP_ENABLE=y
 */

#include <stddef.h>

#include "esp_err.h"
#include "esp_service_manager.h"
#include "ota_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the MCP tool description JSON for the OTA service
 *
 * @param[out]  out_schema  Receives a pointer to the embedded JSON (NUL-terminated)
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @a out_schema is NULL
 */
esp_err_t ota_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  MCP tool invocation handler for ota_service (mock)
 *
 *         Pass this as esp_service_registration_t::tool_invoke when registering
 *         the OTA service with tool support enabled.
 *
 *         Supported tool names:
 *   - ota_service_get_version   — returns current and latest version numbers
 *   - ota_service_get_progress  — returns download progress (percent, bytes)
 *   - ota_service_check_update  — triggers update check, returns availability
 */
esp_err_t ota_service_tool_invoke(esp_service_t *service,
                                  const esp_service_tool_t *tool,
                                  const char *args,
                                  char *result,
                                  size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
