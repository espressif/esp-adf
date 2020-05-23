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
#include "audio_player.h"
#include "audio_player_helper.h"
#include "esp_audio_helper.h"
#include "audio_player_manager.h"

static const char *TAG = "AUDIO_PLAYER";

audio_err_t audio_player_init(audio_player_cfg_t *cfg)
{
    AUDIO_NULL_CHECK(TAG, cfg, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ap_manager_init(cfg);
    return ret;
}

audio_err_t audio_player_deinit(void)
{
    audio_err_t ret = ap_manager_deinit();
    return ret;
}

audio_err_t audio_player_ops_add(const ap_ops_t *ops, uint8_t cnt_of_ops)
{
    AUDIO_NULL_CHECK(TAG, ops, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ap_manager_ops_add(ops, cnt_of_ops);
    return ret;
}

audio_err_t audio_player_music_play(const char *url, uint32_t pos, media_source_type_t type)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_find_ops_by_src(type);
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the media source type:%x", __func__, type);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    ap_ops_attr_t attr = {
        .blocked = false,
        .auto_resume = false,
        .interrupt = false,
        .mixed = false,
    };
    ap_ops_para_t para = {
        .pos = (int)pos,
        .url = (char *)url,
        .media_src = type,
        .ctx = cur_ops->para.ctx,
    };

    if (cur_ops->stop) {
        ESP_LOGI(TAG, "music play, type:%x, cur media type:%x", type, cur_ops->para.media_src);
        ret = cur_ops->play(&attr, &para);
    }
    return ret;
}

audio_err_t audio_player_tone_play(const char *url, bool blocked, bool auto_resume, media_source_type_t type)
{
    AUDIO_NULL_CHECK(TAG, url, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_find_ops_by_src(type);
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the media source type:%x", __func__, type);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    ap_ops_attr_t attr = {
        .blocked = blocked,
        .auto_resume = auto_resume,
        .interrupt = true,
        .mixed = false,
    };
    ap_ops_para_t para = {
        .pos = 0,
        .url = (char *)url,
        .media_src = type,
        .ctx = cur_ops->para.ctx,
    };
    if (cur_ops->stop) {
        ESP_LOGI(TAG, "tone play, type:%x, cur media type:%x", type, cur_ops->para.media_src);
        ret = cur_ops->play(&attr, &para);
    }
    return ret;
}

audio_err_t audio_player_raw_feed_data(const uint8_t *buff, int buff_len)
{
    audio_err_t ret = ESP_OK;
    ret = audio_player_helper_raw_feed_data((char *)buff, buff_len);
    return ret;
}

audio_err_t audio_player_raw_feed_finish(void)
{
    audio_err_t ret = ESP_OK;
    ret = audio_player_helper_raw_feed_done();
    return ret;
}

audio_err_t audio_player_raw_waiting_finished(void)
{
    audio_err_t ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    audio_player_helper_raw_waiting_finished(&cur_ops->attr, &cur_ops->para);
    return ret;
}

audio_err_t audio_player_stop(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->stop) {
        ESP_LOGI(TAG, "stop, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->stop(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_pause(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->pause) {
        ESP_LOGI(TAG, "pause, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->pause(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_resume(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    ESP_LOGI(TAG, "resume, cur media type:%x", cur_ops->para.media_src);
    if (cur_ops->resume) {
        ESP_LOGI(TAG, "resume, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->resume(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_next(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->next) {
        ESP_LOGI(TAG, "next, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->next(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_prev(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->prev) {
        ESP_LOGI(TAG, "prev, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->prev(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_seek(int seek_time_sec)
{
    audio_err_t ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->seek) {
        cur_ops->para.seek_time_sec = seek_time_sec;
        ESP_LOGI(TAG, "prev, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->seek(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t audio_player_duration_get(int *duration)
{
    AUDIO_NULL_CHECK(TAG, duration, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_duration_get(duration);
    return ret;
}

audio_err_t audio_player_state_get(audio_player_state_t *state)
{
    AUDIO_NULL_CHECK(TAG, state, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_state_get((esp_audio_state_t *)state);
    return ret;
}

audio_err_t audio_player_media_type_set(media_source_type_t type)
{
    audio_err_t ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_find_ops_by_src(type);
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the media source type:%x", __func__, type);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    return ret;
}

audio_err_t audio_player_media_src_get(media_source_type_t *type)
{
    AUDIO_NULL_CHECK(TAG, type, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = esp_audio_helper_media_src_get(type);
    return ret;
}

audio_err_t audio_player_time_get(int *time)
{
    AUDIO_NULL_CHECK(TAG, time, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_time_get(time);
    return ret;
}

audio_err_t audio_player_pos_get(int *pos)
{
    AUDIO_NULL_CHECK(TAG, pos, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_pos_get(pos);
    return ret;
}

audio_err_t audio_player_vol_get(int *vol)
{
    AUDIO_NULL_CHECK(TAG, vol, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_vol_get(vol);
    return ret;
}

audio_err_t audio_player_vol_set(int vol)
{
    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_vol_set(vol);
    return ret;
}

audio_err_t audio_player_playlist_set(playlist_handle_t pl)
{
    AUDIO_NULL_CHECK(TAG, pl, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = ap_manager_set_music_playlist(pl);
    return ret;
}

audio_err_t audio_player_playlist_get(playlist_handle_t *pl)
{
    AUDIO_NULL_CHECK(TAG, pl, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = ap_manager_get_music_playlist(pl);
    return ret;
}

audio_err_t audio_player_callback_set(audio_player_evt_callback cb, void *cb_ctx)
{
    audio_err_t ret = ESP_OK;
    ret = ap_manager_set_callback(cb, cb_ctx);
    return ret;
}

audio_err_t audio_player_mode_set(audio_player_mode_t mode)
{
    audio_err_t ret = ESP_OK;
    ret = ap_manager_set_mode(mode);
    return ret;
}

audio_err_t audio_player_mode_get(audio_player_mode_t *mode)
{
    AUDIO_NULL_CHECK(TAG, mode, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ret = ap_manager_get_mode(mode);
    return ret;
}
