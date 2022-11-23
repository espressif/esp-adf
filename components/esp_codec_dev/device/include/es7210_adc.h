/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ES7210_ADC_H_
#define _ES7210_ADC_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ES7210_CODEC_DEFAULT_ADDR (0x80)

#define ES7120_SEL_MIC1           (uint8_t)(1 << 0)
#define ES7120_SEL_MIC2           (uint8_t)(1 << 1)
#define ES7120_SEL_MIC3           (uint8_t)(1 << 2)
#define ES7120_SEL_MIC4           (uint8_t)(1 << 3)

/**
 * @brief ES7210 MCLK clock source when work in master mode
 */
typedef enum {
    ES7210_MCLK_FROM_PAD,
    ES7210_MCLK_FROM_CLOCK_DOUBLER,
} es7210_mclk_src_t;

/**
 * @brief ES7210 codec configuration, only support ADC feature
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if;      /*!< Codec Control interface */
    bool                         master_mode;  /*!< Whether codec works as I2S master or not */
    uint8_t                      mic_selected; /*!< Selected microphone */
    es7210_mclk_src_t            mclk_src;     /*!< MCLK clock source in master mode */
    uint16_t                     mclk_div;     /*!< MCLK/LRCK default is 256 if not provided */
} es7210_codec_cfg_t;

/**
 * @brief         New ES7210 codec interface
 * @param         codec_cfg: ES7210 codec configuration
 * @return        NULL: Fail to new ES7210 codec interface
 *                -Others: ES7210 codec interface
 */
const audio_codec_if_t *es7210_codec_new(es7210_codec_cfg_t *codec_cfg);

#ifdef __cplusplus
}
#endif

#endif
