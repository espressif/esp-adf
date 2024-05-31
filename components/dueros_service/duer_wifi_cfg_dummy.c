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


#include "duer_wifi_cfg_if.h"
#include "esp_log.h"

#ifndef CONFIG_BT_BLE_ENABLED

static char *TAG = "duer_wifi_cfg_dummy";

int duer_ble_send_data(uint8_t *data, uint32_t data_len, uint16_t attr_id)
{
    ESP_LOGE(TAG, "[%s]: Please enable ble in sdkconfig", __func__);
    return -1;
}

int duer_wifi_cfg_ble_host_init(duer_ble_wifi_cfg_callbacks_t *callbacks)
{
    ESP_LOGE(TAG, "[%s]: Please enable ble in sdkconfig", __func__);
    return -1;
}

int duer_wifi_cfg_ble_host_deinit(void)
{
    ESP_LOGE(TAG, "[%s]: Please enable ble in sdkconfig", __func__);
    return -1;
}

#endif /* #ifndef CONFIG_BT_BLE_ENABLED */
