/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_TYPES_H_
#define _ESP_CODEC_DEV_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_CODEC_DEV_VERSION  "1.3.1"

/**
 * @brief Define error number of codec device module
 *        Inherit from `esp_err_t`
 */
#define ESP_CODEC_DEV_OK          (0)
#define ESP_CODEC_DEV_DRV_ERR     (ESP_FAIL)
#define ESP_CODEC_DEV_INVALID_ARG (ESP_ERR_INVALID_ARG)
#define ESP_CODEC_DEV_NO_MEM      (ESP_ERR_NO_MEM)
#define ESP_CODEC_DEV_NOT_SUPPORT (ESP_ERR_NOT_SUPPORTED)
#define ESP_CODEC_DEV_NOT_FOUND   (ESP_ERR_NOT_FOUND)
#define ESP_CODEC_DEV_WRONG_STATE (ESP_ERR_INVALID_STATE)
#define ESP_CODEC_DEV_WRITE_FAIL  (0x10D)
#define ESP_CODEC_DEV_READ_FAIL   (0x10E)

#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(channel) ((uint16_t)1 << (channel))

/**
 * @brief Codec Device type
 */
typedef enum {
    ESP_CODEC_DEV_TYPE_NONE,
    ESP_CODEC_DEV_TYPE_IN = (1 << 0),  /*!< Codec input device like ADC (capture data from microphone) */
    ESP_CODEC_DEV_TYPE_OUT = (1 << 1), /*!< Codec output device like DAC (output analog signal to speaker) */
    ESP_CODEC_DEV_TYPE_IN_OUT = (ESP_CODEC_DEV_TYPE_IN | ESP_CODEC_DEV_TYPE_OUT), /*!< Codec input and output device */
} esp_codec_dev_type_t;

/**
 * @brief Codec audio sample information
 *        Notes: channel_mask is used to filter wanted channels in driver side
 *               when set to 0, default filter all channels
 *               when channel is 2, can filter channel 0 (set to 1) or channel 1 (set to 2)
 *               when channel is 4, can filter either 3,2 channels or 1 channel
 */
typedef struct {
    uint8_t  bits_per_sample;   /*!< Bit lengths of one channel data */
    uint8_t  channel;           /*!< Channels of sample */
    uint16_t channel_mask;      /*!< Channel mask indicate which channel to be selected */
    uint32_t sample_rate;       /*!< Sample rate of sample */
    int mclk_multiple;          /*!< The multiple of MCLK to the sample rate
                                     If value is 0, mclk = sample_rate * 256
                                     If bits_per_sample is 24bit, mclk_multiple should be the multiple of 3
                                */
} esp_codec_dev_sample_info_t;

/**
 * @brief Codec working mode
 */
typedef enum {
    ESP_CODEC_DEV_WORK_MODE_NONE,
    ESP_CODEC_DEV_WORK_MODE_ADC = (1 << 0), /*!< Enable ADC, only support input */
    ESP_CODEC_DEV_WORK_MODE_DAC = (1 << 1), /*!< Enable DAC, only support output */
    ESP_CODEC_DEV_WORK_MODE_BOTH =
        (ESP_CODEC_DEV_WORK_MODE_ADC | ESP_CODEC_DEV_WORK_MODE_DAC), /*!< Support both DAC and ADC */
    ESP_CODEC_DEV_WORK_MODE_LINE = (1 << 2),                         /*!< Line mode */
} esp_codec_dec_work_mode_t;

#ifdef __cplusplus
}
#endif

#endif
