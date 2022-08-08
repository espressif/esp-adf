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

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Pixel Dev Type
*
*/
typedef struct pixel_dev_s pixel_dev_t;

typedef pixel_dev_t *(*pixel_dev_init_func)(uint8_t channel, uint8_t gpio_num, uint16_t led_num);                        /*!< Pattern function pointer */

/**
* @brief Declare of Pixel Dev Type
*
*/
struct pixel_dev_s {
    /**
    * @brief Set RGB for a specific pixel
    *
    * @param  dev:           Pixel Dev
    * @param  index:         Index of pixel to set
    * @param  red:           Red part of color
    * @param  green:         Green part of color
    * @param  blue:          Blue part of color
    *
    * @return
    *     - ESP_OK:          Set RGB for a specific pixel successfully
    *     - ESP_ERR_         INVALID_ARG: Set RGB for a specific pixel failed because of invalid parameters
    *     - ESP_FAIL:        Set RGB for a specific pixel failed because other error occurred
    */
    esp_err_t (*set_pixel)(pixel_dev_t *dev, uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

    /**
    * @brief Refresh memory colors to Pixels
    *
    * @param  dev:           Pixel Dev
    * @param  timeout_ms:    Timeout value for refreshing task
    *
    * @return
    *     - ESP_OK:          Refresh successfully
    *     - ESP_ERR_TIMEOUT: Refresh failed because of timeout
    *     - ESP_FAIL:        Refresh failed because some other error occurred
    *
    * @note:
    *      After updating the LED colors in the memory, a following invocation of this API is needed to flush colors to dev.
    */
    esp_err_t (*refresh)(pixel_dev_t *dev, uint32_t timeout_ms);

    /**
    * @brief Clear Pixel Dev (turn off all Pixels)
    *
    * @param  dev:           Pixel Dev
    * @param  timeout_ms:    Timeout value for clearing task
    *
    * @return
    *     - ESP_OK:          Clear Pixels successfully
    *     - ESP_ERR_TIMEOUT: Clear Pixels failed because of timeout
    *     - ESP_FAIL:        Clear Pixels failed because some other error occurred
    */
    esp_err_t (*clear)(pixel_dev_t *dev, uint32_t timeout_ms);

    /**
    * @brief Free Pixel Dev resources
    *
    * @param dev:            Pixel Dev
    *
    * @return
    *     - ESP_OK:          Free resources successfully
    *     - ESP_FAIL:        Free resources failed because error occurred
    */
    esp_err_t (*del)(pixel_dev_t *dev);
};

/**
 * @brief      Add Pixel Dev
 *
 * @param[in]  fn            Driver initialization function
 * @param[in]  channel       SPI peripheral channel number.
 * @param[in]  gpio          GPIO number for the SPI data output.
 * @param[in]  led_num       Number of addressable LEDs.
 *
 * @return
 *     - Pixel Dev pointer
 */
pixel_dev_t *pixel_dev_add(pixel_dev_init_func fn, uint8_t channel, uint8_t gpio_num, uint16_t led_num);

/**
 * @brief      Remove Pixel device
 *
 * @param[in]  dev    Pixel Dev pointer
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t pixel_dev_remove(pixel_dev_t *dev);

/**
 * @brief      Pixel Dev set dot
 *
 * @param[in]  dev        Pixel Dev pointer
 * @param[in]  led_pos    Pixel position/index
 * @param[in]  red        Red
 * @param[in]  green      Green
 * @param[in]  blue       Blue
 */
void pixel_dev_set_dot(pixel_dev_t *dev, int32_t led_pos, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief      Refresh Pixel Dev
 *
 * @param[in]  dev           Pixel Dev pointer
 * @param[in]  timeout_ms    Timeout_ms
 */
void pixel_dev_refresh(pixel_dev_t *dev, int32_t timeout_ms);

/**
 * @brief      Clear Pixel Dev
 *
 * @param[in]  dev           Pixel Dev pointer
 * @param[in]  timeout_ms    Timeout_ms
 */
void pixel_dev_clear(pixel_dev_t *dev, int32_t timeout_ms);

#ifdef __cplusplus
}
#endif
