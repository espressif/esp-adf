/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tvl320_codec.h"
#include "tvl320_reg.h"
#include "esp_log.h"
#include "es_common.h"
#include <math.h>

#define TAG     "TVL320_CODEC"

/* The volume register mapped to decibel table can get from codec data-sheet
   Volume control register 0x65 description:
       {0x00 - '0.0dB' ... 0x30 - '24.0db'}
       {0x81 - '-63.5dB' ... 0xFF = '-0.5dB'}
*/
const esp_codec_dev_vol_range_t vol_range = {
    .min_vol =
    {
        .vol = -127,
        .db_value = -63.5,
    },
    .max_vol =
    {
        .vol = 48,
        .db_value = 24.0,
    },
};

static int tvl320_write_reg(audio_codec_tvl320_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static int tvl320_read_reg(audio_codec_tvl320_t *codec, int reg, int *value)
{
    *value = 0;
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, value, 1);
}

static int tvl320_write_reg_check(audio_codec_tvl320_t *codec, int reg, int value) {
    int ret = ESP_CODEC_DEV_OK;
    int value_check;

    ret |= tvl320_write_reg(codec, reg, value);
    ret |= tvl320_read_reg(codec, reg, &value_check);
    if (value_check != value) {
        ESP_LOGE(TAG, "Mismatch in value writen (%d) vs value read (%d) to register (%d)", value, value_check, reg);
    }

    return ret;
}

static int tvl320_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) h;
    tvl320_codec_cfg_t *codec_cfg = (tvl320_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg == NULL || cfg_size != sizeof(tvl320_codec_cfg_t) || codec_cfg->ctrl_if == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = ESP_CODEC_DEV_OK;
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->gpio_if = codec_cfg->gpio_if;

    // Set register page to 0
    ret |= tvl320_write_reg_check(codec, TVL320_PAGE_0_CONTROL_REG00, TVL320_PAGE_0);

    // Initiate SW Reset
    ret |= tvl320_write_reg(codec, TVL320_SOFTWARE_RESET_REG01, 0x01);

    // Program and power up NDAC
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_NDAC_VAL_REG11, 0x88);

    // Program and power up MDAC
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_MDAC_VAL_REG12, 0x82);

    // Program OSR value
    // DOSR = 128, DOSR(9:8) = 0, DOSR(7:0) = 128
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_DOSR_VAL_MSB_REG13, 0x00);
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_DOSR_VAL_LSB_REG14, 0x80);

    // mode is i2s, wordlength is 16, slave mode
    ret |= tvl320_write_reg_check(codec, TVL320_CODEC_INTERFACE_CONTROL_REG27, 00);

    // Program the processing block to be used
    // Select DAC DSP Processing Block PRB_P16
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_PROCESSING_BLOCK_MINIDSP_SELECTION_REG60, 0x10);
    ret |= tvl320_write_reg_check(codec, TVL320_PAGE_0_CONTROL_REG00, TVL320_PAGE_8);
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_COEFFICIENT_RAM_CONTROL_REG01, 0x04);
    ret |= tvl320_write_reg_check(codec, TVL320_PAGE_8_CONTROL_REG00, TVL320_PAGE_0);

    // Program analog blocks
    // Page 1 is selected
    ret |= tvl320_write_reg_check(codec, TVL320_PAGE_8_CONTROL_REG00, TVL320_PAGE_1);

    // Program common-mode voltage (default = 1.35V)
    ret |= tvl320_write_reg_check(codec, TVL320_HEADPHONE_DRIVERS_REG31, 0x04);

    // Program headphone-specific depop settings (in case headphone driver is used)
    // De-pop, Power on = 800 ms, Step time = 4 ms
    ret |= tvl320_write_reg_check(codec, TVL320_HP_OUTPUT_DRIVERS_POP_REMOVAL_SETTINGS_REG33, 0x4e);

    // Program routing of DAC output ot the output amplifier (headphone/lineout or speaker)
    // DAC routed to HPOUT
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_OUTPUT_MIXER_ROUTING_REG35, 0x40);

    // Unmute and set gain of output driver
    // Unmute HPOUT, set gain = 0db
    ret |= tvl320_write_reg_check(codec, TVL320_HPOUT_DRIVER_REG40, 0x06);

    // Unmute Class-D, set gain = 18 db
    ret |= tvl320_write_reg_check(codec, TVL320_CLASS_D_OUTPUT_DRIVER_DRIVER_REG42, 0x1C);

    // HPOUT powered up
    ret |= tvl320_write_reg_check(codec, TVL320_HEADPHONE_DRIVERS_REG31, 0x82);

    // Power-up Class-D drivers
    ret |= tvl320_write_reg_check(codec, TVL320_CLASS_D_SPEAKER_AMP_REG32, 0xC6);

    // Enable HPOUT output analog volume, set = -9 dB
    ret |= tvl320_write_reg_check(codec, TVL320_ANALOG_VOLUME_TO_HPOUT_REG36, 0x92);

    // Enable Class-D output analog volume, set = -9 dB
    ret |= tvl320_write_reg_check(codec, TVL320_ANALOG_VOLUME_TO_CLASS_D_OUTPUT_DRIVER_REG38, 0x92);

    // Page 0 is selected
    ret |= tvl320_write_reg_check(codec, TVL320_PAGE_1_CONTROL_REG00, TVL320_PAGE_0);

    // Powerup DAC (soft step disable)
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_DATA_PATH_SETUP_REG63, 0x94);

    // DAC gain = -22 dB
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_VOLUME_CONTROL_REG65, 0xD4);

    // Unmute digital volume control
    // Unmute DAC
    ret |= tvl320_write_reg_check(codec, TVL320_DAC_MUTE_CONTROL_REG64, 0x04);

    if (ret != 0) {
        return ESP_CODEC_DEV_WRITE_FAIL;
    }

    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int tvl320_set_vol(const audio_codec_if_t *h, float db_value)
{
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }

    uint8_t volume = esp_codec_dev_vol_calc_reg(&vol_range, db_value);
    int ret = tvl320_write_reg_check(codec, TVL320_DAC_VOLUME_CONTROL_REG65, volume);
    ESP_LOGD(TAG, "Set volume reg:%x db:%f", volume, db_value);
    return (ret == 0) ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static int tvl320_set_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int regv;
    int ret = tvl320_read_reg(codec, TVL320_DAC_MUTE_CONTROL_REG64, &regv);
    if (ret < 0) {
        return ESP_CODEC_DEV_READ_FAIL;
    }
    if (mute) {
        regv = 0x0C;
    } else {
        regv = 0x04;
    }
    return tvl320_write_reg_check(codec, TVL320_DAC_MUTE_CONTROL_REG64, (uint8_t) regv);
}

static int tvl320_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return tvl320_write_reg_check(codec, reg, value);
}

static int tvl320_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return tvl320_read_reg(codec, reg, value);
}

const audio_codec_if_t *tvl320_codec_new(tvl320_codec_cfg_t *codec_cfg)
{
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_tvl320_t *codec = (audio_codec_tvl320_t *) calloc(1, sizeof(audio_codec_tvl320_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }

    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->base.open = tvl320_open;
    codec->base.set_vol = tvl320_set_vol;
    codec->base.mute = tvl320_set_mute;
    codec->base.set_reg = tvl320_set_reg;
    codec->base.get_reg = tvl320_get_reg;
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(tvl320_codec_cfg_t));
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
