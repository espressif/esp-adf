/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES7243_ADC_H_
#define _ES7243_ADC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES7243_CODEC_DEFAULT_ADDR (0x26)

/**
 * @brief ES7243 codec configuration, only support ADC feature
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if; /*!< Codec Control interface */
} es7243_codec_cfg_t;

/**
 * @brief         New ES7243 codec interface
 *                Notes: this API should called after I2S clock ready
 *                       Or else write register may fail
 * @param         codec_cfg: ES7243 codec configuration
 * @return        NULL: Fail to new ES7243 codec interface
 *                -Others: ES7243 codec interface
 */
const audio_codec_if_t *es7243_codec_new(es7243_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
