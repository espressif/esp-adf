/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "es8156_dac.h"
#include "es8156_reg.h"
#include "esp_log.h"
#include "es_common.h"
#include "esp_codec_dev_os.h"

#define TAG     "ES8156"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    int16_t                      pa_pin;
    bool                         pa_reverted;
    bool                         is_open;
    bool                         enabled;
    float                        hw_gain;
} audio_codec_es8156_t;

/* The volume register mapped to decibel table can get from codec data-sheet
   Volume control register 0x14 description:
       0x00 - '-95.5dB' ... 0xFF - '+32dB'
*/
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

static int es8156_write_reg(audio_codec_es8156_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static int es8156_read_reg(audio_codec_es8156_t *codec, int reg, int *value)
{
    *value = 0;
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, value, 1);
}

static int es8156_stop(audio_codec_es8156_t *codec)
{
    int ret = 0;
    ret = es8156_write_reg(codec, 0x14, 0x00);
    ret |= es8156_write_reg(codec, 0x19, 0x02);
    ret |= es8156_write_reg(codec, 0x21, 0x1F);
    ret |= es8156_write_reg(codec, 0x22, 0x02);
    ret |= es8156_write_reg(codec, 0x25, 0x21);
    ret |= es8156_write_reg(codec, 0x25, 0xA1);
    ret |= es8156_write_reg(codec, 0x18, 0x01);
    ret |= es8156_write_reg(codec, 0x09, 0x02);
    ret |= es8156_write_reg(codec, 0x09, 0x01);
    ret |= es8156_write_reg(codec, 0x08, 0x00);
    return ret;
}

static int es8156_start(audio_codec_es8156_t *codec)
{
    int ret = 0;
    ret |= es8156_write_reg(codec, 0x08, 0x3F);
    ret |= es8156_write_reg(codec, 0x09, 0x00);
    ret |= es8156_write_reg(codec, 0x18, 0x00);

    ret |= es8156_write_reg(codec, 0x25, 0x20);
    ret |= es8156_write_reg(codec, 0x22, 0x00);
    ret |= es8156_write_reg(codec, 0x21, 0x3C);
    ret |= es8156_write_reg(codec, 0x19, 0x20);
    ret |= es8156_write_reg(codec, 0x14, 179);
    return ret;
}

static int es8156_set_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int regv;
    int ret = es8156_read_reg(codec, ES8156_DAC_MUTE_REG13, &regv);
    if (ret < 0) {
        return ESP_CODEC_DEV_READ_FAIL;
    }
    if (mute) {
        regv = regv | BITS(1) | BITS(2);
    } else {
        regv = regv & (~(BITS(1) | BITS(2)));
    }
    return es8156_write_reg(codec, ES8156_DAC_MUTE_REG13, (uint8_t) regv);
}

static int es8156_set_vol(const audio_codec_if_t *h, float volume)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    volume -= codec->hw_gain;
    int reg = esp_codec_dev_vol_calc_reg(&vol_range, volume);
    int ret = es8156_write_reg(codec, ES8156_VOLUME_CONTROL_REG14, reg);
    ESP_LOGD(TAG, "Set volume reg:%x db:%f", reg, volume);
    return (ret == 0) ? ESP_CODEC_DEV_OK : ESP_CODEC_DEV_WRITE_FAIL;
}

static void es8156_pa_power(audio_codec_es8156_t *codec, es_pa_setting_t pa_setting)
{
    int16_t pa_pin = codec->pa_pin;
    if (pa_pin == -1 || codec->gpio_if == NULL) {
        return;
    }
    if (pa_setting & ES_PA_SETUP) {
        codec->gpio_if->setup(pa_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
    } 
    if (pa_setting & ES_PA_ENABLE) {
        codec->gpio_if->set(pa_pin, codec->pa_reverted ? false : true);
    }
    if (pa_setting & ES_PA_DISABLE) {
        codec->gpio_if->set(pa_pin, codec->pa_reverted ? true : false);
    }
}

static int es8156_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    es8156_codec_cfg_t *codec_cfg = (es8156_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg == NULL || cfg_size != sizeof(es8156_codec_cfg_t) || codec_cfg->ctrl_if == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    int ret = ESP_CODEC_DEV_OK;
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->gpio_if = codec_cfg->gpio_if;
    codec->pa_pin = codec_cfg->pa_pin;
    codec->pa_reverted = codec_cfg->pa_reverted;

    ret |= es8156_write_reg(codec, 0x02, 0x04);
    ret |= es8156_write_reg(codec, 0x20, 0x2A);
    ret |= es8156_write_reg(codec, 0x21, 0x3C);
    ret |= es8156_write_reg(codec, 0x22, 0x00);
    ret |= es8156_write_reg(codec, 0x24, 0x07);
    ret |= es8156_write_reg(codec, 0x23, 0x00);

    ret |= es8156_write_reg(codec, 0x0A, 0x01);
    ret |= es8156_write_reg(codec, 0x0B, 0x01);
    ret |= es8156_write_reg(codec, 0x11, 0x00);
    ret |= es8156_write_reg(codec, 0x14, 179); // volume 70%

    ret |= es8156_write_reg(codec, 0x0D, 0x14);
    ret |= es8156_write_reg(codec, 0x18, 0x00);
    ret |= es8156_write_reg(codec, 0x08, 0x3F);
    ret |= es8156_write_reg(codec, 0x00, 0x02);
    ret |= es8156_write_reg(codec, 0x00, 0x03);
    ret |= es8156_write_reg(codec, 0x25, 0x20);
    if (ret != 0) {
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    es8156_pa_power(codec, ES_PA_SETUP | ES_PA_ENABLE);
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es8156_close(const audio_codec_if_t *h)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        es8156_stop(codec);
        es8156_pa_power(codec, ES_PA_DISABLE);
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

static int es8156_enable(const audio_codec_if_t *h, bool enable)
{
    int ret = ESP_CODEC_DEV_OK;
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (codec->enabled == enable) {
        return ESP_CODEC_DEV_OK;
    }
    if (enable) {
        ret = es8156_start(codec);
        es8156_pa_power(codec, ES_PA_ENABLE);
    } else {
        es8156_pa_power(codec, ES_PA_DISABLE);
        ret = es8156_stop(codec);
    }
    if (ret == ESP_CODEC_DEV_OK) {
        codec->enabled = enable;
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

static int es8156_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8156_write_reg(codec, reg, value);
}

static int es8156_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es8156_read_reg(codec, reg, value);
}

static void es8156_dump(const audio_codec_if_t *h)
{
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return;
    }
    for (int i = 0; i <= 0x25; i++) {
        int value = 0;
        int ret = es8156_read_reg(codec, i, &value);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, value);
    }
}

const audio_codec_if_t *es8156_codec_new(es8156_codec_cfg_t *codec_cfg)
{
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_es8156_t *codec = (audio_codec_es8156_t *) calloc(1, sizeof(audio_codec_es8156_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->base.open = es8156_open;
    codec->base.enable = es8156_enable;
    codec->base.set_vol = es8156_set_vol;
    codec->base.mute = es8156_set_mute;
    codec->base.set_reg = es8156_set_reg;
    codec->base.get_reg = es8156_get_reg;
    codec->base.dump_reg = es8156_dump;
    codec->base.close = es8156_close;
    codec->hw_gain = esp_codec_dev_col_calc_hw_gain(&codec_cfg->hw_gain);
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es8156_codec_cfg_t));
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
