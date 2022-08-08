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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_color.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))

static const char *TAG = "ESP_COLOR";

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

esp_err_t esp_color_hsv2rgb(esp_color_hsv_t *hsv, esp_color_rgb_t *rgb)
{
    uint8_t  red   = (uint8_t)rgb->r;
    uint8_t  green = (uint8_t)rgb->g;
    uint8_t  blue  = (uint8_t)rgb->b;
    uint16_t hue   = hsv->h;
    uint8_t  value = hsv->v;
    uint8_t  saturation = hsv->s;
    uint16_t hi = (hue / 60) % 6;
    uint16_t F = 100 * hue / 60 - 100 * hi;
    uint16_t P = value * (100 - saturation) / 100;
    uint16_t Q = value * (10000 - F * saturation) / 10000;
    uint16_t T = value * (10000 - saturation * (100 - F)) / 10000;

    switch (hi) {
        case 0:
            red   = value;
            green = T;
            blue  = P;
            break;

        case 1:
            red   = Q;
            green = value;
            blue  = P;
            break;

        case 2:
            red   = P;
            green = value;
            blue  = T;
            break;

        case 3:
            red   = P;
            green = Q;
            blue  = value;
            break;

        case 4:
            red   = T;
            green = P;
            blue  = value;
            break;

        case 5:
            red   = value;
            green = P;
            blue  = Q;
            break;

        default:
            return ESP_FAIL;
    }

    rgb->r = red * 255 / 100;
    rgb->b = blue * 255 / 100;
    rgb->g = green * 255 / 100;

    return ESP_OK;
}

esp_err_t esp_color_rgb2hsv(esp_color_rgb_t *rgb, esp_color_hsv_t *hsv)
{
    uint16_t red = rgb->r;
    uint16_t green = rgb->g;
    uint16_t blue = rgb->b;
    double hue, saturation, value;
    double m_max = MAX(red, MAX(green, blue));
    double m_min = MIN(red, MIN(green, blue));
    double m_delta = m_max - m_min;

    value = m_max / 255.0;

    if (m_delta == 0) {
        hue = 0;
        saturation = 0;
    } else {
        saturation = m_delta / m_max;

        if (red == m_max) {
            hue = (green - blue) / m_delta;
        } else if (green == m_max) {
            hue = 2 + (blue - red) / m_delta;
        } else {
            hue = 4 + (red - green) / m_delta;
        }
        hue = hue * 60;

        if (hue < 0) {
            hue = hue + 360;
        }
    }

    hsv->h = (int)(hue + 0.5);
    hsv->s = (int)(saturation * 100 + 0.5);
    hsv->v = (int)(value * 100 + 0.5);
    return ESP_OK;
}

esp_err_t esp_color_code2rgb(uint32_t colorcode, esp_color_rgb_t *color)
{
    color->r = (uint8_t)((colorcode >> 16) & 0xFF);
    color->g = (uint8_t)((colorcode >>  8) & 0xFF);
    color->b = (uint8_t)((colorcode >>  0) & 0xFF);
    return ESP_OK;
}

esp_err_t esp_color_update_rainbow(esp_color_rgb_t *color)
{
    static uint16_t hue = 0;
    if (hue >= 3600) {
        hue = 0;
    }
    esp_color_hsv_t hsv;
    if ((hue % 10) == 0)  {
        hsv.h = hue / 10;
        hsv.s = 100;
        hsv.v = 30;
        esp_color_hsv2rgb(&hsv, color);
    }
    ESP_LOGD(TAG, "%d %d %d\n", color->g, color->r, color->b);
    hue ++;
    return ESP_OK;
}

#endif
