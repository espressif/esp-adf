/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define SDIO_SLAVE_SEND_STREAM      0
#define SDIO_SLAVE_RECV_MAX_BUFFER  4092

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef void *sdio_slave_buf_handle_t;
typedef struct {
    int     sending_mode;
    int     send_queue_size;
    size_t  recv_buffer_size;
} sdio_slave_config_t;

esp_err_t sdio_slave_initialize(sdio_slave_config_t *config);
void sdio_slave_deinit(void);
esp_err_t sdio_slave_start(void);
void sdio_slave_stop(void);
sdio_slave_buf_handle_t sdio_slave_recv_register_buf(uint8_t *buffer);
esp_err_t sdio_slave_recv_load_buf(sdio_slave_buf_handle_t handle);
esp_err_t sdio_slave_recv_packet(sdio_slave_buf_handle_t *handle, uint32_t wait);
uint8_t *sdio_slave_recv_get_buf(sdio_slave_buf_handle_t handle, size_t *len);
esp_err_t sdio_slave_send_queue(uint8_t *buffer, size_t len, void *arg, uint32_t wait);
esp_err_t sdio_slave_send_get_finished(void **arg, uint32_t wait);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
