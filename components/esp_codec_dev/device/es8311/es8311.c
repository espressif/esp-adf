/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "es8311_codec.h"
#include "es8311_reg.h"
#include "esp_log.h"
#include "es_common.h"

#define TAG          "ES8311"

typedef struct {
    audio_codec_if_t   base;
    es8311_codec_cfg_t cfg;
    bool               is_open;
    bool               enabled;
    float              hw_gain;
} audio_codec_es8311_t;

/*
 * Clock coefficient structure
 */
struct _coeff_div {
    uint32_t mclk;      /* mclk frequency */
    uint32_t rate;      /* sample rate */
    uint8_t  pre_div;   /* the pre divider with range from 1 to 8 */
    uint8_t  pre_multi; /* the pre multiplier with x1, x2, x4 and x8 selection */
    uint8_t  adc_div;   /* adcclk divider */
    uint8_t  dac_div;   /* dacclk divider */
    uint8_t  fs_mode;   /* double speed or single speed, =0, ss, =1, ds */
    uint8_t  lrck_h;    /* adclrck divider and daclrck divider */
    uint8_t  lrck_l;
    uint8_t  bclk_div; /* sclk divider */
    uint8_t  adc_osr;  /* adc osr */
    uint8_t  dac_osr;  /* dac osr */
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
  //  mclk     rate   pre_div  mult  adc_div dac_div fs_mode lrch  lrcl  bckdiv osr
  /* 8k */
    {12288000, 8000,  0x06, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 8000,  0x03, 0x02, 0x03, 0x03, 0x00, 0x05, 0xff, 0x18, 0x10, 0x20},
    {16384000, 8000,  0x08, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000,  8000,  0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000,  8000,  0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000,  8000,  0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000,  8000,  0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000,  8000,  0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000,  8000,  0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000,  8000,  0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

 /* 11.025k */
    {11289600, 11025, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {5644800,  11025, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2822400,  11025, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1411200,  11025, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

 /* 12k */
    {12288000, 12000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000,  12000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000,  12000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000,  12000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

 /* 16k */
    {12288000, 16000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {18432000, 16000, 0x03, 0x02, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x20},
    {16384000, 16000, 0x04, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {8192000,  16000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {6144000,  16000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {4096000,  16000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {3072000,  16000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {2048000,  16000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1536000,  16000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},
    {1024000,  16000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x20},

 /* 22.05k */
    {11289600, 22050, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800,  22050, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400,  22050, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200,  22050, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

 /* 24k */
    {12288000, 24000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 24000, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000,  24000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000,  24000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000,  24000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

 /* 32k */
    {12288000, 32000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 32000, 0x03, 0x04, 0x03, 0x03, 0x00, 0x02, 0xff, 0x0c, 0x10, 0x10},
    {16384000, 32000, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000,  32000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000,  32000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {4096000,  32000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000,  32000, 0x03, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2048000,  32000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000,  32000, 0x03, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
    {1024000,  32000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

 /* 44.1k */
    {11289600, 44100, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800,  44100, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400,  44100, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200,  44100, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

 /* 48k */
    {12288000, 48000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 48000, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000,  48000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000,  48000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000,  48000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},

 /* 64k */
    {12288000, 64000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 64000, 0x03, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {16384000, 64000, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {8192000,  64000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000,  64000, 0x01, 0x04, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {4096000,  64000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000,  64000, 0x01, 0x08, 0x03, 0x03, 0x01, 0x01, 0x7f, 0x06, 0x10, 0x10},
    {2048000,  64000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000,  64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0xbf, 0x03, 0x18, 0x18},
    {1024000,  64000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

 /* 88.2k */
    {11289600, 88200, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {5644800,  88200, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {2822400,  88200, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1411200,  88200, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},

 /* 96k */
    {24576000, 96000, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {12288000, 96000, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {18432000, 96000, 0x03, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {6144000,  96000, 0x01, 0x04, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {3072000,  96000, 0x01, 0x08, 0x01, 0x01, 0x00, 0x00, 0xff, 0x04, 0x10, 0x10},
    {1536000,  96000, 0x01, 0x08, 0x01, 0x01, 0x01, 0x00, 0x7f, 0x02, 0x10, 0x10},
};

static const esp_codec_dev_vol_range_t vol_range = {
    .min_vol =
    {
        .vol = 0x0,
        .db_value = -95.5,
    },
    .max_vol =
    {
        .vol = 0xFF,
        .db_value = 32.0,
    },
};

static int es8311_write_reg(audio_codec_es8311_t *codec, int reg, int value)
{
    return codec->cfg.ctrl_if->write_reg(codec->cfg.ctrl_if, reg, 1, &value, 1);
}

static int es8311_read_reg(audio_codec_es8311_t *codec, int reg, int *value)
{
    *value = 0;
    return codec->cfg.ctrl_if->read_reg(codec->cfg.ctrl_if, reg, 1, value, 1);
}

static int es8311_config_fmt(audio_codec_es8311_t *codec, es_i2s_fmt_t fmt)
{
    int ret = ESP_CODEC_DEV_OK;
    int adc_iface = 0, dac_iface = 0;
    ret = es8311_read_reg(codec, ES8311_SDPIN_REG09, &dac_iface);
    ret |= es8311_read_reg(codec, ES8311_SDPOUT_REG0A, &adc_iface);
    switch (fmt) {
        case ES_I2S_NORMAL:
            ESP_LOGD(TAG, "ES8311 in I2S Format");
            dac_iface &= 0xFC;
            adc_iface &= 0xFC;
            break;
        case ES_I2S_LEFT:
        case ES_I2S_RIGHT:
            ESP_LOGD(TAG, "ES8311 in LJ Format");
            adc_iface &= 0xFC;
            dac_iface &= 0xFC;
            adc_iface |= 0x01;
            dac_iface |= 0x01;
            break;
        case ES_I2S_DSP:
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
    ret |= es8311_write_reg(codec, ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(codec, ES8311_SDPOUT_REG0A, adc_iface);
    return ret;
}

static int es8311_set_bits_per_sample(audio_codec_es8311_t *codec, int bits)
{
    int ret = ESP_CODEC_DEV_OK;
    int adc_iface = 0, dac_iface = 0;
    ret |= es8311_read_reg(codec, ES8311_SDPIN_REG09, &dac_iface);
    ret |= es8311_read_reg(codec, ES8311_SDPOUT_REG0A, &adc_iface);
    switch (bits) {
        case 16:
        default:
            dac_iface |= 0x0c;
            adc_iface |= 0x0c;
            break;
        case 24:
            dac_iface &= ~0x1c;
            adc_iface &= ~0x1c;
            break;
        case 32:
            dac_iface |= 0x10;
            adc_iface |= 0x10;
            break;
    }
    ret |= es8311_write_reg(codec, ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(codec, ES8311_SDPOUT_REG0A, adc_iface);

    return ret;
}

static int get_coeff(uint32_t mclk, uint32_t rate)
{
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++) {
        if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
            return i;
    }
    return ESP_CODEC_DEV_NOT_FOUND;
}

static int es8311_suspend(audio_codec_es8311_t *codec)
{
    int ret = es8311_write_reg(codec, ES8311_DAC_REG32, 0x00);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG17, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0E, 0xFF);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG12, 0x02);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG14, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0D, 0xFA);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG15, 0x00);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG02, 0x10);
    ret |= es8311_write_reg(codec, ES8311_RESET_REG00, 0x00);
    ret |= es8311_write_reg(codec, ES8311_RESET_REG00, 0x1F);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG01, 0x30);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG01, 0x00);
    ret |= es8311_write_reg(codec, ES8311_GP_REG45, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0D, 0xFC);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG02, 0x00);
    return ret;
}

static int es8311_start(audio_codec_es8311_t *codec)
{
    int ret = ESP_CODEC_DEV_OK;
    int adc_iface = 0, dac_iface = 0;
    int regv = 0x80;
    if (codec->cfg.master_mode) {
        regv |= 0x40;
    } else {
        regv &= 0xBF;
    }
    ret |= es8311_write_reg(codec, ES8311_RESET_REG00, regv);
    regv = 0x3F;
    if (codec->cfg.use_mclk) {
        regv &= 0x7F;
    } else {
        regv |= 0x80;
    }
    if (codec->cfg.invert_mclk) {
        regv |= 0x40;
    } else {
        regv &= ~(0x40);
    }
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG01, regv);

    ret = es8311_read_reg(codec, ES8311_SDPIN_REG09, &dac_iface);
    ret |= es8311_read_reg(codec, ES8311_SDPOUT_REG0A, &adc_iface);
    if (ret != ESP_CODEC_DEV_OK) {
        return ret;
    }
    dac_iface &= 0xBF;
    adc_iface &= 0xBF;
    adc_iface |= BITS(6);
    dac_iface |= BITS(6);
    int codec_mode = codec->cfg.codec_mode;
    if (codec_mode == ESP_CODEC_DEV_WORK_MODE_LINE) {
        ESP_LOGE(TAG, "Codec not support LINE mode");
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    if (codec_mode == ESP_CODEC_DEV_WORK_MODE_ADC || codec_mode == ESP_CODEC_DEV_WORK_MODE_BOTH) {
        adc_iface &= ~(BITS(6));
    }
    if (codec_mode == ESP_CODEC_DEV_WORK_MODE_DAC || codec_mode == ESP_CODEC_DEV_WORK_MODE_BOTH) {
        dac_iface &= ~(BITS(6));
    }

    ret |= es8311_write_reg(codec, ES8311_SDPIN_REG09, dac_iface);
    ret |= es8311_write_reg(codec, ES8311_SDPOUT_REG0A, adc_iface);

    ret |= es8311_write_reg(codec, ES8311_ADC_REG17, 0xBF);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0E, 0x02);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG12, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG14, 0x1A);

    // pdm dmic enable or disable
    regv = 0;
    ret |= es8311_read_reg(codec, ES8311_SYSTEM_REG14, &regv);
    if (codec->cfg.digital_mic) {
        regv |= 0x40;
    } else {
        regv &= ~(0x40);
    }
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG14, regv);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0D, 0x01);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG15, 0x40);
    ret |= es8311_write_reg(codec, ES8311_DAC_REG37, 0x08);
    ret |= es8311_write_reg(codec, ES8311_GP_REG45, 0x00);
    return ret;
}

static int es8311_set_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int regv;
    int ret = es8311_read_reg(codec, ES8311_DAC_REG31, &regv);
    regv &= 0x9f;
    if (mute) {
        es8311_write_reg(codec, ES8311_DAC_REG31, regv | 0x60);
    } else {
        es8311_write_reg(codec, ES8311_DAC_REG31, regv);
    }
    return ret;
}

static int es8311_set_vol(const audio_codec_if_t *h, float db_value)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    db_value -= codec->hw_gain;
    int reg = esp_codec_dev_vol_calc_reg(&vol_range, db_value);
    ESP_LOGD(TAG, "Set volume reg:%x db:%d", reg, (int) db_value);
    return es8311_write_reg(codec, ES8311_DAC_REG32, (uint8_t) reg);
}

static int es8311_set_mic_gain(const audio_codec_if_t *h, float db)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    es8311_mic_gain_t gain_db = ES8311_MIC_GAIN_0DB;
    if (db < 6) {
    } else if (db < 12) {
        gain_db = ES8311_MIC_GAIN_6DB;
    } else if (db < 18) {
        gain_db = ES8311_MIC_GAIN_12DB;
    } else if (db < 24) {
        gain_db = ES8311_MIC_GAIN_18DB;
    } else if (db < 30) {
        gain_db = ES8311_MIC_GAIN_24DB;
    } else if (db < 36) {
        gain_db = ES8311_MIC_GAIN_30DB;
    } else if (db < 42) {
        gain_db = ES8311_MIC_GAIN_36DB;
    } else {
        gain_db = ES8311_MIC_GAIN_42DB;
    }
    int ret = es8311_write_reg(codec, ES8311_ADC_REG16, gain_db); // MIC gain scale
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static void es8311_pa_power(audio_codec_es8311_t *codec, es_pa_setting_t pa_setting)
{
    int16_t pa_pin = codec->cfg.pa_pin;
    if (pa_pin == -1 || codec->cfg.gpio_if == NULL) {
        return;
    }
    if (pa_setting & ES_PA_SETUP) {
        codec->cfg.gpio_if->setup(pa_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
    } 
    if (pa_setting & ES_PA_ENABLE) {
        codec->cfg.gpio_if->set(pa_pin, codec->cfg.pa_reverted ? false : true);
    }
    if (pa_setting & ES_PA_DISABLE) {
        codec->cfg.gpio_if->set(pa_pin, codec->cfg.pa_reverted ? true : false);
    }
}

static int es8311_config_sample(audio_codec_es8311_t *codec, int sample_rate)
{
    int datmp, regv;
    int mclk_fre = sample_rate * codec->cfg.mclk_div;
    int coeff = get_coeff(mclk_fre, sample_rate);
    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_rate, mclk_fre);
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    int ret = 0;
    ret = es8311_read_reg(codec, ES8311_CLK_MANAGER_REG02, &regv);
    regv &= 0x7;
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
    if (codec->cfg.use_mclk == false) {
        datmp = 3;
    }
    regv |= (datmp) << 3;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG02, regv);

    regv = 0x00;
    regv |= (coeff_div[coeff].adc_div - 1) << 4;
    regv |= (coeff_div[coeff].dac_div - 1) << 0;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG05, regv);

    ret |= es8311_read_reg(codec, ES8311_CLK_MANAGER_REG03, &regv);
    regv &= 0x80;
    regv |= coeff_div[coeff].fs_mode << 6;
    regv |= coeff_div[coeff].adc_osr << 0;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG03, regv);

    ret |= es8311_read_reg(codec, ES8311_CLK_MANAGER_REG04, &regv);
    regv &= 0x80;
    regv |= coeff_div[coeff].dac_osr << 0;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG04, regv);

    ret |= es8311_read_reg(codec, ES8311_CLK_MANAGER_REG07, &regv);
    regv &= 0xC0;
    regv |= coeff_div[coeff].lrck_h << 0;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG07, regv);

    regv = 0x00;
    regv |= coeff_div[coeff].lrck_l << 0;
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG08, regv);

    ret = es8311_read_reg(codec, ES8311_CLK_MANAGER_REG06, &regv);
    regv &= 0xE0;
    if (coeff_div[coeff].bclk_div < 19) {
        regv |= (coeff_div[coeff].bclk_div - 1) << 0;
    } else {
        regv |= (coeff_div[coeff].bclk_div) << 0;
    }
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG06, regv);
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int es8311_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    es8311_codec_cfg_t *codec_cfg = (es8311_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(es8311_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    memcpy(&codec->cfg, cfg, sizeof(es8311_codec_cfg_t));
    if (codec->cfg.mclk_div == 0) {
        codec->cfg.mclk_div = MCLK_DEFAULT_DIV;
    }
    int regv;
    int ret = ESP_CODEC_DEV_OK;

    /* Enhance ES8311 I2C noise immunity */
    ret |= es8311_write_reg(codec, ES8311_GPIO_REG44, 0x08);
    /* Due to occasional failures during the first I2C write with the ES8311 chip, a second write is performed to ensure reliability */
    ret |= es8311_write_reg(codec, ES8311_GPIO_REG44, 0x08);

    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG01, 0x30);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG02, 0x00);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG03, 0x10);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG16, 0x24);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG04, 0x10);
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG05, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0B, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG0C, 0x00);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG10, 0x1F);
    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG11, 0x7F);
    ret |= es8311_write_reg(codec, ES8311_RESET_REG00, 0x80);

    ret = es8311_read_reg(codec, ES8311_RESET_REG00, &regv);
    if (codec_cfg->master_mode) {
        ESP_LOGI(TAG, "Work in Master mode");
        regv |= 0x40;
    } else {
        ESP_LOGI(TAG, "Work in Slave mode");
        regv &= 0xBF;
    }
    ret |= es8311_write_reg(codec, ES8311_RESET_REG00, regv);

    // Select clock source for internal mclk
    regv = 0x3F;
    if (codec_cfg->use_mclk) {
        regv &= 0x7F;
    } else {
        regv |= 0x80;
    }
    // MCLK inverted or not
    if (codec_cfg->invert_mclk) {
        regv |= 0x40;
    } else {
        regv &= ~(0x40);
    }
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG01, regv);
    // SCLK inverted or not
    ret |= es8311_read_reg(codec, ES8311_CLK_MANAGER_REG06, &regv);
    if (codec_cfg->invert_sclk) {
        regv |= 0x20;
    } else {
        regv &= ~(0x20);
    }
    ret |= es8311_write_reg(codec, ES8311_CLK_MANAGER_REG06, regv);

    ret |= es8311_write_reg(codec, ES8311_SYSTEM_REG13, 0x10);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG1B, 0x0A);
    ret |= es8311_write_reg(codec, ES8311_ADC_REG1C, 0x6A);
    if (codec_cfg->no_dac_ref == false) {
        /* set internal reference signal (ADCL + DACR) */
        ret |= es8311_write_reg(codec, ES8311_GPIO_REG44, 0x58);
    } else {
        ret |= es8311_write_reg(codec, ES8311_GPIO_REG44, 0x08);
    }
    if (ret != 0) {
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    es8311_pa_power(codec, ES_PA_SETUP | ES_PA_ENABLE);
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es8311_close(const audio_codec_if_t *h)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        es8311_suspend(codec);
        es8311_pa_power(codec, ES_PA_DISABLE);
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

static int es8311_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    es8311_set_bits_per_sample(codec, fs->bits_per_sample);
    es8311_config_fmt(codec, ES_I2S_NORMAL);
    es8311_config_sample(codec, fs->sample_rate);
    return ESP_CODEC_DEV_OK;
}

static int es8311_enable(const audio_codec_if_t *h, bool enable)
{
    int ret = ESP_CODEC_DEV_OK;
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (enable == codec->enabled) {
        return ESP_CODEC_DEV_OK;
    }
    if (enable) {
        ret = es8311_start(codec);
        es8311_pa_power(codec, ES_PA_ENABLE);
    } else {
        es8311_pa_power(codec, ES_PA_DISABLE);
        ret = es8311_suspend(codec);
    }
    if (ret == ESP_CODEC_DEV_OK) {
        codec->enabled = enable;
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

static int es8311_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8311_write_reg(codec, reg, value);
}

static int es8311_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8311_read_reg(codec, reg, value);
}

static void es8311_dump(const audio_codec_if_t *h)
{
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return;
    }
    for (int i = 0; i < ES8311_MAX_REGISTER; i++) {
        int value = 0;
        int ret = es8311_read_reg(codec, i, &value);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, value);
    }
}

const audio_codec_if_t *es8311_codec_new(es8311_codec_cfg_t *codec_cfg)
{
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_es8311_t *codec = (audio_codec_es8311_t *) calloc(1, sizeof(audio_codec_es8311_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }
    codec->base.open = es8311_open;
    codec->base.enable = es8311_enable;
    codec->base.set_fs = es8311_set_fs;
    codec->base.set_vol = es8311_set_vol;
    codec->base.set_mic_gain = es8311_set_mic_gain;
    codec->base.mute = es8311_set_mute;
    codec->base.set_reg = es8311_set_reg;
    codec->base.get_reg = es8311_get_reg;
    codec->base.dump_reg = es8311_dump;
    codec->base.close = es8311_close;
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es8311_codec_cfg_t));
        if (ret != 0) {
            ESP_LOGE(TAG, "Open fail");
            break;
        }
        return &codec->base;
    } while (0);
    if (codec) {
        free(codec);
    }
    return NULL;
}
