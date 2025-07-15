/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_DEFAULTS_H_
#define _ESP_CODEC_DEV_DEFAULTS_H_

#include "audio_codec_if.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_data_if.h"
#include "audio_codec_gpio_if.h"

#ifdef CONFIG_CODEC_ES8311_SUPPORT
#include "es8311_codec.h"
#endif
#ifdef CONFIG_CODEC_ES7210_SUPPORT
#include "es7210_adc.h"
#endif
#ifdef CONFIG_CODEC_ES7243_SUPPORT
#include "es7243_adc.h"
#endif
#ifdef CONFIG_CODEC_ES7243E_SUPPORT
#include "es7243e_adc.h"
#endif
#ifdef CONFIG_CODEC_ES8156_SUPPORT
#include "es8156_dac.h"
#endif
#ifdef CONFIG_CODEC_AW88298_SUPPORT
#include "aw88298_dac.h"
#endif
#ifdef CONFIG_CODEC_ES8389_SUPPORT
#include "es8389_codec.h"
#endif
#ifdef CONFIG_CODEC_ES8374_SUPPORT
#include "es8374_codec.h"
#endif
#ifdef CONFIG_CODEC_ES8388_SUPPORT
#include "es8388_codec.h"
#endif
#ifdef CONFIG_CODEC_TAS5805M_SUPPORT
#include "tas5805m_dac.h"
#endif
#ifdef CONFIG_CODEC_ZL38063_SUPPORT
#include "zl38063_codec.h"
#endif
#if CONFIG_CODEC_CJC8910_SUPPORT
#include "cjc8910_codec.h"
#endif
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec I2C configuration
 */
typedef struct {
    uint8_t port;       /*!< I2C port, this port need pre-installed by other modules */
    uint8_t addr;       /*!< I2C address, default address can be gotten from codec head files */
    void   *bus_handle; /*!< I2C Master bus handle (for IDFv5.3 or higher version) */
} audio_codec_i2c_cfg_t;

/**
 * @brief Codec I2S configuration
 */
typedef struct {
    uint8_t port;      /*!< I2S port, this port need pre-installed by other modules */
    void   *rx_handle; /*!< I2S rx handle, need provide on IDF 5.x */
    void   *tx_handle; /*!< I2S tx handle, need provide on IDF 5.x */
} audio_codec_i2s_cfg_t;

/**
 * @brief Codec SPI configuration
 */
typedef struct {
    uint8_t spi_port;    /*!< SPI port, this port need pre-installed by other modules */
    int16_t cs_pin;      /*!< SPI CS GPIO pin setting */
    int     clock_speed; /*!< SPI clock unit hz (use 10MHZif set to 0)*/
} audio_codec_spi_cfg_t;

/**
 * @brief         Get default codec GPIO interface
 * @return        NULL: Failed
 *                Others: Codec GPIO interface
 */
const audio_codec_gpio_if_t *audio_codec_new_gpio(void);

/**
 * @brief         Get default SPI control interface
 * @return        NULL: Failed
 *                Others: SPI control interface
 */
const audio_codec_ctrl_if_t *audio_codec_new_spi_ctrl(audio_codec_spi_cfg_t *spi_cfg);

/**
 * @brief         Get default I2C control interface
 * @return        NULL: Failed
 *                Others: I2C control interface
 */
const audio_codec_ctrl_if_t *audio_codec_new_i2c_ctrl(audio_codec_i2c_cfg_t *i2c_cfg);

/**
 * @brief         Get default I2S data interface
 * @return        NULL: Failed
 *                Others: I2S data interface
 */
const audio_codec_data_if_t *audio_codec_new_i2s_data(audio_codec_i2s_cfg_t *i2s_cfg);

#ifdef __cplusplus
}
#endif

#endif
