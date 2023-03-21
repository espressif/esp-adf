/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AUDIO_CODEC_GPIO_IF_H_
#define _AUDIO_CODEC_GPIO_IF_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GPIO drive mode
 */
typedef enum {
    AUDIO_GPIO_MODE_FLOAT,                /*!< Float */
    AUDIO_GPIO_MODE_PULL_UP = (1 << 0),   /*!< Internally pullup */
    AUDIO_GPIO_MODE_PULL_DOWN = (1 << 1), /*!< Internally pulldown */
} audio_gpio_mode_t;

/**
 * @brief GPIO direction type
 */
typedef enum {
    AUDIO_GPIO_DIR_OUT, /*!< Output GPIO */
    AUDIO_GPIO_DIR_IN,  /*!< Input GPIO */
} audio_gpio_dir_t;

/**
 * @brief Codec GPIO interface structure
 */
typedef struct {
    int (*setup)(int16_t gpio, audio_gpio_dir_t dir, audio_gpio_mode_t mode); /*!< Setup GPIO */
    int (*set)(int16_t gpio, bool high); /*!< Set GPIO level */
    bool (*get)(int16_t gpio);           /*!< Get GPIO level */
} audio_codec_gpio_if_t;

/**
 * @brief         Delete GPIO interface instance
 * @param         gpio_if: GPIO interface
 * @return        ESP_CODEC_DEV_OK: Delete success
 *                ESP_CODEC_DEV_INVALID_ARG: Input is NULL pointer
 */
int audio_codec_delete_gpio_if(const audio_codec_gpio_if_t *gpio_if);

#ifdef __cplusplus
}
#endif

#endif
