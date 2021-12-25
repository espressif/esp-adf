/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _PERIPH_LCD_H_
#define _PERIPH_LCD_H_

#ifdef __cplusplus
extern "C" {
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "esp_peripherals.h"

typedef esp_err_t (*perph_lcd_rest)(esp_periph_handle_t self, void *ctx);

typedef esp_err_t (*get_lcd_io_bus)(void *bus, esp_lcd_panel_io_spi_config_t *io_config, esp_lcd_panel_io_handle_t *out_panel_io);

typedef esp_err_t (*get_lcd_panel)(const esp_lcd_panel_io_handle_t panel_io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief   The LCD peripheral configurations
 */
typedef struct {
    void                                *io_bus;
    get_lcd_io_bus                      new_panel_io;
    esp_lcd_panel_io_spi_config_t       *lcd_io_cfg;

    get_lcd_panel                       new_lcd_panel;
    esp_lcd_panel_dev_config_t          *lcd_dev_cfg;
    perph_lcd_rest                      rest_cb;
    void                                *rest_cb_ctx;

    bool                                lcd_swap_xy;
    bool                                lcd_mirror_x;
    bool                                lcd_mirror_y;
    bool                                lcd_color_invert;
} periph_lcd_cfg_t;

/**
 * @brief      Create the LCD peripheral handle for esp_peripherals
 *
 * @note       The handle was created by this function automatically destroy when `esp_periph_destroy` is called
 *
 * @param      config  The configuration
 *
 * @return     The esp peripheral handle
 */
esp_periph_handle_t periph_lcd_init(periph_lcd_cfg_t *config);

/**
 * @brief      Get the `esp_lcd_panel_handle_t` with given LCD peripheral handle
 *
 * @param      handle  The LCD peripheral handle
 *
 * @return     The `esp_lcd_panel_handle_t` handle
 */
esp_lcd_panel_handle_t periph_lcd_get_panel_handle(esp_periph_handle_t handle);

#endif

#ifdef __cplusplus
}
#endif

#endif
