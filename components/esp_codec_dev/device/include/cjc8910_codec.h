/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _CJC8910_CODEC_H_
#define _CJC8910_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define CJC8910_CODEC_DEFAULT_ADDR (0x30)

/**
 * @brief  CJC8910 codec configuration
 *
 * @note  This driver only supports codec work in slave mode
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;      /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;      /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;   /*!< Codec work mode: ADC or DAC */
    int16_t                      pa_pin;       /*!< PA chip power pin */
    bool                         pa_reverted;  /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
    bool                         invert_lr;    /*!< Left/Right channel inverted or not */
    bool                         invert_sclk;  /*!< SCLK clock signal inverted or not */
    esp_codec_dev_hw_gain_t      hw_gain;      /*!< Hardware gain */
} cjc8910_codec_cfg_t;

/**
 * @brief  New CJC8910 codec interface
 *
 * @param  codec_cfg  CJC8910 codec configuration
 *
 * @return
 *       - NULL    Not enough memory or codec failed to open
 *       - Others  CJC8910 codec interface
 */
const audio_codec_if_t *cjc8910_codec_new(cjc8910_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _CJC8910_CODEC_H_ */
