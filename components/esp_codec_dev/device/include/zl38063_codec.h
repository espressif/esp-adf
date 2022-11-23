/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ZL38063_CODEC_H_
#define _ZL38063_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ZL38063 codec configuration
 *        Notes: ZL38063 codec driver provide default configuration of I2S settings in firmware.
 *               Defaults are 48khz, 16 bits, 2 channels
 *               To playback other sample rate need do resampling firstly
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;  /*!< Codec work mode: ADC or DAC */
    int16_t                      pa_pin;      /*!< PA chip power pin */
    bool                         pa_reverted; /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
    int16_t                      reset_pin;   /*!< Reset pin */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} zl38063_codec_cfg_t;

/**
 * @brief         New ZL38063 codec interface
 * @param         codec_cfg: ZL38063 codec configuration
 * @return        NULL: Fail to new ZL38063 codec interface
 *                -Others: ZL38063 codec interface
 */
const audio_codec_if_t *zl38063_codec_new(zl38063_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
