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
 *        data: /mcp/messages?sessionId=<32 random hex chars>
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
 *   A GET or OPTIONS that announces a body (Content-Length) is answered 400
 *   and the socket closed unread: esp_http_server serves every connection from
 *   one task and would otherwise purge the unread body with no total deadline.
 *   A chunked body has no Content-Length and is neither refused nor purged
 *   (on ESP-IDF v6.2.0 and later, and on the release/v5.5 and release/v6.0
 *   branches ahead of their next release, the parser answers any
 *   Transfer-Encoding request 501 before a handler runs).
 *
 * ## Requires
 *   ESP-IDF ≥ 5.1 (uses httpd_req_async_handler_begin / complete).
 */

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
    .enable_cors           = false,              \
    .max_clients           = 4,                  \
    .keepalive_interval_ms = 25000,              \
    .auth_token            = NULL,               \
}

/**
 * @brief  SSE transport configuration
 *
 * @note  Security defaults: CORS is off, POSTs without
 *        `Content-Type: application/json` are rejected, a POST is only
 *        executed when its sessionId belongs to a live SSE channel, and
 *        session IDs are 128-bit random values. Set auth_token to
 *        additionally require `Authorization: Bearer <token>` on both the
 *        GET (subscribe) and POST (message) endpoints. The scheme is matched
 *        case-insensitively, the token byte for byte.
 *
 * @note  A POST body must arrive within CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS
 *        (default 15 s) or the request is answered 408 and the socket closed.
 *        A POST whose request cannot be dispatched is answered 500; nothing
 *        is pushed on the SSE channel in that case.
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
    bool        enable_cors;            /*!< Send `Access-Control-Allow-Origin: *` (default: false) */
    uint8_t     max_clients;            /*!< Max concurrent SSE sessions (default: 4) */
    uint32_t    keepalive_interval_ms;  /*!< Keepalive comment interval ms (default: 25000) */
    const char *auth_token;             /*!< Optional bearer token required on every request; NULL disables auth
                                         * (default: NULL). At most 152 bytes, create() rejects longer with
                                         * ESP_ERR_INVALID_ARG. Copied at create */
} esp_service_mcp_sse_config_t;

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
