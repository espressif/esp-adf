/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "music_player_display.h"

#include <stdbool.h>
#include <string.h>

#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "dev_display_lcd.h"
#include "dev_lcd_touch.h"
#include "esp_lv_adapter.h"
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
#include "esp_lv_fs.h"
#include "esp_mmap_assets.h"
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS */
#include "music_player_config.h"

static const char *TAG = "MUSIC_PLAYER_DISPLAY";

static lv_display_t *s_disp = NULL;
static lv_indev_t *s_touch = NULL;
static bool s_lcd_board_inited = false;
static bool s_touch_board_inited = false;
static bool s_adapter_inited = false;
static bool s_touch_registered = false;
static bool s_display_inited = false;
static bool s_started = false;
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
static esp_lv_fs_handle_t s_fs_handle = NULL;
static mmap_assets_handle_t s_assets = NULL;
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS */

static void unmount_assets_fs(void);

static void display_release_board_devices(void)
{
    if (s_touch_board_inited) {
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
        s_touch_board_inited = false;
    }
    if (s_lcd_board_inited) {
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        s_lcd_board_inited = false;
    }
}

static void display_release_adapter(void)
{
    s_started = false;
    unmount_assets_fs();
    if (s_touch_registered && s_touch != NULL) {
        esp_lv_adapter_unregister_touch(s_touch);
        s_touch = NULL;
        s_touch_registered = false;
    }
    if (s_disp != NULL) {
        esp_lv_adapter_unregister_display(s_disp);
        s_disp = NULL;
    }
    if (s_adapter_inited) {
        esp_lv_adapter_deinit();
        s_adapter_inited = false;
    }
}

esp_err_t music_player_display_lock_run(void (*cb)(void *ctx), void *ctx)
{
    ESP_GMF_CHECK(TAG, cb != NULL, return ESP_ERR_INVALID_ARG, "Callback is NULL");
    ESP_GMF_CHECK(TAG, s_adapter_inited, return ESP_ERR_INVALID_STATE, "Display adapter not initialized");
    ESP_GMF_RET_ON_ERROR(TAG, esp_lv_adapter_lock(-1), return err_rc_, "Failed to lock LVGL adapter");
    cb(ctx);
    esp_lv_adapter_unlock();
    return ESP_OK;
}

static esp_err_t mount_assets_fs(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
    const mmap_assets_config_t mmap_cfg = {
        .partition_label = "assets",
        .max_files = 1,
        .checksum = 0,
        .flags = {
            .mmap_enable = true,
            .app_bin_check = false,
        },
    };
    esp_err_t ret = mmap_assets_new(&mmap_cfg, &s_assets);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Assets partition not ready, skip mmap mount (%s)", esp_err_to_name(ret));
        return ret;
    }

    const fs_cfg_t fs_cfg = {
        .fs_letter = 'F',
        .fs_assets = s_assets,
        .fs_nums = 1,
    };
    ret = esp_lv_adapter_fs_mount(&fs_cfg, &s_fs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to mount LVGL assets FS (%s)", esp_err_to_name(ret));
        mmap_assets_del(s_assets);
        s_assets = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "LVGL assets FS mounted as F:");
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS */
    return ESP_OK;
}

static void unmount_assets_fs(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS
    if (s_fs_handle != NULL) {
        esp_lv_adapter_fs_unmount(s_fs_handle);
        s_fs_handle = NULL;
    }
    if (s_assets != NULL) {
        mmap_assets_del(s_assets);
        s_assets = NULL;
    }
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FS */
}

esp_err_t music_player_display_init(void)
{
    ESP_GMF_CHECK(TAG, !s_display_inited, return ESP_ERR_INVALID_STATE, "Display already initialized");

    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to init LCD");
    s_lcd_board_inited = true;

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to init touch");
    s_touch_board_inited = true;

    dev_display_lcd_handles_t *lcd_handles = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    dev_lcd_touch_handles_t *touch_handles = NULL;

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_handles);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && lcd_handles != NULL && lcd_handles->panel_handle != NULL,
                  { ret = ESP_FAIL; goto err_cleanup;}, "Invalid LCD handle");
    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && lcd_cfg != NULL, { ret = ESP_FAIL; goto err_cleanup;}, "Invalid LCD config");
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, (void **)&touch_handles);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && touch_handles != NULL && touch_handles->touch_handle != NULL,
                  { ret = ESP_FAIL; goto err_cleanup;}, "Invalid touch handle");

    esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_cfg.task_core_id = 0;
    adapter_cfg.stack_in_psram = true;
    ret = esp_lv_adapter_init(&adapter_cfg);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to init LVGL adapter");
    s_adapter_inited = true;

    esp_lv_adapter_display_config_t disp_cfg;
    if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        disp_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(lcd_handles->panel_handle,
                                                              lcd_handles->io_handle,
                                                              lcd_cfg->lcd_width,
                                                              lcd_cfg->lcd_height,
                                                              ESP_LV_ADAPTER_ROTATE_0);
        disp_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
        disp_cfg.profile.use_psram = true;
    } else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_RGB) == 0) {
        disp_cfg = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(lcd_handles->panel_handle,
                                                             lcd_handles->io_handle,
                                                             lcd_cfg->lcd_width,
                                                             lcd_cfg->lcd_height,
                                                             ESP_LV_ADAPTER_ROTATE_0);
    } else {
        disp_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(lcd_handles->panel_handle,
                                                                        lcd_handles->io_handle,
                                                                        lcd_cfg->lcd_width,
                                                                        lcd_cfg->lcd_height,
                                                                        ESP_LV_ADAPTER_ROTATE_0);
    }

    s_disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_GMF_CHECK(TAG, s_disp != NULL, { ret = ESP_FAIL; goto err_cleanup;}, "Failed to register LVGL display");

    esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_disp, touch_handles->touch_handle);
    s_touch = esp_lv_adapter_register_touch(&touch_cfg);
    ESP_GMF_CHECK(TAG, s_touch != NULL, { ret = ESP_FAIL; goto err_cleanup;}, "Failed to register touch");
    s_touch_registered = true;

    mount_assets_fs();

    s_display_inited = true;
    ESP_LOGI(TAG, "Display initialized: %ux%u (%s)", lcd_cfg->lcd_width, lcd_cfg->lcd_height, lcd_cfg->sub_type);
    return ESP_OK;

err_cleanup:
    display_release_adapter();
    display_release_board_devices();
    return ret;
}

esp_err_t music_player_display_start(void)
{
    ESP_GMF_CHECK(TAG, s_display_inited, return ESP_ERR_INVALID_STATE, "Display not initialized");
    if (s_started) {
        return ESP_OK;
    }
    esp_err_t ret = esp_lv_adapter_start();
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to start LVGL adapter");
    s_started = true;
    return ESP_OK;
}

void music_player_display_deinit(void)
{
    if (!s_display_inited && !s_started && !s_adapter_inited && !s_lcd_board_inited) {
        return;
    }

    display_release_adapter();
    display_release_board_devices();
    s_display_inited = false;
}
