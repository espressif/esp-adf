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
#ifndef _SIP_SERVICE_H
#define _SIP_SERVICE_H

#include "audio_tone_uri.h"
#include "audio_player_int_tone.h"
#include "esp_peripherals.h"
#include "av_stream.h"
#include "esp_rtc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      Start a sip service
 *
 * @param[in]  av_stream    The av_stream handle
 * @param[in]  uri          SIP uri "Transport://user:pass@server:port/path"
 *
 * @return
 *     - The sip handle if successfully created, NULL on error
 */
esp_rtc_handle_t sip_service_start(av_stream_handle_t av_stream, const char *uri);

/**
 * @brief      Stop sip service
 *
 * @param[in]  esp_sip   The sip handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL on wrong handle
 */
int sip_service_stop(esp_rtc_handle_t esp_sip);

#ifdef __cplusplus
}
#endif

#endif