/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "dummy_codec.h"

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_gpio_if_t *gpio_if;
    int16_t                      pa_pin;
    bool                         pa_reverted;
    bool                         is_open;
    bool                         enabled;
    bool                         muted;
} dummy_codec_t;

static const char *TAG = "DUMMY_CODEC";

static int dummy_codec_pa_set(dummy_codec_t *codec, bool on)
{
    if (codec->gpio_if == NULL || codec->pa_pin < 0) {
        return ESP_CODEC_DEV_OK;
    }
    bool level = codec->pa_reverted ? !on : on;
    return codec->gpio_if->set(codec->pa_pin, level);
}

static bool dummy_codec_is_open(const audio_codec_if_t *h)
{
    const dummy_codec_t *codec = (const dummy_codec_t *)h;
    return codec && codec->is_open;
}

static int dummy_codec_open(const audio_codec_if_t *h, void *cfg, int cfg_size)
{
    dummy_codec_t *codec = (dummy_codec_t *)h;
    dummy_codec_cfg_t *codec_cfg = (dummy_codec_cfg_t *)cfg;
    if (codec == NULL || codec_cfg == NULL || cfg_size != sizeof(dummy_codec_cfg_t)) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }

    codec->gpio_if = codec_cfg->gpio_if;
    codec->pa_pin = codec_cfg->pa_pin;
    codec->pa_reverted = codec_cfg->pa_reverted;
    codec->enabled = false;
    codec->muted = false;

    if (codec->gpio_if == NULL || codec->pa_pin < 0) {
        ESP_LOGW(TAG, "PA control disabled, gpio_if:%p pa_pin:%d", codec->gpio_if, codec->pa_pin);
    } else {
        int ret = codec->gpio_if->setup(codec->pa_pin, AUDIO_GPIO_DIR_OUT, AUDIO_GPIO_MODE_FLOAT);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "PA gpio setup failed:%d", ret);
            return ret;
        }
        ret = dummy_codec_pa_set(codec, false);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "PA default off failed:%d", ret);
            return ret;
        }
    }

    codec->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int dummy_codec_enable(const audio_codec_if_t *h, bool enable)
{
    dummy_codec_t *codec = (dummy_codec_t *)h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    codec->enabled = enable;
    if (codec->muted) {
        return dummy_codec_pa_set(codec, false);
    }
    return dummy_codec_pa_set(codec, enable);
}

static int dummy_codec_mute(const audio_codec_if_t *h, bool mute)
{
    dummy_codec_t *codec = (dummy_codec_t *)h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    codec->muted = mute;
    if (mute) {
        return dummy_codec_pa_set(codec, false);
    }
    return dummy_codec_pa_set(codec, codec->enabled);
}

static int dummy_codec_close(const audio_codec_if_t *h)
{
    dummy_codec_t *codec = (dummy_codec_t *)h;
    if (codec == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (codec->is_open) {
        (void)dummy_codec_pa_set(codec, false);
        codec->enabled = false;
        codec->muted = false;
        codec->is_open = false;
    }
    return ESP_CODEC_DEV_OK;
}

const audio_codec_if_t *dummy_codec_new(dummy_codec_cfg_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Wrong codec config");
        return NULL;
    }

    dummy_codec_t *codec = (dummy_codec_t *)calloc(1, sizeof(dummy_codec_t));
    if (codec == NULL) {
        ESP_LOGE(TAG, "No memory for dummy codec");
        return NULL;
    }

    codec->base.open = dummy_codec_open;
    codec->base.is_open = dummy_codec_is_open;
    codec->base.enable = dummy_codec_enable;
    codec->base.set_fs = NULL;
    codec->base.mute = dummy_codec_mute;
    codec->base.set_vol = NULL;
    codec->base.set_mic_gain = NULL;
    codec->base.set_mic_channel_gain = NULL;
    codec->base.mute_mic = NULL;
    codec->base.set_reg = NULL;
    codec->base.get_reg = NULL;
    codec->base.dump_reg = NULL;
    codec->base.close = dummy_codec_close;

    if (codec->base.open(&codec->base, cfg, sizeof(dummy_codec_cfg_t)) != ESP_CODEC_DEV_OK) {
        free(codec);
        return NULL;
    }
    return &codec->base;
}
