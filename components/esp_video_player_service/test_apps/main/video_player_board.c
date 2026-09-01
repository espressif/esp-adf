/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_log.h"

#include "esp_board_manager_includes.h"

#include "video_player_board.h"

static const char *TAG = "VIDEO_PLAYER_BOARD";

static bool s_lcd_ready = false;

void video_player_board_init(void)
{
#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init display LCD: %s", esp_err_to_name(ret));
    }
    s_lcd_ready = (ret == ESP_OK);
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */
}

void video_player_board_deinit(void)
{
#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    if (s_lcd_ready) {
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        s_lcd_ready = false;
    }
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */
}

bool video_player_board_has_lcd(void)
{
    return s_lcd_ready;
}

const char *video_player_board_lcd_name(void)
{
#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    return ESP_BOARD_DEVICE_NAME_DISPLAY_LCD;
#else
    return NULL;
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */
}
