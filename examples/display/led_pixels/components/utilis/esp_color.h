/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _ESP_COLOR_H_
#define _ESP_COLOR_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RGB value
 */
typedef struct {
    union {
        uint8_t r;        /*!< The red value */
        uint8_t red;      /*!< The red value */
    };
    union {
        uint8_t g;        /*!< The green value */
        uint8_t green;    /*!< The green value */
    };
    union {
        uint8_t b;        /*!< The blue value */
        uint8_t blue;     /*!< The blue value */
    };
} esp_color_rgb_t;

/**
 * @brief HSV value
 */
typedef struct {
    int16_t h;      /*!< The hue value */
    int8_t  s;      /*!< The saturation value */
    int8_t  v;      /*!< The value */
} esp_color_hsv_t;

/**
 * @brief      HSV to RGB
 *
 * @param[in]  hsv    HSV value
 * @param[out] rgb    RGB value
 *
 * @return
 *     - ESP_OK
 */
esp_err_t esp_color_hsv2rgb(esp_color_hsv_t *hsv, esp_color_rgb_t *rgb);

/**
 * @brief      RGB to HSV
 *
 * @param[in]  rgb    RGB value
 * @param[out] hsv    HSV value
 *
 * @return
 *     - ESP_OK
 */
esp_err_t esp_color_rgb2hsv(esp_color_rgb_t *rgb, esp_color_hsv_t *hsv);

/**
 * @brief      Parse color codes as R, G, B values
 *
 * @param[in]  colorcode    Colorcode
 * @param[out] color        Color.r, Color.g, Color.b
 *
 * @return
 *     - ESP_OK
 */
esp_err_t esp_color_code2rgb(uint32_t colorcode, esp_color_rgb_t *color);

/**
 * @brief      Cycle through colors using HSV
 *
 * @param[out] color    RGB value after update
 *
 * @return
 *     - ESP_OK
 */
esp_err_t esp_color_update_rainbow(esp_color_rgb_t *color);

#ifdef __cplusplus
}
#endif
#endif // _ESP_COLOR_H_
