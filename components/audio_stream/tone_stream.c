/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_image_format.h"

#include "audio_mem.h"
#include "audio_error.h"
#include "audio_element.h"
#include "tone_stream.h"

#define FLASH_TONE_FILE_TAG          0x28
#define FLASH_TONE_FILE_INFO_BLOCK   64

static const char *TAG = "TONE_STREAM";

/**
 * @brief  Parameters of each tone
 */
#pragma pack(1)
typedef struct flash_tone_info
{
    uint8_t  file_tag;    /*!< File tag is 0x28 */
    uint8_t  song_index;  /*!< Song index represents the type of tone  */
    uint8_t  file_type;   /*!< The file type of the tone bin, usually the mp3 type. */
    uint8_t  song_ver;    /*!< Song version, default is 0 */
    uint32_t song_adr;    /*!< The address of the bin file corresponding to each tone */
    uint32_t song_len;    /*!< The length of current tone   */
    uint32_t RFU[12];     /*!< Default 0  */
    uint32_t info_crc;    /*!< The crc value of current tone */
} flash_tone_info_t;
#pragma pack()

/**
 * @brief  Parameters of each tone
 */
typedef struct flash_stream
{
    audio_stream_type_t type;  /*!< File operation type */
    bool is_open;              /*!< Flash stream status */
    uint64_t write_addr;       /*!< Address to write to flash */
    uint64_t read_addr;        /*!< Address to read to flash */
} flash_stream_t;

static const esp_partition_t *_flash_partition;
static flash_tone_header_t _header;
static const char *partition_label = "flash_tone";

static esp_err_t _flash_tone_uninit(void)
{
    _flash_partition = NULL;
    memset(&_header, 0x00, sizeof(_header));
    return ESP_OK;
}

static esp_err_t _get_flash_tone_num(void)
{
    return _header.total_num;
}

static esp_err_t _get_flash_tone_info(uint16_t index, flash_tone_info_t *info)
{
    if (NULL == _flash_partition) {
        ESP_LOGE(TAG, "The flash partition is NULL");
        return ESP_FAIL;
    }
    if (_header.total_num < index) {
        ESP_LOGE(TAG, "Wanted index out of range index[%d]", index);
        return ESP_FAIL;
    }
    flash_tone_info_t info_tmp = {0};
    int start_adr = 0;
    if (_header.format == 0) {
        start_adr = sizeof(flash_tone_header_t) + FLASH_TONE_FILE_INFO_BLOCK * index;
    } else if (_header.format == 1) {
        start_adr = sizeof(flash_tone_header_t) + sizeof(esp_app_desc_t) + FLASH_TONE_FILE_INFO_BLOCK * index;
    } else {
        return ESP_FAIL;
    }
    if (ESP_OK == esp_partition_read(_flash_partition, start_adr, &info_tmp, sizeof(info_tmp))) {
        //TODO check crc
        if (info_tmp.file_tag == FLASH_TONE_FILE_TAG) {
            memcpy(info, &info_tmp, sizeof(info_tmp));
        }
    } else {
        ESP_LOGE(TAG, "Get tone file tag error %x", info_tmp.file_tag);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t _get_flash_tone_tail(uint16_t *tail)
{
    if (_header.format == 1) {
        flash_tone_info_t last_file = {0};
        _get_flash_tone_info(_header.total_num - 1, &last_file);
        int tail_addr = last_file.song_adr + last_file.song_len + ((4 - last_file.song_len % 4) % 4) + 4;
        ESP_LOGD(TAG, "addr %X, len %X, tail %X", last_file.song_adr, last_file.song_len, tail_addr);
        return esp_partition_read(_flash_partition, tail_addr, tail, sizeof(uint16_t));
    } else {
        *tail = 0;
        return ESP_FAIL;
    }
}

static esp_err_t _flash_tone_init(void)
{
    if (_flash_partition != NULL) {
        ESP_LOGD(TAG, "already init");
        return ESP_OK;
    }
    _flash_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partition_label);
    if (NULL == _flash_partition) {
        ESP_LOGE(TAG, "Can not found flash tone file partition");
        return ESP_FAIL;
    }
    ESP_LOGD("flashPartition", "%d: type[%x]", __LINE__, _flash_partition->type);
    ESP_LOGD("flashPartition", "%d: subtype[%x]", __LINE__, _flash_partition->subtype);
    ESP_LOGD("flashPartition", "%d: address:0x%x", __LINE__, _flash_partition->address);
    ESP_LOGD("flashPartition", "%d: size:0x%x", __LINE__, _flash_partition->size);
    ESP_LOGI("flashPartition", "%d: label:%s", __LINE__, _flash_partition->label);
    if (ESP_OK == esp_partition_read(_flash_partition, 0, &_header, sizeof(_header))) {
        ESP_LOGI(TAG, "header tag %X, format %d", _header.header_tag, _header.format);
        if (_header.header_tag == FLASH_TONE_HEADER) {
            if (_header.format == 1) {
                uint16_t tail = 0;
                if (ESP_OK == _get_flash_tone_tail(&tail) && tail == FLASH_TONE_TAIL) {
                    ESP_LOGI(TAG, "audio tone's tail is %X", tail);
                    return ESP_OK;
                } else {
                    ESP_LOGE(TAG, "Flash tone init failed at tail check %X", tail);
                }
            } else {
                return ESP_OK;
            }
        }
    } else {
        ESP_LOGE(TAG, "Read flash tone file header failed");
    }
    _flash_partition = NULL;
    return ESP_FAIL;
}

esp_err_t tone_partition_verify(void)
{
    return _flash_tone_init();
}

esp_err_t tone_partition_get_app_desc(esp_app_desc_t *desc)
{
    if (_flash_tone_init() == ESP_OK && _header.format == 1) {
        if (ESP_OK == esp_partition_read(_flash_partition, sizeof(flash_tone_header_t), desc, sizeof(esp_app_desc_t))) {
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

static esp_err_t _flash_open(audio_element_handle_t self)
{
    flash_stream_t *flash = (flash_stream_t *)audio_element_getdata(self);
    audio_element_info_t info = { 0 };
    audio_element_getinfo(self, &info);
    if (_flash_tone_init() != ESP_OK) {
        return ESP_FAIL;
    }
    char *flash_url = audio_element_get_uri(self);

    flash_url += strlen("flash://tone/");
    char *temp = strchr(flash_url, '_');
    char find_num[2] = {0};
    int file_index = 0;
    if (temp != NULL) {
        strncpy(find_num, flash_url, temp - flash_url);
        file_index = strtoul(find_num, 0, 10);
        ESP_LOGI(TAG, "Wanted read flash tone index is %d", file_index);
    } else {
        ESP_LOGE(TAG, "Tone file name is not correct!");
        return ESP_FAIL;
    }
    if (flash->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_FAIL;
    }
    flash_tone_info_t flash_info = { 0 };
    _get_flash_tone_info(file_index, &flash_info);
    ESP_LOGI(TAG, "Tone offset:%08x, Tone length:%d, total num:%d  pos:%d\n", flash_info.song_adr, flash_info.song_len, _get_flash_tone_num(), file_index);
    if (flash_info.song_len <= 0) {
        ESP_LOGE(TAG, "Mayebe the flash tone is empty, please ensure the flash's contex");
        return ESP_FAIL;
    }
    flash->is_open = true;
    info.total_bytes = flash_info.song_len;
    flash->read_addr = flash_info.song_adr + info.byte_pos;
    audio_element_setdata(self, flash);
    audio_element_setinfo(self, &info);
    return ESP_OK;
}

static int _flash_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    audio_element_info_t info = { 0 };
    flash_stream_t *flash = NULL;

    flash = (flash_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);

    if (NULL == _flash_partition) {
        ESP_LOGE(TAG, "flash partition pointer is null, line:%d", __LINE__);
        return ESP_FAIL;
    }
    if (info.byte_pos + len > info.total_bytes) {
        len = info.total_bytes - info.byte_pos;
    }
    if (ESP_OK != esp_partition_read(_flash_partition, info.byte_pos + flash->read_addr, buffer, len)) {
        ESP_LOGE(TAG, "get tone data error, line:%d", __LINE__);
        return ESP_FAIL;
    }

    info.byte_pos += len;
    if (len <= 0) {
        ESP_LOGW(TAG, "No more data,ret:%d ,info.byte_pos:%llu", len, info.byte_pos);
        return ESP_OK;
    }
    audio_element_setinfo(self, &info);

    return len;
}

static int _flash_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    audio_element_info_t info = { 0 };
    flash_stream_t *flash = (flash_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);

    if (ESP_OK == spi_flash_write(flash->write_addr + info.byte_pos, buffer,  len)) {
        info.byte_pos += len;
        audio_element_setinfo(self, &info);
        return len;
    }
    return ESP_FAIL;
}

static int _flash_process(audio_element_handle_t self, char *in_buffer, int in_len)
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

static esp_err_t _flash_close(audio_element_handle_t self)
{
    flash_stream_t *flash = (flash_stream_t *)audio_element_getdata(self);
    if (flash->is_open) {
        flash->is_open = false;
    }
    _flash_tone_uninit();
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t _flash_destroy(audio_element_handle_t self)
{
    flash_stream_t *flash = (flash_stream_t *)audio_element_getdata(self);
    audio_free(flash);
    return ESP_OK;
}

audio_element_handle_t tone_stream_init(tone_stream_cfg_t *config)
{
    audio_element_handle_t el;
    flash_stream_t *flash = audio_calloc(1, sizeof(flash_stream_t));

    AUDIO_MEM_CHECK(TAG, flash, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _flash_open;
    cfg.close = _flash_close;
    cfg.process = _flash_process;
    cfg.destroy = _flash_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.out_rb_size = config->out_rb_size;
    cfg.buffer_len = config->buf_sz;
    if (cfg.buffer_len == 0) {
        cfg.buffer_len = TONE_STREAM_BUF_SIZE;
    }

    cfg.tag = "flash";
    flash->type = config->type;

    if (config->type == AUDIO_STREAM_READER) {
        cfg.read = _flash_read;
    } else if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = _flash_write;
    }
    el = audio_element_init(&cfg);

    AUDIO_MEM_CHECK(TAG, el, goto _flash_init_exit);
    audio_element_setdata(el, flash);
    return el;
_flash_init_exit:
    audio_free(flash);
    return NULL;
}
