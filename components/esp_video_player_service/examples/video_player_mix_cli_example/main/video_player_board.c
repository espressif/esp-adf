/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "esp_log.h"

#include "esp_board_manager_includes.h"

#include "video_player_board.h"

#define VIDEO_PLAYER_BOARD_LCD_FB_NUM  (2)

static const char *TAG = "VIDEO_PLAYER_BOARD";

static bool s_lcd_ready = false;

#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
/* esp_video_render only draws straight into the panel frame buffers when the panel
   owns more than one. With a single buffer it renders into its own heap buffer and
   the panel driver has to copy every frame, which tears and wastes bandwidth. The
   count is fixed when the panel is created, so raise it before initializing. */
static void use_double_frame_buffer(void)
{
    dev_display_lcd_config_t *board_cfg = NULL;
    if (esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&board_cfg) != ESP_OK) {
        return;
    }

    dev_display_lcd_config_t lcd_cfg;
    memcpy(&lcd_cfg, board_cfg, sizeof(lcd_cfg));
#ifdef CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    if (strcmp(board_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        lcd_cfg.sub_cfg.dsi.dpi_config.num_fbs = VIDEO_PLAYER_BOARD_LCD_FB_NUM;
    }
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT */
#ifdef CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT
    if (strcmp(board_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0) {
        lcd_cfg.sub_cfg.rgb.panel_config.num_fbs = VIDEO_PLAYER_BOARD_LCD_FB_NUM;
    }
#endif  /* CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_RGB_SUPPORT */

    esp_err_t ret = esp_board_device_override_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &lcd_cfg, sizeof(lcd_cfg));
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Keep board frame buffer count: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Using %d LCD frame buffers", VIDEO_PLAYER_BOARD_LCD_FB_NUM);
    }
}
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */

void video_player_board_init(void)
{
    esp_err_t ret = ESP_OK;

#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    use_double_frame_buffer();
    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init display LCD: %s", esp_err_to_name(ret));
    }
    s_lcd_ready = (ret == ESP_OK);
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Init audio DAC: %s", esp_err_to_name(ret));
    }

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card: %s", esp_err_to_name(ret));
    }
}

void video_player_board_deinit(void)
{
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    if (s_lcd_ready) {
        (void)esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        s_lcd_ready = false;
    }
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */
}

const char *video_player_board_lcd_name(void)
{
#ifdef ESP_BOARD_DEVICE_NAME_DISPLAY_LCD
    return s_lcd_ready ? ESP_BOARD_DEVICE_NAME_DISPLAY_LCD : NULL;
#else
    return NULL;
#endif  /* ESP_BOARD_DEVICE_NAME_DISPLAY_LCD */
}
