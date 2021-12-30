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

#include "string.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "es8156.h"
#include "driver/gpio.h"
#include "board.h"

#define ES8156_ADDR         0x10
#define VOLUME_STEP_NUM     10

static const char *TAG = "DRV8156";
static bool codec_init_flag = 0;
static i2c_bus_handle_t i2c_handle;

// 0dB = 0xBF;
static uint8_t reg_vol[VOLUME_STEP_NUM] = {0, 139, 151, 163, 171, 175, 179, 183, 187, 191};

audio_hal_func_t AUDIO_CODEC_ES8156_DEFAULT_HANDLE = {
    .audio_codec_initialize = es8156_codec_init,
    .audio_codec_deinitialize = es8156_codec_deinit,
    .audio_codec_ctrl = es8156_codec_ctrl_state,
    .audio_codec_config_iface = es8156_codec_config_i2s,
    .audio_codec_set_mute = es8156_codec_set_voice_mute,
    .audio_codec_set_volume = es8156_codec_set_voice_volume,
    .audio_codec_get_volume = es8156_codec_get_voice_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

static bool es8156_codec_initialized()
{
    return codec_init_flag;
}

static esp_err_t es8156_write_reg(uint8_t reg_addr, uint8_t data)
{
    return i2c_bus_write_bytes(i2c_handle, ES8156_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));
}

static int es8156_read_reg(uint8_t reg_addr)
{
    uint8_t data;
    i2c_bus_read_bytes(i2c_handle, ES8156_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));
    return (int)data;
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

static esp_err_t es8156_standby(void)
{
    esp_err_t ret = 0;
    ret = es8156_write_reg(0x14, 0x00);
    ret |= es8156_write_reg(0x19, 0x02);
    ret |= es8156_write_reg(0x21, 0x1F);
    ret |= es8156_write_reg(0x22, 0x02);
    ret |= es8156_write_reg(0x25, 0x21);
    ret |= es8156_write_reg(0x25, 0xA1);
    ret |= es8156_write_reg(0x18, 0x01);
    ret |= es8156_write_reg(0x09, 0x02);
    ret |= es8156_write_reg(0x09, 0x01);
    ret |= es8156_write_reg(0x08, 0x00);
    return ret;
}

static esp_err_t es8156_resume(void)
{
    esp_err_t ret = 0;
    ret |= es8156_write_reg(0x08, 0x3F);
    ret |= es8156_write_reg(0x09, 0x00);
    ret |= es8156_write_reg(0x18, 0x00);

    ret |= es8156_write_reg(0x25, 0x20);
    ret |= es8156_write_reg(0x22, 0x00);
    ret |= es8156_write_reg(0x21, 0x3C);
    ret |= es8156_write_reg(0x19, 0x20);
    ret |= es8156_write_reg(0x14, 179);
    return ret;
}

void es8156_pa_power(bool enable)
{
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(get_pa_enable_gpio());
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    if (enable) {
        gpio_set_level(get_pa_enable_gpio(), 1);
    } else {
        gpio_set_level(get_pa_enable_gpio(), 0);
    }
}

esp_err_t es8156_codec_init(audio_hal_codec_config_t *cfg)
{
    if (es8156_codec_initialized()) {
        ESP_LOGW(TAG, "The es8156 DAC has been already initialized");
        return ESP_OK;
    }
    codec_init_flag = true;

    i2c_init();
    es8156_write_reg(0x02, 0x04);
    es8156_write_reg(0x20, 0x2A);
    es8156_write_reg(0x21, 0x3C);
    es8156_write_reg(0x22, 0x00);
    es8156_write_reg(0x24, 0x07);
    es8156_write_reg(0x23, 0x00);

    es8156_write_reg(0x0A, 0x01);
    es8156_write_reg(0x0B, 0x01);
    es8156_write_reg(0x11, 0x00);
    es8156_write_reg(0x14, 179);  // volume 70%

    es8156_write_reg(0x0D, 0x14);
    es8156_write_reg(0x18, 0x00);
    es8156_write_reg(0x08, 0x3F);
    es8156_write_reg(0x00, 0x02);
    es8156_write_reg(0x00, 0x03);
    es8156_write_reg(0x25, 0x20);

    es8156_pa_power(true);
    return ESP_OK;
}

esp_err_t es8156_codec_deinit(void)
{
    codec_init_flag = false;
    return ESP_OK;
}

esp_err_t es8156_codec_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    esp_err_t ret = ESP_OK;
    if (ctrl_state == AUDIO_HAL_CTRL_START) {
        ret = es8156_resume();
    } else {
        ESP_LOGW(TAG, "The codec going to stop");
        ret = es8156_standby();
    }
    return ret;
}

esp_err_t es8156_codec_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t es8156_codec_set_voice_mute(bool enable)
{
    int regv = es8156_read_reg(ES8156_DAC_MUTE_REG13);
    if (enable) {
        regv = regv | BIT(1) | BIT(2);
    } else {
        regv = regv & (~(BIT(1) | BIT(2))) ;
    }
    es8156_write_reg(ES8156_DAC_MUTE_REG13, regv);
    return ESP_OK;
}

esp_err_t es8156_codec_set_voice_volume(int volume)
{
    int ret = 0;
    if (volume < 0) {
        volume = 0;
    } else if (volume >= 100) {
        volume = 99;
    }
    int vol = (volume) / VOLUME_STEP_NUM;
    ESP_LOGD(TAG, "SET: volume:%d, regv:%d", volume, reg_vol[vol]);
    es8156_write_reg(ES8156_VOLUME_CONTROL_REG14, reg_vol[vol]);
    return ret;
}

esp_err_t es8156_codec_get_voice_volume(int *volume)
{
    int ret = 0;
    int regv = 0;
    *volume = 0;
    regv = es8156_read_reg(ES8156_VOLUME_CONTROL_REG14);
    if (regv == ESP_FAIL) {
        ret = ESP_FAIL;
    } else {
        for (int i = 0; i < VOLUME_STEP_NUM; ++i) {
            if (reg_vol[i] == regv) {
                *volume = i * VOLUME_STEP_NUM;
            }
        }
    }
    ESP_LOGD(TAG, "GET: regv:%d, volume:%d%%", regv, *volume);
    return ret;
}
