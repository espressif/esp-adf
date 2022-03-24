
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

#ifndef AUDIO_EMBED_TONE_URI_H
#define AUDIO_EMBED_TONE_URI_H

typedef struct {
    const uint8_t * address;
    int size;
} embed_tone_t;

extern const uint8_t alarm_mp3[] asm("_binary_alarm_mp3_start");
extern const uint8_t new_message_mp3[] asm("_binary_new_message_mp3_start");

/**
 * @brief embed tone corresponding resource information, as a variable of the `embed_flash_stream_set_context` function
 */
embed_tone_t g_embed_tone[] = {
    [0] = {
        .address = alarm_mp3,
        .size    = 36018,
        },
    [1] = {
        .address = new_message_mp3,
        .size    = 22284,
        },
};

/**
 * @brief embed tone url index for `embed_tone_url` array
 */
enum tone_url_e {
    ALARM_MP3 = 0,
    NEW_MESSAGE_MP3 = 1,
    EMBED_TONE_URL_MAX = 2,
};


/**
 * @brief embed tone url
 */
const char * embed_tone_url[] = {
    "embed://tone/0_alarm.mp3",
    "embed://tone/1_new_message.mp3",
};

#endif // AUDIO_EMBED_TONE_URI_H