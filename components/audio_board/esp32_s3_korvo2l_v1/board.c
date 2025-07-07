/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "periph_sdcard.h"
#include "periph_adc_button.h"
#include "tca9554.h"
#include "esp_lcd_ili9341.h"

static const char *TAG = "AUDIO_BOARD";

static audio_board_handle_t board_handle = 0;

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

esp_err_t audio_board_usb_cam_init(void)
{
    gpio_config_t usb_camera_pins_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (BIT64(USB_CAM_PIN_PWDN)),
        .pull_up_en = 0,
        .pull_down_en = 0,
    };
    gpio_config(&usb_camera_pins_cfg);
    gpio_set_level(USB_CAM_PIN_PWDN, 0);

    return ESP_OK;
}

esp_err_t _lcd_rest(esp_periph_handle_t self, void *ctx)
{
    // Reset the LCD
    tca9554_set_output_state(LCD_RST_GPIO, TCA9554_IO_LOW);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    tca9554_set_output_state(LCD_RST_GPIO, TCA9554_IO_HIGH);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t _get_lcd_io_bus (void *bus, esp_lcd_panel_io_spi_config_t *io_config,
                           esp_lcd_panel_io_handle_t *out_panel_io)
{
    return esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)bus, io_config, out_panel_io);
}

display_service_handle_t audio_board_led_init(void)
{
    return NULL;
}

void *audio_board_lcd_init(esp_periph_set_handle_t set, void *cb)
{
    esp_tca9554_config_t pca_cfg = {
        .i2c_scl = GPIO_NUM_18,
        .i2c_sda = GPIO_NUM_17,
        .interrupt_output = -1,
    };
    tca9554_init(&pca_cfg);
    // Set LCD_BL_CTRL output
    tca9554_set_io_config(LCD_CTRL_GPIO, TCA9554_IO_OUTPUT);
    // Set LCD_RST output
    tca9554_set_io_config(LCD_RST_GPIO, TCA9554_IO_OUTPUT);
    // Set LCD_CS pin output
    tca9554_set_io_config(LCD_CS_GPIO, TCA9554_IO_OUTPUT);

    tca9554_set_output_state(LCD_CTRL_GPIO, TCA9554_IO_HIGH);
    tca9554_set_output_state(LCD_CS_GPIO, TCA9554_IO_HIGH);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    tca9554_set_output_state(LCD_CS_GPIO, TCA9554_IO_LOW);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_CLK_GPIO,
        .mosi_io_num = LCD_MOSI_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_V_RES * LCD_H_RES * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_GPIO,
        .cs_gpio_num = -1,
        .pclk_hz = 60 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = cb,
        .user_ctx = NULL,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };
    periph_lcd_cfg_t cfg = {
        .io_bus = (void *)SPI2_HOST,
        .new_panel_io = _get_lcd_io_bus,
        .lcd_io_cfg = &io_config,
        .new_lcd_panel = esp_lcd_new_panel_ili9341,
        .lcd_dev_cfg = &panel_config,
        .rest_cb = _lcd_rest,
        .rest_cb_ctx = NULL,
        .lcd_swap_xy = LCD_SWAP_XY,
        .lcd_mirror_x = LCD_MIRROR_X,
        .lcd_mirror_y = LCD_MIRROR_Y,
        .lcd_color_invert = LCD_COLOR_INV,
    };
    esp_periph_handle_t periph_lcd = periph_lcd_init(&cfg);
    AUDIO_NULL_CHECK(TAG, periph_lcd, return NULL);

    esp_periph_start(set, periph_lcd);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    return (void *)periph_lcd_get_panel_handle(periph_lcd);
}

esp_err_t audio_board_key_init(esp_periph_set_handle_t set)
{
    periph_adc_button_cfg_t adc_btn_cfg = PERIPH_ADC_BUTTON_DEFAULT_CONFIG();
    adc_arr_t adc_btn_tag = ADC_DEFAULT_ARR();
    adc_btn_tag.total_steps = 6;
    adc_btn_tag.adc_ch = ADC_CHANNEL_4;
    int btn_array[7] = {190, 600, 1000, 1375, 1775, 2195, 3000};
    adc_btn_tag.adc_level_step = btn_array;
    adc_btn_cfg.arr = &adc_btn_tag;
    adc_btn_cfg.arr_size = 1;
    esp_periph_handle_t adc_btn_handle = periph_adc_button_init(&adc_btn_cfg);
    AUDIO_NULL_CHECK(TAG, adc_btn_handle, return ESP_ERR_ADF_MEMORY_LACK);
    return esp_periph_start(set, adc_btn_handle);

}

esp_err_t audio_board_sdcard_init(esp_periph_set_handle_t set, periph_sdcard_mode_t mode)
{
    if (mode != SD_MODE_1_LINE) {
        ESP_LOGE(TAG, "Current board only support 1-line SD mode!");
        return ESP_FAIL;
    }
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(),
        .mode = mode
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_err_t ret = esp_periph_start(set, sdcard_handle);
    int retry_time = 5;
    bool mount_flag = false;
    while (retry_time --) {
        if (periph_sdcard_is_mounted(sdcard_handle)) {
            mount_flag = true;
            break;
        } else {
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }
    }
    if (mount_flag == false) {
        ESP_LOGE(TAG, "Sdcard mount failed");
        return ESP_FAIL;
    }
    return ret;
}

audio_board_handle_t audio_board_get_handle(void)
{
    return board_handle;
}

esp_err_t audio_board_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret |= audio_hal_deinit(audio_board->audio_hal);
    audio_free(audio_board);
    board_handle = NULL;
    return ret;
}
