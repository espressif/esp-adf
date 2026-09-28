/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>

#include "esp_service_mcp_server.h"

#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the embedded MCP tool schema for LLM sessions
 *
 * @param[out]  out_schema  Receives a pointer to the embedded JSON schema
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_schema is NULL
 */
esp_err_t esp_llm_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Invoke an esp_llm_service MCP tool
 *
 * @note  service is the LLM instance the tool was registered on.
 *
 * @param[in]   service      LLM service as an esp_service_t base pointer
 * @param[in]   tool         MCP tool descriptor
 * @param[in]   args         JSON arguments string
 * @param[out]  result       Caller-provided buffer for the JSON result
 * @param[in]   result_size  Result buffer size in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If arguments or JSON are invalid
 *       - ESP_ERR_NOT_FOUND      If the requested session does not exist
 *       - ESP_ERR_NOT_SUPPORTED  If the tool name is unknown
 *       - Others                 Error returned by the selected session API
 */
esp_err_t esp_llm_service_tool_invoke(esp_service_t *service,
                                      const esp_service_tool_t *tool,
                                      const char *args,
                                      char *result,
                                      size_t result_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
