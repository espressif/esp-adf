/*
 * Espressif Modified MIT License
 *
 * Copyright (c) 2025 Espressif Systems (Shanghai) Co., LTD
 *
 * Permission is hereby granted for use **exclusively** with Espressif Systems products. 
 * This includes the right to use, copy, modify, merge, publish, distribute, and sublicense 
 * the Software, subject to the following conditions:
 *
 * 1. This Software **must be used in conjunction with Espressif Systems products**.
 * 2. The above copyright notice and this permission notice shall be included in all copies 
 *    or substantial portions of the Software.
 * 3. Redistribution of the Software in source or binary form **for use with non-Espressif products** 
 *    is strictly prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
 * FOR ANY CLAIM, DAMAGES, OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT-ESPRESSIF
 */

#include <string.h>

#include "audio_element.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "esp_log.h"
#include "aec_stream.h"

static const char *TAG = "AEC_STREAM";

#define BYTES_PER_SAMPLE (sizeof(uint16_t))

typedef struct {
    afe_type_t        type;
    afe_mode_t        mode;
    int               filter_length;
    char             *input_format;
    afe_aec_handle_t *aec_handle;
    int16_t          *out_buffer;
    int16_t          *in_buffer;
    int               output_size;
    int               input_size;
    bool              debug_aec;
    bool              is_opened;
} aec_stream_t;

static esp_err_t _aec_close(audio_element_handle_t self)
{
    aec_stream_t *aec = (aec_stream_t *)audio_element_getdata(self);
    AUDIO_SAFE_FREE(aec->aec_handle, afe_aec_destroy);
    AUDIO_SAFE_FREE(aec->in_buffer, audio_free);
    AUDIO_SAFE_FREE(aec->out_buffer, audio_free);
    aec->is_opened = false;
    return ESP_OK;
}

static esp_err_t _aec_destroy(audio_element_handle_t self)
{
    aec_stream_t *aec = (aec_stream_t *)audio_element_getdata(self);
    AUDIO_SAFE_FREE(aec, audio_free);
    return ESP_OK;
}

static esp_err_t _aec_open(audio_element_handle_t self)
{
    aec_stream_t *aec = (aec_stream_t *)audio_element_getdata(self);
    if (aec->is_opened) {
        ESP_LOGW(TAG, "aec already opened");
        return ESP_OK;
    }
    aec->aec_handle = afe_aec_create(aec->input_format, aec->filter_length, aec->type, aec->mode);
    ESP_LOGW(TAG, "Create AEC, handle %p, mic: %d, total_ch_num: %d, sample_rate: %d, chunk size: %d", aec->aec_handle, aec->aec_handle->pcm_config.mic_num, 
             aec->aec_handle->pcm_config.total_ch_num, aec->aec_handle->pcm_config.sample_rate, afe_aec_get_chunksize(aec->aec_handle));
    AUDIO_MEM_CHECK(TAG, aec->aec_handle, return ESP_FAIL);
    aec->input_size = afe_aec_get_chunksize(aec->aec_handle) * aec->aec_handle->pcm_config.total_ch_num * BYTES_PER_SAMPLE;
    aec->output_size = afe_aec_get_chunksize(aec->aec_handle) * BYTES_PER_SAMPLE;
    aec->out_buffer = (int16_t *)audio_malloc_align(16, aec->output_size);
    AUDIO_MEM_CHECK(TAG, aec->out_buffer, goto _exit_open);
    aec->in_buffer = (int16_t *)audio_malloc_align(16, aec->input_size);
    AUDIO_MEM_CHECK(TAG, aec->in_buffer, goto _exit_open);
    aec->is_opened = true;
    return ESP_OK;
_exit_open:
    AUDIO_SAFE_FREE(aec->out_buffer, audio_free);
    AUDIO_SAFE_FREE(aec->aec_handle, afe_aec_destroy);
    return ESP_FAIL;
}

static audio_element_err_t _aec_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    aec_stream_t *aec = (aec_stream_t *)audio_element_getdata(self);

    int bytes_read = audio_element_input(self, (char *)aec->in_buffer, aec->input_size);
    if (bytes_read > 0 ){
        if (aec->debug_aec == false) {
            int ret = 0;
            ret = afe_aec_process(aec->aec_handle, (int16_t *)aec->in_buffer, (int16_t *)aec->out_buffer);
            if (ret <= 0) {
                ESP_LOGE(TAG, "AEC process failed, ret %d", ret);
                return ret;
            }
            audio_element_output(self, (char *)aec->out_buffer, aec->output_size);
        } else {
            audio_element_output(self, (char *)aec->in_buffer, bytes_read);
        }
    }
    return bytes_read;
}

audio_element_handle_t aec_stream_init(aec_stream_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);

    aec_stream_t *aec = (aec_stream_t *)audio_calloc(1, sizeof(aec_stream_t));
    AUDIO_NULL_CHECK(TAG, aec, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _aec_open;
    cfg.close = _aec_close;
    cfg.process = _aec_process;
    cfg.destroy = _aec_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.stack_in_ext = config->stack_in_ext;
    cfg.tag = "aec";

    aec->type = config->type;
    aec->mode = config->mode;
    aec->filter_length = config->filter_length;
    aec->input_format = config->input_format;
    aec->debug_aec = config->debug_aec;

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_NULL_CHECK(TAG, el, {audio_free(aec); return NULL;});

    audio_element_setdata(el, aec);
    return el;
}