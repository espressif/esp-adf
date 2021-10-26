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

#ifndef _PIXEL_COORD_H_
#define _PIXEL_COORD_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Coordinate information
 */
typedef struct pixel_coord_s {
    int16_t x;
    int16_t y;
    int16_t z;
} pixel_coord_t;

/**
 * @brief Number axis position
 */
typedef struct {
    uint16_t index;
} pixel_coord_num_axis_t;

/*!< Calculate the LED index according to the coordinates of the LED matrix */
typedef esp_err_t (*pixel_coord_to_index_func)(uint16_t origin_offset, uint16_t y_axis_points, pixel_coord_t *input, pixel_coord_num_axis_t *output);

/**
 * @brief      Convert matrix coordinates into led index
 *
 * @param[in]  origin_offset    Origin offset. Set at pixel_renderer_init
 * @param[in]  y_axis_points    The number of leds on the y-axis, the minimum value is 1
 * @param[in]  input            Enter the coordinates to convert.(e.g. 'pixel_coord_t')
 * @param[out] output           output the converted result.(e.g. 'pixel_coord_num_axis_t')
 *
 * @return
 *     - ESP_OK
 */
esp_err_t pixel_coord_to_index_cb(uint16_t origin_offset, uint16_t y_axis_points, pixel_coord_t *input, pixel_coord_num_axis_t *output);

#ifdef __cplusplus
}
#endif
#endif // _PIXEL_COORD_H_
