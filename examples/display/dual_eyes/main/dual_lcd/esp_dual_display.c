/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_dual_lcd.h"
#include "esp_dual_display.h"
#include "esp_lvgl_port.h"

static const char *TAG = "DUAL_DISPLAY";

esp_err_t esp_dual_display_start(lv_disp_t **disp_left, lv_disp_t **disp_right)
{
    display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .double_buffer = true,
        .trans_size = TRANS_SIZE,
        .buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
        },
    };

    cfg.lvgl_port_cfg.task_stack = 6 * 1024;
    cfg.lvgl_port_cfg.task_affinity = SPI_ISR_CPU_ID;
    return esp_dual_display_start_with_config(&cfg, disp_left, disp_right);
}

esp_err_t esp_dual_display_start_with_config(const display_cfg_t *cfg, lv_disp_t **disp_left, lv_disp_t **disp_right)
{
    assert(cfg != NULL);
    ESP_RETURN_ON_ERROR(lvgl_port_init(&cfg->lvgl_port_cfg), TAG, "LVGL port init failed");

    esp_lcd_panel_io_handle_t left_io = NULL;
    esp_lcd_panel_handle_t left_panel = NULL;
    esp_lcd_panel_io_handle_t right_io = NULL;
    esp_lcd_panel_handle_t right_panel = NULL;

    int ret = ESP_OK;
    ESP_GOTO_ON_ERROR(esp_dual_lcd_driver_init(&left_panel, &left_io, &right_panel, &right_io), err, TAG, "");

    /* Add LCD screen */
    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = left_io,
        .panel_handle = left_panel,
        .buffer_size = cfg->buffer_size,
        .double_buffer = cfg->double_buffer,
        .monochrome = false,
        .trans_size = cfg->trans_size,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = cfg->flags.buff_dma,
            .buff_spiram = cfg->flags.buff_spiram,
        },
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
    };

    *disp_left = lvgl_port_add_disp(&disp_cfg);
    if (*disp_left == NULL) {
        ESP_LOGE(TAG, "Failed to add left display");
        goto err;
    }

    disp_cfg.io_handle = right_io;
    disp_cfg.panel_handle = right_panel;
    *disp_right = lvgl_port_add_disp(&disp_cfg);
    if (*disp_right == NULL) {
        ESP_LOGE(TAG, "Failed to add right display");
        goto err;
    }

    ESP_LOGI(TAG, "Dual display started successfully");
    return ret;

err:
    if (*disp_left != NULL) {
        lvgl_port_remove_disp(*disp_left);
        *disp_left = NULL;
    }
    lvgl_port_deinit();
    esp_dual_lcd_driver_deinit();

    return ESP_FAIL;
}
