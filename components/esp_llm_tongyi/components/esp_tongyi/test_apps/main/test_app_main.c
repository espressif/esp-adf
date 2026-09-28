/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD  2048

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    printf("Running esp_tongyi unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_tongyi]", false);
    UNITY_END();
    printf("esp_tongyi tests complete\n");
}
