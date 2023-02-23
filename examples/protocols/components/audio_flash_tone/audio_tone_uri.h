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

#ifndef __AUDIO_TONEURI_H__
#define __AUDIO_TONEURI_H__

#ifdef __cplusplus
extern "C" {
#endif

extern const char* tone_uri[];

typedef enum {
    TONE_TYPE_ALARM,
    TONE_TYPE_UNDER_SMARTCONFIG,
    TONE_TYPE_WIFI_RECONNECT,
    TONE_TYPE_WIFI_SUCCESS,
    TONE_TYPE_PLEASE_SETTING_WIFI,
    TONE_TYPE_SERVER_CONNECT,
    TONE_TYPE_SERVER_DISCONNECT,
    TONE_TYPE_MAX,
} tone_type_t;

int get_tone_uri_num();

#ifdef __cplusplus
}
#endif

#endif
