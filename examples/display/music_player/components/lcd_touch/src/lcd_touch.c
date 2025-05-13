/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "string.h"
#include "esp_log.h"
#include "esp_check.h"
#include "i2c_bus.h"
#include "esp_lcd_touch_tt21100.h"
#include "esp_lcd_touch_gt911.h"
#include "board.h"
#include "lcd_touch.h"

#define DEFAULT_LCD_TOUCH_I2C_CLK 100000

static const char *TAG = "LCD_TOUCH";

esp_err_t lcd_touch_init(esp_lcd_touch_handle_t *ret_touch)
{
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_handle = i2c_bus_get_master_handle(I2C_NUM_0);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config;

    if (ESP_OK == i2c_master_probe(i2c_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 100)) {
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        memcpy(&tp_io_config, &config, sizeof(config));
        ESP_LOGI(TAG, "Touch is GT911");
    } else if (ESP_OK == i2c_master_probe(i2c_handle, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP, 100)) {
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        memcpy(&tp_io_config, &config, sizeof(config));
        ESP_LOGI(TAG, "Touch is GT911 Backup");
    } else if (ESP_OK == i2c_master_probe(i2c_handle, ESP_LCD_TOUCH_IO_I2C_TT21100_ADDRESS, 100)) {
        esp_lcd_panel_io_i2c_config_t config = ESP_LCD_TOUCH_IO_I2C_TT21100_CONFIG();
        memcpy(&tp_io_config, &config, sizeof(config));
        tp_cfg.flags.mirror_x = 1;
        ESP_LOGI(TAG, "Touch is TT21100");
    } else {
        ESP_LOGE(TAG, "Touch not found");
        return ESP_ERR_NOT_FOUND;
    }
    tp_io_config.scl_speed_hz = DEFAULT_LCD_TOUCH_I2C_CLK;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");
    if (ESP_LCD_TOUCH_IO_I2C_TT21100_ADDRESS == tp_io_config.dev_addr) {
        ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_tt21100(tp_io_handle, &tp_cfg, ret_touch), TAG, "New tt21100 failed");
    } else {
        ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch), TAG, "New gt911 failed");
    }

    return ESP_OK;
}

