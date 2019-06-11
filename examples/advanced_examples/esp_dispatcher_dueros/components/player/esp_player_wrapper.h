/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef ESP_PLAYER_WRAPPER_H
#define ESP_PLAYER_WRAPPER_H

#include "esp_audio.h"

/*
 * init audio wrapper
 *
 * @param cb  audio wrapper callback function
 *
 * @return ESP_OK or ESP_FAIL
 *
 */
audio_err_t esp_player_init(esp_audio_handle_t handle);

/*
 * deinit audio wrapper
 *
 * @param none
 *
 * @return ESP_OK or ESP_FAIL
 *
 */
audio_err_t esp_player_deinit();

audio_err_t esp_player_music_play(const char *url, int pos, media_source_type_t type);

audio_err_t esp_player_http_music_play(const char *url, int pos);

audio_err_t esp_player_sdcard_music_play(const char *url, int pos);

audio_err_t esp_player_flash_music_play(const char *url, int pos);

audio_err_t esp_player_music_stop();

audio_err_t esp_player_music_pause();

audio_err_t esp_player_music_resume();

audio_err_t esp_player_tone_play(const char *url, bool blocked, media_source_type_t type);

audio_err_t esp_player_tone_stop();


audio_err_t esp_player_state_get(esp_audio_state_t *state);

media_source_type_t esp_player_media_src_get();

bool esp_player_is_backup();

audio_err_t esp_player_time_get(int *time);

audio_err_t esp_player_pos_get(int *pos);
audio_err_t esp_player_vol_get(int *vol);
audio_err_t esp_player_vol_set(int vol);


#endif //
