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
 * @brief  Default WebSocket configuration
 */
#define ESP_SERVICE_MCP_WS_CONFIG_DEFAULT()  {  \
    .port             = 8081,                   \
    .uri_path         = "/mcp",                 \
    .max_request_size = 4096,                   \
    .stack_size       = 4096,                   \
    .max_uri_handlers = 8,                      \
    .max_open_sockets = 7,                      \
    .enable_cors      = false,                  \
    .max_clients      = 4,                      \
    .auth_token       = NULL,                   \
}

/**
 * @brief  WebSocket transport configuration
 *
 * @note  Security defaults: with enable_cors = false an upgrade request that
 *        carries an Origin header must match the Host header, which blocks
 *        cross-site WebSocket hijacking from other web pages; non-browser
 *        clients (no Origin) are always accepted, an Origin that cannot be
 *        read (longer than 127 bytes) is rejected. The Origin check only
 *        defends against browsers: it does not stop a non-browser client on
 *        the network, and an attacker who controls DNS for a name the device
 *        is reached by can make a browser's Origin match the Host header
 *        (DNS rebinding). Set auth_token to require
 *        `Authorization: Bearer <token>` on the upgrade request whenever the
 *        network is not trusted; the scheme is matched case-insensitively,
 *        the token byte for byte.
 *
 * @note  esp_http_server answers the upgrade without invoking the URI handler,
 *        so these checks run from the handshake callbacks, which
 *        CONFIG_ESP_MCP_TRANSPORT_WS selects (CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT
 *        rejects with 401/403 before the 101, CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
 *        registers the client for broadcast). If both are disabled, start fails
 *        with ESP_ERR_NOT_SUPPORTED when auth_token is set or enable_cors is false,
 *        instead of accepting every upgrade unchecked.
 *
 * @note  Only TEXT frames are dispatched; BINARY, CONTINUE and PONG frames
 *        are read and dropped, fragmented messages are not reassembled. PING
 *        is answered with a PONG carrying the same payload and CLOSE with a
 *        CLOSE echo, both read by the transport under the frame deadline. A
 *        control frame that is not final, longer than 125 bytes, or a CLOSE
 *        with a 1-byte payload (RFC 6455 §5.5) is refused before its payload
 *        is read, and a CLOSE whose status code is not 1000-1014 (without
 *        1004-1006) or 3000-4999 (RFC 6455 §7.4) is not echoed; both send a
 *        best-effort CLOSE 1002 (protocol error) and fail the handler, which
 *        makes esp_http_server close the connection. The CLOSE reason text
 *        is echoed as-is, its UTF-8 is not validated.
 *
 * @note  A frame must arrive within CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS
 *        (default 15 s) or the client is dropped; esp_http_server serves all
 *        connections from one task, so a client that trickles a frame holds
 *        every other client for that deadline plus two socket receive
 *        timeouts (the deadline is checked before each read, and
 *        esp_http_server reads the frame's first byte before the handler
 *        arms it). A GET on the WebSocket URI that announces a body
 *        (Content-Length) is answered 400 and the socket closed unread.
 */
typedef struct {
    uint16_t    port;              /*!< WebSocket server port (default: 8081) */
    const char *uri_path;          /*!< WebSocket endpoint path (default: "/mcp") */
    size_t      max_request_size;  /*!< Max request frame size (default: 4096) */
    size_t      stack_size;        /*!< HTTP server task stack size (default: 4096) */
    uint16_t    max_uri_handlers;  /*!< Max URI handlers (default: 8) */
    uint16_t    max_open_sockets;  /*!< Max open sockets (default: 7) */
    bool        enable_cors;       /*!< Accept upgrades from any Origin (default: false = same-origin only) */
    uint8_t     max_clients;       /*!< Max concurrent WebSocket clients (default: 4) */
    const char *auth_token;        /*!< Optional bearer token required on the upgrade request; NULL disables auth
                                    * (default: NULL). At most 152 bytes, create() rejects longer with
                                    * ESP_ERR_INVALID_ARG. Copied at create */
} esp_service_mcp_ws_config_t;

/**
 * @brief  Create WebSocket transport
 *
 * @param[in]   config         WebSocket configuration (NULL for defaults)
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_transport is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_service_mcp_trans_ws_create(const esp_service_mcp_ws_config_t *config, esp_service_mcp_trans_t **out_transport);

/**
 * @brief  Get underlying HTTP server handle (transport-specific accessor)
 *
 * @param[in]  transport  Transport pointer (must be a WebSocket transport)
 *
 * @return
 *       - HTTP  server handle, or NULL if not running
 */
httpd_handle_t esp_service_mcp_trans_ws_get_server(esp_service_mcp_trans_t *transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
