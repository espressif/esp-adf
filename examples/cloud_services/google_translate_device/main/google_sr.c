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
#include "google_sr.h"
#include "json_utils.h"

static const char *TAG = "GOOGLE_SR";


#define GOOGLE_SR_ENDPOINT  "https://speech.googleapis.com/v1/speech:recognize?key=%s"
#define GOOGLE_SR_CONFIG           "{\"languageCode\": \"%s\", \"encoding\": \"%s\", \"sampleRateHertz\": %d}"
#define GOOGLE_SR_BEGIN            "{\"config\": " GOOGLE_SR_CONFIG ", \"audio\": {\"content\":\""
#define GOOGLE_SR_END              "\"}}"
#define GOOGLE_SR_TASK_STACK (8*1024)

static const char *encoding_map[] = {
    "LINEAR16"
};

typedef struct google_sr {
    audio_pipeline_handle_t pipeline;
    int                     remain_len;
    int                     sr_total_write;
    bool                    is_begin;
    char                    *buffer;
    char                    *b64_buffer;
    audio_element_handle_t  i2s_reader;
    audio_element_handle_t  http_stream_writer;
    char                    *lang_code;
    char                    *api_key;
    int                     sample_rates;
    int                     buffer_size;
    google_sr_encoding_t    encoding;
    char                    *response_text;
    google_sr_event_handle_t on_begin;
} google_sr_t;


static int _http_write_chunk(esp_http_client_handle_t http, const char *buffer, int len)
{
    char header_chunk_buffer[16];
    int header_chunk_len = sprintf(header_chunk_buffer, "%x\r\n", len);
    if (esp_http_client_write(http, header_chunk_buffer, header_chunk_len) <= 0) {
        return ESP_FAIL;
    }
    int write_len = esp_http_client_write(http, buffer, len);
    if (write_len <= 0) {
        ESP_LOGE(TAG, "Error write chunked content");
        return ESP_FAIL;
    }
    if (esp_http_client_write(http, "\r\n", 2) <= 0) {
        return ESP_FAIL;
    }
    return write_len;
}

static esp_err_t _http_stream_writer_event_handle(http_stream_event_msg_t *msg)
{
    esp_http_client_handle_t http = (esp_http_client_handle_t)msg->http_client;
    google_sr_t *sr = (google_sr_t *)msg->user_data;

    int write_len;
    size_t need_write = 0;

    if (msg->event_id == HTTP_STREAM_PRE_REQUEST) {
        // set header
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_PRE_REQUEST, lenght=%d", msg->buffer_len);
        sr->sr_total_write = 0;
        sr->is_begin = true;
        sr->remain_len = 0;
        esp_http_client_set_method(http, HTTP_METHOD_POST);
        esp_http_client_set_post_field(http, NULL, -1); // Chunk content
        esp_http_client_set_header(http, "Content-Type", "application/json");
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_ON_REQUEST) {
        // ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_ON_REQUEST, lenght=%d, begin=%d", msg->buffer_len, sr->is_begin);
        /* Write first chunk */
        if (sr->is_begin) {
            sr->is_begin = false;
            int sr_begin_len = snprintf(sr->buffer, sr->buffer_size, GOOGLE_SR_BEGIN, sr->lang_code, encoding_map[sr->encoding], sr->sample_rates);
            if (sr->on_begin) {
                sr->on_begin(sr);
            }
            return _http_write_chunk(http, sr->buffer, sr_begin_len);
        }

        if (msg->buffer_len > sr->buffer_size * 3 / 2) {
            ESP_LOGE(TAG, "Please use SR Buffer size greeter than %d", msg->buffer_len * 3 / 2);
            return ESP_FAIL;
        }

        /* Write b64 audio data */
        memcpy(sr->buffer + sr->remain_len, msg->buffer, msg->buffer_len);
        sr->remain_len += msg->buffer_len;
        int keep_next_time = sr->remain_len % 3;

        sr->remain_len -= keep_next_time;
        if (mbedtls_base64_encode((unsigned char *)sr->b64_buffer, sr->buffer_size,  &need_write, (unsigned char *)sr->buffer, sr->remain_len) != 0) {
            ESP_LOGE(TAG, "Error encode b64");
            return ESP_FAIL;
        }
        if (keep_next_time > 0) {
            memcpy(sr->buffer, sr->buffer + sr->remain_len, keep_next_time);
        }
        sr->remain_len = keep_next_time;
        ESP_LOGD(TAG, "\033[A\33[2K\rTotal bytes written: %d", sr->sr_total_write);

        write_len = _http_write_chunk(http, (const char *)sr->b64_buffer, need_write);
        if (write_len <= 0) {
            return write_len;
        }
        sr->sr_total_write += write_len;
        return write_len;
    }

    /* Write End chunk */
    if (msg->event_id == HTTP_STREAM_POST_REQUEST) {
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker");
        need_write = 0;
        if (sr->remain_len) {
            if (mbedtls_base64_encode((unsigned char *)sr->b64_buffer, sr->buffer_size,  &need_write, (unsigned char *)sr->buffer, sr->remain_len) != 0) {
                ESP_LOGE(TAG, "Error encode b64");
                return ESP_FAIL;
            }
            write_len = _http_write_chunk(http, (const char *)sr->b64_buffer, need_write);
            if (write_len <= 0) {
                return write_len;
            }
        }
        write_len = _http_write_chunk(http, GOOGLE_SR_END, strlen(GOOGLE_SR_END));

        if (write_len <= 0) {
            return ESP_FAIL;
        }
        /* Finish chunked */
        if (esp_http_client_write(http, "0\r\n\r\n", 5) <= 0) {
            return ESP_FAIL;
        }
        return write_len;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_REQUEST) {

        int read_len = esp_http_client_read(http, (char *)sr->buffer, sr->buffer_size);
        ESP_LOGI(TAG, "[ + ] HTTP client HTTP_STREAM_FINISH_REQUEST, read_len=%d", read_len);
        if (read_len <= 0) {
            return ESP_FAIL;
        }
        if (read_len > sr->buffer_size - 1) {
            read_len = sr->buffer_size - 1;
        }
        sr->buffer[read_len] = 0;
        ESP_LOGI(TAG, "Got HTTP Response = %s", (char *)sr->buffer);
        if (sr->response_text) {
            free(sr->response_text);
        }
        sr->response_text = json_get_token_value(sr->buffer, "transcript");
        return ESP_OK;
    }
    return ESP_OK;
}

google_sr_handle_t google_sr_init(google_sr_config_t *config)
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    google_sr_t *sr = calloc(1, sizeof(google_sr_t));
    AUDIO_MEM_CHECK(TAG, sr, return NULL);
    sr->pipeline = audio_pipeline_init(&pipeline_cfg);

    sr->buffer_size = config->buffer_size;
    if (sr->buffer_size <= 0) {
        sr->buffer_size = DEFAULT_SR_BUFFER_SIZE;
    }

    sr->buffer = malloc(sr->buffer_size);
    AUDIO_MEM_CHECK(TAG, sr->buffer, goto exit_sr_init);
    sr->b64_buffer = malloc(sr->buffer_size);
    AUDIO_MEM_CHECK(TAG, sr->b64_buffer, goto exit_sr_init);
    sr->lang_code = strdup(config->lang_code);
    AUDIO_MEM_CHECK(TAG, sr->lang_code, goto exit_sr_init);
    sr->api_key = strdup(config->api_key);
    AUDIO_MEM_CHECK(TAG, sr->api_key, goto exit_sr_init);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    sr->i2s_reader = i2s_stream_init(&i2s_cfg);

    http_stream_cfg_t http_cfg = {
        .type = AUDIO_STREAM_WRITER,
        .event_handle = _http_stream_writer_event_handle,
        .user_data = sr,
        .task_stack = GOOGLE_SR_TASK_STACK,
    };
    sr->http_stream_writer = http_stream_init(&http_cfg);
    sr->sample_rates = config->record_sample_rates;
    sr->encoding = config->encoding;
    sr->on_begin = config->on_begin;

    audio_pipeline_register(sr->pipeline, sr->http_stream_writer, "sr_http");
    audio_pipeline_register(sr->pipeline, sr->i2s_reader,         "sr_i2s");
    const char *link_tag[2] = {"sr_i2s", "sr_http"};
    audio_pipeline_link(sr->pipeline, &link_tag[0], 2);
    i2s_stream_set_clk(sr->i2s_reader, config->record_sample_rates, 16, 1);

    return sr;
exit_sr_init:
    google_sr_destroy(sr);
    return NULL;
}

esp_err_t google_sr_destroy(google_sr_handle_t sr)
{
    if (sr == NULL) {
        return ESP_FAIL;
    }
    audio_pipeline_stop(sr->pipeline);
    audio_pipeline_wait_for_stop(sr->pipeline);
    audio_pipeline_terminate(sr->pipeline);
    audio_pipeline_remove_listener(sr->pipeline);
    audio_pipeline_deinit(sr->pipeline);
    free(sr->buffer);
    free(sr->b64_buffer);
    free(sr->lang_code);
    free(sr->api_key);
    free(sr);
    return ESP_OK;
}

esp_err_t google_sr_set_listener(google_sr_handle_t sr, audio_event_iface_handle_t listener)
{
    if (listener) {
        audio_pipeline_set_listener(sr->pipeline, listener);
    }
    return ESP_OK;
}

esp_err_t google_sr_start(google_sr_handle_t sr)
{
    snprintf(sr->buffer, sr->buffer_size, GOOGLE_SR_ENDPOINT, sr->api_key);
    audio_element_set_uri(sr->http_stream_writer, sr->buffer);
    audio_pipeline_reset_items_state(sr->pipeline);
    audio_pipeline_reset_ringbuffer(sr->pipeline);
    audio_pipeline_run(sr->pipeline);
    return ESP_OK;
}

char *google_sr_stop(google_sr_handle_t sr)
{
    audio_pipeline_stop(sr->pipeline);
    audio_pipeline_wait_for_stop(sr->pipeline);
    return sr->response_text;
}
