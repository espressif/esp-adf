/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _TAS5805M_DAC_H_
#define _TAS5805M_DAC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "esp_codec_dev_vol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TAS5805M_CODEC_DEFAULT_ADDR (0x5c)

/**
 * @brief TAS5805M codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;     /*!< Codec Control interface */
    const audio_codec_gpio_if_t *gpio_if;     /*!< Codec GPIO interface */
    esp_codec_dec_work_mode_t    codec_mode;  /*!< Codec work mode: ADC or DAC */
    bool                         master_mode; /*!< Whether codec works as I2S master or not */
    int16_t                      reset_pin;   /*!< Reset pin */
    esp_codec_dev_hw_gain_t      hw_gain;     /*!< Hardware gain */
} tas5805m_codec_cfg_t;

/**
 * @brief         New TAS5805M codec interface
 * @param         codec_cfg: TAS5805M codec configuration
 * @return        NULL: Fail to new TAS5805M codec interface
 *                -Others: TAS5805M codec interface
 */
const audio_codec_if_t *tas5805m_codec_new(tas5805m_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
