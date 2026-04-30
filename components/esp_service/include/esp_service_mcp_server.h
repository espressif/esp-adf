/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  MCP Server - Exposes services as Model Context Protocol tools
 *
 *         Implements MCP specification 2024-11-05:
 *         - tools/list: List available tools
 *         - tools/call: Invoke tools
 *         - notifications/tools/list_changed: Notify tool list changes
 *
 *         Architecture:
 *         - Transport-agnostic: receives a pre-created esp_service_mcp_trans_t*
 *           and communicates through the abstract transport interface.
 *         - Tool-source-agnostic: receives an esp_service_mcp_tool_provider_t that
 *           abstracts the underlying tool registry; use
 *           esp_service_manager_as_tool_provider() to wrap an
 *           esp_service_manager_t, or supply custom callbacks.
 *         - JSON-RPC 2.0 protocol handling
 *         - Request/response validation
 *
 *         Typical usage:
 * @code
 *           // 1. Create a concrete transport
 *           esp_service_mcp_trans_t *transport = NULL;
 *           esp_service_mcp_trans_http_create(&http_cfg, &transport);
 *
 *           // 2. Wrap the service manager as a tool provider
 *           esp_service_mcp_server_config_t cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
 *           esp_service_manager_as_tool_provider(mgr, &cfg.tool_provider);
 *           cfg.transport = transport;
 *           esp_service_mcp_server_create(&cfg, &server);
 *
 *           // 3. Start (starts both server logic and transport)
 *           esp_service_mcp_server_start(server);
 *
 *           // 4. Cleanup
 *           esp_service_mcp_server_stop(server);
 *           esp_service_mcp_server_destroy(server);
 *           esp_service_mcp_trans_destroy(transport);
 *         @endcode
 */

/* Forward declarations */
typedef struct esp_service_mcp_server esp_service_mcp_server_t;
typedef struct esp_service_tool esp_service_tool_t;

/**
 * @brief  MCP tool descriptor
 *
 *         Populated at registration time by parsing the tool_desc JSON.
 *         Fields are read-only from the MCP server's perspective.
 */
struct esp_service_tool {
    char *name;          /*!< Tool name */
    char *description;   /*!< Tool description */
    char *input_schema;  /*!< Input schema (JSON object string) */
};

/**
 * @brief  Abstract tool provider interface
 *
 *         Decouples esp_service_mcp_server from any concrete tool registry.
 *         Implement these two callbacks to expose tools to the MCP server.
 *         Use esp_service_manager_as_tool_provider() to wrap an
 *         esp_service_manager_t instance.
 */
typedef struct {
    /**
     * @brief  Enumerate all available tools
     *
     * @param[in]   ctx        Opaque context
     * @param[out]  out_tools  Array to fill with tool pointers
     * @param[in]   max_tools  Capacity of out_tools
     * @param[out]  out_count  Number of tools written
     *
     * @return
     *       - ESP_OK  On success
     */
    esp_err_t (*get_tools)(void *ctx, const esp_service_tool_t **out_tools, uint16_t max_tools, uint16_t *out_count);

    /**
     * @brief  Invoke a named tool
     *
     * @param[in]   ctx          Opaque context
     * @param[in]   tool_name    Tool name
     * @param[in]   args_json    JSON-encoded arguments (may be NULL or "{}")
     * @param[out]  result       Buffer for JSON result string
     * @param[in]   result_size  Size of result buffer
     *
     * @return
     *       - ESP_OK             On success
     *       - ESP_ERR_NOT_FOUND  If the tool name is unknown
     */
    esp_err_t (*invoke_tool)(void *ctx, const char *tool_name, const char *args_json, char *result, size_t result_size);

    void *ctx;  /*!< Opaque context passed to both callbacks */
} esp_service_mcp_tool_provider_t;

/**
 * @brief  MCP request handler result
 */
typedef struct {
    char      *response;         /*!< JSON-RPC response string (allocated) */
    bool       is_notification;  /*!< True if no response needed */
    esp_err_t  error;            /*!< Error code */
} esp_service_mcp_response_t;

/**
 * @brief  MCP server configuration
 *
 *         No transport-specific fields. All transport configuration
 *         is handled by the concrete transport's own create function.
 */
typedef struct {
    esp_service_mcp_tool_provider_t  tool_provider;      /*!< Tool source (esp_service_manager_as_tool_provider()) */
    esp_service_mcp_trans_t         *transport;          /*!< Abstract transport instance (required) */
    const char                      *server_name;        /*!< Server name for MCP initialize (default: "esp-mcp-server") */
    const char                      *server_version;     /*!< Server version for MCP initialize (default: "1.0.0") */
    size_t                           max_request_size;   /*!< Max request size in bytes (default: 4096) */
    size_t                           max_response_size;  /*!< Max tool result + response size in bytes (default: 4096) */
    uint16_t                         max_tools;          /*!< Max tools for tools/list (default: 64) */
    uint32_t                         timeout_ms;         /*!< Request timeout (default: 5000) */
} esp_service_mcp_server_config_t;

/**
 * @brief  Default server configuration
 */
#define ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT()  {  \
    .tool_provider     = {NULL, NULL, NULL},        \
    .transport         = NULL,                      \
    .server_name       = "esp-mcp-server",          \
    .server_version    = "1.0.0",                   \
    .max_request_size  = 4096,                      \
    .max_response_size = 4096,                      \
    .max_tools         = 64,                        \
    .timeout_ms        = 5000,                      \
}

/**
 * @brief  MCP server capabilities
 */
typedef struct {
    bool  tools;               /*!< Supports tools */
    bool  tools_list_changed;  /*!< Supports list_changed notification */
} esp_service_mcp_server_capabilities_t;

/**
 * @brief  Create MCP server
 *
 *         Binds to the given transport by setting a request handler callback.
 *         The server does NOT take ownership of the transport; the caller is
 *         responsible for destroying it after the server is destroyed.
 *
 * @param[in]   config   Configuration (tool_provider callbacks and transport are required)
 * @param[out]  out_srv  Output: server instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  config, tool_provider callbacks, or transport is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_service_mcp_server_create(const esp_service_mcp_server_config_t *config, esp_service_mcp_server_t **out_srv);

/**
 * @brief  Destroy MCP server
 *
 *         Stops the server if still running. Does NOT destroy the transport.
 *
 * @param[in]  srv  Server instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If srv is NULL
 */
esp_err_t esp_service_mcp_server_destroy(esp_service_mcp_server_t *srv);

/**
 * @brief  Start MCP server
 *
 *         Starts the bound transport, which begins accepting client connections/data.
 *
 * @param[in]  srv  Server instance
 *
 * @return
 *       - ESP_OK               On success (including when the server is already running)
 *       - ESP_ERR_INVALID_ARG  If srv is NULL
 *       - Other                Error code returned by the bound transport's start routine
 */
esp_err_t esp_service_mcp_server_start(esp_service_mcp_server_t *srv);

/**
 * @brief  Stop MCP server
 *
 *         Stops the bound transport.
 *
 * @param[in]  srv  Server instance
 *
 * @return
 *       - ESP_OK               On success (including when the server is not running)
 *       - ESP_ERR_INVALID_ARG  If srv is NULL
 */
esp_err_t esp_service_mcp_server_stop(esp_service_mcp_server_t *srv);

/**
 * @brief  Handle MCP request (synchronous, public API)
 *
 *         Parses a JSON-RPC request and dispatches to the appropriate handler.
 *         Protocol-level failures (parse error, unknown method, invalid params) are
 *         reported in the returned response structure, not via the return code.
 *
 * @param[in]   srv       Server instance
 * @param[in]   request   JSON-RPC request string
 * @param[out]  response  Output: response structure (caller must call esp_service_mcp_response_free)
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If srv, request or response is NULL
 */
esp_err_t esp_service_mcp_server_handle_request(esp_service_mcp_server_t *srv, const char *request, esp_service_mcp_response_t *response);

/**
 * @brief  Send notification to all connected clients
 *
 *         Broadcasts a JSON-RPC notification via the bound transport.
 *
 * @param[in]  srv     Server instance
 * @param[in]  method  Notification method (e.g., "notifications/tools/list_changed")
 * @param[in]  params  Notification params (JSON string, can be NULL)
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If srv or method is NULL
 *       - ESP_ERR_NO_MEM       If notification serialization fails
 *       - Other                Error code returned by the bound transport's broadcast routine
 */
esp_err_t esp_service_mcp_server_notify(esp_service_mcp_server_t *srv, const char *method, const char *params);

/**
 * @brief  Get server capabilities
 *
 * @param[in]   srv       Server instance
 * @param[out]  out_caps  Output: capabilities
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If srv or out_caps is NULL
 */
esp_err_t esp_service_mcp_server_get_capabilities(esp_service_mcp_server_t *srv, esp_service_mcp_server_capabilities_t *out_caps);

/**
 * @brief  Free response structure
 *
 * @param[in]  response  Response to free
 */
void esp_service_mcp_response_free(esp_service_mcp_response_t *response);

/**
 * @brief  Helper: Build JSON-RPC success response
 *
 * @param[in]  id      Request ID (JSON string or number)
 * @param[in]  result  Result object (JSON string)
 *
 * @return
 *       - Non-NULL  Heap-allocated JSON-RPC response string; caller must free()
 *       - NULL      On allocation failure (OOM)
 */
char *esp_service_mcp_build_response(const char *id, const char *result);

/**
 * @brief  Helper: Build JSON-RPC error response
 *
 * @param[in]  id       Request ID (can be NULL)
 * @param[in]  code     Error code
 * @param[in]  message  Error message
 *
 * @return
 *       - Non-NULL  Heap-allocated JSON-RPC error response string; caller must free()
 *       - NULL      On allocation failure (OOM)
 */
char *esp_service_mcp_build_error(const char *id, int code, const char *message);

/**
 * @brief  Helper: Build tools/list response
 *
 * @param[in]  tools  Array of tools
 * @param[in]  count  Tool count
 *
 * @return
 *       - Non-NULL  Heap-allocated JSON array string; caller must free()
 *       - NULL      On allocation failure (OOM)
 */
char *esp_service_mcp_build_tools_list(const esp_service_tool_t **tools, uint16_t count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
