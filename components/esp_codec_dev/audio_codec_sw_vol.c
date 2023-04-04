/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "audio_codec_sw_vol.h"

#define GAIN_0DB_SHIFT (15)

typedef struct {
    audio_codec_vol_if_t        base;
    esp_codec_dev_sample_info_t fs;
    uint16_t                    gain;
    bool                        is_open;
    int                         cur;
    int                         step;
    int                         block_size;
    int                         duration;
} audio_vol_t;

static int _sw_vol_close(const audio_codec_vol_if_t *h)
{
    audio_vol_t *vol = (audio_vol_t *)h;
    if (h) {
        vol->is_open = false;
        return ESP_CODEC_DEV_OK;
    }
    return ESP_CODEC_DEV_INVALID_ARG;
}

static int _sw_vol_open(const audio_codec_vol_if_t *h, esp_codec_dev_sample_info_t *fs, int duration)
{
    audio_vol_t *vol = (audio_vol_t *)h;
    if (vol == NULL || fs == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (fs->bits_per_sample != 16) {
        return ESP_CODEC_DEV_NOT_SUPPORT;
    }
    vol->fs = *fs;
    vol->block_size = (vol->fs.bits_per_sample * vol->fs.channel) >> 3;
    vol->duration = duration;
    vol->is_open = true;
    return ESP_CODEC_DEV_OK;
}

static int _sw_vol_process(const audio_codec_vol_if_t *h, uint8_t *in, int len,
                           uint8_t *out, int out_len)
{
    audio_vol_t *vol = (audio_vol_t *) h;
    if (vol == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    if (vol->is_open == false) {
        return ESP_CODEC_DEV_WRONG_STATE;
    }
    int sample = len / vol->block_size;
    if (vol->fs.bits_per_sample == 16) {
        int16_t *v_in = (int16_t *) in;
        int16_t *v_out = (int16_t *) out;
        if (vol->cur == vol->gain) {
            if (vol->gain == 0) {
                memset(out, 0, len);
                return 0;
            } else {
                for (int i = 0; i < sample; i++) {
                    for (int j = 0; j < vol->fs.channel; j++) {
                        *(v_out++) = ((*v_in++) * vol->cur) >> GAIN_0DB_SHIFT;
                    }
                }
                return 0;
            }
        }
        for (int i = 0; i < sample; i++) {
            for (int j = 0; j < vol->fs.channel; j++) {
                *(v_out++) = ((*v_in++) * vol->cur) >> GAIN_0DB_SHIFT;
            }
            if (vol->step) {
                vol->cur += vol->step;
                if (vol->step > 0) {
                    if (vol->cur > vol->gain) {
                        vol->cur = vol->gain;
                        vol->step = 0;
                    }
                } else {
                    if (vol->cur < vol->gain) {
                        vol->cur = vol->gain;
                        vol->step = 0;
                    }
                }
            }
        }
    }
    return 0;
}

static int _sw_vol_set(const audio_codec_vol_if_t *h, float db_value)
{
    audio_vol_t *vol = (audio_vol_t *) h;
    if (vol == NULL) {
        return ESP_CODEC_DEV_INVALID_ARG;
    }
    // Support set volume when not opened
    int gain;
    if (db_value <= -96.0) {
        gain = 0;
    } else {
        gain = (int) (exp(db_value / 20 * log(10)) * (1 << GAIN_0DB_SHIFT));
    }
    vol->gain = gain;
    if (vol->is_open) {
        float step = (float) (vol->gain - vol->cur) * 1000 / vol->duration / vol->fs.sample_rate;
        vol->step = (int) step;
        if (step == 0) {
            vol->cur = vol->gain;
        }
    } else {
        vol->step = 0;
        vol->cur = vol->gain;
    }
    return ESP_CODEC_DEV_OK;
}

const audio_codec_vol_if_t *audio_codec_new_sw_vol(void)
{
    audio_vol_t *vol = calloc(1, sizeof(audio_vol_t));
    if (vol == NULL) {
        return NULL;
    }
    vol->base.open = _sw_vol_open;
    vol->base.set_vol = _sw_vol_set;
    vol->base.process = _sw_vol_process;
    vol->base.close = _sw_vol_close;
    // Default no audio output
    vol->cur = vol->gain = 0;
    return &vol->base;
}
