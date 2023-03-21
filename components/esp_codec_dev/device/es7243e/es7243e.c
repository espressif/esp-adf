/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "es7243e_adc.h"
#include "esp_log.h"
#include "es_common.h"
#include "esp_codec_dev_os.h"

#define TAG "ES7243E"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    bool                         is_open;
    bool                         enabled;
} audio_codec_es7243e_t;

static uint8_t get_db_reg(float db)
{
    // reg: 12 - 34.5dB, 13 - 36dB, 14 - 37.5dB
    db += 0.5;
    if (db <= 33.0) {
        return (uint8_t) db / 3;
    }
    if (db < 36.0) {
        return 12;
    }
    if (db < 37.0) {
        return 13;
    }
    return 14;
}

static int es7243e_write_reg(audio_codec_es7243e_t *codec, int reg, int value)
{
    return codec->ctrl_if->write_reg(codec->ctrl_if, reg, 1, &value, 1);
}

int es7243e_adc_enable(audio_codec_es7243e_t *codec, bool enable)
{
    int ret = ESP_CODEC_DEV_OK;
    if (enable) {
        // slave mode only
        ret |= es7243e_write_reg(codec, 0xF9, 0x00);
        ret |= es7243e_write_reg(codec, 0x04, 0x01);
        ret |= es7243e_write_reg(codec, 0x17, 0x01);
        ret |= es7243e_write_reg(codec, 0x20, 0x10);
        ret |= es7243e_write_reg(codec, 0x21, 0x10);
        ret |= es7243e_write_reg(codec, 0x00, 0x80);
        ret |= es7243e_write_reg(codec, 0x01, 0x3A);
        ret |= es7243e_write_reg(codec, 0x16, 0x3F);
        ret |= es7243e_write_reg(codec, 0x16, 0x00);
    } else {
        ret |= es7243e_write_reg(codec, 0x04, 0x02);
        ret |= es7243e_write_reg(codec, 0x04, 0x01);
        ret |= es7243e_write_reg(codec, 0xF7, 0x30);
        ret |= es7243e_write_reg(codec, 0xF9, 0x01);
        ret |= es7243e_write_reg(codec, 0x16, 0xFF);
        ret |= es7243e_write_reg(codec, 0x17, 0x00);
        ret |= es7243e_write_reg(codec, 0x01, 0x38);
        ret |= es7243e_write_reg(codec, 0x20, 0x00);
        ret |= es7243e_write_reg(codec, 0x21, 0x00);
        ret |= es7243e_write_reg(codec, 0x00, 0x00);
        ret |= es7243e_write_reg(codec, 0x00, 0x1E);
        ret |= es7243e_write_reg(codec, 0x01, 0x30);
        ret |= es7243e_write_reg(codec, 0x01, 0x00);
    }
    return ret;
}

static int es7243e_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    es7243e_codec_cfg_t *codec_cfg = (es7243e_codec_cfg_t *) cfg;
    if (codec == NULL || codec_cfg->ctrl_if == NULL || cfg_size != sizeof(es7243e_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    codec->ctrl_if = codec_cfg->ctrl_if;

    int ret = 0;
    ret |= es7243e_write_reg(codec, 0x01, 0x3A);
    ret |= es7243e_write_reg(codec, 0x00, 0x80);
    ret |= es7243e_write_reg(codec, 0xF9, 0x00);
    ret |= es7243e_write_reg(codec, 0x04, 0x02);
    ret |= es7243e_write_reg(codec, 0x04, 0x01);
    ret |= es7243e_write_reg(codec, 0xF9, 0x01);
    ret |= es7243e_write_reg(codec, 0x00, 0x1E);
    ret |= es7243e_write_reg(codec, 0x01, 0x00);

    ret |= es7243e_write_reg(codec, 0x02, 0x00);
    ret |= es7243e_write_reg(codec, 0x03, 0x20);
    ret |= es7243e_write_reg(codec, 0x04, 0x01);
    ret |= es7243e_write_reg(codec, 0x0D, 0x00);
    ret |= es7243e_write_reg(codec, 0x05, 0x00);
    ret |= es7243e_write_reg(codec, 0x06, 0x03); // SCLK=MCLK/4
    ret |= es7243e_write_reg(codec, 0x07, 0x00); // LRCK=MCLK/256
    ret |= es7243e_write_reg(codec, 0x08, 0xFF); // LRCK=MCLK/256

    ret |= es7243e_write_reg(codec, 0x09, 0xCA);
    ret |= es7243e_write_reg(codec, 0x0A, 0x85);
    ret |= es7243e_write_reg(codec, 0x0B, 0x00);
    ret |= es7243e_write_reg(codec, 0x0E, 0xBF);
    ret |= es7243e_write_reg(codec, 0x0F, 0x80);
    ret |= es7243e_write_reg(codec, 0x14, 0x0C);
    ret |= es7243e_write_reg(codec, 0x15, 0x0C);
    ret |= es7243e_write_reg(codec, 0x17, 0x02);
    ret |= es7243e_write_reg(codec, 0x18, 0x26);
    ret |= es7243e_write_reg(codec, 0x19, 0x77);
    ret |= es7243e_write_reg(codec, 0x1A, 0xF4);
    ret |= es7243e_write_reg(codec, 0x1B, 0x66);
    ret |= es7243e_write_reg(codec, 0x1C, 0x44);
    ret |= es7243e_write_reg(codec, 0x1E, 0x00);
    ret |= es7243e_write_reg(codec, 0x1F, 0x0C);
    ret |= es7243e_write_reg(codec, 0x20, 0x1A); // PGA gain +30dB
    ret |= es7243e_write_reg(codec, 0x21, 0x1A);

    ret |= es7243e_write_reg(codec, 0x00, 0x80); // Slave  Mode
    ret |= es7243e_write_reg(codec, 0x01, 0x3A);
    ret |= es7243e_write_reg(codec, 0x16, 0x3F);
    ret |= es7243e_write_reg(codec, 0x16, 0x00);
    if (ret != 0 || es7243e_adc_enable(codec, true)) {
        ESP_LOGI(TAG, "Fail to write register");
        return ESP_CODEC_DEV_WRITE_FAIL;
    }
    codec->enabled = true;
    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int es7243e_enable(const audio_codec_if_t *h, bool enable)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    if (codec->enabled == enable) {
        return ESP_CODEC_DEV_OK;
    }
    int ret = es7243e_adc_enable(codec, enable);
    if (ret == ESP_CODEC_DEV_OK) {
        codec->enabled = enable;
        ESP_LOGD(TAG, "Codec is %s", enable ? "enabled" : "disabled");
    }
    return ret;
}

static int es7243e_set_gain(const audio_codec_if_t *h, float db_value)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    uint8_t reg = get_db_reg(db_value);
    int ret = ESP_CODEC_DEV_OK;
    ret |= es7243e_write_reg(codec, 0x20, 0x10 | reg);
    ret |= es7243e_write_reg(codec, 0x21, 0x10 | reg);
    return ret;
}

static int es7243e_set_mute(const audio_codec_if_t *h, bool mute)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int ret;
    if (mute) {
        ret = es7243e_write_reg(codec, 0x0B, 0xC0);
    } else {
        ret = es7243e_write_reg(codec, 0x0B, 0x00);
    }
    ESP_LOGD(TAG, "%s", mute ? "Muted" : "Unmuted");
    return ret;
}

static int es7243e_close(const audio_codec_if_t *h)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        es7243e_adc_enable(codec, false);
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

static int es7243e_set_reg(const audio_codec_if_t *h, int reg, int value)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return es7243e_write_reg(codec, reg, value);
}

static int es7243e_get_reg(const audio_codec_if_t *h, int reg, int *value)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    return codec->ctrl_if->read_reg(codec->ctrl_if, reg, 1, &value, 1);
}

static void es7243e_dump(const audio_codec_if_t *h)
{
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) h;
    if (codec == NULL || codec->is_open == false) {
        return;
    }
    for (int i = 0; i < 0xFF; i++) {
        int value = 0;
        int ret = codec->ctrl_if->read_reg(codec->ctrl_if, i, 1, &value, 1);
        if (ret != ESP_CODEC_DEV_OK) {
            break;
        }
        ESP_LOGI(TAG, "%02x: %02x", i, value);
    }
}

const audio_codec_if_t *es7243e_codec_new(es7243e_codec_cfg_t *codec_cfg)
{
    if (codec_cfg == NULL || codec_cfg->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }
    if (codec_cfg->ctrl_if->is_open(codec_cfg->ctrl_if) == false) {
        ESP_LOGE(TAG, "Control interface not open yet");
        return NULL;
    }
    audio_codec_es7243e_t *codec = (audio_codec_es7243e_t *) calloc(1, sizeof(audio_codec_es7243e_t));
    if (codec == NULL) {
        CODEC_MEM_CHECK(codec);
        return NULL;
    }
    codec->base.open = es7243e_open;
    codec->base.enable = es7243e_enable;
    codec->base.mute_mic = es7243e_set_mute;
    codec->base.set_mic_gain = es7243e_set_gain;
    codec->base.set_reg = es7243e_set_reg;
    codec->base.get_reg = es7243e_get_reg;
    codec->base.dump_reg = es7243e_dump;
    codec->base.close = es7243e_close;
    do {
        int ret = codec->base.open(&codec->base, codec_cfg, sizeof(es7243e_codec_cfg_t));
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
