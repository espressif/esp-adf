/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  HTTP + Server-Sent Events (SSE) transport for MCP
 *
 *         Implements the MCP-recommended HTTP+SSE transport as specified by the
 *         Model Context Protocol (MCP 2024-11-05). This transport is preferred
 *         over plain HTTP POST when clients need server-initiated notifications
 *         (e.g. Vercel AI SDK, Claude Desktop).
 *
 *         ## Protocol flow
 *
 *         1. Client opens a persistent GET connection to `<sse_path>` (e.g. `/mcp/sse`).
 *         2. Server sends an SSE "endpoint" event telling the client where to send requests:
 * @code
 *        event: endpoint
 *        data: /mcp/messages?sessionId=00000001
 *      @endcode
 *   3. Client sends JSON-RPC 2.0 requests via HTTP POST to that URL.
 *   4. Server processes each request and pushes the response back over the SSE
 *      channel as an "message" event:
 * @code
 *        event: message
 *        data: {"jsonrpc":"2.0","id":1,"result":{...}}
 *      @endcode
 *   5. Server can also push unsolicited notifications (e.g. tools/list_changed)
 *      via broadcast, reaching all active SSE clients.
 *
 * ## Endpoints registered
 *
 *   - `GET  <sse_path>`      – SSE subscription (long-lived connection)
 *   - `POST <messages_path>` – JSON-RPC request delivery
 *   - `OPTIONS <messages_path>` – CORS preflight
 *
 * ## Requires
 *   ESP-IDF ≥ 5.1 (uses httpd_req_async_handler_begin / complete).
 */

/**
 * @brief  SSE transport configuration
 */
typedef struct {
    uint16_t    port;                   /*!< HTTP server port (default: 8080) */
    const char *sse_path;               /*!< SSE subscription path (default: "/mcp/sse") */
    const char *messages_path;          /*!< JSON-RPC delivery path (default: "/mcp/messages") */
    size_t      max_request_size;       /*!< Max POST body size in bytes (default: 4096) */
    size_t      stack_size;             /*!< HTTP server task stack (default: 8192) */
    size_t      keepalive_stack_size;   /*!< Keepalive task stack size in bytes (default: 4096) */
    uint16_t    max_uri_handlers;       /*!< Max registered URI handlers (default: 8) */
    uint16_t    max_open_sockets;       /*!< Max open sockets (default: 7) */
    bool        enable_cors;            /*!< Add CORS headers (default: true) */
    uint8_t     max_clients;            /*!< Max concurrent SSE sessions (default: 4) */
    uint32_t    keepalive_interval_ms;  /*!< Keepalive comment interval ms (default: 25000) */
} esp_service_mcp_sse_config_t;

/**
 * @brief  Default SSE transport configuration
 */
#define ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT()  {  \
    .port                  = 8080,               \
    .sse_path              = "/mcp/sse",         \
    .messages_path         = "/mcp/messages",    \
    .max_request_size      = 4096,               \
    .stack_size            = 8192,               \
    .keepalive_stack_size  = 4096,               \
    .max_uri_handlers      = 8,                  \
    .max_open_sockets      = 7,                  \
    .enable_cors           = true,               \
    .max_clients           = 4,                  \
    .keepalive_interval_ms = 25000,              \
}

/**
 * @brief  Create an HTTP+SSE transport instance
 *
 *         Allocates and initialises an SSE transport. The returned pointer can be
 *         passed directly to esp_service_mcp_server_create() as the abstract transport.
 *
 * @note  Requires ESP-IDF >= 5.1 (httpd async handler API).
 *
 * @param[in]   config         SSE configuration; pass NULL to use defaults
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_transport is NULL
 *       - ESP_ERR_NO_MEM       Heap allocation failed
 */
esp_err_t esp_service_mcp_trans_sse_create(const esp_service_mcp_sse_config_t *config, esp_service_mcp_trans_t **out_transport);

/**
 * @brief  Get the underlying HTTP server handle
 *
 *         Useful for registering additional URI handlers on the same server.
 *
 * @param[in]  transport  Transport pointer (must be an SSE transport)
 *
 * @return
 *       - HTTP  server handle, or NULL if not running or wrong type
 */
httpd_handle_t esp_service_mcp_trans_sse_get_server(esp_service_mcp_trans_t *transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
