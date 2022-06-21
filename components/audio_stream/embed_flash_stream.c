/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "audio_mem.h"
#include "embed_flash_stream.h"

static const char * const TAG = "EMBED_FLASH_STREAM";

#define EMBED_MAX_FILES   (2)   // 2 bytes, which means 100 files can be saved(10 * 10)
#define EMBED_PREFIX_STR  "embed://tone/"

/**
 * @brief   Embed status information
 */
typedef struct embed_flash_s {
    audio_stream_type_t    type;
    bool                   is_open;
    int                    cur_index;
    embed_item_info_t      *info;
} embed_flash_stream_t;

static esp_err_t _embed_open(audio_element_handle_t self)
{
    embed_flash_stream_t *stream = (embed_flash_stream_t *)audio_element_getdata(self);
    if (stream->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_FAIL;
    }
    if (stream->info == NULL) {
        ESP_LOGE(TAG, "please call embed_flash_stream_set_context function first");
        return ESP_FAIL;
    }
    char *url = audio_element_get_uri(self);
    url += strlen(EMBED_PREFIX_STR);
    char *temp = strchr(url, '_');
    char find_num[EMBED_MAX_FILES] = { 0 };
    int file_index = 0;
    if (temp != NULL) {
        strncpy(find_num, url, temp - url);
        file_index = strtoul(find_num, 0, 10);
        ESP_LOGD(TAG, "Wanted read flash tone index is %d", file_index);
    } else {
        ESP_LOGE(TAG, "Embed flash file name is not correct!");
        return ESP_FAIL;
    }
    stream->cur_index = file_index;
    audio_element_info_t info = { 0 };
    info.total_bytes = stream->info[stream->cur_index].size;
    audio_element_setdata(self, stream);
    audio_element_set_total_bytes(self, info.total_bytes);

    stream->is_open = true;
    return ESP_OK;
}

static int _embed_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    audio_element_info_t info = { 0 };
    embed_flash_stream_t *stream = NULL;

    stream = (embed_flash_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);
    if (info.byte_pos + len > info.total_bytes) {
        len = info.total_bytes - info.byte_pos;
    }
    if (len <= 0) {
        ESP_LOGW(TAG, "No more data,ret:%d ,info.byte_pos:%llu", len, info.byte_pos);
        return ESP_OK;
    }
    memcpy(buffer, stream->info[stream->cur_index].address + info.byte_pos, len);

    audio_element_update_byte_pos(self, len);
    ESP_LOGD(TAG, "req lengh=%d, pos=%d/%d", len, (int)info.byte_pos, (int)info.total_bytes);

    return len;
}

static int _embed_process(audio_element_handle_t self, char *in_buffer, int in_len)
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

static esp_err_t _embed_close(audio_element_handle_t self)
{
    embed_flash_stream_t *stream = (embed_flash_stream_t *)audio_element_getdata(self);
    if (stream->is_open) {
       stream->is_open = false;
    }
    return ESP_OK;
}

static esp_err_t _embed_destroy(audio_element_handle_t self)
{
    embed_flash_stream_t *stream = (embed_flash_stream_t *)audio_element_getdata(self);
    if (stream->info) {
        audio_free(stream->info);
    }
    audio_free(stream);
    return ESP_OK;
}

audio_element_handle_t embed_flash_stream_init(embed_flash_stream_cfg_t *config)
{
    audio_element_handle_t el;
    embed_flash_stream_t *stream = audio_calloc(1, sizeof(embed_flash_stream_t));
    AUDIO_MEM_CHECK(TAG, stream, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _embed_open;
    cfg.close = _embed_close;
    cfg.process = _embed_process;
    cfg.destroy = _embed_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    if (config->extern_stack == true) {
        ESP_LOGW(TAG, "Just support %s task stack on internal memory\n \
        will remove task on internal memory", "embed flash task");
        config->extern_stack = false;
    }
    cfg.tag = "embed";

    cfg.read = _embed_read;
    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, goto __exit);
    audio_element_setdata(el, stream);
    return el;

__exit:
    audio_free(stream);
    stream = NULL;
    return NULL;
}

esp_err_t  embed_flash_stream_set_context(audio_element_handle_t embed_stream, const embed_item_info_t *context, int max_num)
{
    AUDIO_MEM_CHECK(TAG, context, return ESP_FAIL);
    embed_flash_stream_t *stream = NULL;
    int max_can_saved_num = 1;
    stream = (embed_flash_stream_t *)audio_element_getdata(embed_stream);

    for (size_t i = 0; i < EMBED_MAX_FILES; i++) {
        max_can_saved_num *= 10;
    }

    if (max_num > max_can_saved_num) {
        ESP_LOGW(TAG, "The maximum storage quantity is exceeded. It is recommended to modify the `EMBED_MAX_FILES` value");
        return ESP_FAIL;
    }

    if (stream->info) {
        audio_free(stream->info);
    }
    stream->info = audio_calloc(max_num, sizeof(embed_item_info_t));
    AUDIO_MEM_CHECK(TAG, stream->info, return ESP_FAIL);
    memcpy(stream->info, context, sizeof(embed_item_info_t) * max_num);

    return ESP_OK;
}
