/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_service.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the MCP tool-description JSON for esp_media_service link/unlink
 *
 * @param[out]  out_schema  Receives a pointer to the embedded schema string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_schema is NULL
 */
esp_err_t esp_media_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Get the MCP tool-description JSON for media_dummy_sink stats
 *
 * @param[out]  out_schema  Receives a pointer to the embedded schema string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_schema is NULL
 */
esp_err_t esp_media_dummy_service_mcp_schema_get(const char **out_schema);

/**
 * @brief  Invoke an esp_media_service MCP tool (link / unlink)
 *
 *         Resolves source and sink services by name from the persistent media
 *         service table maintained across init/deinit. The `service` argument
 *         is ignored (tools are not bound to one instance).
 *
 * @param[in]   service      Unused placeholder service pointer
 * @param[in]   tool         MCP tool descriptor
 * @param[in]   args         JSON arguments string
 * @param[out]  result       Caller-provided buffer for the JSON result
 * @param[in]   result_size  Size of result in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_NOT_FOUND      Named source or sink is not registered
 *       - ESP_ERR_NOT_SUPPORTED  Unknown tool name
 *       - Other                  Error from link/unlink
 */
esp_err_t esp_media_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                        char *result, size_t result_size);

/**
 * @brief  Invoke a media_dummy_sink MCP tool (get_stats)
 *
 * @param[in]   service      Dummy sink service as an esp_service_t base pointer
 * @param[in]   tool         MCP tool descriptor
 * @param[in]   args         JSON arguments string
 * @param[out]  result       Caller-provided buffer for the JSON result
 * @param[in]   result_size  Size of result in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_NOT_SUPPORTED  Unknown tool name or sink support disabled
 *       - Other                  Error from get_stats
 */
esp_err_t esp_media_dummy_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                              char *result, size_t result_size);

/**
 * @brief  Register global media link/unlink MCP tools with a service manager
 *
 *         Idempotent. Registration is kept for the process lifetime and is not
 *         unbound when individual media services are destroyed.
 *
 * @param[in]  mgr  Service manager that owns MCP tool dispatch
 *
 * @return
 *       - ESP_OK               On success (including already registered)
 *       - ESP_ERR_INVALID_ARG  mgr is NULL
 *       - Others               Error returned by manager registration
 */
esp_err_t esp_media_service_mcp_register(esp_service_manager_t *mgr);

/**
 * @brief  Track a media service in the persistent MCP name table
 *
 *         Called automatically from esp_media_service_init() when MCP is enabled.
 *
 * @param[in]  service  Media service base pointer
 */
void esp_media_service_mcp_on_init(esp_service_t *service);

/**
 * @brief  Remove a media service from the persistent MCP name table
 *
 *         Called automatically from esp_media_service_deinit() when MCP is enabled.
 *         Does not unregister MCP tools from the service manager.
 *
 * @param[in]  service  Media service base pointer
 */
void esp_media_service_mcp_on_deinit(esp_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
