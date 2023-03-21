/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AUDIO_CODEC_CTRL_IF_H_
#define _AUDIO_CODEC_CTRL_IF_H_

#include "esp_codec_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_codec_ctrl_if_t audio_codec_ctrl_if_t;

/**
 * @brief Audio codec control interface structure
 */
struct audio_codec_ctrl_if_t {
    int (*open)(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size); /*!< Open codec control interface */
    bool (*is_open)(const audio_codec_ctrl_if_t *ctrl);                      /*!< Check whether codec control opened or not */
    int (*read_reg)(const audio_codec_ctrl_if_t *ctrl,
                     int reg, int reg_len, void *data, int data_len);        /*!< Read data from codec device register */
    int (*write_reg)(const audio_codec_ctrl_if_t *ctrl,
                      int reg, int reg_len, void *data, int data_len);       /*!< Write data to codec device register */
    int (*close)(const audio_codec_ctrl_if_t *ctrl);                         /*!< Close codec control interface */
};

/**
 * @brief         Delete codec control interface instance
 * @param         ctrl_if: Audio codec interface
 * @return        ESP_CODEC_DEV_OK: Delete success
 *                ESP_CODEC_DEV_INVALID_ARG: Input is NULL pointer
 */
int audio_codec_delete_ctrl_if(const audio_codec_ctrl_if_t *ctrl_if);

#ifdef __cplusplus
}
#endif

#endif
