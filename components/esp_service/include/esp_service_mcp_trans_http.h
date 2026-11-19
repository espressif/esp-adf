/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default HTTP configuration
 */
#define ESP_SERVICE_MCP_HTTP_CONFIG_DEFAULT()  {  \
    .port             = 8080,                     \
    .uri_path         = "/mcp",                   \
    .max_request_size = 4096,                     \
    .stack_size       = 4096,                     \
    .max_uri_handlers = 8,                        \
    .max_open_sockets = 7,                        \
    .enable_cors      = false,                    \
    .auth_token       = NULL,                     \
}

/**
 * @brief  HTTP transport configuration
 *
 * @note  Security defaults: CORS is off, so browsers refuse to read responses
 *        from other origins, and requests without `Content-Type: application/json`
 *        are rejected so no-preflight "simple requests" cannot reach the server.
 *        Set auth_token to additionally require `Authorization: Bearer <token>`
 *        on every request; without it any host on the network can invoke tools.
 *        The scheme is matched case-insensitively, the token byte for byte.
 *
 * @note  A request body must arrive within CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS
 *        (default 15 s) or the POST is answered 408 and the socket closed;
 *        esp_http_server serves all connections from one task, so a client
 *        that trickles its body holds every other client for that deadline
 *        plus one socket receive timeout (the deadline is checked after each
 *        read returns). An OPTIONS request that announces a body
 *        (Content-Length) is answered 400 and closed unread, so httpd has no
 *        such body to purge; a chunked body has no Content-Length and is
 *        neither refused nor purged (on ESP-IDF v6.2.0 and later, and on the
 *        release/v5.5 and release/v6.0 branches ahead of their next release,
 *        the parser answers any Transfer-Encoding request 501 before a
 *        handler runs).
 */
typedef struct {
    uint16_t    port;              /*!< HTTP server port (default: 8080) */
    const char *uri_path;          /*!< MCP endpoint path (default: "/mcp") */
    size_t      max_request_size;  /*!< Max request body size (default: 4096) */
    size_t      stack_size;        /*!< HTTP server task stack size (default: 4096) */
    uint16_t    max_uri_handlers;  /*!< Max URI handlers (default: 8) */
    uint16_t    max_open_sockets;  /*!< Max open sockets (default: 7) */
    bool        enable_cors;       /*!< Send `Access-Control-Allow-Origin: *` (default: false) */
    const char *auth_token;        /*!< Optional bearer token required on every request; NULL disables auth
                                    * (default: NULL). At most 152 bytes, create() rejects longer with
                                    * ESP_ERR_INVALID_ARG. Copied at create */
} esp_service_mcp_http_config_t;

/**
 * @brief  Create HTTP transport
 *
 *         Allocates and initializes an HTTP transport instance.
 *         The returned esp_service_mcp_trans_t* can be passed to esp_service_mcp_server_create().
 *
 * @param[in]   config         HTTP configuration (NULL for defaults)
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_transport is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_service_mcp_trans_http_create(const esp_service_mcp_http_config_t *config, esp_service_mcp_trans_t **out_transport);

/**
 * @brief  Get underlying HTTP server handle (transport-specific accessor)
 *
 * @param[in]  transport  Transport pointer (must be an HTTP transport)
 *
 * @return
 *       - HTTP  server handle, or NULL if not running
 */
httpd_handle_t esp_service_mcp_trans_http_get_server(esp_service_mcp_trans_t *transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
