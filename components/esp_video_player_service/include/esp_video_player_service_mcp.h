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
 * @brief  Get the complete video player MCP tool JSON
 *
 *         Flash-resident playback tools plus container track extras. Tool
 *         names use the esp_video_player_service_ prefix. Do not free the pointer.
 *
 * @param[out]  out_schema  Receives a pointer to the schema
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_schema is NULL
 */
esp_err_t esp_video_player_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Invoke video player MCP tools (playback plus container tracks)
 *
 * @param[in]   service      Service handle from the MCP server
 * @param[in]   tool         Tool descriptor with the tool name
 * @param[in]   args         JSON arguments; NULL or empty uses `{}`
 * @param[out]  result       Buffer to receive a JSON result
 * @param[in]   result_size  Size of result in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_NOT_SUPPORTED  If the tool name is unknown
 *       - Others                 If the underlying player operation fails
 */
esp_err_t esp_video_player_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                               const char *args, char *result, size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
