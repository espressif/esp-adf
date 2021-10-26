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

#include "pixel_coord.h"

esp_err_t pixel_coord_to_index_cb(uint16_t origin_offset, uint16_t y_axis_points, pixel_coord_t *input, pixel_coord_num_axis_t *output)
{
    int16_t pixel_index = 0;
    if (origin_offset / y_axis_points % 2) {
        if (input->x % 2) {
            pixel_index = input->x * y_axis_points - 1 + (y_axis_points - input->y);
        } else {
            pixel_index = input->x * y_axis_points + input->y;
        }
    } else {
        if (input->x == 0) {
            output->index = origin_offset - input->y;
            return ESP_OK;
        }
        if (input->x % 2) {
            pixel_index = (input->x - 1) * y_axis_points + input->y + 1;
        } else {
            pixel_index = input->x * y_axis_points - input->y;
        }
    }
    output->index = pixel_index + origin_offset;
    return ESP_OK;
}
