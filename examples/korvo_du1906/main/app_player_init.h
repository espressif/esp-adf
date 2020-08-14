/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __APP_SYS_TOOLS_H__
#define __APP_SYS_TOOLS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "a2dp_stream.h"
#include "esp_peripherals.h"
#include "audio_player_type.h"
#include "freertos/queue.h"

/**
 * @brief Initializes audio player
 *
 * @param[in] que       A specific queue to receive player events
 * @param[in] cb        A specific callback functions to receive player events
 * @param[in] set       A specific peripheral set handle
 * @param[in] a2dp_cb   A specific callback functions to receive A2DP events
 *
 * @return
 *      - ESP_OK: on success
        - Others: fail
 *
 */
esp_err_t app_player_init(QueueHandle_t que, audio_player_evt_callback cb, esp_periph_set_handle_t set, void* a2dp_cb);

#ifdef __cplusplus
}
#endif

#endif
