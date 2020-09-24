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
#include "esp_peripherals.h"
#include "a2dp_stream.h"
#include "audio_player_type.h"
#include "audio_player_manager.h"
#include "esp_audio_helper.h"

static const char *TAG = "BT_HELPER";

static audio_element_handle_t a2dp_stream_hd;

audio_err_t ap_helper_a2dp_handle_set(void *handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    a2dp_stream_hd = handle;
    ESP_LOGI(TAG, "%s, %d, 0x%p, %p", __func__, __LINE__, a2dp_stream_hd, &a2dp_stream_hd);
    return ESP_OK;
}

audio_err_t ap_helper_a2dp_play(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para || at, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);

    int ret = ESP_OK;
#if CONFIG_BT_ENABLED
    ret = periph_bt_play((esp_periph_handle_t)para->ctx);
#endif
    ret = ap_manager_play(para->url, para->pos, at->blocked, at->auto_resume, at->mixed, at->interrupt, para->media_src);
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t ap_helper_a2dp_stop(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);

    int ret = ESP_OK;
    ESP_LOGI(TAG, "%s, %d, 0x%p", __func__, __LINE__, a2dp_stream_hd);
    ESP_LOGW(TAG, "a2dp_stop");
    audio_element_set_ringbuf_done(a2dp_stream_hd);
    audio_element_finish_state(a2dp_stream_hd);
    audio_element_stop(a2dp_stream_hd);
#if CONFIG_BT_ENABLED
    ret = periph_bt_pause((esp_periph_handle_t)para->ctx);
#endif
    ret = esp_audio_helper_stop(TERMINATION_TYPE_NOW);
    return ret;
}

audio_err_t ap_helper_a2dp_pause(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    int ret = ESP_OK;
#if CONFIG_BT_ENABLED
    ret = periph_bt_pause((esp_periph_handle_t)para->ctx);
#endif
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t ap_helper_a2dp_resume(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    int ret = ESP_OK;
#if CONFIG_BT_ENABLED
    ret = periph_bt_play((esp_periph_handle_t)para->ctx);
#endif
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t ap_helper_a2dp_next(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    int ret = ESP_OK;
#if CONFIG_BT_ENABLED
    ret = periph_bt_avrc_next((esp_periph_handle_t)para->ctx);
#endif
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t ap_helper_a2dp_prev(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    int ret = ESP_OK;
#if CONFIG_BT_ENABLED
    ret = periph_bt_avrc_prev((esp_periph_handle_t)para->ctx);
#endif
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t default_a2dp_player_init(void *periph)
{
    AUDIO_NULL_CHECK(TAG, a2dp_stream_hd || periph, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    ap_ops_t ops = {
        .play = ap_helper_a2dp_play,
        .stop = ap_helper_a2dp_stop,
        .pause = ap_helper_a2dp_pause,
        .resume = ap_helper_a2dp_resume,
        .next = ap_helper_a2dp_next,
        .prev = ap_helper_a2dp_prev,
        .seek = NULL,
    };
    memset(&ops.para, 0, sizeof(ops.para));
    memset(&ops.attr, 0, sizeof(ops.attr));
    ops.para.ctx = periph;
    ops.para.media_src = MEDIA_SRC_TYPE_MUSIC_A2DP;
    audio_err_t ret = ap_manager_ops_add(&ops, 1);
    return ret;
}
