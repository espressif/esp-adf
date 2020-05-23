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
#include "audio_player_helper.h"
#include "audio_player_type.h"
#include "audio_player_manager.h"
#include "sdcard_list.h"
#include "dram_list.h"
#include "sdcard_scan.h"

static const char *TAG = "SD_HELPER";

static void sdcard_url_save_cb(void *user_data, char *url)
{
    playlist_operator_handle_t sdcard_handle = (playlist_operator_handle_t)user_data;
    esp_err_t ret = sdcard_list_save(sdcard_handle, url);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save sdcard url to sdcard playlist");
    }
}

esp_err_t ap_helper_scan_sdcard(playlist_operator_handle_t list, const char *path)
{
    sdcard_list_reset(list);
    sdcard_scan(sdcard_url_save_cb, path,
    0, (const char *[]) {"mp3", "m4a", "flac", "ogg", "opus", "amr", "ts", "aac", "wav"}, 9, list);
    sdcard_list_show(list);

    return ESP_OK;
}

audio_err_t default_sdcard_player_init(const char *scan_path)
{
    ESP_LOGI(TAG, "default_sdcard_player_init");
    playlist_handle_t music_list = NULL;
    audio_player_playlist_get(&music_list);
    if (music_list == NULL) {
        ESP_LOGI(TAG, "default_sdcard_player_init  playlist_create");
        music_list = playlist_create();
    }
    audio_player_playlist_set(music_list);
    // playlist_operator_handle_t sdcard_handle = NULL;
    // sdcard_list_create(&sdcard_handle);
    // playlist_add(music_list, sdcard_handle, 0);
    // ap_helper_scan_sdcard(sdcard_handle, scan_path);

    playlist_operator_handle_t dram_handle = NULL;
    dram_list_create(&dram_handle);
    playlist_add(music_list, dram_handle, 0);

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
    ops.para.ctx = music_list;
    ops.para.media_src = MEDIA_SRC_TYPE_MUSIC_SD;
    audio_err_t ret  = ap_manager_ops_add(&ops, 1);
    return ret;
}