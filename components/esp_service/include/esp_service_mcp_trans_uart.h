/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "driver/uart.h"
#include "esp_service_mcp_trans.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  UART transport configuration
 *
 *         Line-based protocol over UART:
 *         - Input:  one JSON-RPC request per line (terminated by '\n')
 *         - Output: one JSON-RPC response per line (terminated by '\n')
 */
typedef struct {
    uart_port_t            uart_port;         /*!< UART port number (default: UART_NUM_1) */
    int                    baud_rate;         /*!< Baud rate (default: 115200) */
    int                    tx_pin;            /*!< TX GPIO pin (default: UART_PIN_NO_CHANGE) */
    int                    rx_pin;            /*!< RX GPIO pin (default: UART_PIN_NO_CHANGE) */
    int                    rts_pin;           /*!< RTS GPIO pin (default: UART_PIN_NO_CHANGE) */
    int                    cts_pin;           /*!< CTS GPIO pin (default: UART_PIN_NO_CHANGE) */
    size_t                 rx_buffer_size;    /*!< UART RX ring buffer size (default: 2048) */
    size_t                 tx_buffer_size;    /*!< UART TX ring buffer size (default: 0, blocking) */
    size_t                 max_request_size;  /*!< Max request line length (default: 4096) */
    size_t                 stack_size;        /*!< Reader task stack size (default: 4096) */
    int                    task_priority;     /*!< Reader task priority (default: 5) */
    int                    task_core;         /*!< Reader task core affinity (-1 = any, default: -1) */
    uart_word_length_t     data_bits;         /*!< Data bits (default: UART_DATA_8_BITS) */
    uart_parity_t          parity;            /*!< Parity (default: UART_PARITY_DISABLE) */
    uart_stop_bits_t       stop_bits;         /*!< Stop bits (default: UART_STOP_BITS_1) */
    uart_hw_flowcontrol_t  flow_ctrl;         /*!< Flow control (default: UART_HW_FLOWCTRL_DISABLE) */
} esp_service_mcp_uart_config_t;

/**
 * @brief  Default UART configuration
 */
#define ESP_SERVICE_MCP_UART_CONFIG_DEFAULT()  {   \
    .uart_port        = UART_NUM_1,                \
    .baud_rate        = 115200,                    \
    .tx_pin           = UART_PIN_NO_CHANGE,        \
    .rx_pin           = UART_PIN_NO_CHANGE,        \
    .rts_pin          = UART_PIN_NO_CHANGE,        \
    .cts_pin          = UART_PIN_NO_CHANGE,        \
    .rx_buffer_size   = 2048,                      \
    .tx_buffer_size   = 0,                         \
    .max_request_size = 4096,                      \
    .stack_size       = 4096,                      \
    .task_priority    = 5,                         \
    .task_core        = -1,                        \
    .data_bits        = UART_DATA_8_BITS,          \
    .parity           = UART_PARITY_DISABLE,       \
    .stop_bits        = UART_STOP_BITS_1,          \
    .flow_ctrl        = UART_HW_FLOWCTRL_DISABLE,  \
}

/**
 * @brief  Create UART transport
 *
 * @param[in]   config         UART configuration (NULL for defaults)
 * @param[out]  out_transport  Output: abstract transport pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_transport is NULL
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_service_mcp_trans_uart_create(const esp_service_mcp_uart_config_t *config, esp_service_mcp_trans_t **out_transport);

/**
 * @brief  Get the UART port number used by this transport
 *
 * @param[in]  transport  Transport pointer (must be a UART transport)
 *
 * @return
 *       - UART  port number, or -1 if invalid
 */
int esp_service_mcp_trans_uart_get_port(esp_service_mcp_trans_t *transport);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
