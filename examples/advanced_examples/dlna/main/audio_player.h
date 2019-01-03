/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#ifndef _AUDIO_PLAYER_H_
#define _AUDIO_PLAYER_H_

#include "esp_err.h"
#include "audio_event_iface.h"
#include "audio_element.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLAYER_EVENT_NONE = 0,
    PLAYER_EVENT_PLAY,
    PLAYER_EVENT_STOP,
    PLAYER_EVENT_PAUSE,
    PLAYER_EVENT_RESUME,
} player_event_t;

typedef struct audio_player* audio_player_handle_t;
typedef esp_err_t (*player_event)(audio_player_handle_t ap, player_event_t event);

typedef struct {
    int task_stack;
    int task_prio;
    player_event event_handler;
} audio_player_config_t;

audio_player_handle_t audio_player_init(audio_player_config_t *config);
esp_err_t audio_player_deinit(audio_player_handle_t ap);
esp_err_t audio_player_play(audio_player_handle_t ap, const char *url);
esp_err_t audio_player_stop(audio_player_handle_t ap);
esp_err_t audio_player_pause(audio_player_handle_t ap);
esp_err_t audio_player_resume(audio_player_handle_t ap);
int audio_player_vol(audio_player_handle_t ap, int vol_set);

int audio_player_time_played_seconds(audio_player_handle_t ap);
int audio_player_time_totals_seconds(audio_player_handle_t ap);


#ifdef __cplusplus
}
#endif

#endif
