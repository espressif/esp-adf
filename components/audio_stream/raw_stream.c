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
#include <stdio.h>
#include <string.h>
#include "errno.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "raw_stream.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "RAW_STREAM";

typedef struct raw_stream {
    audio_stream_type_t type;
} raw_stream_t;

static audio_element_handle_t read_raw_el;
static audio_element_handle_t write_raw_el;

int raw_stream_read(char *buffer, int len)
{
    int ret = audio_element_input(read_raw_el, buffer, len);
    if (ret <= 0) {
        audio_element_report_status(read_raw_el, AEL_STATUS_STATE_STOPPED);
    }
    return ret;

}
int raw_stream_write(char *buffer, int len)
{
    int ret = audio_element_output(write_raw_el, buffer, len);
    if (ret <= 0) {
        audio_element_report_status(write_raw_el, AEL_STATUS_STATE_STOPPED);
    }
    return ret;
}

static esp_err_t _raw_destroy(audio_element_handle_t self)
{
    raw_stream_t *raw = (raw_stream_t *)audio_element_getdata(self);
    audio_free(raw);
    return ESP_OK;
}

audio_element_handle_t raw_stream_init(raw_stream_cfg_t *config)
{
    if (read_raw_el && write_raw_el) {
        ESP_LOGI(TAG, "stream init,failed,%p,%p", read_raw_el, write_raw_el);
        return NULL;
    }
    raw_stream_t *raw = audio_calloc(1, sizeof(raw_stream_t));
    AUDIO_MEM_CHECK(TAG, raw, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.task_stack = -1; // No need task
    cfg.destroy = _raw_destroy;
    cfg.tag = "raw";
    cfg.out_rb_size = config->out_rb_size;
    raw->type = config->type;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {
        audio_free(raw);
        return NULL;
    });
    if (config->type == AUDIO_STREAM_READER) {
        read_raw_el = el;
    } else if (config->type == AUDIO_STREAM_WRITER) {
        write_raw_el = el;
    }
    audio_element_setdata(el, raw);
    ESP_LOGD(TAG, "stream init,el:%p", el);
    return el;
}
