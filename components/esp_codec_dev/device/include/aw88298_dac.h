/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AW88298_DAC_H_
#define _AW88298_DAC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AW88298_CODEC_DEFAULT_ADDR (0x36 << 1)

/**
 * @brief AW88298 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    int16_t                      reset_pin;   /*!< Reset pin */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} aw88298_codec_cfg_t;

/**
 * @brief         New AW88298 codec interface
 * @attention     Need set mclk_multiple to I2S_MCLK_MULTIPLE_384 in esp_codec_dev_sample_info_t to support 44100
 * @param         codec_cfg: AW88298 codec configuration
 * @return        NULL: Fail to new AW88298 codec interface
 *                -Others: AW88298 codec interface
 */
const audio_codec_if_t *aw88298_codec_new(aw88298_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
