/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AUDIO_CODEC_DATA_IF_H_
#define _AUDIO_CODEC_DATA_IF_H_

#include "esp_codec_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_codec_data_if_t audio_codec_data_if_t;

/**
 * @brief Audio codec data interface structure
 */
struct audio_codec_data_if_t {
    int (*open)(const audio_codec_data_if_t *h, void *data_cfg, int cfg_size); /*!< Open data interface */
    bool (*is_open)(const audio_codec_data_if_t *h); /*!< Check whether data interface is opened */
    int (*enable)(const audio_codec_data_if_t *h,
                    esp_codec_dev_type_t dev_type,
                    bool enable);  /*!< Enable input or output channel */
    int (*set_fmt)(const audio_codec_data_if_t *h,
                    esp_codec_dev_type_t dev_type,
                    esp_codec_dev_sample_info_t *fs);                       /*!< Set audio format to data interface */
    int (*read)(const audio_codec_data_if_t *h, uint8_t *data, int size);  /*!< Read data from data interface */
    int (*write)(const audio_codec_data_if_t *h, uint8_t *data, int size); /*!< Write data to data interface */
    int (*close)(const audio_codec_data_if_t *h);                          /*!< Close data interface */
};

/**
 * @brief         Delete codec data interface instance
 * @param         data_if: Codec data interface
 * @return        ESP_CODEC_DEV_OK: Delete success
 *                ESP_CODEC_DEV_INVALID_ARG: Input is NULL pointer
 */
int audio_codec_delete_data_if(const audio_codec_data_if_t *data_if);

#ifdef __cplusplus
}
#endif

#endif
