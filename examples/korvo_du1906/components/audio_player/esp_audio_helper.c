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

#include <string.h>
#include "esp_log.h"
#include "audio_error.h"
#include "esp_audio.h"
#include "audio_player.h"
#include "esp_audio_helper.h"

static const char *TAG = "EP_HELPER";

static esp_audio_handle_t esp_audio_instance;

audio_err_t esp_audio_helper_set_instance(void *hd)
{
    AUDIO_NULL_CHECK(TAG, hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    esp_audio_instance = hd;
    return ESP_OK;
}

audio_err_t esp_audio_helper_play(const char *uri, int pos, bool sync)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    int ret = ESP_OK;
    if (sync == true) {
        ret = esp_audio_sync_play(esp_audio_instance, uri, pos);
    } else {
        ret = esp_audio_play(esp_audio_instance, AUDIO_CODEC_TYPE_DECODER, uri, pos);
    }
    return ret;
}

audio_err_t esp_audio_helper_stop(audio_termination_type_t type)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);

    int ret = ESP_OK;
    ret = esp_audio_stop(esp_audio_instance, type);
    return ret;
}

audio_err_t esp_audio_helper_pause()
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);

    int ret = ESP_OK;
    ret = esp_audio_pause(esp_audio_instance);
    return ret;
}

audio_err_t esp_audio_helper_resume()
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);

    int ret = ESP_OK;
    ret = esp_audio_resume(esp_audio_instance);
    return ret;
}

audio_err_t esp_audio_helper_seek(int seek_time_sec)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_seek(esp_audio_instance, seek_time_sec);
    return ret;
}

audio_err_t esp_audio_helper_duration_get(int *duration)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_duration_get(esp_audio_instance, duration);
    return ret;
}

audio_err_t esp_audio_helper_state_get(esp_audio_state_t *state)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret =  esp_audio_state_get(esp_audio_instance, state);
    return ret;
}

audio_err_t esp_audio_helper_media_type_set(media_source_type_t type)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_media_type_set(esp_audio_instance, type);
    return ret;
}

audio_err_t esp_audio_helper_media_src_get(media_source_type_t *type)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    esp_audio_state_t st = {0};
    audio_err_t ret = esp_audio_state_get(esp_audio_instance, &st);
    *type = st.media_src;
    return ret;
}

audio_err_t esp_audio_helper_time_get(int *time)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret =  esp_audio_time_get(esp_audio_instance, time);
    return ret;
}

audio_err_t esp_audio_helper_pos_get(int *pos)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_pos_get(esp_audio_instance, pos);
    return ret;
}

audio_err_t esp_audio_helper_vol_get(int *vol)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_vol_get(esp_audio_instance, vol);
    return ret;
}

audio_err_t esp_audio_helper_vol_set(int vol)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_vol_set(esp_audio_instance, vol);
    return ret;
}

audio_err_t esp_audio_helper_play_timeout_set(int time_ms)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_play_timeout_set(esp_audio_instance, time_ms);
    return ret;
}

audio_err_t esp_audio_helper_prefer_type_get(esp_audio_prefer_t *type)
{
    AUDIO_NULL_CHECK(TAG, esp_audio_instance, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = esp_audio_prefer_type_get(esp_audio_instance, type);
    return ret;
}
