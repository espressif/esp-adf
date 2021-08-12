/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "mbedtls/base64.h"

#include "esp_http_client.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "http_stream.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "google_tts.h"
#include "json_utils.h"

static const char *TAG = "GOOGLE_TTS";


#define GOOGLE_TTS_ENDPOINT         "https://texttospeech.googleapis.com/v1beta1/text:synthesize?key=%s"
#define GOOGLE_TTS_TEMPLATE         "{"\
                                        "\"audioConfig\": { \"audioEncoding\" : \"MP3\", \"sampleRateHertz\": %d },"\
                                        "\"voice\": { \"languageCode\" : \"%s\" },"\
                                        "\"input\": { \"text\" : \"%s\" }"\
                                    "}"
#define GOOGLE_TTS_TASK_STACK (8*1024)

typedef struct google_tts {
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t  i2s_writer;
    audio_element_handle_t  http_stream_reader;
    audio_element_handle_t  mp3_decoder;
    char                    *api_key;
    char                    *lang_code;
    int                     buffer_size;
    char                    *buffer;
    char                    *text;
    bool                    is_begin;
    int                     tts_total_read;
    int                     sample_rate;
    int                     remain_len;
} google_tts_t;


static esp_err_t _http_stream_reader_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    google_tts_t *tts = (google_tts_t *)msg->user_data;

    int read_len = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // Post text data
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        tts->tts_total_read = 0;
        tts->is_begin = true;
        int payload_len = snprintf(tts->buffer, tts->buffer_size, GOOGLE_TTS_TEMPLATE, tts->sample_rate, tts->lang_code, tts->text);
        esp_http_client_set_post_field(http, tts->buffer, payload_len);
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        esp_http_client_set_header(http, "Content-Type", "application/json");
        tts->remain_len = 0;
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_RESPONSE) {
        ESP_LOGD(TAG, "[ + ] HTTP client HTTP_STREAM_ON_RESPONSE, lenght=%d", msg->buffer_len);
        /* Read first chunk */
        int process_data_len = 0;
        char *data_ptr = tts->buffer;
        int align_read = msg->buffer_len * 4 / 3;
        align_read -= (align_read % 4);
        align_read -= tts->remain_len;
        read_len = esp_http_client_read(http, tts->remain_len + tts->buffer, align_read);

        // ESP_LOGI(TAG, "Need read = %d, read_len=%d, tts->remain_len=%d", align_read, read_len, tts->remain_len);
        if (read_len <= 0) {
            return read_len;
        }
        process_data_len = read_len + tts->remain_len;

        if (tts->is_begin) {
            tts->is_begin = false;

            while (*data_ptr != ':' && process_data_len) {
                process_data_len--;
                data_ptr++;
            }
            while (*data_ptr != '"' && process_data_len) {
                process_data_len--;
                data_ptr++;
            }
            // *data_ptr = 0;
            data_ptr++;
            process_data_len--;

        }
        unsigned int mp3_len = 0;
        int keep_next_time = 0;
        if (process_data_len > 0) {
            data_ptr[process_data_len] = 0;
            keep_next_time = process_data_len % 4;
            process_data_len -= keep_next_time;
            tts->remain_len = keep_next_time;
            mbedtls_base64_decode(msg->buffer, msg->buffer_len, &mp3_len, (unsigned char *)data_ptr, process_data_len);
            memcpy(tts->buffer, data_ptr + process_data_len, keep_next_time);
        }
        if (mp3_len == 0) {
            return ESP_FAIL;
        }
        return mp3_len;
    }

    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");

    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {

    }

    return ESP_OK;
}

google_tts_handle_t google_tts_init(google_tts_config_t *config)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    google_tts_t *tts = calloc(1, sizeof(google_tts_t));
    AUDIO_MEM_CHECK(TAG, tts, return NULL);

    tts->pipeline = audio_pipeline_init(&pipeline_cfg);

    tts->buffer_size = config->buffer_size;
    if (tts->buffer_size <= 0) {
        tts->buffer_size = DEFAULT_TTS_BUFFER_SIZE;
    }

    tts->buffer = malloc(tts->buffer_size);
    AUDIO_MEM_CHECK(TAG, tts->buffer, goto exit_tts_init);

    tts->api_key = strdup(config->api_key);
    AUDIO_MEM_CHECK(TAG, tts->api_key, goto exit_tts_init);


    tts->sample_rate = config->playback_sample_rate;

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    tts->i2s_writer = i2s_stream_init(&i2s_cfg);

    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_READER,
        .event_handle = _http_stream_reader_event_handle,
        .user_data = tts,
        .task_stack = GOOGLE_TTS_TASK_STACK,
    };
    tts->http_stream_reader = http_stream_init(&http_cfg);

    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    tts->mp3_decoder = mp3_decoder_init(&mp3_cfg);

    audio_pipeline_register(tts->pipeline, tts->http_stream_reader, "tts_http");
    audio_pipeline_register(tts->pipeline, tts->mp3_decoder,        "tts_mp3");
    audio_pipeline_register(tts->pipeline, tts->i2s_writer,         "tts_i2s");
    const char *link_tag[3] = {"tts_http", "tts_mp3", "tts_i2s"};
    audio_pipeline_link(tts->pipeline, &link_tag[0], 3);
    i2s_stream_set_clk(tts->i2s_writer, config->playback_sample_rate, 16, 1);
    return tts;
exit_tts_init:
    google_tts_destroy(tts);
    return NULL;
}

esp_err_t google_tts_destroy(google_tts_handle_t tts)
{
    if (tts == NULL) {
        return ESP_FAIL;
    }
    audio_pipeline_stop(tts->pipeline);
    audio_pipeline_wait_for_stop(tts->pipeline);
    audio_pipeline_terminate(tts->pipeline);
    audio_pipeline_remove_listener(tts->pipeline);
    audio_pipeline_deinit(tts->pipeline);
    free(tts->buffer);
    free(tts->api_key);
    free(tts);
    return ESP_OK;
}

esp_err_t google_tts_set_listener(google_tts_handle_t tts, audio_event_iface_handle_t listener)
{
    if (listener) {
        audio_pipeline_set_listener(tts->pipeline, listener);
    }
    return ESP_OK;
}

bool google_tts_check_event_finish(google_tts_handle_t tts, audio_event_iface_msg_t *msg)
{
    if (msg->source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg->source == (void *) tts->i2s_writer
            && msg->cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg->data == AEL_STATUS_STATE_STOPPED) || ((int)msg->data == AEL_STATUS_STATE_FINISHED))) {
        return true;
    }
    return false;
}


esp_err_t google_tts_start(google_tts_handle_t tts, const char *text, const char *lang_code)
{
    free(tts->lang_code);
    free(tts->text);
    tts->lang_code = strdup(lang_code);
    if (tts->lang_code == NULL) {
        ESP_LOGE(TAG, "Error no mem");
        return ESP_ERR_NO_MEM;
    }
    tts->text = strdup(text);
    if (tts->text == NULL) {
        free(tts->lang_code);
        ESP_LOGE(TAG, "Error no mem");
        return ESP_ERR_NO_MEM;
    }
    snprintf(tts->buffer, tts->buffer_size, GOOGLE_TTS_ENDPOINT, tts->api_key);
    audio_pipeline_reset_items_state(tts->pipeline);
    audio_pipeline_reset_ringbuffer(tts->pipeline);
    audio_element_set_uri(tts->http_stream_reader, tts->buffer);
    audio_pipeline_run(tts->pipeline);
    return ESP_OK;
}

esp_err_t google_tts_stop(google_tts_handle_t tts)
{
    audio_pipeline_stop(tts->pipeline);
    audio_pipeline_wait_for_stop(tts->pipeline);
    ESP_LOGD(TAG, "TTS Stopped");
    return ESP_OK;
}
