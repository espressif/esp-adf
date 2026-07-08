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
 * @brief  Codec control interface type
 */
typedef enum {
    AUDIO_CODEC_CTRL_NONE = 0,  /*!< No control interface */
    AUDIO_CODEC_CTRL_I2C,       /*!< I2C control interface */
    AUDIO_CODEC_CTRL_SPI,       /*!< SPI control interface */
} audio_codec_ctrl_type_t;

/**
 * @brief  Codec control interface info
 */
typedef struct {
    audio_codec_ctrl_type_t  type;  /*!< Control interface type */
    union {
        struct {
            uint16_t  addr;  /*!< I2C device address */
            uint8_t   port;  /*!< I2C port */
        } i2c;
        struct {
            int16_t  cs_pin;  /*!< SPI CS pin */
        } spi;
    };
} audio_codec_ctrl_info_t;

/**
 * @brief  Audio codec control interface structure
 */
struct audio_codec_ctrl_if_t {
    int (*open)(const audio_codec_ctrl_if_t *ctrl, void *cfg, int cfg_size);  /*!< Open codec control interface */
    bool (*is_open)(const audio_codec_ctrl_if_t *ctrl);                       /*!< Check whether codec control opened or not */
    int (*read_reg)(const audio_codec_ctrl_if_t *ctrl,
                    int reg, int reg_len, void *data, int data_len);  /*!< Read data from codec device register */
    int (*write_reg)(const audio_codec_ctrl_if_t *ctrl,
                     int reg, int reg_len, void *data, int data_len);  /*!< Write data to codec device register */
    int (*get_info)(const audio_codec_ctrl_if_t *ctrl,
                    audio_codec_ctrl_info_t *info);                          /*!< Get control interface info */
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
