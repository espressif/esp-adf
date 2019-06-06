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
#include "driver/i2c.h"
#include "board.h"
#include "esp_log.h"

/*
 * ES8374 address
 */
#define ES7243_ADDR         0x26

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

static char *TAG = "DRV7243";

audio_hal_func_t AUDIO_CODEC_ES7243_DEFAULT_HANDLE = {
    .audio_codec_initialize = es7243_adc_init,
    .audio_codec_deinitialize = es7243_adc_deinit,
    .audio_codec_ctrl = es7243_adc_ctrl_state,
    .audio_codec_config_iface = es7243_adc_config_i2s,
    .audio_codec_set_volume = es7243_adc_set_voice_volume,
    .audio_codec_get_volume = es7243_adc_get_voice_volume,
};

static int es7243_write_reg(uint8_t reg_addr, uint8_t data)
{
    int res = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, ES7243_ADDR, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, reg_addr, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, data, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    ES_ASSERT(res, "Es7243 Write Reg error", -1);
    return res;
}

int es7243_read_reg(uint8_t reg_addr)
{
    uint8_t data;
    int res = 0;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, ES7243_ADDR, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_write_byte(cmd, reg_addr, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    res |= i2c_master_start(cmd);
    res |= i2c_master_write_byte(cmd, ES7243_ADDR | 0x01, 1 /*ACK_CHECK_EN*/);
    res |= i2c_master_read_byte(cmd, &data, 0x01 /*NACK_VAL*/);
    res |= i2c_master_stop(cmd);
    res |= i2c_master_cmd_begin(0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    ES_ASSERT(res, "Es7243 Read Reg error", -1);
    return (int)data;
}

esp_err_t es7243_adc_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
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
