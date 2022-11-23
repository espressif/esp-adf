/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "es7243_adc.h"
#include "esp_log.h"
#include "es_common.h"
#include "esp_codec_dev_os.h"

#define MCLK_PULSES_NUMBER (20)
#define TAG                "ES7243"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    bool                         is_open;
    bool                         enabled;
} audio_codec_es7243_t;

static int es7243_write_reg(audio_codec_es7243_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static int es7243_adc_set_voice_mute(audio_codec_es7243_t *codec, bool mute)
{
    ESP_LOGI(TAG, "mute = %d", mute);
    if (mute) {
        es7243_write_reg(codec, 0x05, 0x1B);
    } else {
        es7243_write_reg(codec, 0x05, 0x13);
    }
    return 0;
}

static int es7243_adc_set_gain(audio_codec_es7243_t *codec, float db)
{
    int ret = 0;
    if (db <= 1) {
        ret = es7243_write_reg(codec, 0x08, 0x11); // 1db
    } else if (db <= 4) {
        ret = es7243_write_reg(codec, 0x08, 0x13); // 3.5db
    } else if (db <= 18) {
        ret = es7243_write_reg(codec, 0x08, 0x21); // 18db
    } else if (db < 21) {
        ret = es7243_write_reg(codec, 0x08, 0x23); // 20.5db
    } else if (db < 23) {
        ret = es7243_write_reg(codec, 0x08, 0x06); // 22.5db
    } else if (db < 25) {
        ret = es7243_write_reg(codec, 0x08, 0x41); // 24.5db
    } else {
        ret = es7243_write_reg(codec, 0x08, 0x43); // 27db
    }
    ESP_LOGI(TAG, "Set DB %f", db);
    return ret;
}

static int es7243_adc_enable(audio_codec_es7243_t *codec, bool enable)
{
    int ret = ESP_CODEC_DEV_OK;
    if (enable) {
        // slave mode only
        ret |= es7243_write_reg(codec, 0x00, 0x01);
        ret |= es7243_write_reg(codec, 0x06, 0x00);
        ret |= es7243_write_reg(codec, 0x05, 0x1B);
        ret |= es7243_write_reg(codec, 0x01, 0x0C);
        ret |= es7243_write_reg(codec, 0x08, 0x43);
        ret |= es7243_write_reg(codec, 0x05, 0x13);
    } else {
        ret |= es7243_write_reg(codec, 0x06, 0x05);
        ret |= es7243_write_reg(codec, 0x05, 0x1B);
        ret |= es7243_write_reg(codec, 0x06, 0x5C);
        ret |= es7243_write_reg(codec, 0x07, 0x3F);
        ret |= es7243_write_reg(codec, 0x08, 0x4B);
        ret |= es7243_write_reg(codec, 0x09, 0x9F);
    }
    ESP_LOGI(TAG, "Set enable %d", enable);
    return ret;
}

static int es7243_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    es7243_codec_cfg_t *codec_cfg = (es7243_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(es7243_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (es7243_adc_enable(codec, true)) {
        ESP_LOGE(TAG, "Fail to write register");
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    codec->enabled = true;
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es7243_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7243_adc_set_voice_mute(codec, mute);
}

static int es7243_enable(const audio_codec_if_t *h, bool enable)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (codec->enabled == enable) {
        return ESP_CODEC_DEV_OK;
    }
    int ret = es7243_adc_enable(codec, enable);
    if (ret == ESP_CODEC_DEV_OK) {
        codec->enabled = enable;
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

static int es7243_set_gain(const audio_codec_if_t *h, float db)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7243_adc_set_gain(codec, db);
}

static int es7243_close(const audio_codec_if_t *h)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

static int es7243_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7243_write_reg(codec, reg, value);
}

static int es7243_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static void es7243_dump(const audio_codec_if_t *h)
{
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return;
    }
    for (int i = 0; i < 10; i++) {
        int value = 0;
        int ret = codec->ctrl_if->read_reg(codec->ctrl_if, i, 1, &value, 1);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, value);
    }
}

const audio_codec_if_t *es7243_codec_new(es7243_codec_cfg_t *codec_cfg)
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
    audio_codec_es7243_t *codec = (audio_codec_es7243_t *) calloc(1, sizeof(audio_codec_es7243_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;
    codec->base.open = es7243_open;
    codec->base.enable = es7243_enable;
    codec->base.set_mic_gain = es7243_set_gain;
    codec->base.mute_mic = es7243_mute;
    codec->base.set_reg = es7243_set_reg;
    codec->base.get_reg = es7243_get_reg;
    codec->base.dump_reg = es7243_dump;
    codec->base.close = es7243_close;
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es7243_codec_cfg_t));
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
