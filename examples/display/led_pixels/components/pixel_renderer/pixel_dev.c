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

#include "pixel_dev.h"

pixel_dev_t *pixel_dev_add(pixel_dev_init_func fn, uint8_t channel, uint8_t gpio_num, uint16_t led_num)
{
    pixel_dev_t *dev = fn(channel, gpio_num, led_num);
    return dev;
}

esp_err_t pixel_dev_remove(pixel_dev_t *dev)
{
    return dev->del(dev);
}

void pixel_dev_set_dot(pixel_dev_t *dev, int32_t led_pos, uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_ERROR_CHECK(dev->set_pixel(dev, led_pos, red, green, blue));
}

void pixel_dev_refresh(pixel_dev_t *dev, int32_t timeout_ms)
{
    ESP_ERROR_CHECK(dev->refresh(dev, timeout_ms));
}

void pixel_dev_clear(pixel_dev_t *dev, int32_t timeout_ms)
{
    ESP_ERROR_CHECK(dev->clear(dev, timeout_ms));     /*  Clear LED dev (turn off all LEDs) */
}
