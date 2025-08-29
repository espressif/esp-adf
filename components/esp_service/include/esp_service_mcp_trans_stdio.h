/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  STDIO transport configuration
 *
 *         Line-based protocol over stdin/stdout:
 *         - Input:  one JSON-RPC request per line (terminated by '\n')
 *         - Output: one JSON-RPC response per line (terminated by '\n')
 */
typedef struct {
    size_t  max_request_size;  /*!< Max line length in bytes (default: 4096) */
    size_t  stack_size;        /*!< Reader task stack size (default: 4096) */
    int     task_priority;     /*!< Reader task priority (default: 5) */
    int     task_core;         /*!< Reader task core affinity (-1 = any, default: -1) */
} esp_service_mcp_stdio_config_t;

/**
 * @brief  Default STDIO configuration
 */
#define ESP_SERVICE_MCP_STDIO_CONFIG_DEFAULT()  {  \
    .max_request_size = 4096,                      \
    .stack_size       = 4096,                      \
    .task_priority    = 5,                         \
    .task_core        = -1,                        \
}

/**
 * @brief  Create STDIO transport
 *
 * @param[in]   config         STDIO configuration (NULL for defaults)
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_transport is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_service_mcp_trans_stdio_create(const esp_service_mcp_stdio_config_t *config, esp_service_mcp_trans_t **out_transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
