/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_main.h"

#define USE_AVI_PLAYER    1
#define ENABLE_BENCHMARK  0

static void system_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for other components to initialize
    while (1) {
        esp_gmf_oal_sys_get_real_time_stats(1000, false);
        ESP_GMF_MEM_SHOW("MONITOR");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_GMF_MEM_SHOW("app_main");
    esp_log_level_set("gpio", ESP_LOG_WARN);
    esp_log_level_set("avifile", ESP_LOG_WARN);
    esp_log_level_set("avi player", ESP_LOG_WARN);

    esp_littlefs_init("/littlefs", "storage");
    esp_mmap_init("assets", MMAP_ASSETS_FILES, MMAP_ASSETS_CHECKSUM);
    esp_sdcard_init();

#if ENABLE_BENCHMARK
    storage_rw_random_peform_test("/littlefs/test.txt", true);
    storage_rw_random_peform_test("/littlefs/test.txt", false);
    storage_rw_random_peform_test("/sdcard/test.txt", true);
    storage_rw_random_peform_test("/sdcard/test.txt", false);
#endif  /* ENABLE_BENCHMARK */

#if USE_AVI_PLAYER
    avi_player_start();  // If you show 240*240 and more, you can try this
#else
    dual_eye_ui_start();  // If just show gif in 128*128, you can try this
    // dual_eye_ui_avi_start();  // If you show 240*240 and more with LVGL UI by avi player, you can try this
#endif  /* USE_AVI_PLAYER */

    esp_gmf_oal_thread_create(NULL, "system_task", system_task, NULL, 4096, 15, true, 1);
}
