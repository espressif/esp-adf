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
#include "audio_mem.h"
#include "audio_error.h"

#include "audio_player.h"
#include "audio_player_type.h"
#include "audio_player_manager.h"
#include "audio_player_helper.h"
#include "esp_audio_helper.h"

#include "raw_stream.h"
#include "http_stream.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "tone_stream.h"
#include "wav_decoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "amr_decoder.h"
#include "flac_decoder.h"
#include "ogg_decoder.h"
#include "opus_decoder.h"
#include "audio_hal.h"


static const char *TAG = "AP_HELPER";

#define ESP_AUDIO_DEFAULT_RESAMPLE_RATE  48000

static int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

esp_audio_handle_t setup_esp_audio_instance(esp_audio_cfg_t *cfg)
{
    if (cfg == NULL) {
        esp_audio_cfg_t default_cfg = DEFAULT_ESP_AUDIO_CONFIG();
        default_cfg.vol_handle = NULL;
        default_cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
        default_cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
        default_cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
        default_cfg.evt_que = NULL;
        ESP_LOGI(TAG, "stack:%d", default_cfg.task_stack);
        cfg = &default_cfg;
    }
    esp_audio_handle_t handle = esp_audio_create(cfg);
    AUDIO_MEM_CHECK(TAG, handle, return NULL;);
    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    fs_reader.task_prio = 12;

    tone_stream_cfg_t tn_reader = TONE_STREAM_CFG_DEFAULT();
    tn_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(handle, tone_stream_init(&tn_reader));
    esp_audio_input_stream_add(handle, fatfs_stream_init(&fs_reader));

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t esp_raw_handle = raw_stream_init(&raw_cfg);
    esp_audio_input_stream_add(handle, esp_raw_handle);
    // Set the raw handle to raw player helper
    ap_helper_raw_handle_set(esp_raw_handle);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_cfg.task_prio = 12;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);
    http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);

    // Add decoders and encoders to esp_audio
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_dec_cfg.task_prio = 12;
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    mp3_dec_cfg.task_prio = 12;
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_cfg.task_core = 1;
    aac_cfg.task_prio = 12;
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_cfg));
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_writer.i2s_config.sample_rate = 48000;
    esp_audio_output_stream_add(handle, i2s_stream_init(&i2s_writer));

    // Set default volume
    esp_audio_vol_set(handle, 60);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", handle);
    esp_audio_helper_set_instance(handle);
    return handle;
}

audio_err_t audio_player_helper_default_play(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, at, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    audio_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "%s, type:%x,url:%s,pos:%d,block:%d,auto:%d,mixed:%d,inter:%d", __func__,
             para->media_src, para->url, para->pos, at->blocked, at->auto_resume, at->mixed, at->interrupt);

    ret = ap_manager_play(para->url, para->pos, at->blocked, at->auto_resume, at->mixed, at->interrupt, para->media_src);
    return ret;
}

audio_err_t audio_player_helper_default_stop(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, at, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    ESP_LOGW(TAG, "default stop");

    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_stop(TERMINATION_TYPE_NOW);
    return ret;
}

audio_err_t audio_player_helper_default_pause(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, at, return ESP_ERR_AUDIO_INVALID_PARAMETER);

    audio_err_t ret = ESP_OK;
    ret = ap_manager_backup_audio_info();
    return ret;
}

audio_err_t audio_player_helper_default_resume(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, at, return ESP_ERR_AUDIO_INVALID_PARAMETER);

    audio_err_t ret = ESP_OK;
    ret =  ap_manager_restore_audio_info();
    ret |= ap_manager_resume_audio();
    return ret;
}

audio_err_t audio_player_helper_default_next(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    audio_err_t ret = ESP_OK;
    playlist_handle_t music_list = NULL;
    audio_player_playlist_get(&music_list);
    if (music_list == NULL) {
        ESP_LOGE(TAG, "%s, not found can be used playlist", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_PLAYLIST;
    }
    char *url = NULL;
    playlist_next(music_list, 1, &url);
    ret = audio_player_music_play(url, 0, MEDIA_SRC_TYPE_MUSIC_SD);
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}

audio_err_t audio_player_helper_default_prev(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    audio_err_t ret = ESP_OK;
    playlist_handle_t music_list = NULL;
    audio_player_playlist_get(&music_list);
    if (music_list == NULL) {
        ESP_LOGE(TAG, "%s, not found can be used playlist", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_PLAYLIST;
    }
    char *url = NULL;
    playlist_prev(music_list, 1, &url);
    ret = audio_player_music_play(url, 0, MEDIA_SRC_TYPE_MUSIC_SD);
    ESP_LOGI(TAG, "%s, %d", __func__, __LINE__);
    return ret;
}


audio_err_t audio_player_helper_default_seek(ap_ops_attr_t *at, ap_ops_para_t *para)
{
    AUDIO_NULL_CHECK(TAG, para, return ESP_ERR_AUDIO_INVALID_PARAMETER);

    audio_err_t ret = ESP_OK;
    ret = esp_audio_helper_seek(para->seek_time_sec);
    return ret;
}

audio_err_t default_http_player_init()
{
    ap_ops_t ops = {
        .play = audio_player_helper_default_play,
        .stop = audio_player_helper_default_stop,
        .pause = audio_player_helper_default_pause,
        .resume = audio_player_helper_default_resume,
        .next = audio_player_helper_default_next,
        .prev = audio_player_helper_default_prev,
        .seek = audio_player_helper_default_seek,
    };
    memset(&ops.para, 0, sizeof(ops.para));
    memset(&ops.attr, 0, sizeof(ops.attr));
    ops.para.media_src = MEDIA_SRC_TYPE_MUSIC_HTTP;
    audio_err_t ret = ap_manager_ops_add(&ops, 1);
    return ret;
}

audio_err_t default_flash_music_player_init()
{
    ap_ops_t ops = {
        .play = audio_player_helper_default_play,
        .stop = audio_player_helper_default_stop,
        .pause = audio_player_helper_default_pause,
        .resume = audio_player_helper_default_resume,
        .next = audio_player_helper_default_next,
        .prev = audio_player_helper_default_prev,
        .seek = audio_player_helper_default_seek,
    };
    memset(&ops.para, 0, sizeof(ops.para));
    memset(&ops.attr, 0, sizeof(ops.attr));
    ops.para.media_src = MEDIA_SRC_TYPE_MUSIC_FLASH;
    audio_err_t ret = ap_manager_ops_add(&ops, 1);
    return ret;
}

audio_err_t default_flash_tone_player_init()
{
    ap_ops_t ops = {
        .play = audio_player_helper_default_play,
        .stop = audio_player_helper_default_stop,
        .pause = NULL,
        .resume = NULL,
        .next = NULL,
        .prev = NULL,
        .seek = NULL,
    };
    memset(&ops.para, 0, sizeof(ops.para));
    memset(&ops.attr, 0, sizeof(ops.attr));
    ops.para.media_src = MEDIA_SRC_TYPE_TONE_FLASH;
    audio_err_t ret = ap_manager_ops_add(&ops, 1);
    return ret;
}
