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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "audio_element.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "audio_thread.h"
#include "esp_log.h"

#include "algorithm_stream.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"

#define ALGORITHM_CHUNK_MAX_SIZE            (1024)
#define ALGORITHM_FETCH_TASK_STACK_SIZE     (3 * 1024)
#define ALGORITHM_GET_REFERENCE_TIMEOUT     (32 / portTICK_PERIOD_MS)

static const char *TAG = "ALGORITHM_STREAM";

const int FETCH_STOPPED_BIT = BIT0;

typedef struct {
    int16_t                      *record;
    int16_t                      *reference;
    int16_t                      *aec_buff;
    int16_t                       aec_buff_size;
    int8_t                        algo_mask;
    bool                          afe_fetch_run;
    int                           sample_rate;
    const esp_afe_sr_iface_t     *afe_handle;
    esp_afe_sr_data_t            *afe_data;
    EventGroupHandle_t            state;
    bool                          debug_input;
    int                           agc_gain;
    srmodel_list_t               *models;
    const char                   *input_format;
    int                           rec_linear_factor;
    int                           ref_linear_factor;
    algorithm_stream_input_type_t input_type;
    bool                          enable_se;
    afe_type_t                    afe_type;
    afe_agc_mode_t                agc_mode;
    int                           agc_compression_gain_db;
    int                           agc_target_level_dbfs;
} algo_stream_t;

esp_err_t algorithm_mono_fix(uint8_t *sbuff, uint32_t len)
{
    int16_t *temp_buf = (int16_t *)sbuff;
    int16_t temp_box;
    int k = len >> 1;
    for (int i = 0; i < k; i += 2) {
        temp_box = temp_buf[i];
        temp_buf[i] = temp_buf[i + 1];
        temp_buf[i + 1] = temp_box;
    }
    return ESP_OK;
}

static esp_err_t _algo_close(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);

    algo->afe_fetch_run = false;

    /* Feed some data to prevent afe->fetch getting stuck */
    while (xEventGroupWaitBits(algo->state, FETCH_STOPPED_BIT, false, true, 10 / portTICK_PERIOD_MS) != FETCH_STOPPED_BIT) {
        algo->afe_handle->feed(algo->afe_data, algo->aec_buff);
    }
    AUDIO_SAFE_FREE(algo->afe_data, algo->afe_handle->destroy);
    AUDIO_SAFE_FREE(algo->aec_buff, audio_free);
    return ESP_OK;
}

static esp_err_t _algo_destroy(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);

    AUDIO_SAFE_FREE(algo->afe_data, algo->afe_handle->destroy);
    AUDIO_SAFE_FREE(algo->record, audio_free);
    AUDIO_SAFE_FREE(algo->reference, audio_free);
    AUDIO_SAFE_FREE(algo->state, vEventGroupDelete);
    AUDIO_SAFE_FREE(algo->models, esp_srmodel_deinit);
    AUDIO_SAFE_FREE(algo, audio_free);

    return ESP_OK;
}

void _algo_fetch_task(void *pv)
{
    audio_element_handle_t self = pv;
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);

    while (algo->afe_fetch_run && algo->afe_data) {
        afe_fetch_result_t* res = algo->afe_handle->fetch(algo->afe_data);
        if (res && res->ret_value != ESP_FAIL) {
            audio_element_output(self, (char *)res->data, res->data_size);
            switch (res->vad_state) {
                case VAD_SILENCE:
                    ESP_LOGD(TAG, "VAD state : SILENCE");
                    break;
                case VAD_SPEECH:
                    ESP_LOGD(TAG, "VAD state : SPEECH");
                    break;
            }
        }
    }
    ESP_LOGI(TAG, "_algo_fetch_task is stopped");
    xEventGroupSetBits(algo->state, FETCH_STOPPED_BIT);
    vTaskDelete(NULL);
}

static esp_err_t _algo_open(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    AUDIO_NULL_CHECK(TAG, algo, return ESP_FAIL);
    char *model_name = NULL;

    afe_config_t *afe_config = afe_config_init(algo->input_format, algo->models, AFE_TYPE_VC, AFE_MODE_LOW_COST);
    afe_config->wakenet_init = false;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 21;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    if (algo->models) {
        model_name = esp_srmodel_filter(algo->models, ESP_NSNET_PREFIX, NULL);
    }

    if (!(algo->algo_mask & ALGORITHM_STREAM_USE_AEC)) {
        afe_config->aec_init = false;
    }

    if (algo->algo_mask & ALGORITHM_STREAM_USE_NS) {
        if (model_name) {
            afe_config->ns_init = true;
            afe_config->afe_ns_mode = AFE_NS_MODE_NET;
            afe_config->ns_model_name = model_name;
        }
    } else {
        afe_config->ns_init = false;
    }
    afe_config->se_init = algo->enable_se;

    if (algo->algo_mask & ALGORITHM_STREAM_USE_VAD) {
        afe_config->vad_init = true;
    } else {
        afe_config->vad_init = false;
    }

    if (algo->algo_mask & ALGORITHM_STREAM_USE_AGC) {
        afe_config->agc_init = true;
        afe_config->agc_mode = algo->agc_mode;
        // afe_config->agc_compression_gain_db = algo->agc_compression_gain_db;
        afe_config->agc_target_level_dbfs = algo->agc_target_level_dbfs;
    } else {
        afe_config->agc_init = false;
    }
    if (algo->sample_rate) {
        afe_config->pcm_config.sample_rate = algo->sample_rate;
    }
    algo->afe_handle = esp_afe_handle_from_config(afe_config);
    algo->afe_data = algo->afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    algo->afe_fetch_run = true;
    xEventGroupClearBits(algo->state, FETCH_STOPPED_BIT);

    if (algo->debug_input) {
        xEventGroupSetBits(algo->state, FETCH_STOPPED_BIT);
    } else {
        if (audio_thread_create(NULL, "algo_fetch", _algo_fetch_task, (void *)self, ALGORITHM_FETCH_TASK_STACK_SIZE,
                            ALGORITHM_STREAM_TASK_PERIOD, true, ALGORITHM_STREAM_PINNED_TO_CORE) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create algo_fetch task");
                _algo_close(self);
                return ESP_FAIL;
            }
    }
    AUDIO_NULL_CHECK(TAG, algo->afe_data, {
        _algo_close(self);
        return ESP_FAIL;
    });

    int audio_chunksize = algo->afe_handle->get_feed_chunksize(algo->afe_data);
    int nch = algo->afe_handle->get_feed_channel_num(algo->afe_data);
    algo->aec_buff_size = audio_chunksize * nch * sizeof(int16_t);
    algo->aec_buff = audio_calloc(1, algo->aec_buff_size);
    AUDIO_NULL_CHECK(TAG, algo->aec_buff, {
        _algo_close(self);
        return ESP_FAIL;
    });
    return ESP_OK;
}

static inline esp_err_t algorithm_data_gain(int16_t *raw_buff, int len, int linear_lfac, int linear_rfac)
{
    for (int i = 0; i < len / 4; i++) {
        raw_buff[i << 1] = raw_buff[i << 1] * linear_lfac;
        raw_buff[(i << 1) + 1] = raw_buff[(i << 1) + 1] * linear_rfac;
    }
    return ESP_OK;
}


static int algorithm_data_process_for_type1(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    int bytes_read = 0;

    bytes_read = audio_element_input(self, (char *)algo->aec_buff, algo->aec_buff_size);
    if (bytes_read > 0) {
        if (algo->debug_input) {
            audio_element_output(self, (char *)algo->aec_buff, algo->aec_buff_size);
        } else {
            algorithm_data_gain(algo->aec_buff, algo->aec_buff_size, algo->rec_linear_factor, algo->ref_linear_factor);
            algo->afe_handle->feed(algo->afe_data, algo->aec_buff);
        }
    }
    return bytes_read;
}

static int algorithm_data_process_for_type2(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    int bytes_read = 0;

    int audio_chunksize = algo->afe_handle->get_feed_chunksize(algo->afe_data);
    int size = audio_chunksize * sizeof(int16_t);

    memset(algo->reference, 0, size);
    bytes_read = audio_element_multi_input(self, (char *)algo->reference, size, 0, ALGORITHM_GET_REFERENCE_TIMEOUT);
    if (bytes_read == AEL_IO_TIMEOUT) {
        bytes_read = size;
    } else if (bytes_read < 0) {
        return bytes_read;
    }

    bytes_read = audio_element_input(self, (char *)algo->record, size);
    if (bytes_read > 0) {
        int16_t *temp = algo->aec_buff;
        for (int i = 0; i < (size / 2); i++) {
            temp[i << 1] = algo->record[i];
            temp[(i << 1) + 1] = algo->reference[i];
        }

        if (algo->debug_input) {
            audio_element_output(self, (char *)algo->aec_buff, 2 * size);
        } else {
            algo->afe_handle->feed(algo->afe_data, algo->aec_buff);
        }
    }
    return bytes_read;
}

static audio_element_err_t _algo_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int ret = ESP_OK;
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);

    if (algo->input_type == ALGORITHM_STREAM_INPUT_TYPE1) {
        ret = algorithm_data_process_for_type1(self);
    } else if (algo->input_type == ALGORITHM_STREAM_INPUT_TYPE2) {
        ret = algorithm_data_process_for_type2(self);
    } else {
        ESP_LOGE(TAG, "Type %d is not supported", algo->input_type);
        return AEL_IO_FAIL;
    }
    return ret;
}

audio_element_handle_t algo_stream_init(algorithm_stream_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);
    if ((config->rec_linear_factor <= 0) || (config->ref_linear_factor <= 0)) {
        ESP_LOGE(TAG, "The linear amplication factor should be greater than 0");
        return NULL;
    }
    audio_element_handle_t el = NULL;
    algo_stream_t *algo = (algo_stream_t *)audio_calloc(1, sizeof(algo_stream_t));
    AUDIO_NULL_CHECK(TAG, algo, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _algo_open;
    cfg.close = _algo_close;
    cfg.process = _algo_process;
    cfg.destroy = _algo_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.multi_in_rb_num = config->multi_in_rb_num;
    cfg.stack_in_ext = config->stack_in_ext;
    cfg.out_rb_size = config->out_rb_size;
    cfg.tag = "algorithm";

    algo->input_type = config->input_type;
    algo->algo_mask = config->algo_mask;
    algo->rec_linear_factor = config->rec_linear_factor;
    algo->ref_linear_factor = config->ref_linear_factor;
    algo->afe_type = config->afe_type;
    algo->debug_input = config->debug_input;
    algo->input_format = config->input_format;
    algo->sample_rate = config->sample_rate;
    algo->enable_se = config->enable_se;
    algo->agc_mode = config->agc_mode;
    algo->agc_compression_gain_db = config->agc_compression_gain_db;
    algo->agc_target_level_dbfs = config->agc_target_level_dbfs;
    algo->state = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, algo->state, goto _exit);

    el = audio_element_init(&cfg);
    AUDIO_NULL_CHECK(TAG, el, goto _exit);

    bool _success = true;
    if (algo->input_type == ALGORITHM_STREAM_INPUT_TYPE2) {
        _success &= ((algo->record = audio_calloc(1, ALGORITHM_CHUNK_MAX_SIZE)) != NULL);
        _success &= ((algo->reference = audio_calloc(1, ALGORITHM_CHUNK_MAX_SIZE)) != NULL);
    }

    algo->models = esp_srmodel_init(config->partition_label);
    if (algo->models != NULL) {
        for (int i = 0; i < algo->models->num; i++) {
            ESP_LOGI(TAG, "Load: %s", algo->models->model_name[i]);
        }
    } else {
        ESP_LOGE(TAG, "Failed to load models");
    }
    audio_element_setdata(el, algo);
    return el;

_exit:
    AUDIO_SAFE_FREE(algo->record, audio_free);
    AUDIO_SAFE_FREE(algo->reference, audio_free);
    AUDIO_SAFE_FREE(algo->state, vEventGroupDelete);
    AUDIO_SAFE_FREE(el, audio_element_deinit);
    AUDIO_SAFE_FREE(algo, audio_free);
    return NULL;
}

audio_element_err_t algo_stream_set_delay(audio_element_handle_t el, ringbuf_handle_t ringbuf, int delay_ms)
{
    AUDIO_NULL_CHECK(TAG, el, return ESP_ERR_INVALID_ARG);
    AUDIO_NULL_CHECK(TAG, ringbuf, return ESP_ERR_INVALID_ARG);

    if (delay_ms > 0) {
        audio_element_info_t info;
        audio_element_getinfo(el, &info);

        uint32_t delay_size = delay_ms * ((uint32_t)(info.sample_rates * info.channels * info.bits / 8) / 1000);
        char *in_buffer = (char *)audio_calloc(1, delay_size);
        AUDIO_MEM_CHECK(TAG, in_buffer, return ESP_FAIL);
        if (rb_write(ringbuf, in_buffer, delay_size, 0) <= 0) {
            ESP_LOGW(TAG, "Can't set ringbuf delay, please make sure element ringbuf size is enough!");
        }
        audio_free(in_buffer);
    }

    return ESP_OK;
}
