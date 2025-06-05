/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ES8389_CODEC_H_
#define _ES8389_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8389_CODEC_DEFAULT_ADDR (0x20)

/**
 * @brief ES8389 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;  /*!< Codec work mode: ADC or DAC */
    int16_t                      pa_pin;      /*!< PA chip power pin */
    bool                         pa_reverted; /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
    bool                         master_mode; /*!< Whether codec works as I2S master or not */
    bool                         use_mclk;    /*!< Whether use external MCLK clock */
    bool                         digital_mic; /*!< Whether use digital microphone */
    bool                         invert_mclk; /*!< MCLK clock signal inverted or not */
    bool                         invert_sclk; /*!< SCLK clock signal inverted or not */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
    bool                         no_dac_ref;  /*!< When record 2 channel data
                                                   false: right channel filled with dac output
                                                   true: right channel leave empty
                                              */
    uint16_t                     mclk_div;    /*!< MCLK/LRCK default is 256 if not provided */
} es8389_codec_cfg_t;

/**
 * @brief         New ES8389 codec interface
 * @param         codec_cfg: ES8389 codec configuration
 * @return        NULL: Fail to new ES8389 codec interface
 *                -Others: ES8389 codec interface
 */
const audio_codec_if_t *es8389_codec_new(es8389_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
