/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_service_mcp_server.h"
#include "esp_ota_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the MCP tool-description JSON for esp_ota_service
 *
 *         On success, sets @a *out_schema to the embedded esp_ota_service_mcp.json content
 *         (NUL-terminated, process-lifetime static).
 *
 * @param[out]  out_schema  Receives a pointer to the schema string; unchanged on failure
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @a out_schema is NULL
 */
esp_err_t esp_ota_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  MCP tool invocation handler for esp_ota_service
 *
 *         Dispatches on @a tool->name, writes a JSON object into @a result, and
 *         returns an appropriate @c esp_err_t.  Supported tools include
 *         @c esp_ota_service_get_version, @c esp_ota_service_get_progress, and
 *         @c esp_ota_service_check_update.
 *
 * @param[in]   service      OTA service instance (as @c esp_service_t base handle)
 * @param[in]   tool         MCP tool descriptor; @a tool->name selects the operation
 * @param[in]   args         JSON arguments string (may be NULL; unused by current tools)
 * @param[out]  result       Caller buffer for the JSON response (NUL-terminated when successful)
 * @param[in]   result_size  Size of @a result in bytes, including space for the terminator
 *
 * @return
 *       - ESP_OK                 On success (JSON written to @a result)
 *       - ESP_ERR_INVALID_ARG    If @a service, @a tool, or @a result is NULL, or @a result_size is 0
 *       - ESP_ERR_NOT_SUPPORTED  If @a tool->name is not a known OTA MCP tool
 *       - Other                  Error code returned by esp_ota_service_check_update() when that tool fails
 */
esp_err_t esp_ota_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                      char *result, size_t result_size);

/**
 * @brief  Update MCP-side cached check_update metadata from the application
 *
 *         Copies @a info into static storage consumed by MCP tools (e.g. latest version).
 *         Does nothing if @a info is NULL.
 *
 * @param[in]  info  Last @c esp_ota_service_check_update() result to cache; may be NULL
 */
void esp_ota_service_mcp_cache_update_info(const esp_ota_service_update_info_t *info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
