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
#include "audio_player_type.h"
#include "audio_player_manager.h"
#include "audio_player_helper.h"
#include "raw_stream.h"

static const char *TAG = "RAW_HELPER";
static audio_element_handle_t raw_write_hd;

audio_err_t ap_helper_raw_handle_set(void *handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    raw_write_hd = handle;
    return ESP_OK;
}

audio_err_t ap_helper_raw_play(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    audio_err_t ret = ESP_OK;
    xQueueHandle que = para->ctx;
    ESP_LOGI(TAG, "ap_helper_raw_play,%p", que);
    ret = audio_player_helper_default_play(at, para);
    audio_player_state_t dummy;
    while (xQueueReceive(que, &dummy, 0) == pdTRUE);
    return ret;
}

audio_err_t audio_player_helper_raw_feed_data(void *buff, int buffer_length)
{
    audio_err_t ret = ESP_OK;
    AUDIO_NULL_CHECK(TAG, raw_write_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    ret = raw_stream_write(raw_write_hd, buff, buffer_length);
    return ret;
}

audio_err_t audio_player_helper_raw_feed_done()
{
    audio_err_t ret = ESP_OK;
    AUDIO_NULL_CHECK(TAG, raw_write_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_element_set_ringbuf_done(raw_write_hd);
    audio_element_finish_state(raw_write_hd);
    return ret;
}

audio_err_t audio_player_helper_raw_waiting_finished(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    audio_err_t ret = ESP_OK;
    xQueueHandle que = para->ctx;
    ESP_LOGI(TAG, "waiting raw finish [enter],que:%p", que);
    audio_player_state_t st = {0};
    audio_player_state_get(&st);
    if (st.media_src != MEDIA_SRC_TYPE_MUSIC_RAW) {
        ESP_LOGE(TAG, "It's not MUSIC RAW playing, media_src:%x", st.media_src);
        return ESP_ERR_AUDIO_NOT_SUPPORT;
    }
    while (1) {
        xQueueReceive(que, &st, portMAX_DELAY);
        ESP_LOGD(TAG, "waiting raw, media_src:%x, status:%d", st.media_src, st.status);
        if ((st.media_src == MEDIA_SRC_TYPE_MUSIC_RAW)
            && ((st.status == AUDIO_PLAYER_STATUS_STOPPED)
                || (st.status == AUDIO_PLAYER_STATUS_FINISHED)
                || (st.status == AUDIO_PLAYER_STATUS_ERROR))) {
            break;
        }
    }
    ESP_LOGI(TAG, "waiting raw finish [exit],%x", st.status);
    return ret;
}

audio_err_t ap_helper_raw_play_stop(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, raw_write_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    audio_element_abort_output_ringbuf(raw_write_hd);
    // audio_element_set_ringbuf_done(raw_write_hd);
    audio_element_finish_state(raw_write_hd);
    ret = audio_player_helper_default_stop(at, para);
    ESP_LOGW(TAG, "%s", __func__);

    return ret;
}

audio_err_t default_raw_player_init(void)
{
    ap_ops_t ops = {
        .play = ap_helper_raw_play,
        .stop = ap_helper_raw_play_stop,
        .pause = NULL,
        .resume = NULL,
        .next = NULL,
        .prev = NULL,
        .seek = NULL,
    };
    memset(&ops.para, 0, sizeof(ops.para));
    memset(&ops.attr, 0, sizeof(ops.attr));
    // TODO where to free the queue
    xQueueHandle que = xQueueCreate(3, sizeof(audio_player_state_t));
    AUDIO_MEM_CHECK(TAG, que, return ESP_ERR_AUDIO_MEMORY_LACK);
    ap_manager_event_register(que);
    ops.para.media_src = MEDIA_SRC_TYPE_MUSIC_RAW;
    ops.para.ctx = que;
    ESP_LOGI(TAG, "default_raw_player_init :%x, que:%p", ops.para.media_src, que);
    audio_err_t ret  = ap_manager_ops_add(&ops, 1);
    return ret;
}
