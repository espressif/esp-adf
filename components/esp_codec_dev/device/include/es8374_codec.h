/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES8374_CODEC_H_
#define _ES8374_CODEC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES8374_CODEC_DEFAULT_ADDR   (0x20)
#define ES8374_CODEC_DEFAULT_ADDR_1 (0x21)

/**
 * @brief ES8374 codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;  /*!< Codec work mode: ADC or DAC */
    bool                         master_mode; /*!< Whether codec works as I2S master or not */
    int16_t                      pa_pin;      /*!< PA chip power pin */
    bool                         pa_reverted; /*!< false: enable PA when pin set to 1, true: enable PA when pin set to 0 */
} es8374_codec_cfg_t;

/**
 * @brief         New ES8374 codec interface
 * @param         codec_cfg: ES8374 codec configuration
 * @return        NULL: Fail to new ES8374 codec interface
 *                -Others: ES8374 codec interface
 */
const audio_codec_if_t *es8374_codec_new(es8374_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif //__ES8374_H__
