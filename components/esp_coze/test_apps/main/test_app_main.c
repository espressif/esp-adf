/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

#include "nvs_flash.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD (500)

static char *TAG = "test_app_main";

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main()
{

    printf(
    " _            _                                        \n"
    "| |          | |                                       \n"
    "| |_ ___  ___| |_    ___  ___ _ __     ___ ___ _______ \n"
    "| __/ _ \\/ __| __|  / _ \\/ __| '_ \\   / __/ _ \\_  / _ \\\n"
    "| ||  __/\\__ \\ |_  |  __/\\__ \\ |_) | | (_| (_) / /  __/\n"
    " \\__\\___||___/\\__|  \\___||___/ .__/   \\___\\___/___\\___|\n"
    "                             | |                       \n"
    "                             |_|                       \n"
    );
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");

    unity_run_menu();
}
