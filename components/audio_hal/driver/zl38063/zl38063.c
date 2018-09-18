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
#include "lyratd_msc_v2_1_board.h"

static int8_t tw_vol[15] = { -90, -45, -30, -23, -16, -10, -4, -1, 0, 1, 2, 3, 4, 5, 6};

esp_err_t zl38063_init(audio_hal_codec_config_t *cfg)
{
    tw_upload_dsp_firmware(0);
    gpio_config_t BORAD_CONF = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1UL << (BOARD_RESET_CTRL),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config_t PA_CONF = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1UL << (GPIO_PA_EN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&PA_CONF);
    gpio_config(&BORAD_CONF);
    gpio_set_level(GPIO_PA_EN, 1);            //enable PA
    gpio_set_level(BOARD_RESET_CTRL, 0);      //enable DSP
    return ESP_OK;
}

esp_err_t zl38063_deinit(void)
{
    gpio_set_level(GPIO_PA_EN, 0);
    gpio_set_level(BOARD_RESET_CTRL, 1);
    return ESP_OK;
}

esp_err_t zl38063_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t zl38063_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t zl38063_set_voice_volume(int volume)
{
    int ret = 0;
    if (volume < 0 ) {
        volume = 0;
    } else if (volume >= 100) {
        volume = 100;
    }
    int k = volume / 7;
    ret = VprocTwolfSetVolume(tw_vol[k]);
    return ret;
}

esp_err_t zl38063_get_voice_volume(int *volume)
{
    int ret = 0;
    int8_t vol = 0;
    ret = VprocTwolfGetVolume(&vol);
    *volume = 0;
    for (int i = 0; i < sizeof(tw_vol); ++i) {
        if (vol == tw_vol[i]) {
            *volume = i * 7;
        }
    }
    return ret;
}
