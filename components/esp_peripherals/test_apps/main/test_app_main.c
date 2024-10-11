/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_event.h"

extern void test_main(void);

void app_main()
{
    printf(
    " _                  _                                                    _           _            \n"
    "| |_    ___   ___  | |_      ___   ___   _ __      _ __     ___   _ __  (_)  _ __   | |__         \n"
    "| __|  / _ \\ / __| | __|    / _ \\ / __| | '_ \\    | '_ \\   / _ \\ | '__| | | | '_ \\  | '_ \\ \n"
    "| |_  |  __/ \\__ \\ | |_    |  __/ \\__ \\ | |_) |   | |_) | |  __/ | |    | | | |_) | | | | |   \n"
    " \\__|  \\___| |___/  \\__|    \\___| |___/ | .__/    | .__/   \\___| |_|    |_| | .__/  |_| |_|  \n"
    "                                        |_|       |_|                       |_|                   \n"
    );

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    test_main();
}
