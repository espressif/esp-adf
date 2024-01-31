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
#include "audio_mem.h"
#include "audio_player_helper.h"
#include "esp_audio_helper.h"
#include "audio_player_manager.h"
#include "audio_pipeline.h"
#include "tone_stream.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "filter_resample.h"

static const char *TAG = "PLAYER_INT_TONE";

#define DEFAULT_MAX_ELEMENT_NUM (4)
#define DEFAULT_RINGBUF_SIZE (4 * 1024)

typedef struct {
    SemaphoreHandle_t sem_mutex;
    audio_event_iface_handle_t evt;
    audio_element_handle_t element[DEFAULT_MAX_ELEMENT_NUM];
    ringbuf_handle_t ringbuffer[DEFAULT_MAX_ELEMENT_NUM - 1];
} audio_player_int_tone_t;

audio_element_handle_t app_player_get_i2s_handle(void);

audio_player_int_tone_t *g_int_tone_handle;
audio_err_t audio_player_int_tone_init(void)
{
    g_int_tone_handle = audio_calloc(1, sizeof(audio_player_int_tone_t));

    AUDIO_NULL_CHECK(TAG, g_int_tone_handle, return ESP_FAIL);
    g_int_tone_handle->sem_mutex = xSemaphoreCreateMutex();

    tone_stream_cfg_t tone_cfg = TONE_STREAM_CFG_DEFAULT();
    tone_cfg.type = AUDIO_STREAM_READER;
    tone_cfg.task_prio = 17;
    g_int_tone_handle->element[0] = tone_stream_init(&tone_cfg);

    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_cfg.task_prio = 16;
    g_int_tone_handle->element[1] = mp3_decoder_init(&mp3_cfg);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.dest_rate = 48000;
    rsp_cfg.dest_ch = 2;
    rsp_cfg.task_prio = 15;
    g_int_tone_handle->element[2] = rsp_filter_init(&rsp_cfg);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.uninstall_drv = false;
    g_int_tone_handle->element[3] = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(g_int_tone_handle->element[3], 48000, 16, 2);

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    g_int_tone_handle->evt = audio_event_iface_init(&evt_cfg);
    for (int i = 0; i < DEFAULT_MAX_ELEMENT_NUM; i++) {
        audio_element_msg_set_listener(g_int_tone_handle->element[i], g_int_tone_handle->evt);
    }
    for (int i = 0; i < (DEFAULT_MAX_ELEMENT_NUM - 1); i++) {
        g_int_tone_handle->ringbuffer[i] = rb_create(DEFAULT_RINGBUF_SIZE, 1);
    }
    return ESP_OK;
}

audio_element_err_t i2s_write_idle_cb(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    vTaskDelay(1 / portTICK_PERIOD_MS);
    return ESP_OK;
}


audio_err_t audio_player_int_tone_play(char *url)
{
    AUDIO_NULL_CHECK(TAG, g_int_tone_handle, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, url, return ESP_FAIL);
    if (!strstr(url, "mp3") && !strstr(url, "MP3")) {
        ESP_LOGE(TAG, "For now, this API only support for MP3 type of tone");
        return ESP_FAIL;
    }

    audio_element_handle_t g_player_i2s_write_handle = app_player_get_i2s_handle();
    if (g_player_i2s_write_handle == NULL) {
        ESP_LOGE(TAG, "Fail to get i2s handle, maybe the player hasn't been initialized");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Start play int tone (add reset state), get handle: %p", g_int_tone_handle);
    xSemaphoreTake(g_int_tone_handle->sem_mutex, portMAX_DELAY);
    ESP_LOGI(TAG, "Play intterupt tone URL: %s", url);
    audio_element_set_uri(g_int_tone_handle->element[0], url);

    audio_element_set_output_ringbuf(g_int_tone_handle->element[0], g_int_tone_handle->ringbuffer[0]);
    audio_element_set_input_ringbuf(g_int_tone_handle->element[1], g_int_tone_handle->ringbuffer[0]);

    audio_element_set_output_ringbuf(g_int_tone_handle->element[1], g_int_tone_handle->ringbuffer[1]);
    audio_element_set_input_ringbuf(g_int_tone_handle->element[2], g_int_tone_handle->ringbuffer[1]);

    audio_element_set_output_ringbuf(g_int_tone_handle->element[2], g_int_tone_handle->ringbuffer[2]);
    audio_element_set_input_ringbuf(g_int_tone_handle->element[3], g_int_tone_handle->ringbuffer[2]);

    audio_element_run(g_int_tone_handle->element[0]);
    audio_element_run(g_int_tone_handle->element[1]);

    ESP_LOGI(TAG, "Get state before resume: flash: %d, mp3: %d", audio_element_get_state(g_int_tone_handle->element[0]), audio_element_get_state(g_int_tone_handle->element[1]));
    audio_element_reset_state(g_int_tone_handle->element[0]);
    audio_element_reset_state(g_int_tone_handle->element[1]);

    audio_element_resume(g_int_tone_handle->element[0], 0, 10 / portTICK_PERIOD_MS);
    audio_element_resume(g_int_tone_handle->element[1], 0, 10 / portTICK_PERIOD_MS);

    stream_func backup_func = NULL;
    if (g_player_i2s_write_handle) {
        ESP_LOGI(TAG, "Set null cb");
        backup_func = audio_element_get_write_cb(g_player_i2s_write_handle);
        audio_element_set_write_cb(g_player_i2s_write_handle, i2s_write_idle_cb, NULL);
        ESP_LOGI(TAG, "Set done");
    }

    while (1) {
        audio_event_iface_msg_t msg = { 0 };
        esp_err_t ret = audio_event_iface_listen(g_int_tone_handle->evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) g_int_tone_handle->element[1]
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(g_int_tone_handle->element[1], &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);

            rsp_filter_set_src_info(g_int_tone_handle->element[2], music_info.sample_rates, music_info.channels);

            audio_element_run(g_int_tone_handle->element[2]);
            audio_element_run(g_int_tone_handle->element[3]);

            ESP_LOGI(TAG, "Get state before resume: rsp: %d, i2s: %d", audio_element_get_state(g_int_tone_handle->element[2]), audio_element_get_state(g_int_tone_handle->element[3]));

            audio_element_reset_state(g_int_tone_handle->element[2]);
            audio_element_reset_state(g_int_tone_handle->element[3]);

            audio_element_resume(g_int_tone_handle->element[2], 0, 10 / portTICK_PERIOD_MS);
            audio_element_resume(g_int_tone_handle->element[3], 0, 10 / portTICK_PERIOD_MS);

            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) g_int_tone_handle->element[3]
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && ((int)msg.data == AEL_STATUS_ERROR_OPEN ||
            (int)msg.data == AEL_STATUS_ERROR_INPUT || (int)msg.data == AEL_STATUS_ERROR_PROCESS || (int)msg.data == AEL_STATUS_ERROR_OUTPUT ||
            (int)msg.data == AEL_STATUS_ERROR_CLOSE || (int)msg.data == AEL_STATUS_ERROR_TIMEOUT || (int)msg.data == AEL_STATUS_ERROR_UNKNOWN)) {
            ESP_LOGE(TAG, "Error occured when play int tone");
            break;
        }
    }

    for (int i = 0; i < DEFAULT_MAX_ELEMENT_NUM; i++) {
        audio_element_reset_state(g_int_tone_handle->element[i]);
        audio_element_reset_input_ringbuf(g_int_tone_handle->element[i]);
        audio_element_reset_output_ringbuf(g_int_tone_handle->element[i]);
    }
    if (backup_func) {
        ESP_LOGI(TAG, "Back up cb");
        audio_element_set_write_cb(g_player_i2s_write_handle, backup_func, NULL);
    }
    xSemaphoreGive(g_int_tone_handle->sem_mutex);
    ESP_LOGI(TAG, "Interrupt tone play done");
    return ESP_OK;
}

audio_err_t audio_player_int_tone_deinit(void)
{
    esp_err_t ret = ESP_OK;
    if (g_int_tone_handle) {
        for (int i = 0; i < DEFAULT_MAX_ELEMENT_NUM; i++) {
            ret |= audio_element_stop(g_int_tone_handle->element[i]);
            ret |= audio_element_wait_for_stop(g_int_tone_handle->element[i]);
            ret |= audio_element_terminate(g_int_tone_handle->element[i]);
            ret |= audio_element_msg_remove_listener(g_int_tone_handle->element[i], g_int_tone_handle->evt);
            ret |= audio_element_deinit(g_int_tone_handle->element[i]);
        }

        ret |= audio_event_iface_destroy(g_int_tone_handle->evt);
        vSemaphoreDelete(g_int_tone_handle->sem_mutex);

        for (int i = 0; i < (DEFAULT_MAX_ELEMENT_NUM - 1); i++) {
            ret |= rb_destroy(g_int_tone_handle->ringbuffer[i]);
        }
        free(g_int_tone_handle);
        g_int_tone_handle = NULL;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "INT tone player init fail");
        return ESP_FAIL;
    }
    return ESP_OK;
}
