/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "bt_hub_console.h"

static const char *TAG = "BT_HUB_EXAMPLE";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Initialize NVS failed: %s", esp_err_to_name(err));
        return;
    }
    err = bt_hub_console_run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Run example failed: %s", esp_err_to_name(err));
    }
}
