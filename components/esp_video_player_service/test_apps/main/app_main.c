/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "unity.h"
#include "unity_test_utils_memory.h"

#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"
#include "esp_video_dec_default.h"

#include "video_player_board.h"

#define TEST_MEMORY_LEAK_THRESHOLD  (1 * 1024)

extern void esp_video_player_service_ut_force_link(void);

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    /* GMF pool_deinit returns before worker tasks free their last blocks. */
    vTaskDelay(pdMS_TO_TICKS(100));
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    esp_video_player_service_ut_force_link();
    video_player_board_init();
    ESP_ERROR_CHECK(esp_extractor_register_default());
    ESP_ERROR_CHECK(esp_audio_dec_register_default());
    ESP_ERROR_CHECK(esp_video_dec_register_default());

    UNITY_BEGIN();
    unity_run_menu();
    UNITY_END();

    video_player_board_deinit();
}
