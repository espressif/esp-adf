/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "lightduer_dcs.h"
#include "duer_audio_action.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "esp_action_def.h"
#include "esp_audio.h"
#include "esp_player_wrapper.h"

static const char *TAG = "DCS_WRAPPER";

esp_err_t duer_dcs_action_vol_set(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s,vol:%d", __func__, (int)arg->data);
    esp_player_vol_set((int)arg->data);
    return ESP_OK;
}

esp_err_t duer_dcs_action_vol_adj(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s, adj_volume:%d", __func__, (int)arg->data);
    esp_player_vol_set((int)arg->data);
    return ESP_OK;
}

esp_err_t duer_dcs_action_mute_on(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s", __func__);
    return ESP_OK;
}

esp_err_t duer_dcs_action_mute_off(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s", __func__);
    return ESP_OK;
}

esp_err_t duer_dcs_action_get_state(void *instance, action_arg_t *arg, action_result_t *result)
{
    int *vol = audio_calloc(1, sizeof(int));
    AUDIO_MEM_CHECK(TAG, vol, {
        result->data = 0;
        result->len = 0;
        result->err = ESP_FAIL;
        return ESP_FAIL;
    });
    esp_audio_vol_get((esp_audio_handle_t)instance, vol);
    result->data = vol;
    result->len = sizeof(int);
    result->err = ESP_OK;
    ESP_LOGI(TAG, "%s, vol:%d, %p ", __func__, *vol, result->data);
    return ESP_OK;
}

esp_err_t duer_dcs_action_speak(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s, %p, %s, %d", __func__, arg->data, (char *)arg->data, arg->len);
    esp_player_music_play((char *)arg->data, 0, MEDIA_SRC_TYPE_DUER_SPEAK);
    return ESP_OK;
}

esp_err_t duer_dcs_action_audio_play(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s, %p, %s, %d", __func__, arg->data, (char *)arg->data, arg->len);
    duer_dcs_audio_info_t *info = (duer_dcs_audio_info_t *)arg->data;
    int ret = esp_player_music_play((char *)info->url, info->offset, MEDIA_SRC_TYPE_DUER_MUSIC);
    return ret;
}

esp_err_t duer_dcs_action_audio_stop(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s", __func__);
    return esp_player_music_stop();
}

esp_err_t duer_dcs_action_audio_pause(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGE(TAG, "%s", __func__);
    return esp_player_music_pause();
}

esp_err_t duer_dcs_action_audio_resume(void *instance, action_arg_t *arg, action_result_t *result)
{
    ESP_LOGI(TAG, "%s, %p, %s, %d", __func__, arg->data, (char *)arg->data, arg->len);
    duer_dcs_audio_info_t *info = (duer_dcs_audio_info_t *)arg->data;
    int ret = esp_player_music_play((char *)info->url, info->offset, MEDIA_SRC_TYPE_DUER_MUSIC);
    return ret;
}

esp_err_t duer_dcs_action_get_progress(void *instance, action_arg_t *arg, action_result_t *result)
{
    int *pos = audio_calloc(1, sizeof(int));
    AUDIO_MEM_CHECK(TAG, pos, {
        result->data = 0;
        result->len = 0;
        result->err = ESP_FAIL;
        return ESP_FAIL;
    });
    esp_player_pos_get(pos);
    result->data = pos;
    result->len = sizeof(int);
    result->err = ESP_OK;
    ESP_LOGI(TAG, "%s, pos:%d, %p ", __func__, *pos, result->data);
    return ESP_OK;
}
