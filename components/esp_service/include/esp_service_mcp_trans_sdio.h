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
 * @brief  SDIO slave transport configuration
 *
 *         Uses the ESP32 SDIO slave peripheral to communicate with a host MCU.
 *         The host sends JSON-RPC requests and receives responses over the
 *         SDIO bus.
 *
 *         Protocol:
 *         - Host writes a complete JSON-RPC request (null-terminated) to the slave
 *         - One host packet is one request, including across receive buffers
 *         - Packets exceeding recv_buffer_size bytes are discarded in full
 *         - Slave processes the request and sends the response back to the host
 *
 * @note  SDIO slave is only available on certain ESP32 variants
 *         (ESP32, ESP32-C6). On unsupported chips, esp_service_mcp_trans_sdio_create()
 *         returns ESP_ERR_NOT_SUPPORTED.
 */
typedef struct {
    size_t  recv_buffer_size;   /*!< Receive buffer and maximum request size, excluding trailing NUL
                                 * (default: 4092, at most SDIO_SLAVE_RECV_MAX_BUFFER); create() rejects
                                 * values below 2 or above SDIO_SLAVE_RECV_MAX_BUFFER with ESP_ERR_INVALID_ARG */
    int     recv_buffer_count;  /*!< Number of receive buffers (default: 4) */
    size_t  send_buffer_size;   /*!< Size of send buffer (default: 4096) */
    size_t  stack_size;         /*!< Reader task stack size (default: 4096) */
    int     task_priority;      /*!< Reader task priority (default: 5) */
    int     task_core;          /*!< Reader task core affinity (-1 = any, default: -1) */
} esp_service_mcp_sdio_config_t;

/**
 * @brief  Default SDIO configuration
 */
#define ESP_SERVICE_MCP_SDIO_CONFIG_DEFAULT()  {  \
    .recv_buffer_size  = 4092,                    \
    .recv_buffer_count = 4,                       \
    .send_buffer_size  = 4096,                    \
    .stack_size        = 4096,                    \
    .task_priority     = 5,                       \
    .task_core         = -1,                      \
}

/**
 * @brief  Create SDIO slave transport
 *
 * @param[in]   config         SDIO configuration (NULL for defaults)
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_NOT_SUPPORTED  SDIO slave not available on this chip
 *       - ESP_ERR_INVALID_ARG    out_transport is NULL or receive buffer config is invalid
 *       - ESP_ERR_NO_MEM         Allocation failed
 */
esp_err_t esp_service_mcp_trans_sdio_create(const esp_service_mcp_sdio_config_t *config, esp_service_mcp_trans_t **out_transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
