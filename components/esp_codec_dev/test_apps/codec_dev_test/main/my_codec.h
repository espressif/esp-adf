/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _MY_CODEC_H_
#define _MY_CODEC_H_

#include <stdio.h>
#include <string.h>
#include "esp_codec_dev.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Customized codec configuration
 */
typedef struct {
    const audio_codec_ctrl_if_t *ctrl_if; /*!< Codec control interface
                                               If use i2c can use `audio_codec_new_i2c_ctrl` directly */
    const audio_codec_gpio_if_t *gpio_if; /*!< GPIO interface to control gpio */
    esp_codec_dev_hw_gain_t      hw_gain; /*!< Hardware gain, see `esp_codec_dev_vol.h` for details */
} my_codec_cfg_t;

/**
 * @brief Customized codec control instance
 */
typedef enum {
    MY_CODEC_REG_VOL,      /*!< Register to control volume */
    MY_CODEC_REG_MUTE,     /*!< Register to mute microphone */
    MY_CODEC_REG_MIC_GAIN, /*!< Register for microphone gain */
    MY_CODEC_REG_MIC_MUTE, /*!< Register to mute microphone */
    MY_CODEC_REG_SUSPEND,  /*!< Register to suspend codec chip */
    MY_CODEC_REG_MAX,
} my_codec_reg_type_t;

typedef struct {
    audio_codec_ctrl_if_t base;
    uint8_t               reg[MY_CODEC_REG_MAX];
    bool                  is_open;
} my_codec_ctrl_t;

/**
 * @brief Customized codec data instance
 */
typedef struct {
    audio_codec_data_if_t       base;
    esp_codec_dev_sample_info_t fmt;
    int                         read_idx;
    int                         write_idx;
    bool                        is_open;
} my_codec_data_t;

/**
 * @brief Customized codec volume instance
 */
typedef struct {
    audio_codec_vol_if_t        base;
    esp_codec_dev_sample_info_t fs;
    int                         shift;
    int                         process_len;
    float                       vol_db;
    bool                        is_open;
} my_codec_vol_t;

/**
 * @brief Customized codec control interface
 */
const audio_codec_ctrl_if_t *my_codec_ctrl_new();

/**
 * @brief Customized codec data interface
 *        If use i2s for data path can use `audio_codec_new_i2s_data` directly
 */
const audio_codec_data_if_t *my_codec_data_new();

/**
 * @brief Customized codec realization
 */
const audio_codec_if_t *my_codec_new(my_codec_cfg_t *codec_cfg);

/**
 * @brief Customized volume algorithm
 */
const audio_codec_vol_if_t *my_codec_vol_new();

#ifdef __cplusplus
}
#endif

#endif
