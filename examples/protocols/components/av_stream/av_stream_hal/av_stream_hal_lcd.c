/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_log.h"
#include "av_stream_hal.h"

static const char *TAG = "AV_STREAM_HAL";

int av_stream_lcd_render(esp_lcd_panel_handle_t panel_handle, unsigned char *data)
{
    for (int y = 0; y < 240; y += 40) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y + 40, data + y * (320*2));
    }

    return ESP_OK;
}

esp_lcd_panel_handle_t av_stream_lcd_init(esp_periph_set_handle_t set)
{
    AUDIO_NULL_CHECK(TAG, set, return NULL);

#if !CONFIG_IDF_TARGET_ESP32
    return audio_board_lcd_init(set, NULL);
#else
    return NULL;
#endif
}

int av_stream_lcd_deinit(esp_lcd_panel_handle_t panel_handle)
{
    AUDIO_NULL_CHECK(TAG, panel_handle, return ESP_FAIL);
    return ESP_OK;
}
