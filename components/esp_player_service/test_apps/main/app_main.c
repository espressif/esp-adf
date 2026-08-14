/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "unity.h"
#include "unity_test_utils_memory.h"

#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"

#define TEST_MEMORY_LEAK_THRESHOLD  512

extern void esp_player_service_ut_force_link(void);

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    esp_player_service_ut_force_link();
    ESP_ERROR_CHECK(esp_extractor_register_default());
    ESP_ERROR_CHECK(esp_audio_dec_register_default());

    printf("Running esp_player_service unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_player_service]", false);
    UNITY_END();
    printf("ESP_PLAYER_SERVICE_UT_DONE\n");
    fflush(stdout);
}
