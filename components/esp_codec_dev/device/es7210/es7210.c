/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "es7210_adc.h"
#include "es7210_reg.h"
#include "es_common.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev_vol.h"

#define I2S_DSP_MODE          0
#define ENABLE_TDM_MAX_NUM    3
#define TAG                   "ES7210"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    bool                         is_open;
    bool                         enabled;
    es7210_input_mics_t          mic_select;
    es7210_gain_value_t          gain;
    bool                         master_mode;
    uint8_t                      off_reg;
    uint16_t                     mclk_div;
} audio_codec_es7210_t;

/*
 * Clock coefficient structure
 */
struct _coeff_div {
    uint32_t mclk;     /* mclk frequency */
    uint32_t lrck;     /* lrck */
    uint8_t  ss_ds;
    uint8_t  adc_div;  /* adcclk divider */
    uint8_t  dll;      /* dll_bypass */
    uint8_t  doubler;  /* doubler enable */
    uint8_t  osr;      /* adc osr */
    uint8_t  mclk_src; /* select mclk  source */
    uint32_t lrck_h;   /* The high 4 bits of lrck */
    uint32_t lrck_l;   /* The low 8 bits of lrck */
};

/* Codec hifi mclk clock divider coefficients
 *           MEMBER      REG
 *           mclk:       0x03
 *           lrck:       standard
 *           ss_ds:      --
 *           adc_div:    0x02
 *           dll:        0x06
 *           doubler:    0x02
 *           osr:        0x07
 *           mclk_src:   0x03
 *           lrckh:      0x04
 *           lrckl:      0x05
 */
static const struct _coeff_div coeff_div[] = {
  //  mclk      lrck    ss_ds adc_div  dll  doubler osr  mclk_src  lrckh   lrckl
  /* 8k */
    {12288000, 8000,  0x00, 0x03, 0x01, 0x00, 0x20, 0x00, 0x06, 0x00},
    {16384000, 8000,  0x00, 0x04, 0x01, 0x00, 0x20, 0x00, 0x08, 0x00},
    {19200000, 8000,  0x00, 0x1e, 0x00, 0x01, 0x28, 0x00, 0x09, 0x60},
    {4096000,  8000,  0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

 /* 11.025k */
    {11289600, 11025, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},

 /* 12k */
    {12288000, 12000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {19200000, 12000, 0x00, 0x14, 0x00, 0x01, 0x28, 0x00, 0x06, 0x40},

 /* 16k */
    {4096000,  16000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 16000, 0x00, 0x0a, 0x00, 0x00, 0x1e, 0x00, 0x04, 0x80},
    {16384000, 16000, 0x00, 0x02, 0x01, 0x00, 0x20, 0x00, 0x04, 0x00},
    {12288000, 16000, 0x00, 0x03, 0x01, 0x01, 0x20, 0x00, 0x03, 0x00},

 /* 22.05k */
    {11289600, 22050, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},

 /* 24k */
    {12288000, 24000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 24000, 0x00, 0x0a, 0x00, 0x01, 0x28, 0x00, 0x03, 0x20},

 /* 32k */
    {8192000,  32000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {12288000, 32000, 0x00, 0x03, 0x00, 0x00, 0x20, 0x00, 0x01, 0x80},
    {16384000, 32000, 0x00, 0x01, 0x01, 0x00, 0x20, 0x00, 0x02, 0x00},
    {19200000, 32000, 0x00, 0x05, 0x00, 0x00, 0x1e, 0x00, 0x02, 0x58},

 /* 44.1k */
    {11289600, 44100, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},

 /* 48k */
    {12288000, 48000, 0x00, 0x01, 0x01, 0x01, 0x20, 0x00, 0x01, 0x00},
    {19200000, 48000, 0x00, 0x05, 0x00, 0x01, 0x28, 0x00, 0x01, 0x90},

 /* 64k */
    {16384000, 64000, 0x01, 0x01, 0x01, 0x00, 0x20, 0x00, 0x01, 0x00},
    {19200000, 64000, 0x00, 0x05, 0x00, 0x01, 0x1e, 0x00, 0x01, 0x2c},

 /* 88.2k */
    {11289600, 88200, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},

 /* 96k */
    {12288000, 96000, 0x01, 0x01, 0x01, 0x01, 0x20, 0x00, 0x00, 0x80},
    {19200000, 96000, 0x01, 0x05, 0x00, 0x01, 0x28, 0x00, 0x00, 0xc8},
};

static int es7210_write_reg(audio_codec_es7210_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static int es7210_read_reg(audio_codec_es7210_t *codec, int reg, int *value)
{
    *value = 0;
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, value, 1);
}

static int es7210_update_reg_bit(audio_codec_es7210_t *codec, uint8_t reg_addr, uint8_t update_bits, uint8_t data)
{
    int regv = 0;
    es7210_read_reg(codec, reg_addr, &regv);
    regv = (regv & (~update_bits)) | (update_bits & data);
    return es7210_write_reg(codec, reg_addr, regv);
}

static int get_coeff(uint32_t mclk, uint32_t lrck)
{
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++) {
        if (coeff_div[i].lrck == lrck && coeff_div[i].mclk == mclk)
            return i;
    }
    return -1;
}

static int es7210_config_sample(audio_codec_es7210_t *codec, int sample_fre)
{
    if (codec->master_mode == false) {
        return ESP_CODEC_DEV_OK;
    }
    int regv;
    int coeff;
    int mclk_fre = 0;
    int ret = 0;
    mclk_fre = sample_fre * codec->mclk_div;
    coeff = get_coeff(mclk_fre, sample_fre);
    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_fre, mclk_fre);
        return ESP_FAIL;
    }
    /* Set clock parameters */
    if (coeff >= 0) {
        /* Set adc_div & doubler & dll */
        ret |= es7210_read_reg(codec, ES7210_MAINCLK_REG02, &regv);
        regv &= 0x00;
        regv |= coeff_div[coeff].adc_div;
        regv |= coeff_div[coeff].doubler << 6;
        regv |= coeff_div[coeff].dll << 7;
        ret |= es7210_write_reg(codec, ES7210_MAINCLK_REG02, regv);
        /* Set osr */
        regv = coeff_div[coeff].osr;
        ret |= es7210_write_reg(codec, ES7210_OSR_REG07, regv);
        /* Set lrck */
        regv = coeff_div[coeff].lrck_h;
        ret |= es7210_write_reg(codec, ES7210_LRCK_DIVH_REG04, regv);
        regv = coeff_div[coeff].lrck_l;
        ret |= es7210_write_reg(codec, ES7210_LRCK_DIVL_REG05, regv);
    }
    return ret;
}

static bool es7210_is_tdm_mode(audio_codec_es7210_t *codec)
{
    uint16_t mic_num = 0;
    for (int i = ES7210_INPUT_MIC1; i <= ES7210_INPUT_MIC4; i = i << 1) {
        if (codec->mic_select & i) {
            mic_num++;
        }
    }
    return (mic_num >= ENABLE_TDM_MAX_NUM);
}

static int es7210_mic_select(audio_codec_es7210_t *codec, es7210_input_mics_t mic)
{
    int ret = 0;
    if (codec->mic_select & (ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2 | ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4)) {
        for (int i = 0; i < 4; i++) {
            ret |= es7210_update_reg_bit(codec, ES7210_MIC1_GAIN_REG43 + i, 0x10, 0x00);
        }
        ret |= es7210_write_reg(codec, ES7210_MIC12_POWER_REG4B, 0xff);
        ret |= es7210_write_reg(codec, ES7210_MIC34_POWER_REG4C, 0xff);
        if (codec->mic_select & ES7210_INPUT_MIC1) {
            ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC1");
            ret |= es7210_update_reg_bit(codec, ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
            ret |= es7210_write_reg(codec, ES7210_MIC12_POWER_REG4B, 0x00);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC1_GAIN_REG43, 0x10, 0x10);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC1_GAIN_REG43, 0x0f, codec->gain);
        }
        if (codec->mic_select & ES7210_INPUT_MIC2) {
            ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC2");
            ret |= es7210_update_reg_bit(codec, ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
            ret |= es7210_write_reg(codec, ES7210_MIC12_POWER_REG4B, 0x00);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC2_GAIN_REG44, 0x10, 0x10);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC2_GAIN_REG44, 0x0f, codec->gain);
        }
        if (codec->mic_select & ES7210_INPUT_MIC3) {
            ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC3");
            ret |= es7210_update_reg_bit(codec, ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
            ret |= es7210_write_reg(codec, ES7210_MIC34_POWER_REG4C, 0x00);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC3_GAIN_REG45, 0x10, 0x10);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC3_GAIN_REG45, 0x0f, codec->gain);
        }
        if (codec->mic_select & ES7210_INPUT_MIC4) {
            ESP_LOGI(TAG, "Enable ES7210_INPUT_MIC4");
            ret |= es7210_update_reg_bit(codec, ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
            ret |= es7210_write_reg(codec, ES7210_MIC34_POWER_REG4C, 0x00);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC4_GAIN_REG46, 0x10, 0x10);
            ret |= es7210_update_reg_bit(codec, ES7210_MIC4_GAIN_REG46, 0x0f, codec->gain);
        }
    } else {
        ESP_LOGE(TAG, "Microphone selection error");
        return ESP_FAIL;
    }
    if (es7210_is_tdm_mode(codec)) {
        ret |= es7210_write_reg(codec, ES7210_SDP_INTERFACE2_REG12, 0x02);
        ESP_LOGI(TAG, "Enable TDM mode");
    } else {
        ret |= es7210_write_reg(codec, ES7210_SDP_INTERFACE2_REG12, 0x00);
    }
    return ret;
}

static int es7210_config_fmt(audio_codec_es7210_t *codec, es_i2s_fmt_t fmt)
{
    int ret = 0;
    int adc_iface = 0;
    ret = es7210_read_reg(codec, ES7210_SDP_INTERFACE1_REG11, &adc_iface);
    adc_iface &= 0xfc;
    switch (fmt) {
        case ES_I2S_NORMAL:
            ESP_LOGD(TAG, "ES7210 in I2S Format");
            adc_iface |= 0x00;
            break;
        case ES_I2S_LEFT:
        case ES_I2S_RIGHT:
            ESP_LOGD(TAG, "ES7210 in LJ Format");
            adc_iface |= 0x01;
            break;
        case ES_I2S_DSP:
            if (I2S_DSP_MODE) {
                ESP_LOGD(TAG, "ES7210 in DSP-A Format");
                adc_iface |= 0x13;
            } else {
                ESP_LOGD(TAG, "ES7210 in DSP-B Format");
                adc_iface |= 0x03;
            }
            break;
        default:
            adc_iface &= 0xfc;
            break;
    }
    ret |= es7210_write_reg(codec, ES7210_SDP_INTERFACE1_REG11, adc_iface);
    return ret;
}

static int es7210_set_bits(audio_codec_es7210_t *codec, uint8_t bits)
{
    int ret = 0;
    int adc_iface = 0;
    ret = es7210_read_reg(codec, ES7210_SDP_INTERFACE1_REG11, &adc_iface);
    adc_iface &= 0x1f;
    switch (bits) {
        case 16:
            adc_iface |= 0x60;
            break;
        case 24:
            adc_iface |= 0x00;
            break;
        case 32:
            adc_iface |= 0x80;
            break;
        default:
            adc_iface |= 0x60;
            break;
    }
    ret |= es7210_write_reg(codec, ES7210_SDP_INTERFACE1_REG11, adc_iface);
    ESP_LOGI(TAG, "Bits %d", bits);
    return ret;
}

static int es7210_start(audio_codec_es7210_t *codec, uint8_t clock_reg_value)
{
    int ret = 0;
    ret |= es7210_write_reg(codec, ES7210_CLOCK_OFF_REG01, clock_reg_value);
    ret |= es7210_write_reg(codec, ES7210_POWER_DOWN_REG06, 0x00);
    ret |= es7210_write_reg(codec, ES7210_ANALOG_REG40, 0x43);
    ret |= es7210_write_reg(codec, ES7210_MIC1_POWER_REG47, 0x08);
    ret |= es7210_write_reg(codec, ES7210_MIC2_POWER_REG48, 0x08);
    ret |= es7210_write_reg(codec, ES7210_MIC3_POWER_REG49, 0x08);
    ret |= es7210_write_reg(codec, ES7210_MIC4_POWER_REG4A, 0x08);
    ret |= es7210_mic_select(codec, codec->mic_select);
    ret |= es7210_write_reg(codec, ES7210_ANALOG_REG40, 0x43);
    ret |= es7210_write_reg(codec, ES7210_RESET_REG00, 0x71);
    ret |= es7210_write_reg(codec, ES7210_RESET_REG00, 0x41);
    return ret;
}

static int es7210_stop(audio_codec_es7210_t *codec)
{
    int ret = 0;
    ret |= es7210_write_reg(codec, ES7210_MIC1_POWER_REG47, 0xff);
    ret |= es7210_write_reg(codec, ES7210_MIC2_POWER_REG48, 0xff);
    ret |= es7210_write_reg(codec, ES7210_MIC3_POWER_REG49, 0xff);
    ret |= es7210_write_reg(codec, ES7210_MIC4_POWER_REG4A, 0xff);
    ret |= es7210_write_reg(codec, ES7210_MIC12_POWER_REG4B, 0xff);
    ret |= es7210_write_reg(codec, ES7210_MIC34_POWER_REG4C, 0xff);
    ret |= es7210_write_reg(codec, ES7210_ANALOG_REG40, 0xc0);
    ret |= es7210_write_reg(codec, ES7210_CLOCK_OFF_REG01, 0x7f);
    ret |= es7210_write_reg(codec, ES7210_POWER_DOWN_REG06, 0x07);
    return ret;
}

static es7210_gain_value_t get_db(float db)
{
    db += 0.5;
    if (db < 33) {
        int idx = db < 3 ? 0 : db / 3;
        return GAIN_0DB + idx;
    }
    if (db < 34.5) {
        return GAIN_30DB;
    }
    if (db < 36) {
        return GAIN_34_5DB;
    }
    if (db < 37) {
        return GAIN_36DB;
    }
    return GAIN_37_5DB;
}

static int _es7210_set_channel_gain(audio_codec_es7210_t *codec, uint16_t channel_mask, float db)
{
    int ret = 0;
    es7210_gain_value_t gain = get_db(db);
    if ((codec->mic_select & ES7210_INPUT_MIC1) & (channel_mask & ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0))) {
        ret |= es7210_update_reg_bit(codec, ES7210_MIC1_GAIN_REG43, 0x0f, gain);
    }
    if ((codec->mic_select & ES7210_INPUT_MIC2) & (channel_mask & ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1))) {
        ret |= es7210_update_reg_bit(codec, ES7210_MIC2_GAIN_REG44, 0x0f, gain);
    }
    if ((codec->mic_select & ES7210_INPUT_MIC3) & (channel_mask & ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2))) {
        ret |= es7210_update_reg_bit(codec, ES7210_MIC3_GAIN_REG45, 0x0f, gain);
    }
    if ((codec->mic_select & ES7210_INPUT_MIC4) & (channel_mask & ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3))) {
        ret |= es7210_update_reg_bit(codec, ES7210_MIC4_GAIN_REG46, 0x0f, gain);
    }
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int _es7210_set_mute(audio_codec_es7210_t *codec, bool mute)
{
    int ret = 0;
    if (mute) {
        ret |= es7210_update_reg_bit(codec, 0x14, 0x03, 0x03);
        ret |= es7210_update_reg_bit(codec, 0x15, 0x03, 0x03);
    } else {
        ret |= es7210_update_reg_bit(codec, 0x14, 0x03, 0x00);
        ret |= es7210_update_reg_bit(codec, 0x15, 0x03, 0x00);
    }
    ESP_LOGI(TAG, "%s", mute ? "Muted" : "Unmuted");
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int es7210_enable(const audio_codec_if_t *h, bool enable)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (enable == codec->enabled) {
        return ESP_CODEC_DEV_OK;
    }
    int ret = 0;
    if (enable) {
        ret |= es7210_start(codec, codec->off_reg);
    } else {
        ret |= es7210_stop(codec);
    }
    if (ret == ESP_CODEC_DEV_OK) {
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
        codec->enabled = enable;
    }
    return ret;
}

static int es7210_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    es7210_codec_cfg_t *codec_cfg = (es7210_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(es7210_codec_cfg_t)) {
        ESP_LOGE(TAG, "Wrong codec config");
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = 0;
    codec->ctrl_if = codec_cfg->ctrl_if;
    ret |= es7210_write_reg(codec, ES7210_RESET_REG00, 0xff);
    ret |= es7210_write_reg(codec, ES7210_RESET_REG00, 0x41);
    ret |= es7210_write_reg(codec, ES7210_CLOCK_OFF_REG01, 0x3f);
    ret |= es7210_write_reg(codec, ES7210_TIME_CONTROL0_REG09, 0x30); /* Set chip state cycle */
    ret |= es7210_write_reg(codec, ES7210_TIME_CONTROL1_REG0A, 0x30); /* Set power on state cycle */
    ret |= es7210_write_reg(codec, ES7210_ADC12_HPF2_REG23, 0x2a);    /* Quick setup */
    ret |= es7210_write_reg(codec, ES7210_ADC12_HPF1_REG22, 0x0a);
    ret |= es7210_write_reg(codec, ES7210_ADC34_HPF2_REG20, 0x0a);
    ret |= es7210_write_reg(codec, ES7210_ADC34_HPF1_REG21, 0x2a);
    if (ret != 0) {
        ESP_LOGE(TAG, "Write register fail");
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    if (codec_cfg->master_mode) {
        ESP_LOGI(TAG, "Work in Master mode");
        ret |= es7210_update_reg_bit(codec, ES7210_MODE_CONFIG_REG08, 0x01, 0x01);
        /* Select clock source for internal mclk */
        switch (codec_cfg->mclk_src) {
            case ES7210_MCLK_FROM_PAD:
            default:
                ret |= es7210_update_reg_bit(codec, ES7210_MASTER_CLK_REG03, 0x80, 0x00);
                break;
            case ES7210_MCLK_FROM_CLOCK_DOUBLER:
                ret |= es7210_update_reg_bit(codec, ES7210_MASTER_CLK_REG03, 0x80, 0x80);
                break;
        }
    } else {
        ESP_LOGI(TAG, "Work in Slave mode");
        ret |= es7210_update_reg_bit(codec, ES7210_MODE_CONFIG_REG08, 0x01, 0x00);
    }
    /* Select power off analog, vdda = 3.3V, close vx20ff, VMID select 5KΩ start */
    ret |= es7210_write_reg(codec, ES7210_ANALOG_REG40, 0x43);
    ret |= es7210_write_reg(codec, ES7210_MIC12_BIAS_REG41, 0x70); /* Select 2.87v */
    ret |= es7210_write_reg(codec, ES7210_MIC34_BIAS_REG42, 0x70); /* Select 2.87v */
    ret |= es7210_write_reg(codec, ES7210_OSR_REG07, 0x20);
    /* Set the frequency division coefficient and use dll except clock doubler, and need to set 0xc1 to clear the state */
    ret |= es7210_write_reg(codec, ES7210_MAINCLK_REG02, 0xc1); 
    codec->mic_select = (es7210_input_mics_t) codec_cfg->mic_selected;
    if (codec->mic_select == 0) {
        codec->mic_select = ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2;
    }
    ret |= es7210_mic_select(codec, codec->mic_select);
    ret |= _es7210_set_channel_gain(codec, 0xF, 30.0);
    if (ret != 0) {
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    codec->master_mode = codec_cfg->master_mode;
    codec->mclk_div = codec_cfg->mclk_div;
    if (codec->mclk_div == 0) {
        codec->mclk_div = MCLK_DEFAULT_DIV;
    }
    es7210_read_reg(codec, ES7210_CLOCK_OFF_REG01, &ret);
    if (ret >= 0) {
        codec->off_reg = (uint8_t) ret;
    }
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es7210_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int ret = 0;
    uint8_t bits = fs->bits_per_sample;
    // Use 2 channel to fetch TDM data
    if (es7210_is_tdm_mode(codec) && fs->channel <= 2 && fs->channel_mask == 0) {
        bits >>= 1;
    }
    ret |= es7210_set_bits(codec, bits);
    ret |= es7210_config_sample(codec, fs->sample_rate);
    ret |= es7210_config_fmt(codec, ES_I2S_NORMAL);
    return ret == 0 ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int es7210_set_gain(const audio_codec_if_t *h, float db)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return _es7210_set_channel_gain(codec, 0xF, db);
}

static int es7210_set_channel_gain(const audio_codec_if_t *h, uint16_t channel_mask, float db)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return _es7210_set_channel_gain(codec, channel_mask, db);
}

static int es7210_set_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return _es7210_set_mute(codec, mute);
}

static int es7210_close(const audio_codec_if_t *h)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = 0;
    if (codec->is_open) {
        ret = es7210_enable(h, false);
        codec->is_open = false;
    }
    return ret;
}

static int es7210_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7210_write_reg(codec, reg, value);
}

static int es7210_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7210_read_reg(codec, reg, value);
}

static void es7210_dump(const audio_codec_if_t *h)
{
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) h;
    if (codec == NULL) {
        return;
    }
    for (int i = 0; i <= 0x4E; i++) {
        int reg = 0;
        if (es7210_read_reg(codec, i, &reg) != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, reg);
    }
}

const audio_codec_if_t *es7210_codec_new(es7210_codec_cfg_t *codec_cfg)
{
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_es7210_t *codec = (audio_codec_es7210_t *) calloc(1, sizeof(audio_codec_es7210_t));
    CODEC_MEM_CHECK(codec);
    if (codec == NULL) {
        return NULL;
    }
    codec->base.open = es7210_open;
    codec->base.enable = es7210_enable;
    codec->base.set_fs = es7210_set_fs;
    codec->base.set_mic_gain = es7210_set_gain;
    codec->base.set_mic_channel_gain = es7210_set_channel_gain;
    codec->base.mute_mic = es7210_set_mute;
    codec->base.set_reg = es7210_set_reg;
    codec->base.get_reg = es7210_get_reg;
    codec->base.dump_reg = es7210_dump;
    codec->base.close = es7210_close;
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es7210_codec_cfg_t));
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
