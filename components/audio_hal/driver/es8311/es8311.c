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
#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"
#include "es8311.h"
#include "audio_volume.h"

/* ES8311 address
 * 0x32:CE=1;0x30:CE=0
 */
#define ES8311_ADDR         0x30

/*
 * to define the clock soure of MCLK
 */
#define FROM_MCLK_PIN       0
#define FROM_SCLK_PIN       1

/*
 * to define whether to reverse the clock
 */
#define INVERT_MCLK         0 // do not invert
#define INVERT_SCLK         0

#define IS_DMIC             0 // Is it a digital microphone

#define MCLK_DIV_FRE        256

static i2c_bus_handle_t i2c_handle;
static codec_dac_volume_config_t *dac_vol_handle;

#define ES8311_DAC_VOL_CFG_DEFAULT() {                      \
    .max_dac_volume = 32,                                   \
    .min_dac_volume = -95.5,                                \
    .board_pa_gain = BOARD_PA_GAIN,                         \
    .volume_accuracy = 0.5,                                 \
    .dac_vol_symbol = 1,                                    \
    .zero_volume_reg = 0xBF,                                \
    .reg_value = 0,                                         \
    .user_volume = 0,                                       \
    .offset_conv_volume = NULL,                             \
}

/*
 * operate function of codec
 */
audio_hal_func_t AUDIO_CODEC_ES8311_DEFAULT_HANDLE = {
    .audio_codec_initialize = es8311_codec_init,
    .audio_codec_deinitialize = es8311_codec_deinit,
    .audio_codec_ctrl = es8311_codec_ctrl_state,
    .audio_codec_config_iface = es8311_codec_config_i2s,
    .audio_codec_set_mute = es8311_set_voice_mute,
    .audio_codec_set_volume = es8311_codec_set_voice_volume,
    .audio_codec_get_volume = es8311_codec_get_voice_volume,
    .audio_codec_enable_pa = es8311_pa_power,
    .audio_hal_lock = NULL,
    .handle = NULL,
};

/*
 * Clock coefficient structer
 */
struct _coeff_div {
    uint32_t mclk;        /* mclk frequency */
    uint32_t rate;        /* sample rate */
    uint8_t pre_div;      /* the pre divider with range from 1 to 8 */
    uint8_t pre_multi;    /* the pre multiplier with x1, x2, x4 and x8 selection */
    uint8_t adc_div;      /* adcclk divider */
    uint8_t dac_div;      /* dacclk divider */
    uint8_t fs_mode;      /* double speed or single speed, =0, ss, =1, ds */
    uint8_t lrck_h;       /* adclrck divider and daclrck divider */
    uint8_t lrck_l;
    uint8_t bclk_div;     /* sclk divider */
    uint8_t adc_osr;      /* adc osr */
    uint8_t dac_osr;      /* dac osr */
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
    //mclk     rate   pre_div  mult  adc_div dac_div fs_mode lrch  lrcl  bckdiv osr
    /* 8k */
    {12288000, 8000 , 0x06, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 8000 , 0x03, 0x02, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x20},
    {16384000, 8000 , 0x08, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000 , 8000 , 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000 , 8000 , 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000 , 8000 , 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000 , 8000 , 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000 , 8000 , 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000 , 8000 , 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000 , 8000 , 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

    /* 11.025k */
    {11289600, 11025, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {5644800 , 11025, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2822400 , 11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1411200 , 11025, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

    /* 12k */
    {12288000, 12000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000 , 12000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000 , 12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000 , 12000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

    /* 16k */
    {12288000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 16000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x20},
    {16384000, 16000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000 , 16000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000 , 16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000 , 16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000 , 16000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000 , 16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000 , 16000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000 , 16000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

    /* 22.05k */
    {11289600, 22050, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800 , 22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400 , 22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200 , 22050, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 24k */
    {12288000, 24000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000 , 24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000 , 24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000 , 24000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 32k */
    {12288000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x04, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000 , 32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000 , 32000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000 , 32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000 , 32000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000 , 32000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000 , 32000, 0x03, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000 , 32000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 44.1k */
    {11289600, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800 , 44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400 , 44100, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200 , 44100, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 48k */
    {12288000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000 , 48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000 , 48000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000 , 48000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

    /* 64k */
    {12288000, 64000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000 , 64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000 , 64000, 0x01, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {4096000 , 64000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000 , 64000, 0x01, 0x08, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {2048000 , 64000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000 , 64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0xbf, 0x03, 0x18, 0x18},
    {1024000 , 64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 88.2k */
    {11289600, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800 , 88200, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400 , 88200, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200 , 88200, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

    /* 96k */
    {12288000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000 , 96000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000 , 96000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000 , 96000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

static const char *TAG = "DRV8311";

#define ES_ASSERT(a, format, b, ...) \
    if ((a) != 0) { \
        ESP_LOGE(TAG, format, ##__VA_ARGS__); \
        return b;\
    }

int8_t get_es8311_mclk_src(void);

static esp_err_t es8311_write_reg(uint8_t reg_addr, uint8_t data)
{
    return i2c_bus_write_bytes(i2c_handle, ES8311_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));
}

static int es8311_read_reg(uint8_t reg_addr)
{
    uint8_t data;
    i2c_bus_read_bytes(i2c_handle, ES8311_ADDR, &reg_addr, sizeof(reg_addr), &data, sizeof(data));
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
    ES_ASSERT(res, "getting i2c pins error", -1);
    i2c_handle = i2c_bus_create(I2C_NUM_0, &es_i2c_cfg);
    return res;
}

/*
* look for the coefficient in coeff_div[] table
*/
static int get_coeff(uint32_t mclk, uint32_t rate)
{
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++) {
        if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
            return i;
    }
    return -1;
}

/*
* set es8311 dac mute or not
* if mute = 0, dac un-mute
* if mute = 1, dac mute
*/
static void es8311_mute(int mute)
{
    uint8_t regv;
    ESP_LOGI(TAG, "Enter into es8311_mute(), mute = %d\n", mute);
    regv = es8311_read_reg(ES8311_DAC_REG31) & 0x9f;
    if (mute) {
        es8311_write_reg(ES8311_DAC_REG31, regv | 0x60);
    } else {
        es8311_write_reg(ES8311_DAC_REG31, regv);
    }
}

/*
* set es8311 into suspend mode
*/
static void es8311_suspend(void)
{
    ESP_LOGI(TAG, "Enter into es8311_suspend()");
    es8311_write_reg(ES8311_DAC_REG32, 0x00);
    es8311_write_reg(ES8311_ADC_REG17, 0x00);
    es8311_write_reg(ES8311_SYSTEM_REG0E, 0xFF);
    es8311_write_reg(ES8311_SYSTEM_REG12, 0x02);
    es8311_write_reg(ES8311_SYSTEM_REG14, 0x00);
    es8311_write_reg(ES8311_SYSTEM_REG0D, 0xFA);
    es8311_write_reg(ES8311_ADC_REG15, 0x00);
    es8311_write_reg(ES8311_GP_REG45, 0x01);
}

/*
* enable pa power
*/
esp_err_t es8311_pa_power(bool enable)
{
    esp_err_t ret = ESP_OK;
    if (get_pa_enable_gpio() == -1) {
        return ret;
    }
    if (enable) {
        ret = gpio_set_level(get_pa_enable_gpio(), 1);
    } else {
        ret = gpio_set_level(get_pa_enable_gpio(), 0);
    }
    return ret;
}

esp_err_t es8311_config_sample(int sample_rate)
{
    esp_err_t ret = ESP_OK;
    uint8_t datmp, regv;
    int sample_fre = 0;
    int mclk_fre = 0;
    int coeff;

    switch (sample_rate) {
        case AUDIO_HAL_08K_SAMPLES:
            sample_fre = 8000;
            break;
        case AUDIO_HAL_11K_SAMPLES:
            sample_fre = 11025;
            break;
        case AUDIO_HAL_16K_SAMPLES:
            sample_fre = 16000;
            break;
        case AUDIO_HAL_22K_SAMPLES:
            sample_fre = 22050;
            break;
        case AUDIO_HAL_24K_SAMPLES:
            sample_fre = 24000;
            break;
        case AUDIO_HAL_32K_SAMPLES:
            sample_fre = 32000;
            break;
        case AUDIO_HAL_44K_SAMPLES:
            sample_fre = 44100;
            break;
        case AUDIO_HAL_48K_SAMPLES:
            sample_fre = 48000;
            break;
        default:
            ESP_LOGE(TAG, "Unable to configure sample rate %dHz", sample_fre);
            break;
    }

    mclk_fre = sample_fre * MCLK_DIV_FRE;
    coeff = get_coeff(mclk_fre, sample_fre);
    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_fre, mclk_fre);
        return ESP_FAIL;
    }
    /* Set clock parammeters */
    if (coeff >= 0) {
        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG02) & 0x07;
        regv |= (coeff_div[coeff].pre_div - 1) << 5;
        datmp = 0;
        switch (coeff_div[coeff].pre_multi) {
            case 1:
                datmp = 0;
                break;
            case 2:
                datmp = 1;
                break;
            case 4:
                datmp = 2;
                break;
            case 8:
                datmp = 3;
                break;
            default:
                break;
        }

        if (get_es8311_mclk_src() == FROM_SCLK_PIN) {
            datmp = 3;  /* DIG_MCLK = LRCK * 256 = BCLK * 8 */
            if (sample_fre == 8000) { 
                /* When the sample rate is 8kHz, BCLK requires at least 512K (slot bit needs to be configured to 32bit).
                   DIG_MCLK = LRCK * 256 = BCLK * 4 */
                datmp = 2;
            }
        }

        regv |= (datmp) << 3;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG02, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG05) & 0x00;
        regv |= (coeff_div[coeff].adc_div - 1) << 4;
        regv |= (coeff_div[coeff].dac_div - 1) << 0;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG05, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG03) & 0x80;
        regv |= coeff_div[coeff].fs_mode << 6;
        regv |= coeff_div[coeff].adc_osr << 0;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG03, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG04) & 0x80;
        regv |= coeff_div[coeff].dac_osr << 0;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG04, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG07) & 0xC0;
        regv |= coeff_div[coeff].lrck_h << 0;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG07, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG08) & 0x00;
        regv |= coeff_div[coeff].lrck_l << 0;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG08, regv);

        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG06) & 0xE0;
        if (coeff_div[coeff].bclk_div < 19) {
            regv |= (coeff_div[coeff].bclk_div - 1) << 0;
        } else {
            regv |= (coeff_div[coeff].bclk_div) << 0;
        }
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG06, regv);
    }

    return ESP_OK;
}

esp_err_t es8311_codec_init(audio_hal_codec_config_t *codec_cfg)
{
    uint8_t regv;
    esp_err_t ret = ESP_OK;
    i2c_init();  // ESP32 in master mode

    /* Enhance ES8311 I2C noise immunity */
    ret |= es8311_write_reg(ES8311_GPIO_REG44, 0x08);
    /* Due to occasional failures during the first I2C write with the ES8311 chip, a second write is performed to ensure reliability */
    ret |= es8311_write_reg(ES8311_GPIO_REG44, 0x08);

    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x30);
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG02, 0x00);
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG03, 0x10);
    ret |= es8311_write_reg(ES8311_ADC_REG16, 0x24);
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG04, 0x10);
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG05, 0x00);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG0B, 0x00);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG0C, 0x00);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG10, 0x1F);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG11, 0x7F);
    ret |= es8311_write_reg(ES8311_RESET_REG00, 0x80);
    /* Set Codec into Master or Slave mode */
    regv = es8311_read_reg(ES8311_RESET_REG00);
    /* Set master/slave audio interface */
    audio_hal_codec_i2s_iface_t *i2s_cfg = &(codec_cfg->i2s_iface);
    switch (i2s_cfg->mode) {
        case AUDIO_HAL_MODE_MASTER:  /* MASTER MODE */
            ESP_LOGI(TAG, "ES8311 in Master mode");
            regv |= 0x40;
            break;
        case AUDIO_HAL_MODE_SLAVE:  /* SLAVE MODE */
            ESP_LOGI(TAG, "ES8311 in Slave mode");
            regv &= 0xBF;
            break;
        default:
            regv &= 0xBF;
    }
    ret |= es8311_write_reg(ES8311_RESET_REG00, regv);
    ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, 0x3F);
    /* Select clock source for internal mclk */
    switch (get_es8311_mclk_src()) {
        case FROM_MCLK_PIN:
            regv = es8311_read_reg(ES8311_CLK_MANAGER_REG01);
            regv &= 0x7F;
            ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
            break;
        case FROM_SCLK_PIN:
            regv = es8311_read_reg(ES8311_CLK_MANAGER_REG01);
            regv |= 0x80;
            ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
            break;
        default:
            regv = es8311_read_reg(ES8311_CLK_MANAGER_REG01);
            regv &= 0x7F;
            ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
            break;
    }

    es8311_config_sample(i2s_cfg->samples);

    /* mclk inverted or not */
    if (INVERT_MCLK) {
        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG01);
        regv |= 0x40;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
    } else {
        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG01);
        regv &= ~(0x40);
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG01, regv);
    }
    /* sclk inverted or not */
    if (INVERT_SCLK) {
        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG06);
        regv |= 0x20;
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG06, regv);
    } else {
        regv = es8311_read_reg(ES8311_CLK_MANAGER_REG06);
        regv &= ~(0x20);
        ret |= es8311_write_reg(ES8311_CLK_MANAGER_REG06, regv);
    }

    ret |= es8311_write_reg(ES8311_SYSTEM_REG13, 0x10);
    ret |= es8311_write_reg(ES8311_ADC_REG1B, 0x0A);
    ret |= es8311_write_reg(ES8311_ADC_REG1C, 0x6A);
    AUDIO_RET_ON_FALSE(TAG, ret, return ret, "es8311 initialize failed");

    /* pa power gpio init */
    if (get_pa_enable_gpio() != -1) {
        gpio_config_t io_conf;
        memset(&io_conf, 0, sizeof(io_conf));
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = BIT64(get_pa_enable_gpio());
        io_conf.pull_down_en = 0;
        io_conf.pull_up_en = 0;
        gpio_config(&io_conf);
        /* enable pa power */
        es8311_pa_power(true);
    }

    codec_dac_volume_config_t vol_cfg = ES8311_DAC_VOL_CFG_DEFAULT();
    dac_vol_handle = audio_codec_volume_init(&vol_cfg);
    return ESP_OK;
}

esp_err_t es8311_codec_deinit()
{
    i2c_bus_delete(i2c_handle);
    audio_codec_volume_deinit(dac_vol_handle);
    return ESP_OK;
}

esp_err_t es8311_config_fmt(es_i2s_fmt_t fmt)
{
    esp_err_t ret = ESP_OK;
    uint8_t adc_iface = 0, dac_iface = 0;
    dac_iface = es8311_read_reg(ES8311_SDPIN_REG09);
    adc_iface = es8311_read_reg(ES8311_SDPOUT_REG0A);
    switch (fmt) {
        case AUDIO_HAL_I2S_NORMAL:
            ESP_LOGD(TAG, "ES8311 in I2S Format");
            dac_iface &= 0xFC;
            adc_iface &= 0xFC;
            break;
        case AUDIO_HAL_I2S_LEFT:
        case AUDIO_HAL_I2S_RIGHT:
            ESP_LOGD(TAG, "ES8311 in LJ Format");
            adc_iface &= 0xFC;
            dac_iface &= 0xFC;
            adc_iface |= 0x01;
            dac_iface |= 0x01;
            break;
        case AUDIO_HAL_I2S_DSP:
            ESP_LOGD(TAG, "ES8311 in DSP-A Format");
            adc_iface &= 0xDC;
            dac_iface &= 0xDC;
            adc_iface |= 0x03;
            dac_iface |= 0x03;
            break;
        default:
            dac_iface &= 0xFC;
            adc_iface &= 0xFC;
            break;
    }
    ret |= es8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    return ret;
}

esp_err_t es8311_set_bits_per_sample(audio_hal_iface_bits_t bits)
{
    esp_err_t ret = ESP_OK;
    uint8_t adc_iface = 0, dac_iface = 0;
    dac_iface = es8311_read_reg(ES8311_SDPIN_REG09);
    adc_iface = es8311_read_reg(ES8311_SDPOUT_REG0A);
    switch (bits) {
        case AUDIO_HAL_BIT_LENGTH_16BITS:
            dac_iface |= 0x0c;
            adc_iface |= 0x0c;
            break;
        case AUDIO_HAL_BIT_LENGTH_24BITS:
            break;
        case AUDIO_HAL_BIT_LENGTH_32BITS:
            dac_iface |= 0x10;
            adc_iface |= 0x10;
            break;
        default:
            dac_iface |= 0x0c;
            adc_iface |= 0x0c;
            break;

    }
    ret |= es8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    return ret;
}

esp_err_t es8311_codec_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    int ret = ESP_OK;
    ESP_LOGI(TAG, "CFG I2S: mode = %d, bits = %d, fmt = %d, samples = %d", mode, iface->bits, iface->fmt, iface->samples);
    ret |= es8311_set_bits_per_sample(iface->bits);
    ret |= es8311_config_sample(iface->samples);
    ret |= es8311_config_fmt(iface->fmt);
    return ret;
}

esp_err_t es8311_codec_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    esp_err_t ret = ESP_OK;
    es_module_t es_mode = ES_MODULE_MIN;

    switch (mode) {
        case AUDIO_HAL_CODEC_MODE_ENCODE:
            es_mode  = ES_MODULE_ADC;
            break;
        case AUDIO_HAL_CODEC_MODE_LINE_IN:
            es_mode  = ES_MODULE_LINE;
            break;
        case AUDIO_HAL_CODEC_MODE_DECODE:
            es_mode  = ES_MODULE_DAC;
            break;
        case AUDIO_HAL_CODEC_MODE_BOTH:
            es_mode  = ES_MODULE_ADC_DAC;
            break;
        default:
            es_mode = ES_MODULE_DAC;
            ESP_LOGW(TAG, "Codec mode not support, default is decode mode");
            break;
    }

    if (ctrl_state == AUDIO_HAL_CTRL_START) {
        ret |= es8311_start(es_mode);
    } else {
        ESP_LOGW(TAG, "The codec is about to stop");
        ret |= es8311_stop(es_mode);
    }

    return ret;
}

esp_err_t es8311_start(es_module_t mode)
{
    esp_err_t ret = ESP_OK;
    uint8_t adc_iface = 0, dac_iface = 0;

    dac_iface = es8311_read_reg(ES8311_SDPIN_REG09) & 0xBF;
    adc_iface = es8311_read_reg(ES8311_SDPOUT_REG0A) & 0xBF;
    adc_iface |= BIT(6);
    dac_iface |= BIT(6);

    if (mode == ES_MODULE_LINE) {
        ESP_LOGE(TAG, "The codec es8311 doesn't support ES_MODULE_LINE mode");
        return ESP_FAIL;
    }
    if (mode == ES_MODULE_ADC || mode == ES_MODULE_ADC_DAC) {
        adc_iface &= ~(BIT(6));
    }
    if (mode == ES_MODULE_DAC || mode == ES_MODULE_ADC_DAC) {
        dac_iface &= ~(BIT(6));
    }

    ret |= es8311_write_reg(ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(ES8311_SDPOUT_REG0A, adc_iface);

    ret |= es8311_write_reg(ES8311_ADC_REG17, 0xBF);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG0E, 0x02);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG12, 0x00);
    ret |= es8311_write_reg(ES8311_SYSTEM_REG14, 0x1A);

    /*
     * pdm dmic enable or disable
     */
    int regv = 0;
    if (IS_DMIC) {
        regv = es8311_read_reg(ES8311_SYSTEM_REG14);
        regv |= 0x40;
        ret |= es8311_write_reg(ES8311_SYSTEM_REG14, regv);
    } else {
        regv = es8311_read_reg(ES8311_SYSTEM_REG14);
        regv &= ~(0x40);
        ret |= es8311_write_reg(ES8311_SYSTEM_REG14, regv);
    }

    ret |= es8311_write_reg(ES8311_SYSTEM_REG0D, 0x01);
    ret |= es8311_write_reg(ES8311_ADC_REG15, 0x40);
    ret |= es8311_write_reg(ES8311_DAC_REG37, 0x08);
    ret |= es8311_write_reg(ES8311_GP_REG45, 0x00);

    /* set internal reference signal (ADCL + DACR) */
    ret |= es8311_write_reg(ES8311_GPIO_REG44, 0x58);

    return ret;
}

esp_err_t es8311_stop(es_module_t mode)
{
    esp_err_t ret = ESP_OK;
    es8311_suspend();
    return ret;
}

/**
 * @brief Set voice volume
 *
 * @note Register values. 0x00: -95.5 dB, 0x5B: -50 dB, 0xBF: 0 dB, 0xFF: 32 dB
 * @note Accuracy of gain is 0.5 dB
 *
 * @param volume: voice volume (0~100)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t es8311_codec_set_voice_volume(int volume)
{
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    reg = audio_codec_get_dac_reg_value(dac_vol_handle, volume);
    res = es8311_write_reg(ES8311_DAC_REG32, reg);
    ESP_LOGD(TAG, "Set volume:%.2d reg_value:0x%.2x dB:%.1f", dac_vol_handle->user_volume, reg,
            audio_codec_cal_dac_volume(dac_vol_handle));
    return res;
}

esp_err_t es8311_codec_get_voice_volume(int *volume)
{
    esp_err_t res = ESP_OK;
    int regv = 0;
    regv = es8311_read_reg(ES8311_DAC_REG32);
    if (regv == ESP_FAIL) {
        *volume = 0;
        res = ESP_FAIL;
    } else {
        if (regv == dac_vol_handle->reg_value) {
            *volume = dac_vol_handle->user_volume;
        } else {
            *volume = 0;
            res = ESP_FAIL;
        }
    }
    ESP_LOGD(TAG, "Get volume:%.2d reg_value:0x%.2x", *volume, regv);
    return res;
}

esp_err_t es8311_set_voice_mute(bool enable)
{
    ESP_LOGD(TAG, "Es8311SetVoiceMute volume:%d", enable);
    es8311_mute(enable);
    return ESP_OK;
}

esp_err_t es8311_get_voice_mute(int *mute)
{
    esp_err_t res = ESP_OK;
    uint8_t reg = 0;
    res = es8311_read_reg(ES8311_DAC_REG31);
    if (res != ESP_FAIL) {
        reg = (res & 0x20) >> 5;
    }
    *mute = reg;
    return res;
}

esp_err_t es8311_set_mic_gain(es8311_mic_gain_t gain_db)
{
    esp_err_t res = ESP_OK;
    res = es8311_write_reg(ES8311_ADC_REG16, gain_db); // MIC gain scale
    return res;
}

void es8311_read_all()
{
    for (int i = 0; i < 0x4A; i++) {
        uint8_t reg = es8311_read_reg(i);
        ESP_LOGI(TAG, "REG:%02x, %02x", i, reg);
    }
}
