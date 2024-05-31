/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __DUER_WIFI_CFG_IF_H__
#define __DUER_WIFI_CFG_IF_H__

#include "esp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*on_ble_recv_data)(void *data, size_t len, uint16_t handle);
    void (*on_ble_connect_status)(bool status);
} duer_ble_wifi_cfg_callbacks_t;

int duer_ble_send_data(uint8_t *data, uint32_t data_len, uint16_t attr_id);

int duer_wifi_cfg_ble_host_init(duer_ble_wifi_cfg_callbacks_t *callbacks);

int duer_wifi_cfg_ble_host_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __DUER_WIFI_CFG_IF_H__ */
