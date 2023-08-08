/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TVL320_CODEC_H_
#define _TVL320_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TVL320_CODEC_DEFAULT_ADDR       (0x30)

typedef struct {
    audio_codec_if_t             base;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    int16_t                      pa_pin;
    bool                         pa_reverted;
    bool                         is_open;
    bool                         enabled;
    float                        hw_gain;
} audio_codec_tvl320_t;

/**
 * @brief tvl320 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} tvl320_codec_cfg_t;

const audio_codec_if_t *tvl320_codec_new(tvl320_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
