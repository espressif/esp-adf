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

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_event_iface.h"
#include "audio_player.h"
#include "esp_log.h"
#include "esp_audio.h"
#include "audio_player_setup_common.h"
#include "audio_player_common.h"

static const char *TAG = "AUDIO_PLAY_COM";

const char *dec_uri[] = {
    "file://sdcard/ut/测试UTF8.mp3",
    "file://sdcard/ut/test.mp3",
    "file://sdcard/ut/test.wav",
    "file://sdcard/ut/test.aac",
    "file://sdcard/ut/test.m4a",
    "file://sdcard/ut/test.ts",
    "file://sdcard/ut/test.amr",
    "file://sdcard/ut/test.flac",
    "file://sdcard/ut/test.ogg",
    "file://sdcard/ut/test.opus",
    "http://dl.espressif.com/dl/audio/adf_music.mp3",
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.aac",
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3",
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.m4a",
    "http://open.ls.qingting.fm/live/274/64k.m3u8?format=aac",
    "file://sdcard/ut/test.mp3#raw",
    "raw://sdcard/ut/test.mp3",
    "aadp://44100:2@bt/sink/stream.pcm",
};

const char *dec_err_uri[] = {
    "file://sdcard/ut//err/test.mp3",
    "file://sdcard/ut/测试UTF8_err.mp3",
    "file://sdcard/ut/test_err.wav",
    "file://sdcard/ut/test_err.aac",
    "file://sdcard/ut/test_err.m4a",
    "file://sdcard/ut/test_err.ts",
    "file://sdcard/ut/test_err.amr",
    "file://sdcard/ut/test_err.flac",
    "file://sdcard/ut/test_err.ogg",
    "file://sdcard/ut/test_err.opus",
    "http://dl.espressif.com/dl/audio/adf_music_err.mp3",
    "http://iot.espressif.com:8008/file/44k_2ch_emily_en.m4a",
    "http://iot.espressif.com:8008/file/44k_2ch_sbr_dec.aac",
    "http://open.ls.qingting.fm/live/274/64k.m3u8?format=aac",
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3",
};

static void opt_task(void *para)
{
    opt_para_t *opt = (opt_para_t *) para;
    int delay = opt->ticks_to_wait_ms / portTICK_RATE_MS;
    ESP_LOGW(TAG, "Delay %d", delay);
    vTaskDelay(delay);
    opt->cb(opt->ctx);
    vTaskDelete(NULL);
}

void request_opt(opt_para_t *p)
{
    ESP_LOGW(TAG, "request_opt %p", p);
    if (xTaskCreate(opt_task, "opt_task", 3072, (void *)p, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ERROR creating opt_task task! Out of memory?");
    }
}

void ut_tone_unblock_no_resume_play (void *ctx)
{
    audio_player_tone_play("flash://tone/11_please_setting_wifi.mp3", false, false, MEDIA_SRC_TYPE_TONE_FLASH);
}

void ut_tone_block_no_resume_play (void *ctx)
{
    audio_player_tone_play("flash://tone/12_please_setting_wifi.mp3", true, false, MEDIA_SRC_TYPE_TONE_FLASH);
}

void ut_tone_unblock_resume_play (void *ctx)
{
    audio_player_tone_play("flash://tone/10_please_setting_wifi.mp3", false, true, MEDIA_SRC_TYPE_TONE_FLASH);
}

void ut_tone_block_resume_play (void *ctx)
{
    audio_player_tone_play("flash://tone/9_please_setting_wifi.mp3", true, true, MEDIA_SRC_TYPE_TONE_FLASH);
}

void ut_audio_player_raw_play (void *ctx)
{
    raw_write(ctx);
}

void ut_audio_player_stop (void *ctx)
{
    audio_player_stop();
}

void ut_audio_player_raw_stop (void *ctx)
{
    raw_stop();
    audio_player_stop();
}

void async_sync_play (void *ctx)
{
    async_play_para_t *player = (async_play_para_t *) ctx;
    ESP_LOGE(TAG, "uri:%s", player->uri);
    esp_audio_sync_play(player->handle, player->uri, player->pos);
}

void uri_list_play_stop_opt(esp_audio_handle_t handle, QueueHandle_t que, async_opt_cb cb, const char **uri)
{
    esp_audio_state_t st = {0};
    int i = 0;
    while (i != (TEST_DEC_URI_MAX - 2)) {
#if 0
        if (/*i== 2 ||
            i==3 ||
            i==4 ||
            i==5 ||
            i==6 ||*/
            // i == 7 ||
            // i == 8 ||
            // i == 9 ||
            //      i == TEST_DEC_URI_LIVE_AAC_I2S ) {
            //     i++;
            //     continue;
            // }
#endif
            esp_audio_play(handle, AUDIO_CODEC_TYPE_DECODER, uri[i++], 0);
        opt_para_t para = {
        .cb = cb,
        .ctx = handle,
        .ticks_to_wait_ms = esp_random() % 10000,
        };
        request_opt(&para);
        while (1) {
        xQueueReceive(que, &st, portMAX_DELAY);
            if (st.status == AUDIO_STATUS_STOPPED
                || st.status == AUDIO_STATUS_ERROR
                || st.status == AUDIO_STATUS_FINISHED) {
                break;
            } else {
                ESP_LOGE(TAG, "player status:%d", st.status);
            }
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
        show_task_list();
    }
}

void uri_list_sync_play_stop_opt(esp_audio_handle_t handle, QueueHandle_t que, const char **uri)
{
    int i = 0;
    esp_audio_state_t st = {0};
    while (i != (TEST_DEC_URI_MAX - 2)) {
        async_play_para_t instance = {
            .handle = handle,
            .uri = uri[i++],
            .pos = 0,
        };
        opt_para_t para = {
            .cb = async_sync_play,
            .ctx = &instance,
            .ticks_to_wait_ms = 0,
        };
        request_opt(&para);
        vTaskDelay(100 / portTICK_RATE_MS);

        para.cb = ut_audio_player_stop;
        para.ctx = handle;
        para.ticks_to_wait_ms = esp_random() % 10000;
        request_opt(&para);

        while (1) {
            xQueueReceive(que, &st, portMAX_DELAY);
            if (st.status == AUDIO_STATUS_STOPPED
                || st.status == AUDIO_STATUS_ERROR
                || st.status == AUDIO_STATUS_FINISHED) {
                break;
            } else {
                ESP_LOGE(TAG, "player status:%d", st.status);
            }
        }
    }
}