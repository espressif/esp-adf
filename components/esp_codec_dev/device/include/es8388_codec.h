/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES8388_CODEC_H_
#define _ES8388_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ES8388 default I2C address
 */
#define ES8388_CODEC_DEFAULT_ADDR   (0x20)
#define ES8388_CODEC_DEFAULT_ADDR_1 (0x22)
/**
 * @brief ES8388 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;  /*!< Codec work mode on ADC or DAC */
    bool                         master_mode; /*!< Whether codec works as I2S master or not */
    int16_t                      pa_pin;      /*!< PA chip power pin */
    bool                         pa_reverted; /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} es8388_codec_cfg_t;

/**
 * @brief         New ES8388 codec interface
 * @param         codec_cfg: ES8388 codec configuration
 * @return        NULL: Fail to new ES8388 codec interface
 *                -Others: ES8388 codec interface
 */
const audio_codec_if_t *es8388_codec_new(es8388_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
