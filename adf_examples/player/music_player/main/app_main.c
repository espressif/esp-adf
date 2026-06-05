/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "music_player_board.h"
#include "music_player_display.h"
#include "music_player_playback.h"
#include "music_player_ui.h"
#include "music_player_config.h"
#include "esp_gmf_oal_sys.h"

static const char *TAG = "MUSIC_PLAYER";

void app_main(void)
{
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);

    music_player_board_t board = {0};
    QueueHandle_t cmd_queue = xQueueCreate(MUSIC_PLAYER_QUEUE_LEN, sizeof(music_player_msg_t));
    ESP_GMF_CHECK(TAG, cmd_queue != NULL, return, "Failed to create command queue");

    ESP_LOGI(TAG, "[ 1 ] Initialize board peripherals");
    ESP_GMF_RET_ON_ERROR(TAG, music_player_board_init(&board), goto cleanup, "Failed to init board");

    ESP_LOGI(TAG, "[ 2 ] Initialize display and LVGL music UI");
    ESP_GMF_RET_ON_ERROR(TAG, music_player_display_init(), goto cleanup, "Failed to init display");
    ESP_GMF_RET_ON_ERROR(TAG, music_player_ui_init(cmd_queue), goto cleanup, "Failed to init UI");

    ESP_LOGI(TAG, "[ 3 ] Scan SD card playlist from %s", board.scan_dir);
    bool playlist_ready = (music_player_playback_scan(board.scan_dir) == ESP_OK);
    if (!playlist_ready) {
        ESP_LOGW(TAG, "No music found, place mp3/aac/wav files in %s", board.scan_dir);
    }

    ESP_LOGI(TAG, "[ 4 ] Start playback controller");
    ESP_GMF_RET_ON_ERROR(TAG, music_player_playback_start(cmd_queue, board.playback_codec), goto cleanup,
                         "Failed to start playback controller");

    ESP_LOGI(TAG, "[ 5 ] Music player ready");
    if (playlist_ready) {
        music_player_playback_post(MUSIC_PLAYER_CMD_PLAY);
    } else {
        music_player_playback_post(MUSIC_PLAYER_CMD_UPDATE_UI);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_gmf_oal_sys_get_real_time_stats(1000, false);
    }

cleanup:
    music_player_playback_stop();
    music_player_ui_deinit();
    music_player_display_deinit();
    music_player_board_deinit(&board);
    if (cmd_queue != NULL) {
        vQueueDelete(cmd_queue);
        cmd_queue = NULL;
    }
}
