/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _AUDIO_PLAYER_INT_TONE_H_
#define _AUDIO_PLAYER_INT_TONE_H_

#include "audio_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Initialize tone player
 *
 * @return
 *     - ESP_OK : on success
 *     - ESP_FAIL : other errors
 */
audio_err_t audio_player_int_tone_init(void);

/*
 * @brief Play tone
 *
 * @param url Always be url in audio_tone_uri.h
 *
 * @return
 *     - ESP_OK : on success
 *     - ESP_FAIL : other errors
 */
audio_err_t audio_player_int_tone_play(const char *url);

/*
 * @brief Stop play tone
 *
 * @return
 *     - ESP_OK : on success
 *     - ESP_FAIL : other errors
 */
audio_err_t audio_player_int_tone_stop(void);

#ifdef __cplusplus
}
#endif

#endif
