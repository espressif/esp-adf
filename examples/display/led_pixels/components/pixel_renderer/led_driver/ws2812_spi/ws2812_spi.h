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

#ifdef __cplusplus
extern "C" {
#endif

#include "pixel_dev.h"

typedef struct pixel_dev_s ws2812_t;

/**
 * @brief      Init the SPI peripheral and WS2812 configuration.
 *
 * @param[in]  channel:    SPI peripheral channel number.
 * @param[in]  gpio:       GPIO number for the SPI data output.
 * @param[in]  led_num:    Number of addressable LEDs.
 *
 * @return
 *     - WS2812 instance or NULL
 */
ws2812_t * ws2812_spi_init(uint8_t channel, uint8_t gpio, uint16_t led_num);

/**
 * @brief      Deinit the SPI peripheral.
 *
 * @param[in]  dev:    WS2812 instance
 *
 * @return
 *     - ESP_OK
 */
esp_err_t ws2812_spi_deinit(ws2812_t *strip);

#ifdef __cplusplus
}
#endif
