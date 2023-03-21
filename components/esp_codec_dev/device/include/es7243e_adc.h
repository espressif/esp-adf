/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES7243E_ADC_H_
#define _ES7243E_ADC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES7243E_CODEC_DEFAULT_ADDR (0x20)

/**
 * @brief ES7243E codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if; /*!< Codec Control interface */
} es7243e_codec_cfg_t;

/**
 * @brief         New ES7243E codec interface
 * @param         codec_cfg: ES7243E codec configuration
 * @return        NULL: Fail to new ES7243E codec interface
 *                -Others: ES7243E codec interface
 */
const audio_codec_if_t *es7243e_codec_new(es7243e_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
