/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES8156_DAC_H_
#define _ES8156_DAC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8156_CODEC_DEFAULT_ADDR (0x10)

/**
 * @brief ES8156 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    int16_t                      pa_pin;      /*!< PA chip power pin */
    bool                         pa_reverted; /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} es8156_codec_cfg_t;

/**
 * @brief         New ES8156 codec interface
 * @param         codec_cfg: ES8156 codec configuration
 * @return        NULL: Fail to new ES8156 codec interface
 *                -Others: ES8156 codec interface
 */
const audio_codec_if_t *es8156_codec_new(es8156_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
