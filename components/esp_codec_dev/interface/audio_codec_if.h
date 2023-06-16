/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AUDIO_CODEC_IF_H_
#define _AUDIO_CODEC_IF_H_

#include "esp_codec_dev_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_codec_if_t audio_codec_if_t;

/**
 * @brief Structure for codec interface
 */
struct audio_codec_if_t {
    int (*open)(const audio_codec_if_t *h, void *cfg, int cfg_size);           /*!< Open codec */
    bool (*is_open)(const audio_codec_if_t *h);                                /*!< Check whether codec is opened */
    int (*enable)(const audio_codec_if_t *h, bool enable);                     /*!< Enable codec, when codec disabled it can use less power if provided */
    int (*set_fs)(const audio_codec_if_t *h, esp_codec_dev_sample_info_t *fs); /*!< Set audio format to codec */
    int (*mute)(const audio_codec_if_t *h, bool mute);                         /*!< Mute and un-mute DAC output */
    int (*set_vol)(const audio_codec_if_t *h, float db);                       /*!< Set DAC volume in decibel */
    int (*set_mic_gain)(const audio_codec_if_t *h, float db);                  /*!< Set microphone gain in decibel */
    int (*set_mic_channel_gain)(const audio_codec_if_t *h, 
               uint16_t channel_mask, float db);                               /*!< Set microphone gain in decibel by channel */
    int (*mute_mic)(const audio_codec_if_t *h, bool mute);                     /*!< Mute and un-mute microphone */
    int (*set_reg)(const audio_codec_if_t *h, int reg, int value);             /*!< Set register value to codec */
    int (*get_reg)(const audio_codec_if_t *h, int reg, int *value);            /*!< Get register value from codec */
    void (*dump_reg)(const audio_codec_if_t *h);                               /*!< Dump all register settings */
    int (*close)(const audio_codec_if_t *h);                                   /*!< Close codec */
};

/**
 * @brief         Delete codec interface instance
 * @param         codec_if: Codec interface
 * @return        ESP_CODEC_DEV_OK: Delete success
 *                ESP_CODEC_DEV_INVALID_ARG: Input is NULL pointer
 */
int audio_codec_delete_codec_if(const audio_codec_if_t *codec_if);

#ifdef __cplusplus
}
#endif

#endif
