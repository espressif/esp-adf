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

#include <sys/unistd.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "errno.h"
#include "http_stream.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include <strings.h>

static const char *TAG = "HTTP_STREAM";
#define HTTP_STREAM_TASK_STACK (6 * 1024)

typedef struct http_stream {
    audio_stream_type_t type;
    char *uri;
    bool is_open;
    esp_http_client_handle_t client;
    client_callback_t        before_http_request;
} http_stream_t;

static audio_codec_t get_audio_type(const char *content_type)
{
    if (strcasecmp(content_type, "audio/mp3")) {
        return AUDIO_CODEC_MP3;
    }
    if (strcasecmp(content_type, "audio/mpeg")) {
        return AUDIO_CODEC_MP3;
    }
    if (strcasecmp(content_type, "audio/aac")) {
        return AUDIO_CODEC_AAC;
    }
    if (strcasecmp(content_type, "audio/wav")) {
        return AUDIO_CODEC_WAV;
    }
    if (strcasecmp(content_type, "audio/opus")) {
        return AUDIO_CODEC_OPUS;
    }
    return AUDIO_CODEC_NONE;
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    audio_element_info_t *info = (audio_element_info_t *)evt->user_data;

    if (evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }

    if (strcasecmp(evt->header_key, "Content-Disposition") == 0 || strcasecmp(evt->header_key, "Content-Type") == 0) {
        ESP_LOGI(TAG, "%s = %s", evt->header_key, evt->header_value);
        info->codec_fmt = get_audio_type(evt->header_value);
    }

    return ESP_OK;
}

static esp_err_t _http_open(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);

    audio_element_info_t info;
    char *uri = audio_element_get_uri(self);
    audio_element_getinfo(self, &info);
    ESP_LOGD(TAG, "_http_open");
    if (uri == NULL) {
        ESP_LOGE(TAG, "Error, need uri to open");
        return ESP_FAIL;
    }
    if (http->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_FAIL;
    }
    esp_http_client_config_t http_cfg = {
        .uri = uri,
        .event_handle = _http_event_handle,
        .user_data = &info,
        .timeout_ms = 30*1000,
    };

    http->client = esp_http_client_init(&http_cfg);

    if (info.byte_pos) {
        char rang_header[32];
        snprintf(rang_header, 32, "bytes=%d-", (int)info.byte_pos);
        esp_http_client_set_header(http->client, "Range", rang_header);
    }

    if (http->before_http_request) {
        if (http->before_http_request(http->client) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to process user callback");
            return ESP_FAIL;
        }
    }

    char *buffer = NULL;
    int post_len = esp_http_client_get_post_field(http->client, &buffer);

    if (esp_http_client_open(http->client, post_len) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open http stream");
        return ESP_FAIL;
    }

    if (post_len && buffer) {
        if (esp_http_client_write(http->client, buffer, post_len) <= 0) {
            ESP_LOGE(TAG, "Failed to write data to http stream");
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "len=%d, data=%s", post_len, buffer);
    }
    info.total_bytes = esp_http_client_fetch_headers(http->client);;
    ESP_LOGD(TAG, "total_bytes=%d", (int)info.total_bytes);
    if (esp_http_client_get_status_code(http->client) != 200) {
        ESP_LOGE(TAG, "Invalid HTTP stream");
        return ESP_FAIL;
    }
    http->is_open = true;
    audio_element_setinfo(self, &info);
    audio_element_report_codec_fmt(self);
    return ESP_OK;
}

static int _http_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_info_t info;
    audio_element_getinfo(self, &info);


    int rlen = esp_http_client_read(http->client, buffer, len);

    if (rlen <= 0) {
        ESP_LOGW(TAG, "No more data,errno:%d", errno);
    } else {
        info.byte_pos += rlen;
        audio_element_setinfo(self, &info);
    }
    ESP_LOGD(TAG, "req lengh=%d, read=%d, pos=%d/%d", len, rlen, (int)info.byte_pos, (int)info.total_bytes);
    return rlen;
}

static int _http_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    // http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    return 0;
}

static int _http_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0) {
        w_size = audio_element_output(self, in_buffer, r_size);
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t _http_close(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    if (http->is_open) {
        esp_http_client_close(http->client);
        http->is_open = false;
    }
    esp_http_client_cleanup(http->client);
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t _http_destroy(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_free(http);
    return ESP_OK;
}

audio_element_handle_t http_stream_init(http_stream_cfg_t *config)
{
    http_stream_t *http = audio_calloc(1, sizeof(http_stream_t));
    mem_assert(http);
    audio_element_handle_t el;

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _http_open;
    cfg.close = _http_close;
    cfg.process = _http_process;
    cfg.destroy = _http_destroy;
    cfg.task_stack = HTTP_STREAM_TASK_STACK;
    cfg.tag = "http";

    http->type = config->type;
    http->before_http_request = config->before_http_request;

    if (config->type == AUDIO_STREAM_READER) {
        cfg.read = _http_read;
    } else if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = _http_write;
    }
    el = audio_element_init(&cfg);
    mem_assert(el);
    audio_element_setdata(el, http);
    return el;
}
