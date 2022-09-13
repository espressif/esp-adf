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

#include <string.h>
#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "periph_lcd.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "PERIPH_LCD";

typedef struct periph_lcd {
    void                                *io_bus;
    get_lcd_io_bus                      new_panel_io;
    esp_lcd_panel_io_spi_config_t       lcd_io_cfg;
    get_lcd_panel                       new_lcd_panel;
    esp_lcd_panel_dev_config_t          lcd_dev_cfg;

    esp_lcd_panel_io_handle_t           lcd_io_handle;
    esp_lcd_panel_handle_t              lcd_panel_handle;

    perph_lcd_rest                      rest_cb;
    void                                *rest_cb_ctx;
    bool                                lcd_swap_xy;
    bool                                lcd_mirror_x;
    bool                                lcd_mirror_y;
    bool                                lcd_color_invert;
} periph_lcd_t;


esp_err_t _lcd_rest_default(esp_periph_handle_t self, void *ctx);

static esp_err_t _lcd_run(esp_periph_handle_t self, audio_event_iface_msg_t *msg)
{
    return ESP_OK;
}

static esp_err_t _lcd_init(esp_periph_handle_t self)
{
    periph_lcd_t *periph_lcd = esp_periph_get_data(self);
    if (periph_lcd->rest_cb) {
        periph_lcd->rest_cb(self, periph_lcd->rest_cb_ctx);
    }
    // Initialize LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(periph_lcd->lcd_panel_handle));


    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(periph_lcd->lcd_panel_handle, periph_lcd->lcd_color_invert));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(periph_lcd->lcd_panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(periph_lcd->lcd_panel_handle, periph_lcd->lcd_swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(periph_lcd->lcd_panel_handle, periph_lcd->lcd_mirror_x, periph_lcd->lcd_mirror_y));
    return ESP_OK;
}

static esp_err_t _lcd_destroy(esp_periph_handle_t self)
{
    periph_lcd_t *periph_lcd = esp_periph_get_data(self);
    esp_lcd_panel_del(periph_lcd->lcd_panel_handle);
    esp_lcd_panel_io_del(periph_lcd->lcd_io_handle);
    return ESP_OK;
}

esp_err_t _lcd_rest_default(esp_periph_handle_t self, void *ctx)
{
    periph_lcd_t *periph_lcd = esp_periph_get_data(self);
    ESP_ERROR_CHECK(esp_lcd_panel_reset(periph_lcd->lcd_panel_handle));
    return ESP_OK;
}

esp_err_t _setup_lcd(esp_periph_handle_t self)
{
    periph_lcd_t *periph_lcd = esp_periph_get_data(self);
    // Attach the LCD to the specific bus
    ESP_ERROR_CHECK(periph_lcd->new_panel_io((esp_lcd_spi_bus_handle_t)periph_lcd->io_bus,
                    &periph_lcd->lcd_io_cfg, &periph_lcd->lcd_io_handle));
    // Initialize the LCD configuration
    ESP_ERROR_CHECK(periph_lcd->new_lcd_panel(periph_lcd->lcd_io_handle, &periph_lcd->lcd_dev_cfg, &periph_lcd->lcd_panel_handle));
    return ESP_OK;
}

esp_periph_handle_t periph_lcd_init(periph_lcd_cfg_t *config)
{
    periph_lcd_t *periph_lcd = audio_calloc(1, sizeof(periph_lcd_t));
    AUDIO_MEM_CHECK(TAG, periph_lcd, return NULL;);
    periph_lcd->io_bus = config->io_bus;

    memcpy(&periph_lcd->lcd_io_cfg, config->lcd_io_cfg, sizeof(esp_lcd_panel_io_spi_config_t));
    memcpy(&periph_lcd->lcd_dev_cfg, config->lcd_dev_cfg, sizeof(esp_lcd_panel_dev_config_t));
    periph_lcd->new_panel_io = config->new_panel_io;
    periph_lcd->new_lcd_panel = config->new_lcd_panel;
    periph_lcd->lcd_swap_xy = config->lcd_swap_xy;
    periph_lcd->lcd_mirror_x = config->lcd_mirror_x;
    periph_lcd->lcd_mirror_y = config->lcd_mirror_y;
    periph_lcd->lcd_color_invert = config->lcd_color_invert;
    periph_lcd->rest_cb = config->rest_cb;

    esp_periph_handle_t periph = esp_periph_create(PERIPH_ID_LCD, "periph_lcd");
    AUDIO_MEM_CHECK(TAG, periph, {audio_free(periph_lcd);
                                  return NULL;
                                 });
    if (periph_lcd->rest_cb == NULL) {
        periph_lcd->rest_cb = _lcd_rest_default;
    }
    periph_lcd->rest_cb_ctx = config->rest_cb_ctx;

    esp_periph_set_data(periph, periph_lcd);
    esp_periph_set_function(periph, _lcd_init, _lcd_run, _lcd_destroy);
    _setup_lcd(periph);
    return periph;
}

esp_lcd_panel_handle_t periph_lcd_get_panel_handle(esp_periph_handle_t handle)
{
    periph_lcd_t *periph_lcd = esp_periph_get_data(handle);
    if (periph_lcd) {
        return periph_lcd->lcd_panel_handle;
    }
    return NULL;
}

#endif