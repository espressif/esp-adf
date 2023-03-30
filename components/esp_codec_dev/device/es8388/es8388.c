/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "esp_log.h"
#include "es8388_reg.h"
#include "es8388_codec.h"
#include "es_common.h"
#include "esp_codec_dev_vol.h"

#define TAG "ES8388"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    bool                         is_open;
    bool                         enabled;
    esp_codec_dec_work_mode_t    codec_mode;
    int16_t                      pa_pin;
    bool                         pa_reverted;
    float                        hw_gain;
} audio_codec_es8388_t;

static const esp_codec_dev_vol_range_t vol_range = {
    .min_vol =
    {
        .vol = 0xC0,
        .db_value = -96,
    },
    .max_vol =
    {
        .vol = 0,
        .db_value = 0.0,
    },
};

static int es8388_write_reg(audio_codec_es8388_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static int es8388_read_reg(audio_codec_es8388_t *codec, int reg, int *value)
{
    *value = 0;
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, value, 1);
}

static int es8388_set_adc_dac_volume(audio_codec_es8388_t *codec, esp_codec_dec_work_mode_t mode, int volume, int dot)
{
    int res = 0;
    if (volume < -96 || volume > 0) {
        if (volume < -96)
            volume = -96;
        else
            volume = 0;
    }
    dot = (dot >= 5 ? 1 : 0);
    volume = (-volume << 1) + dot;
    if (mode & ESP_CODEC_DEV_WORK_MODE_ADC) {
        res |= es8388_write_reg(codec, ES8388_ADCCONTROL8, volume);
        res |= es8388_write_reg(codec, ES8388_ADCCONTROL9, volume); // ADC Right Volume=0db
    }
    if (mode & ESP_CODEC_DEV_WORK_MODE_DAC) {
        res |= es8388_write_reg(codec, ES8388_DACCONTROL5, volume);
        res |= es8388_write_reg(codec, ES8388_DACCONTROL4, volume);
    }
    return res;
}

static int es8388_set_voice_mute(audio_codec_es8388_t *codec, bool enable)
{
    int res = 0;
    int reg = 0;
    res = es8388_read_reg(codec, ES8388_DACCONTROL3, &reg);
    reg = reg & 0xFB;
    res |= es8388_write_reg(codec, ES8388_DACCONTROL3, reg | (((int) enable) << 2));
    return res;
}

static int es8388_start(audio_codec_es8388_t *codec, esp_codec_dec_work_mode_t mode)
{
    int res = 0;
    int prev_data = 0, data = 0;
    es8388_read_reg(codec, ES8388_DACCONTROL21, &prev_data);
    if (mode == ESP_CODEC_DEV_WORK_MODE_LINE) {
        res |= es8388_write_reg(codec, ES8388_DACCONTROL16,
                                0x09); // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2 by pass enable
        res |= es8388_write_reg(
            codec, ES8388_DACCONTROL17,
            0x50); // left DAC to left mixer enable  and  LIN signal to left mixer enable 0db  : bupass enable
        res |= es8388_write_reg(
            codec, ES8388_DACCONTROL20,
            0x50); // right DAC to right mixer enable  and  LIN signal to right mixer enable 0db : bupass enable
        res |= es8388_write_reg(codec, ES8388_DACCONTROL21, 0xC0); // enable adc
    } else {
        res |= es8388_write_reg(codec, ES8388_DACCONTROL21, 0x80); // enable dac
    }
    es8388_read_reg(codec, ES8388_DACCONTROL21, &data);
    if (prev_data != data) {
        res |= es8388_write_reg(codec, ES8388_CHIPPOWER, 0xF0); // start state machine
        res |= es8388_write_reg(codec, ES8388_CHIPPOWER, 0x00); // start state machine
    }
    if ((mode & ESP_CODEC_DEV_WORK_MODE_ADC) || mode == ESP_CODEC_DEV_WORK_MODE_LINE) {
        res |= es8388_write_reg(codec, ES8388_ADCPOWER, 0x00); // power up adc and line in
    }
    if ((mode & ESP_CODEC_DEV_WORK_MODE_DAC) || mode == ESP_CODEC_DEV_WORK_MODE_LINE) {
        res |= es8388_write_reg(codec, ES8388_DACPOWER, 0x3c); // power up dac and line out
        res |= es8388_set_voice_mute(codec, false);
        ESP_LOGI(TAG, "Start on mode:%d", mode);
    }
    return res;
}

static int es8388_stop(audio_codec_es8388_t *codec, esp_codec_dec_work_mode_t mode)
{
    int res = 0;
    if (mode == ESP_CODEC_DEV_WORK_MODE_LINE) {
        res |= es8388_write_reg(codec, ES8388_DACCONTROL21, 0x80); // enable dac
        res |= es8388_write_reg(codec, ES8388_DACCONTROL16, 0x00); // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
        res |= es8388_write_reg(codec, ES8388_DACCONTROL17, 0x90); // only left DAC to left mixer enable 0db
        res |= es8388_write_reg(codec, ES8388_DACCONTROL20, 0x90); // only right DAC to right mixer enable 0db
        return res;
    }
    if (mode & ESP_CODEC_DEV_WORK_MODE_DAC) {
        res |= es8388_write_reg(codec, ES8388_DACPOWER, 0x00);
        res |= es8388_set_voice_mute(codec, true);
    }
    if (mode & ESP_CODEC_DEV_WORK_MODE_ADC) {
        res |= es8388_write_reg(codec, ES8388_ADCPOWER, 0xFF); // power down adc and line in
    }
    if (mode == ESP_CODEC_DEV_WORK_MODE_BOTH) {
        res |= es8388_write_reg(codec, ES8388_DACCONTROL21, 0x9C); // disable mclk
    }
    return res;
}

static int es8388_config_fmt(audio_codec_es8388_t *codec, esp_codec_dec_work_mode_t mode, es_i2s_fmt_t fmt)
{
    int res = 0;
    int reg = 0;
    if (mode & ESP_CODEC_DEV_WORK_MODE_ADC) {
        res = es8388_read_reg(codec, ES8388_ADCCONTROL4, &reg);
        reg = reg & 0xfc;
        res |= es8388_write_reg(codec, ES8388_ADCCONTROL4, reg | fmt);
    }
    if (mode & ESP_CODEC_DEV_WORK_MODE_DAC) {
        res = es8388_read_reg(codec, ES8388_DACCONTROL1, &reg);
        reg = reg & 0xf9;
        res |= es8388_write_reg(codec, ES8388_DACCONTROL1, reg | (fmt << 1));
    }
    return res;
}

static int es8388_set_mic_gain(audio_codec_es8388_t *codec, float db)
{
    es_mic_gain_t gain = db > 0 ? (int) (db / 3) + MIC_GAIN_0DB : MIC_GAIN_0DB;
    int res, gain_n;
    gain_n = (int) gain / 3;
    gain_n = (gain_n << 4) + gain_n;
    res = es8388_write_reg(codec, ES8388_ADCCONTROL1, gain_n); // MIC PGA
    return res;
}

static es_bits_length_t get_bits_enum(uint8_t bits)
{
    switch (bits) {
        case 16:
        default:
            return BIT_LENGTH_16BITS;
        case 18:
            return BIT_LENGTH_18BITS;
        case 20:
            return BIT_LENGTH_20BITS;
        case 24:
            return BIT_LENGTH_24BITS;
        case 32:
            return BIT_LENGTH_32BITS;
    }
}

static int es8388_set_bits_per_sample(audio_codec_es8388_t *codec, esp_codec_dec_work_mode_t mode, uint8_t bits_length)
{
    int res = 0;
    int reg = 0;
    int bits = (int) get_bits_enum(bits_length);

    if (mode & ESP_CODEC_DEV_WORK_MODE_ADC) {
        res = es8388_read_reg(codec, ES8388_ADCCONTROL4, &reg);
        reg = reg & 0xe3;
        res |= es8388_write_reg(codec, ES8388_ADCCONTROL4, reg | (bits << 2));
    }
    if (mode & ESP_CODEC_DEV_WORK_MODE_DAC) {
        res = es8388_read_reg(codec, ES8388_DACCONTROL1, &reg);
        reg = reg & 0xc7;
        res |= es8388_write_reg(codec, ES8388_DACCONTROL1, reg | (bits << 3));
    }
    return res;
}

static void es8388_pa_power(audio_codec_es8388_t *codec, bool enable)
{
    int16_t pa_pin = codec->pa_pin;
    if (pa_pin == -1 || codec->gpio_if == NULL) {
        return;
    }
    codec->gpio_if->setup(pa_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
    codec->gpio_if->set(pa_pin, codec->pa_reverted ? !enable : enable);
}

static int es8388_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    es8388_codec_cfg_t *codec_cfg = (es8388_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(es8388_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int res = ESP_CODEC_DEV_OK;
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->gpio_if = codec_cfg->gpio_if;
    codec->pa_pin = codec_cfg->pa_pin;
    codec->pa_reverted = codec_cfg->pa_reverted;
    codec->codec_mode = codec_cfg->codec_mode;

    // 0x04 mute/0x00 unmute&ramp;
    res |= es8388_write_reg(codec, ES8388_DACCONTROL3, 0x04);
    /* Chip Control and Power Management */
    res |= es8388_write_reg(codec, ES8388_CONTROL2, 0x50);
    res |= es8388_write_reg(codec, ES8388_CHIPPOWER, 0x00); // normal all and power up all

    // Disable the internal DLL to improve 8K sample rate
    res |= es8388_write_reg(codec, 0x35, 0xA0);
    res |= es8388_write_reg(codec, 0x37, 0xD0);
    res |= es8388_write_reg(codec, 0x39, 0xD0);

    res |= es8388_write_reg(codec, ES8388_MASTERMODE, codec_cfg->master_mode); // CODEC IN I2S SLAVE MODE

    /* dac */
    res |= es8388_write_reg(codec, ES8388_DACPOWER, 0xC0); // disable DAC and disable Lout/Rout/1/2
    res |= es8388_write_reg(codec, ES8388_CONTROL1, 0x12); // Enfr=0,Play&Record Mode,(0x17-both of mic&paly)
    //    res |= es8388_write_reg(codec, ES8388_CONTROL2, 0);  //LPVrefBuf=0,Pdn_ana=0
    res |= es8388_write_reg(codec, ES8388_DACCONTROL1, 0x18);  // 1a 0x18:16bit iis , 0x00:24
    res |= es8388_write_reg(codec, ES8388_DACCONTROL2, 0x02);  // DACFsMode,SINGLE SPEED; DACFsRatio,256
    res |= es8388_write_reg(codec, ES8388_DACCONTROL16, 0x00); // 0x00 audio on LIN1&RIN1,  0x09 LIN2&RIN2
    res |= es8388_write_reg(codec, ES8388_DACCONTROL17, 0x90); // only left DAC to left mixer enable 0db
    res |= es8388_write_reg(codec, ES8388_DACCONTROL20, 0x90); // only right DAC to right mixer enable 0db
    // set internal ADC and DAC use the same LRCK clock, ADC LRCK as internal LRCK
    res |= es8388_write_reg(codec, ES8388_DACCONTROL21, 0x80);
    res |= es8388_write_reg(codec, ES8388_DACCONTROL23, 0x00);    // vroi=0
    res |= es8388_set_adc_dac_volume(codec, ES_MODULE_DAC, 0, 0); // 0db

    res |= es8388_write_reg(codec, ES8388_DACCONTROL24, 0x1E); // Set L1 R1 L2 R2 volume. 0x00: -30dB, 0x1E: 0dB, 0x21: 3dB
    res |= es8388_write_reg(codec, ES8388_DACCONTROL25, 0x1E);
    res |= es8388_write_reg(codec, ES8388_DACCONTROL26, 0);
    res |= es8388_write_reg(codec, ES8388_DACCONTROL27, 0);

    // TODO default use DAC_ALL
    int tmp = DAC_OUTPUT_LOUT1 | DAC_OUTPUT_LOUT2 | DAC_OUTPUT_ROUT1 | DAC_OUTPUT_ROUT2;
    res |= es8388_write_reg(codec, ES8388_DACPOWER, tmp); // 0x3c Enable DAC and Enable Lout/Rout/1/2
    /* adc */
    res |= es8388_write_reg(codec, ES8388_ADCPOWER, 0xFF);
    res |= es8388_write_reg(codec, ES8388_ADCCONTROL1, 0xbb); // MIC Left and Right channel PGA gain
    tmp = 0;
    // TODO default use ADC LINE1
    // 0x00 LINSEL & RINSEL, LIN1/RIN1 as ADC Input; DSSEL,use one DS Reg11; DSR, LINPUT1-RINPUT1
    res |= es8388_write_reg(codec, ES8388_ADCCONTROL2, ADC_INPUT_LINPUT1_RINPUT1);
    res |= es8388_write_reg(codec, ES8388_ADCCONTROL3, 0x02);
    res |= es8388_write_reg(codec, ES8388_ADCCONTROL4, 0x0c); // 16 Bits length and I2S serial audio data format
    res |= es8388_write_reg(codec, ES8388_ADCCONTROL5, 0x02); // ADCFsMode,singel SPEED,RATIO=256
    // ALC for Microphone
    res |= es8388_set_adc_dac_volume(codec, ESP_CODEC_DEV_WORK_MODE_ADC, 0, 0); // 0db
    res |= es8388_write_reg(codec, ES8388_ADCPOWER, 0x09); // Power on ADC
    if (res != 0) {
        ESP_LOGI(TAG, "Fail to write register");
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es8388_enable(const audio_codec_if_t *h, bool enable)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (codec->enabled == enable) {
        return ESP_CODEC_DEV_OK;
    }
    int res;
    if (enable == false) {
        es8388_pa_power(codec, false);
        res = es8388_stop(codec, codec->codec_mode);
    } else {
        res = es8388_start(codec, codec->codec_mode);
        es8388_pa_power(codec, true);
    }
    if (res == ESP_CODEC_DEV_OK) {
        codec->enabled = enable;
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    }
    return res;
}

static int es8388_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8388_set_voice_mute(codec, mute);
}

static int es8388_set_vol(const audio_codec_if_t *h, float db_value)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    db_value -= codec->hw_gain;
    int volume = esp_codec_dev_vol_calc_reg(&vol_range, db_value);
    int res = es8388_write_reg(codec, ES8388_DACCONTROL5, volume);
    res |= es8388_write_reg(codec, ES8388_DACCONTROL4, volume);
    ESP_LOGD(TAG, "Set volume reg:%x db:%f", volume, db_value);
    return res ? ESP_CODEC_DEV_WRITE_FAIL : ESP_CODEC_DEV_OK;
}

static int es8388_set_fs(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL || fs == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int res = 0;
    res |= es8388_config_fmt(codec, ESP_CODEC_DEV_WORK_MODE_BOTH, ES_I2S_NORMAL);
    res |= es8388_set_bits_per_sample(codec, ESP_CODEC_DEV_WORK_MODE_BOTH, fs->bits_per_sample);
    return res;
}

static int es8388_set_gain(const audio_codec_if_t *h, float db)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8388_set_mic_gain(codec, db);
}

static int es8388_close(const audio_codec_if_t *h)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        es8388_pa_power(codec, false);
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

static void es8388_dump(const audio_codec_if_t *h)
{
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return;
    }
    for (int i = 0; i <= ES8388_DACCONTROL30; i++) {
        int value = 0;
        int ret = es8388_read_reg(codec, i, &value);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, value);
    }
}

const audio_codec_if_t *es8388_codec_new(es8388_codec_cfg_t *codec_cfg)
{
    // verify param
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_es8388_t *codec = (audio_codec_es8388_t *) calloc(1, sizeof(audio_codec_es8388_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->base.open = es8388_open;
    codec->base.enable = es8388_enable;
    codec->base.set_fs = es8388_set_fs;
    codec->base.set_vol = es8388_set_vol;
    codec->base.mute = es8388_mute;
    codec->base.set_mic_gain = es8388_set_gain;
    codec->base.dump_reg = es8388_dump;
    codec->base.close = es8388_close;
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es8388_codec_cfg_t));
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to open");
            break;
        }
        return &codec->base;
    } while (0);
    if (codec) {
        free(codec);
    }
    return NULL;
}
