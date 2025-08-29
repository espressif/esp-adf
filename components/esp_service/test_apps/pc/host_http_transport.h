/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Host-native HTTP transport for the MCP server.
 * Uses POSIX sockets; suitable for Linux/macOS testing.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct {
    uint16_t    port;
    const char *uri_path;
    size_t      max_request_size;
    bool        enable_cors;
} host_http_transport_config_t;

#define HOST_HTTP_TRANSPORT_CONFIG_DEFAULT()  {  \
    .port             = 8080,                    \
    .uri_path         = "/mcp",                  \
    .max_request_size = 8192,                    \
    .enable_cors      = true,                    \
}

/**
 * @brief  Create a host-native HTTP transport for MCP
 *
 * @param  config         Configuration (NULL for defaults)
 * @param  out_transport  Output: abstract transport pointer
 * @return
 *       - ESP_OK  on success
 */
esp_err_t host_http_transport_create(const host_http_transport_config_t *config,
                                     esp_service_mcp_trans_t **out_transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
