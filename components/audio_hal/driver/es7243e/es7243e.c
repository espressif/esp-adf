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

#include "es7243e.h"
#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"

static char *TAG = "DRV7243E";
static i2c_bus_handle_t i2c_handle;
static int es7243e_addr = 0x20;

audio_hal_func_t AUDIO_CODEC_ES7243E_DEFAULT_HANDLE = {
    .audio_codec_initialize = es7243e_adc_init,
    .audio_codec_deinitialize = es7243e_adc_deinit,
    .audio_codec_ctrl = es7243e_adc_ctrl_state,
    .audio_codec_config_iface = es7243e_adc_config_i2s,
    .audio_codec_set_mute = NULL,
    .audio_codec_set_volume = es7243e_adc_set_voice_volume,
    .audio_codec_get_volume = es7243e_adc_get_voice_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

static esp_err_t es7243e_write_reg(uint8_t reg_add, uint8_t data)
{
    return i2c_bus_write_bytes(i2c_handle, es7243e_addr, &reg_add, sizeof(reg_add), &data, sizeof(data));
}

static int i2c_init()
{
    int res = 0;
    i2c_config_t es_i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    res = get_i2c_pins(I2C_NUM_0, &es_i2c_cfg);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "getting i2c pins error");
    }
    i2c_handle = i2c_bus_create(I2C_NUM_0, &es_i2c_cfg);
    return res;
}

esp_err_t es7243e_adc_set_addr(int addr)
{
    es7243e_addr = addr;
    return ESP_OK;
}

esp_err_t es7243e_adc_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
    i2c_init();
    ret |= es7243e_write_reg(0x01, 0x3A);
    ret |= es7243e_write_reg(0x00, 0x80);
    ret |= es7243e_write_reg(0xF9, 0x00);
    ret |= es7243e_write_reg(0x04, 0x02);
    ret |= es7243e_write_reg(0x04, 0x01);
    ret |= es7243e_write_reg(0xF9, 0x01);
    ret |= es7243e_write_reg(0x00, 0x1E);
    ret |= es7243e_write_reg(0x01, 0x00);

    ret |= es7243e_write_reg(0x02, 0x00);
    ret |= es7243e_write_reg(0x03, 0x20);
    ret |= es7243e_write_reg(0x04, 0x01);
    ret |= es7243e_write_reg(0x0D, 0x00);
    ret |= es7243e_write_reg(0x05, 0x00);
    ret |= es7243e_write_reg(0x06, 0x03); // SCLK=MCLK/4
    ret |= es7243e_write_reg(0x07, 0x00); // LRCK=MCLK/256
    ret |= es7243e_write_reg(0x08, 0xFF); // LRCK=MCLK/256

    ret |= es7243e_write_reg(0x09, 0xCA);
    ret |= es7243e_write_reg(0x0A, 0x85);
    ret |= es7243e_write_reg(0x0B, 0x00);
    ret |= es7243e_write_reg(0x0E, 0xBF);
    ret |= es7243e_write_reg(0x0F, 0x80);
    ret |= es7243e_write_reg(0x14, 0x0C);
    ret |= es7243e_write_reg(0x15, 0x0C);
    ret |= es7243e_write_reg(0x17, 0x02);
    ret |= es7243e_write_reg(0x18, 0x26);
    ret |= es7243e_write_reg(0x19, 0x77);
    ret |= es7243e_write_reg(0x1A, 0xF4);
    ret |= es7243e_write_reg(0x1B, 0x66);
    ret |= es7243e_write_reg(0x1C, 0x44);
    ret |= es7243e_write_reg(0x1E, 0x00);
    ret |= es7243e_write_reg(0x1F, 0x0C);
    ret |= es7243e_write_reg(0x20, 0x1A); //PGA gain +30dB
    ret |= es7243e_write_reg(0x21, 0x1A); //PGA gain +30dB

    ret |= es7243e_write_reg(0x00, 0x80); //Slave  Mode
    ret |= es7243e_write_reg(0x01, 0x3A);
    ret |= es7243e_write_reg(0x16, 0x3F);
    ret |= es7243e_write_reg(0x16, 0x00);
    if (ret) {
        ESP_LOGE(TAG, "Es7243e initialize failed!");
        return ESP_FAIL;
    }
    return ret;
}

esp_err_t es7243e_adc_deinit(void)
{
    return ESP_OK;
}

esp_err_t es7243e_adc_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    esp_err_t ret = ESP_OK;
    if (ctrl_state == AUDIO_HAL_CTRL_START) {
        ret |= es7243e_write_reg(0xF9, 0x00);
        ret |= es7243e_write_reg(0x04, 0x01);
        ret |= es7243e_write_reg(0x17, 0x01);
        ret |= es7243e_write_reg(0x20, 0x10);
        ret |= es7243e_write_reg(0x21, 0x10);
        ret |= es7243e_write_reg(0x00, 0x80);
        ret |= es7243e_write_reg(0x01, 0x3A);
        ret |= es7243e_write_reg(0x16, 0x3F);
        ret |= es7243e_write_reg(0x16, 0x00);
    } else {
        ESP_LOGW(TAG, "The codec going to stop");
        ret |= es7243e_write_reg(0x04, 0x02);
        ret |= es7243e_write_reg(0x04, 0x01);
        ret |= es7243e_write_reg(0xF7, 0x30);
        ret |= es7243e_write_reg(0xF9, 0x01);
        ret |= es7243e_write_reg(0x16, 0xFF);
        ret |= es7243e_write_reg(0x17, 0x00);
        ret |= es7243e_write_reg(0x01, 0x38);
        ret |= es7243e_write_reg(0x20, 0x00);
        ret |= es7243e_write_reg(0x21, 0x00);
        ret |= es7243e_write_reg(0x00, 0x00);
        ret |= es7243e_write_reg(0x00, 0x1E);
        ret |= es7243e_write_reg(0x01, 0x30);
        ret |= es7243e_write_reg(0x01, 0x00);
    }
    return ret;
}

esp_err_t es7243e_adc_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t es7243e_adc_set_voice_volume(int volume)
{
    return ESP_OK;
}

esp_err_t es7243e_adc_get_voice_volume(int *volume)
{
    return ESP_OK;
}
