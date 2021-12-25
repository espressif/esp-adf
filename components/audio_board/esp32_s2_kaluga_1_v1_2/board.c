/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"
#include "periph_adc_button.h"

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t board_handle = NULL;

audio_board_handle_t audio_board_init(void)
{
    if (board_handle) {
        ESP_LOGW(TAG, "The board has already been initialized!");
        return board_handle;
    }
    board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    AUDIO_MEM_CHECK(TAG, board_handle, return NULL);
    board_handle->audio_hal = audio_board_codec_init();

    return board_handle;
}

audio_hal_handle_t audio_board_codec_init(void)
{
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_hal_handle_t codec_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    AUDIO_NULL_CHECK(TAG, codec_hal, return NULL);
    return codec_hal;
}
esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode)
{
    esp_err_t ret = ESP_OK;
    return ret;
}

display_service_handle_t audio_board_led_init(void)
{
    // TODO
    return NULL;
}
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
esp_err_t _get_lcd_io_bus (void *bus, esp_lcd_panel_io_spi_config_t *io_config,
                           esp_lcd_panel_io_handle_t *out_panel_io)
{
    return esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)bus, io_config, out_panel_io);
}

void *audio_board_lcd_init(esp_periph_set_handle_t set, void *cb)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_CTRL_GPIO
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_CLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16 * LCD_H_RES * 2 + 8
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = LCD_CS_GPIO,
        .pclk_hz = 60 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = cb,
        .user_ctx = NULL,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    periph_lcd_cfg_t cfg = {
        .io_bus = (void *)SPI2_HOST,
        .new_panel_io = _get_lcd_io_bus,
        .lcd_io_cfg = &io_config,
        .new_lcd_panel = esp_lcd_new_panel_st7789,
        .lcd_dev_cfg = &panel_config,
        .rest_cb = NULL,
        .rest_cb_ctx = NULL,
        .lcd_swap_xy = LCD_SWAP_XY,
        .lcd_mirror_x = LCD_MIRROR_X,
        .lcd_mirror_y = LCD_MIRROR_Y,
        .lcd_color_invert = LCD_COLOR_INV,
    };
    esp_periph_handle_t periph_lcd = periph_lcd_init(&cfg);
    AUDIO_NULL_CHECK(TAG, periph_lcd, return NULL;);
    esp_periph_start(set, periph_lcd);
    return (void *)periph_lcd_get_panel_handle(periph_lcd);
}
#endif

esp_err_t audio_board_key_init(esp_periph_set_handle_t set)
{
    esp_err_t ret = ESP_OK;
    periph_adc_button_cfg_t adc_btn_cfg = PERIPH_ADC_BUTTON_DEFAULT_CONFIG();
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_btn_tag.adc_ch = ADC1_CHANNEL_5;
    adc_btn_tag.total_steps = 6;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 2610};
    adc_btn_tag.adc_level_step = btn_array;
    adc_btn_cfg.arr = &adc_btn_tag;
    adc_btn_cfg.arr_size = 1;
    esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
    AUDIO_NULL_CHECK(TAG, adc_btn_handle, return ESP_ERR_ADF_MEMORY_LACK);
    ret = esp_periph_start(set, adc_btn_handle);
    return ret;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret = audio_hal_deinit(audio_board->audio_hal);
    audio_board->audio_hal = NULL;
    audio_free(audio_board);
    board_handle = NULL;
    return ret;
}
