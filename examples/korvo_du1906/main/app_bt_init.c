/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <string.h>
#include "esp_log.h"
#include "app_bt_init.h"
#include "a2dp_stream.h"
#include "ble_gatts_module.h"

static const char *TAG = "APP_BT_INIT";

#define BT_DEVICE_NAME  "ESP_BT_COEX_DEV"

esp_periph_handle_t app_bluetooth_init(esp_periph_set_handle_t set)
{
#if CONFIG_BT_ENABLED
    ESP_LOGI(TAG, "Init Bluetooth module");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ble_gatts_module_init();

    esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name(BT_DEVICE_NAME);
    if (set_dev_name_ret) {
        ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
    }
    esp_periph_handle_t bt_periph = bt_create_periph();
    esp_periph_start(set, bt_periph);
    ESP_LOGI(TAG, "Start Bluetooth peripherals");

    esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    return bt_periph;
#else
    return NULL;
#endif
}

void app_bluetooth_deinit(void)
{
#if CONFIG_BT_ENABLED
    ESP_LOGI(TAG, "Deinit Bluetooth module");
    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());

    ble_gatts_module_deinit();
#endif
}