/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Stub of driver/uart.h for EMU fault injection: enough of the driver
 * API for the UART transport to compile; the functions are implemented by the
 * test and only create()/destroy() are exercised.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define UART_NUM_0          0
#define UART_NUM_1          1
#define UART_PIN_NO_CHANGE  (-1)

typedef int uart_port_t;

typedef enum { UART_DATA_8_BITS = 3 } uart_word_length_t;
typedef enum { UART_PARITY_DISABLE = 0 } uart_parity_t;
typedef enum { UART_STOP_BITS_1 = 1 } uart_stop_bits_t;
typedef enum { UART_HW_FLOWCTRL_DISABLE = 0 } uart_hw_flowcontrol_t;
typedef enum { UART_SCLK_DEFAULT = 0 } uart_sclk_t;

typedef struct {
    int                    baud_rate;
    uart_word_length_t     data_bits;
    uart_parity_t          parity;
    uart_stop_bits_t       stop_bits;
    uart_hw_flowcontrol_t  flow_ctrl;
    uint8_t                rx_flow_ctrl_thresh;
    uart_sclk_t            source_clk;
} uart_config_t;

esp_err_t uart_param_config(uart_port_t port, const uart_config_t *cfg);
esp_err_t uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(uart_port_t port, int rx_buf, int tx_buf, int queue_size, void *queue, int intr_flags);
esp_err_t uart_driver_delete(uart_port_t port);
int uart_write_bytes(uart_port_t port, const void *src, size_t size);
int uart_read_bytes(uart_port_t port, void *buf, uint32_t length, uint32_t ticks_to_wait);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
