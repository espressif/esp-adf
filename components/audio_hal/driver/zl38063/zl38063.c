/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "driver/i2c.h"
#include "zl38063.h"
#include "vprocTwolf_access.h"
#include "driver/gpio.h"
#include "tw_spi_access.h"
#include "board.h"
#include "audio_volume.h"

static const char *TAG = "zl38063";

static int codec_init_flag = 0;
static codec_dac_volume_config_t *dac_vol_handle;

#define ZL38063_DAC_VOL_CFG_DEFAULT() {                     \
    .max_dac_volume = 6,                                    \
    .min_dac_volume = -90,                                  \
    .board_pa_gain = BOARD_PA_GAIN,                         \
    .volume_accuracy = 1,                                   \
    .dac_vol_symbol = 1,                                    \
    .zero_volume_reg = 0,                                   \
    .reg_value = 0,                                         \
    .user_volume = 0,                                       \
    .offset_conv_volume = NULL,                             \
}

audio_hal_func_t AUDIO_CODEC_ZL38063_DEFAULT_HANDLE = {
    .audio_codec_initialize = zl38063_codec_init,
    .audio_codec_deinitialize = zl38063_codec_deinit,
    .audio_codec_ctrl = zl38063_codec_ctrl_state,
    .audio_codec_config_iface = zl38063_codec_config_i2s,
    .audio_codec_set_mute = zl38063_codec_set_voice_mute,
    .audio_codec_set_volume = zl38063_codec_set_voice_volume,
    .audio_codec_get_volume = zl38063_codec_get_voice_volume,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

static bool zl38063_codec_initialized()
{
    return codec_init_flag;
}

esp_err_t zl38063_codec_init(audio_hal_codec_config_t *cfg)
{
    if (zl38063_codec_initialized()) {
        ESP_LOGW(TAG, "The zl38063 codec has been already initialized");
        return ESP_OK;
    }
    tw_upload_dsp_firmware(0);
    gpio_config_t  borad_conf;
    memset(&borad_conf, 0, sizeof(borad_conf));
    borad_conf.mode = GPIO_MODE_OUTPUT;
    borad_conf.pin_bit_mask = 1UL << (get_reset_board_gpio());
    borad_conf.pull_down_en = 0;
    borad_conf.pull_up_en = 0;

    gpio_config_t  pa_conf;
    memset(&pa_conf, 0, sizeof(pa_conf));
    pa_conf.mode = GPIO_MODE_OUTPUT;
    pa_conf.pin_bit_mask = 1UL << (get_pa_enable_gpio());
    pa_conf.pull_down_en = 0;
    pa_conf.pull_up_en = 0;

    gpio_config(&pa_conf);
    gpio_config(&borad_conf);
    gpio_set_level(get_pa_enable_gpio(), 1);            //enable PA
    gpio_set_level(get_reset_board_gpio(), 0);      //enable DSP
    codec_init_flag = 1;

    codec_dac_volume_config_t vol_cfg = ZL38063_DAC_VOL_CFG_DEFAULT();
    dac_vol_handle = audio_codec_volume_init(&vol_cfg);
    return ESP_OK;
}

esp_err_t zl38063_codec_deinit(void)
{
    gpio_set_level(get_pa_enable_gpio(), 0);
    gpio_set_level(get_reset_board_gpio(), 1);
    codec_init_flag = 0;
    audio_codec_volume_deinit(dac_vol_handle);
    return ESP_OK;
}

esp_err_t zl38063_codec_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t zl38063_codec_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t zl38063_codec_set_voice_mute(bool mute)
{
    /* For now we do not have implementation for this */
    return ESP_OK;
}

/**
 * @param volume: 0 ~ 100, 0 means mute
 *
 * @note Register values. 0xA6: -90 dB, 0xCE: -50 dB, 0x00: 0 dB, 0x06: 6 dB
 * @note Accuracy of gain is 1 dB
 *
 * @return
 *     - (-1)  Error
 *     - (0)   Success
 */
esp_err_t zl38063_codec_set_voice_volume(int volume)
{
    int ret = 0;
    uint8_t reg = 0;
    reg = audio_codec_get_dac_reg_value(dac_vol_handle, volume);
    ret = VprocTwolfSetVolume(reg);
    ESP_LOGD(TAG, "Set volume:%.2d reg_value:0x%.2x dB:%.1f", dac_vol_handle->user_volume, reg,
            (int8_t) reg * 1.0);
    return ret;
}

esp_err_t zl38063_codec_get_voice_volume(int *volume)
{
    int ret = 0;
    int8_t reg = 0;
    ret = VprocTwolfGetVolume(&reg);
    if (reg == (int8_t) dac_vol_handle->reg_value) {
        *volume = dac_vol_handle->user_volume;
    } else {
        *volume = 0;
        ret = ESP_FAIL;
    }
    ESP_LOGD(TAG, "Get volume:%.2d reg_value:0x%x", *volume, (uint8_t)reg);
    return ret;
}
