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

#ifndef ESP_AUDIO_HELPER_H
#define ESP_AUDIO_HELPER_H

#include "audio_player_type.h"
#include "audio_def.h"

#ifdef __cplusplus
extern "C" {
#endif

void *esp_audio_helper_get_raw_handle(void);

audio_err_t esp_audio_helper_set_instance(void *hd);

audio_err_t esp_audio_helper_play(const char *uri, int pos, bool sync);

audio_err_t esp_audio_helper_stop(audio_termination_type_t type);

audio_err_t esp_audio_helper_pause();

audio_err_t esp_audio_helper_resume();

audio_err_t esp_audio_helper_seek(int seek_time_sec);

audio_err_t esp_audio_helper_duration_get(int *duration);

audio_err_t esp_audio_helper_state_get(esp_audio_state_t *state);

audio_err_t esp_audio_helper_media_type_set(media_source_type_t type);

audio_err_t esp_audio_helper_media_src_get(media_source_type_t *type);

audio_err_t esp_audio_helper_time_get(int *time);

audio_err_t esp_audio_helper_pos_get(int *pos);

audio_err_t esp_audio_helper_vol_get(int *vol);

audio_err_t esp_audio_helper_vol_set(int vol);

audio_err_t esp_audio_helper_play_timeout_set(int time_ms);

audio_err_t esp_audio_helper_prefer_type_get(esp_audio_prefer_t *type);

#ifdef __cplusplus
}
#endif

#endif //
