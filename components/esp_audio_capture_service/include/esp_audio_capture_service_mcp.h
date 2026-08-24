/**
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
 * @brief  Get the MCP tool-description JSON for esp_audio_capture_service
 *
 * @param[out]  out_schema  Receives a pointer to the embedded schema string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_schema is NULL
 */
esp_err_t esp_audio_capture_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Invoke an esp_audio_capture_service MCP tool
 *
 * @param[in]   service      Capture service as an esp_service_t base pointer
 * @param[in]   tool         MCP tool descriptor
 * @param[in]   args         JSON arguments string
 * @param[out]  result       Caller-provided buffer for the JSON result
 * @param[in]   result_size  Size of result in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_NOT_SUPPORTED  Unknown tool name
 *       - Other                  Error from the underlying capture API
 */
esp_err_t esp_audio_capture_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                                const char *args, char *result, size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
