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
#include "esp_log.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "esp_aec.h"
#include "esp_agc.h"
#include "esp_ns.h"
#include "esp_resample.h"
#include "algorithm_stream.h"

#define NS_FRAME_BYTES       320   // 10ms data frame (10 * 16 * 2)
#define AGC_FRAME_BYTES      320   // 10ms data frame (10 * 16 * 2)
#define AEC_FRAME_BYTES      512   // 16ms data frame (16 * 16 * 2)

#define ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ   16000 //Hz
#define ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT       16
#define ALGORITHM_STREAM_DEFAULT_CHANNEL          1

#define ALGORITHM_STREAM_DEFAULT_AGC_MODE         3
#define ALGORITHM_STREAM_DEFAULT_AGC_FRAME_LENGTH 10    //ms

#define ALGORITHM_STREAM_RESAMPE_DEFAULT_COMPLEXITY     0
#define ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE (AEC_FRAME_BYTES * 8)

#define ALGORITHM_STREAM_FULL_FUCTION_MASK  (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS)

static const char *TAG = "ALGORITHM_STREAM";

typedef struct {
    void *rsp_handle;
    unsigned char *rsp_in;
    unsigned char *rsp_out;
    int  in_offset;
    int  ap_factor;
    int16_t *aec_buff;
    resample_info_t rsp_info;
    ringbuf_handle_t input_rb;
    bool data_need_be_resampled;
    bool data_need_be_divided_after_rsp;  //The encode mode of resample function doesn't support change channels
} algorithm_data_info_t;

typedef struct {
    void *ns_handle;
    void *aec_handle;
    void *agc_handle;
    char *scale_buff;
    int16_t *aec_buff;
    int16_t *ns_buff;
    int16_t *agc_buff;
    int8_t algo_mask;
    algorithm_data_info_t record;
    algorithm_data_info_t reference;
    algorithm_stream_input_type_t input_type;
} algo_stream_t;

static esp_err_t algorithm_data_info_destroy(algorithm_data_info_t *data_info)
{
    static void *rsp_handle;
    if (rsp_handle == data_info->rsp_handle) {  // Avoid the rsp handle be destroyed twice.
        return ESP_OK;
    }
    if (data_info->rsp_handle) {
        rsp_handle = data_info->rsp_handle;
        esp_resample_destroy(data_info->rsp_handle);
        data_info->rsp_handle = NULL;
    }
    if (data_info->aec_buff) {
        free(data_info->aec_buff);
        data_info->aec_buff = NULL;
    }
    return ESP_OK;
}

static esp_err_t _algo_close(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    algorithm_data_info_t *record    = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;

    if (algo->ns_handle) {
        ns_destroy(algo->ns_handle);
        algo->ns_handle = NULL;
    }
    if (algo->aec_handle) {
        aec_destroy(algo->aec_handle);
        algo->aec_handle = NULL;
    }
    if (algo->agc_handle) {
        esp_agc_clse(algo->agc_handle);
        algo->agc_handle = NULL;
    }
    if (algo->ns_buff) {
        free(algo->ns_buff);
        algo->ns_buff = NULL;
    }
    if (algo->aec_buff) {
        free(algo->aec_buff);
        algo->aec_buff = NULL;
    }
    if (algo->agc_buff) {
        free(algo->agc_buff);
        algo->agc_buff = NULL;
    }
    if (algo->scale_buff) {
        free(algo->scale_buff);
        algo->scale_buff = NULL;
    }

    algorithm_data_info_destroy(record);
    algorithm_data_info_destroy(reference);

    return ESP_OK;
}

static esp_err_t _algo_open(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    bool _success = true;

    if (algo->algo_mask & ALGORITHM_STREAM_USE_AEC) {
        _success &= ((algo->aec_handle = aec_create(ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ, AEC_FRAME_LENGTH_MS, AEC_FILTER_LENGTH)) != NULL);
    }
    if (algo->algo_mask & ALGORITHM_STREAM_USE_AGC) {
        _success &= ((algo->agc_handle = esp_agc_open(ALGORITHM_STREAM_DEFAULT_AGC_MODE, ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ)) != NULL);
    }
    if (algo->algo_mask & ALGORITHM_STREAM_USE_NS) {
        _success &= ((algo->ns_handle = ns_create(ALGORITHM_STREAM_DEFAULT_AGC_FRAME_LENGTH)) != NULL);
    }
    AUDIO_NULL_CHECK(TAG, _success, {
        _algo_close(self);
        return ESP_FAIL;
    });
    return ESP_OK;
}

static int algorithm_process(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    static int copy_cnt, cur_pos;
    algorithm_data_info_t *record    = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;

    if (algo->algo_mask & ALGORITHM_STREAM_USE_AEC) {
        aec_process(algo->aec_handle, record->aec_buff, reference->aec_buff, algo->aec_buff);
        memcpy(algo->scale_buff + cur_pos, algo->aec_buff, AEC_FRAME_BYTES);
        cur_pos += AEC_FRAME_BYTES;
        copy_cnt = cur_pos / AGC_FRAME_BYTES;

        for (int i = 0; i < copy_cnt; i++) {
            if ((algo->algo_mask & ALGORITHM_STREAM_USE_NS) && (algo->algo_mask & ALGORITHM_STREAM_USE_AGC)) {
                ns_process(algo->ns_handle, (int16_t *)algo->scale_buff + i * (NS_FRAME_BYTES >> 1), algo->ns_buff);
                esp_agc_process(algo->agc_handle, algo->ns_buff, algo->agc_buff, (AGC_FRAME_BYTES >> 1), ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ);
                audio_element_output(self, (char *)algo->agc_buff, AGC_FRAME_BYTES);
            } else if (algo->algo_mask & ALGORITHM_STREAM_USE_NS) {
                ns_process(algo->ns_handle, (int16_t *)algo->scale_buff + i * (NS_FRAME_BYTES >> 1), algo->ns_buff);
                audio_element_output(self, (char *)algo->ns_buff, NS_FRAME_BYTES);
            } else if (algo->algo_mask & ALGORITHM_STREAM_USE_AGC) {
                esp_agc_process(algo->agc_handle, (int16_t *)algo->scale_buff + i * (AGC_FRAME_BYTES >> 1), algo->agc_buff, (AGC_FRAME_BYTES >> 1), ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ);
                audio_element_output(self, (char *)algo->ns_buff, AGC_FRAME_BYTES);
            } else {
                audio_element_output(self, (char *)algo->aec_buff, AEC_FRAME_BYTES);
                cur_pos -= AEC_FRAME_BYTES;
                return AEC_FRAME_BYTES;
            }
        }
        memcpy(algo->scale_buff, algo->scale_buff + AGC_FRAME_BYTES * copy_cnt, cur_pos - AGC_FRAME_BYTES * copy_cnt);
        cur_pos -= AGC_FRAME_BYTES * copy_cnt;
        return AGC_FRAME_BYTES * copy_cnt;
    } else {
        if((algo->algo_mask & ALGORITHM_STREAM_USE_AGC) && (algo->algo_mask & ALGORITHM_STREAM_USE_NS)) {
            ns_process(algo->ns_handle, (int16_t *)algo->scale_buff, algo->ns_buff);
            esp_agc_process(algo->agc_handle, algo->ns_buff, algo->agc_buff, (AGC_FRAME_BYTES >> 1), ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ);
            audio_element_output(self, (char *)algo->agc_buff, AGC_FRAME_BYTES);
            return AGC_FRAME_BYTES;
        } else if (algo->algo_mask & ALGORITHM_STREAM_USE_NS) {
            ns_process(algo->ns_handle, (int16_t *)algo->scale_buff, algo->ns_buff);
            audio_element_output(self, (char *)algo->ns_buff, NS_FRAME_BYTES);
            return NS_FRAME_BYTES;
        } else if (algo->algo_mask & ALGORITHM_STREAM_USE_AGC) {
            esp_agc_process(algo->agc_handle, (int16_t *)algo->scale_buff, algo->agc_buff, (AGC_FRAME_BYTES >> 1), ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ);
            audio_element_output(self, (char *)algo->agc_buff, AGC_FRAME_BYTES);
            return AGC_FRAME_BYTES;
        } else {
            return AEL_IO_FAIL;
        }
    }
}

static esp_err_t algorithm_data_divided(int16_t *raw_buff, int len, int16_t *left_channel, int linear_lfac, int16_t *right_channel, int linear_rfac)
{
    // To improve efficiency, data splitting and linear amplification are integrated into one function
    for (int i = 0; i < len / 4; i++) {
        if (left_channel) {
            left_channel[i]  = raw_buff[i << 1] * linear_lfac;
        }
        if (right_channel) {
            right_channel[i] = raw_buff[(i << 1) + 1] * linear_rfac;
        }
    }
    return ESP_OK;
}

static esp_err_t algorithm_data_linear_amplication(int16_t *raw_buff, int len, int linear_factor)
{
    for (int i = 0; i < len / 2; i++) {
        raw_buff[i] *= linear_factor;
    }
    return ESP_OK;
}

static int algorithm_data_process_for_type1(audio_element_handle_t self)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    algorithm_data_info_t *record = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;
    int in_ret = 0, bytes_consume = 0, out_len_bytes = record->rsp_info.out_len_bytes;
    char tmp_buffer[2 * AEC_FRAME_BYTES] = {0};

    if (record->data_need_be_resampled) { // When use input type1, use record or reference handle is the same.
        if (record->in_offset > 0) {
            memmove(record->rsp_in, &record->rsp_in[ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE - record->in_offset], record->in_offset);
        }
        in_ret = audio_element_input(self, (char *)&record->rsp_in[record->in_offset], ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE - record->in_offset);
        if (in_ret <= 0) {
            return in_ret;
        } else {
            record->in_offset += in_ret;
            bytes_consume = esp_resample_run(record->rsp_handle, (void *)&record->rsp_info,
                                             record->rsp_in, record->rsp_out,
                                             record->in_offset, &out_len_bytes);
            ESP_LOGD(TAG, "in_ret = %d, out_len_bytes = %d, bytes_consume = %d", in_ret, out_len_bytes, bytes_consume);

            if ((bytes_consume > 0) && (out_len_bytes == record->rsp_info.out_len_bytes)) {
                record->in_offset -= bytes_consume;
                algorithm_data_divided((int16_t *)record->rsp_out, out_len_bytes, reference->aec_buff, reference->ap_factor, record->aec_buff, record->ap_factor);
                return algorithm_process(self);
            } else {
                ESP_LOGE(TAG, "Fail to resample");
                return AEL_IO_FAIL;
            }
        }
    } else {
        in_ret = audio_element_input(self, tmp_buffer, AEC_FRAME_BYTES * 2);
        if (in_ret <= 0) {
            return in_ret;
        } else {
            algorithm_data_divided((int16_t *)tmp_buffer, AEC_FRAME_BYTES * 2, reference->aec_buff, reference->ap_factor, record->aec_buff, record->ap_factor);
            return algorithm_process(self);
        }
    }
}

static int algorithm_data_pre_process_for_type2(audio_element_handle_t self, algorithm_data_info_t *data_info, int input_buffer_index)
{
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    int in_ret = 0, bytes_consume = 0, out_len_bytes = data_info->rsp_info.out_len_bytes, basic_frame_size;
    char tmp_buffer[2 * AEC_FRAME_BYTES] = {0};
    int16_t *input_buffer;

    if (algo->algo_mask & ALGORITHM_STREAM_USE_AEC) {
        input_buffer = data_info->aec_buff;
        basic_frame_size = AEC_FRAME_BYTES;
    } else {
        basic_frame_size = AGC_FRAME_BYTES;
        input_buffer = (int16_t *)algo->scale_buff;
    }

    if (data_info->data_need_be_resampled) {
        if (data_info->in_offset > 0) {
            memmove(data_info->rsp_in, &data_info->rsp_in[ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE - data_info->in_offset], data_info->in_offset);
        }
        if (input_buffer_index < 0) {
            in_ret = audio_element_input(self, (char *)&data_info->rsp_in[data_info->in_offset], ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE - data_info->in_offset);
        } else {
            in_ret = audio_element_multi_input(self, (char *)&data_info->rsp_in[data_info->in_offset], ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE - data_info->in_offset, input_buffer_index, portMAX_DELAY);
        }
        if (in_ret <= 0) {
            return in_ret;
        } else {
            data_info->in_offset += in_ret;
            bytes_consume = esp_resample_run(data_info->rsp_handle, (void *)&data_info->rsp_info,
                                             data_info->rsp_in, data_info->rsp_out,
                                             data_info->in_offset, &out_len_bytes);

            ESP_LOGD(TAG, "in_ret = %d, out_len_bytes = %d, bytes_consume = %d", in_ret, out_len_bytes, bytes_consume);
            if ((bytes_consume > 0) && (out_len_bytes == data_info->rsp_info.out_len_bytes)) {
                data_info->in_offset -= bytes_consume;
                if (data_info->data_need_be_divided_after_rsp) {
                    algorithm_data_divided((int16_t *)data_info->rsp_out, out_len_bytes, input_buffer, data_info->ap_factor, NULL, 0);
                } else {
                    memcpy(input_buffer, data_info->rsp_out, out_len_bytes);
                    algorithm_data_linear_amplication(input_buffer, out_len_bytes, data_info->ap_factor);
                }
            } else {
                ESP_LOGE(TAG, "Fail to resample");
                return AEL_IO_FAIL;
            }
        }
    } else {
        if (data_info->data_need_be_divided_after_rsp) {
            if (input_buffer_index < 0) {
                in_ret = audio_element_input(self, tmp_buffer, basic_frame_size * 2);
            } else {
                in_ret = audio_element_multi_input(self, tmp_buffer, basic_frame_size * 2, input_buffer_index, portMAX_DELAY);
            }
            if (in_ret <= 0) {
                return in_ret;
            } else {
                algorithm_data_divided((int16_t *)tmp_buffer, basic_frame_size * 2, input_buffer, data_info->ap_factor, NULL, 0);
            }
        } else {
            if (input_buffer_index < 0) {
                in_ret = audio_element_input(self, (char *)input_buffer, basic_frame_size);
            } else {
                in_ret = audio_element_multi_input(self, (char *)input_buffer, basic_frame_size, input_buffer_index, portMAX_DELAY);
            }
            if (in_ret <= 0) {
                return in_ret;
            } else {
                algorithm_data_linear_amplication(input_buffer, basic_frame_size, data_info->ap_factor);
            }
        }
    }
    return basic_frame_size;
}

static int algorithm_data_process_for_type2(audio_element_handle_t self)
{
    int ret = 0;
    algo_stream_t *algo = (algo_stream_t *)audio_element_getdata(self);
    algorithm_data_info_t *record = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;

    if (algo->algo_mask & ALGORITHM_STREAM_USE_AEC) {
        ret |= algorithm_data_pre_process_for_type2(self, reference, 0);
    }
    ret |= algorithm_data_pre_process_for_type2(self, record, -1);
    if (ret <= 0) {
        return ret;
    }
    return algorithm_process(self);
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

static esp_err_t algorithm_resample_config(algorithm_data_info_t *data_info, algorithm_stream_input_type_t type, int src_fre, int src_ch)
{
    if (type == ALGORITHM_STREAM_INPUT_TYPE1) {
        data_info->data_need_be_divided_after_rsp = false;
        if (src_fre != ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ) {
            data_info->data_need_be_resampled = true;
        } else {
            data_info->data_need_be_resampled = false;
        }
        return ESP_OK;
    }

    if ((src_fre == ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ) && (src_ch == ALGORITHM_STREAM_DEFAULT_CHANNEL)) {
        data_info->data_need_be_resampled = false;
        data_info->data_need_be_divided_after_rsp = false;
    } else if (src_fre == ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ) {
        data_info->data_need_be_resampled = false;
        data_info->data_need_be_divided_after_rsp = true;
    } else {
        data_info->data_need_be_resampled = true;
        if (src_ch == 2) {
            data_info->data_need_be_divided_after_rsp = true;
        } else if (src_ch == 1) {
            data_info->data_need_be_divided_after_rsp = false;
        } else {
            ESP_LOGE(TAG, "The channel number should be 0 or 1");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static esp_err_t create_rsp_handle(algo_stream_t *algo, algorithm_stream_cfg_t *cfg)
{
    AUDIO_NULL_CHECK(TAG, algo, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, cfg, return ESP_FAIL);

    algorithm_data_info_t *record    = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;

    resample_info_t rsp_cfg = {
        .dest_rate        = ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ,
        .dest_ch          = ALGORITHM_STREAM_DEFAULT_CHANNEL,                  // The encode resample cannot process diffrent channel
        .mode             = RESAMPLE_ENCODE_MODE,
        .sample_bits      = ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT,
        .max_indata_bytes = ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE,   // The max input data maybe 48K 2ch --> 16k 1ch, so max_data = AEC_FRAME_BYTES * 6
        .complexity       = ALGORITHM_STREAM_RESAMPE_DEFAULT_COMPLEXITY,
        .type             = ESP_RESAMPLE_TYPE_AUTO
    };

    algorithm_resample_config(record, cfg->input_type, cfg->rec_sample_rate, cfg->rec_ch);
    algorithm_resample_config(reference, cfg->input_type, cfg->ref_sample_rate, cfg->ref_ch);

    if (algo->input_type == ALGORITHM_STREAM_INPUT_TYPE1) {
        if (record->data_need_be_resampled) {
            rsp_cfg.src_rate = cfg->rec_sample_rate;
            rsp_cfg.src_ch   = 2;
            rsp_cfg.dest_ch  = 2;
            rsp_cfg.out_len_bytes = AEC_FRAME_BYTES * 2;
            memcpy(&record->rsp_info, &rsp_cfg, sizeof(resample_info_t));
            record->rsp_handle = esp_resample_create(&rsp_cfg, &record->rsp_in, &record->rsp_out);
            AUDIO_NULL_CHECK(TAG, record->rsp_handle, {
                ESP_LOGE(TAG, "Fail to create resample handle");
                return ESP_FAIL;
            });
            return ESP_OK;
        } else {
            return ESP_OK;
        }
    }

    if (record->data_need_be_resampled) {
        rsp_cfg.src_rate = cfg->rec_sample_rate;
        rsp_cfg.dest_ch = cfg->rec_ch;
        rsp_cfg.src_ch = cfg->rec_ch;
        if (record->data_need_be_divided_after_rsp) {
            if (cfg->algo_mask & ALGORITHM_STREAM_USE_AEC) {
                rsp_cfg.out_len_bytes = AEC_FRAME_BYTES * 2;
            } else {
                rsp_cfg.out_len_bytes = AGC_FRAME_BYTES * 2;
            }
        } else {
            if (cfg->algo_mask & ALGORITHM_STREAM_USE_AEC) {
                rsp_cfg.out_len_bytes = AEC_FRAME_BYTES;
            } else {
                rsp_cfg.out_len_bytes = AGC_FRAME_BYTES;
            }
        }

        memcpy(&record->rsp_info, &rsp_cfg, sizeof(resample_info_t));
        record->rsp_handle = esp_resample_create(&rsp_cfg, &record->rsp_in, &record->rsp_out);
        AUDIO_NULL_CHECK(TAG, record->rsp_handle, {
            ESP_LOGE(TAG, "Fail to create recorder resample handle");
            return ESP_FAIL;
        });
    } else {
        return ESP_OK;
    }
    if (reference->data_need_be_resampled) {
        rsp_cfg.src_rate = cfg->ref_sample_rate;
        rsp_cfg.dest_ch = cfg->ref_ch;
        rsp_cfg.src_ch = cfg->ref_ch;
        if (reference->data_need_be_divided_after_rsp) {
            if (cfg->algo_mask & ALGORITHM_STREAM_USE_AEC) {
                rsp_cfg.out_len_bytes = AEC_FRAME_BYTES * 2;
            } else {
                rsp_cfg.out_len_bytes = AGC_FRAME_BYTES * 2;
            }
        } else {
            if (cfg->algo_mask & ALGORITHM_STREAM_USE_AEC) {
                rsp_cfg.out_len_bytes = AEC_FRAME_BYTES;
            } else {
                rsp_cfg.out_len_bytes = AGC_FRAME_BYTES;
            }
        }
        memcpy(&reference->rsp_info, &rsp_cfg, sizeof(resample_info_t));
        reference->rsp_handle = esp_resample_create(&rsp_cfg, &reference->rsp_in, &reference->rsp_out);
        AUDIO_NULL_CHECK(TAG, reference->rsp_handle, {
            esp_resample_destroy(record->rsp_handle);
            record->rsp_handle = NULL;
            ESP_LOGE(TAG, "Fail to create reference resample handle");
            return ESP_FAIL;
        });
    } else {
        return ESP_OK;
    }
    return ESP_OK;
}

esp_err_t algo_stream_set_multi_input_rb(audio_element_handle_t algo_handle, ringbuf_handle_t input_rb)
{
    AUDIO_NULL_CHECK(TAG, algo_handle, return ESP_FAIL);
    AUDIO_NULL_CHECK(TAG, input_rb, return ESP_FAIL);

    if (rb_get_size(input_rb) < ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE) {
        ESP_LOGE(TAG, "The ringbuffer size should be better than %d", ALGORITHM_STREAM_RESAMPE_DEFAULT_MAX_INPUT_SIZE);
        return ESP_FAIL;
    }

    return audio_element_set_multi_input_ringbuf(algo_handle, input_rb, 0);
}

audio_element_handle_t algo_stream_init(algorithm_stream_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);
    if ((config->rec_linear_factor <= 0) || (config->ref_linear_factor <= 0)) {
        ESP_LOGE(TAG, "The linear amplication factor should be greater than 0");
        return NULL;
    }
    if ((config->algo_mask < 0) || (config->algo_mask > ALGORITHM_STREAM_FULL_FUCTION_MASK)) { //
        ESP_LOGE(TAG, "Please choose a reasonable mask value");
        return NULL;
    }
    algo_stream_t *algo = (algo_stream_t *)audio_calloc(1, sizeof(algo_stream_t));
    AUDIO_NULL_CHECK(TAG, algo, return NULL);
    algorithm_data_info_t *record = &algo->record;
    algorithm_data_info_t *reference = &algo->reference;

    record->ap_factor = config->rec_linear_factor;
    reference->ap_factor = config->ref_linear_factor;

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _algo_open;
    cfg.close = _algo_close;
    cfg.process = _algo_process;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.multi_in_rb_num = config->input_type;
    cfg.tag = "algorithm";
    if (config->input_type == ALGORITHM_STREAM_INPUT_TYPE1) {
        if ((config->ref_sample_rate != config->rec_sample_rate) || (config->ref_ch != config->rec_ch)) {
            ESP_LOGE(TAG, "The frequence and channel number should be the same, please check about that!");
            free(algo);
            return NULL;
        }
        if (config->algo_mask != (ALGORITHM_STREAM_USE_AEC | ALGORITHM_STREAM_USE_AGC | ALGORITHM_STREAM_USE_NS)) {
            ESP_LOGE(TAG, "When type1 is choosen, both these algorithms should be used");
            free(algo);
            return NULL;
        }
    }

    cfg.buffer_len = AEC_FRAME_BYTES;
    algo->input_type = config->input_type;
    algo->algo_mask = config->algo_mask;
    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_NULL_CHECK(TAG, el, {
        free(algo);
        return NULL;
    });
    bool _success = true;
    _success &= (create_rsp_handle(algo, config) == ESP_OK);
    _success &= ((algo->scale_buff = audio_calloc(1, AEC_FRAME_BYTES + AGC_FRAME_BYTES)) != NULL);
    if (algo->algo_mask & ALGORITHM_STREAM_USE_AEC) {
        _success &= (
                        (algo->aec_buff = audio_calloc(1, AEC_FRAME_BYTES)) &&
                        (record->aec_buff = audio_calloc(1, AEC_FRAME_BYTES)) &&
                        (reference->aec_buff = audio_calloc(1, AEC_FRAME_BYTES))
                    );
    }
    if (algo->algo_mask & ALGORITHM_STREAM_USE_AGC) {
        _success &= ((algo->agc_buff = audio_calloc(1, AGC_FRAME_BYTES)) != NULL);
    }
    if (algo->algo_mask & ALGORITHM_STREAM_USE_NS) {
        _success &= ((algo->ns_buff = audio_calloc(1, NS_FRAME_BYTES)) != NULL);

    }
    AUDIO_NULL_CHECK(TAG, _success, {
        ESP_LOGE(TAG, "Error occured");
        _algo_close(el);
        free(algo);
        return NULL;
    });

    audio_element_setdata(el, algo);
    return el;
}
