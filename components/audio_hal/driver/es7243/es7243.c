/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "es7243.h"
#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

static char *TAG = "DRV7243";
static i2c_bus_handle_t i2c_handle;
static int es7243_addr = 0x26;

audio_hal_func_t AUDIO_CODEC_ES7243_DEFAULT_HANDLE = {
    .audio_codec_initialize = es7243_adc_init,
    .audio_codec_deinitialize = es7243_adc_deinit,
    .audio_codec_ctrl = es7243_adc_ctrl_state,
    .audio_codec_config_iface = es7243_adc_config_i2s,
    .audio_codec_set_mute = es7243_adc_set_voice_mute,
    .audio_codec_set_volume = es7243_adc_set_voice_volume,
    .audio_codec_get_volume = es7243_adc_get_voice_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

static esp_err_t es7243_write_reg(uint8_t reg_add, uint8_t data)
{
    return i2c_bus_write_bytes(i2c_handle, es7243_addr, &reg_add, sizeof(reg_add), &data, sizeof(data));
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
    ES_ASSERT(res, "getting i2c pins error", -1);
    i2c_handle = i2c_bus_create(I2C_NUM_0, &es_i2c_cfg);
    return res;
}

esp_err_t es7243_adc_set_addr(int addr)
{
    es7243_addr = addr;
    return ESP_OK;
}

esp_err_t es7243_adc_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
    i2c_init();
    ret |= es7243_write_reg(0x00, 0x01);
    ret |= es7243_write_reg(0x06, 0x00);
    ret |= es7243_write_reg(0x05, 0x1B);
    ret |= es7243_write_reg(0x01, 0x0C);
    ret |= es7243_write_reg(0x08, 0x43);
    ret |= es7243_write_reg(0x05, 0x13);
    if (ret) {
        ESP_LOGE(TAG, "Es7243 initialize failed!");
        return ESP_FAIL;
    }
    return ret;
}

esp_err_t es7243_adc_deinit(void)
{
    return ESP_OK;
}

esp_err_t es7243_adc_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t es7243_adc_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t es7243_adc_set_voice_mute(bool mute)
{
    /* Not implemented yet */
    return ESP_OK;
}

esp_err_t es7243_adc_set_voice_volume(int volume)
{
    esp_err_t ret = ESP_OK;
    if (volume > 100) {
        volume = 100;
    }
    if (volume < 0) {
        volume = 0;
    }
    switch (volume) {
        case 0 ... 12:
            ret |= es7243_write_reg(0x08, 0x11); // 1db
            break;
        case 13 ... 25:
            ret |= es7243_write_reg(0x08, 0x13); //3.5db
            break;
        case 26 ... 38:
            ret |= es7243_write_reg(0x08, 0x21); //18db
            break;
        case 39 ... 51:
            ret |= es7243_write_reg(0x08, 0x23); //20.5db
            break;
        case 52 ... 65:
            ret |= es7243_write_reg(0x08, 0x06); //22.5db
            break;
        case 66 ... 80:
            ret |= es7243_write_reg(0x08, 0x41); //24.5db
            break;
        case 81 ... 90:
            ret |= es7243_write_reg(0x08, 0x07); //25db
            break;
        case 91 ... 100:
            ret |= es7243_write_reg(0x08, 0x43); //27db
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t es7243_adc_get_voice_volume(int *volume)
{
    return ESP_OK;
}
