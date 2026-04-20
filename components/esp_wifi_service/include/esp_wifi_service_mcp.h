/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_service.h"
#include "esp_service_mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the MCP tool-description JSON for esp_wifi_service
 *
 * @param[out]  out_schema  Receives a pointer to the embedded schema string; unchanged on failure
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_schema is NULL
 */
esp_err_t esp_wifi_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Invoke an esp_wifi_service MCP tool
 *
 * @param[in]   service      Wi-Fi service instance as an esp_service_t base pointer
 * @param[in]   tool         MCP tool descriptor; tool->name selects the operation
 * @param[in]   args         JSON arguments string; may be NULL for argument-free tools
 * @param[out]  result       Caller-provided buffer for the JSON result
 * @param[in]   result_size  Size of result in bytes, including the terminator
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service, tool, result, or tool arguments are invalid
 *       - ESP_ERR_NO_MEM         JSON result does not fit in result
 *       - ESP_ERR_NOT_SUPPORTED  tool->name is not a known Wi-Fi MCP tool
 *       - Other                  Error code returned by the underlying Wi-Fi service operation
 */
esp_err_t esp_wifi_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                       char *result, size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
