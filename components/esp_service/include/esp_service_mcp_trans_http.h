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
 * @brief  HTTP transport configuration
 */
typedef struct {
    uint16_t    port;              /*!< HTTP server port (default: 8080) */
    const char *uri_path;          /*!< MCP endpoint path (default: "/mcp") */
    size_t      max_request_size;  /*!< Max request body size (default: 4096) */
    size_t      stack_size;        /*!< HTTP server task stack size (default: 4096) */
    uint16_t    max_uri_handlers;  /*!< Max URI handlers (default: 8) */
    uint16_t    max_open_sockets;  /*!< Max open sockets (default: 7) */
    bool        enable_cors;       /*!< Enable CORS headers (default: true) */
} esp_service_mcp_http_config_t;

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
    .enable_cors      = true,                     \
}

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
