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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "audio_thread.h"
#include "cnv.h"
#include "pixel_renderer.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#define   CNV_AUDIO_DEFAULT_MAX_REC (3000000)
#define   CNV_STOP_WAITTIME         (8000 / portTICK_RATE_MS)

static const char *TAG = "CNV";
const static int CNV_STOPPED_BIT = BIT0;

cnv_handle_t *cnv_init(cnv_config_t *config)
{
    cnv_handle_t *handle = audio_calloc(1, sizeof(cnv_handle_t));
    AUDIO_NULL_CHECK(TAG, handle, goto _cnv_failed);

    /* Create source */
    handle->source_data = audio_calloc(config->source_size, 1);
    AUDIO_NULL_CHECK(TAG, handle->source_data, goto _cnv_failed);
    handle->source_data_cb = config->source_data_cb;
    AUDIO_CHECK(TAG, handle->source_data_cb, goto _cnv_failed, "The 'source_data_cb' not set");
    handle->source_size = config->source_size;
    AUDIO_CHECK(TAG, handle->source_size, goto _cnv_failed, "The 'source_size' not set");

    /* Create rgb */
    handle->total_leds = config->total_leds;
    handle->color = audio_calloc(1, sizeof(esp_color_rgb_t));
    AUDIO_NULL_CHECK(TAG, handle->color, goto _cnv_failed);
    memset(handle->color, 0, sizeof(esp_color_rgb_t));

    /* Create audio */
    handle->audio = audio_calloc(1, sizeof(cnv_audio_t));
    AUDIO_NULL_CHECK(TAG, handle->audio, goto _cnv_failed);
    handle->audio->audio_energy = 0;
    handle->audio->max_rec = 0;
    handle->audio->default_max_rec = CNV_AUDIO_DEFAULT_MAX_REC;
    handle->audio->min_sound_limit.energy_sum = config->min_energy_sum;
    handle->audio->samplerate = config->audio_samplerate;
    handle->audio->n_samples = config->n_samples;

    /* Create fft related array */
    handle->fft_array = config->fft_array;

    /* Create message */
    handle->output_data = audio_calloc(1, sizeof(cnv_data_t));
    AUDIO_NULL_CHECK(TAG, handle->output_data, goto _cnv_failed);
    handle->output_data->need_free = true;
    handle->output_data->command = 0;

    handle->task_stack = config->task_stack;
    handle->task_core = config->task_core;
    handle->task_prio = config->task_prio;
    handle->state_event = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, handle->state_event, goto _cnv_failed);
    STAILQ_INIT(&handle->pattern_list);
    return handle;
_cnv_failed:
    if (handle) {
        if (handle->source_data) {
            audio_free(handle->source_data);
        }
        if (handle->color) {
            audio_free(handle->color);
        }
        if (handle->audio) {
            audio_free(handle->audio);
        }
        if (handle->output_data) {
            audio_free(handle->output_data);
        }
        if (handle->state_event) {
            vEventGroupDelete(handle->state_event);
        }
        audio_free(handle);
    }
    return NULL;
}

esp_err_t cnv_deinit(cnv_handle_t *handle)
{
    if (handle) {
        cnv_pattern_func_t *item, *tmp;
        STAILQ_FOREACH_SAFE(item, &handle->pattern_list, next, tmp) {
                STAILQ_REMOVE(&handle->pattern_list, item, cnv_pattern_func_s, next);
                audio_free(item);
        }
        if (handle->source_data) {
            audio_free(handle->source_data);
        }
        if (handle->output_data) {
            audio_free(handle->output_data);
        }
        if (handle->audio) {
            audio_free(handle->audio);
        }
        if (handle->color) {
            audio_free(handle->color);
        }
        if (handle->state_event) {
            vEventGroupDelete(handle->state_event);
        }
        audio_free(handle);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief      Task of converting source data to RGB value.
 *
 * @param[in]  arg    The pointer of parameter
 */
static void cnv_task(void *arg)
{
    cnv_handle_t *handle = (cnv_handle_t *)arg;
    uint16_t in_len = handle->audio->n_samples;
    float *source_data = (float *)handle->source_data;

    while (handle->task_run) {
        ESP_LOGD(TAG, "CNV is running");
        handle->source_data_cb(source_data, (in_len * sizeof(float)), NULL);
        /* The following is the pattern function, which can be implemented by 'handle->fft_array' or 'handle->volume' */
        if (handle->cur_pattern != NULL) {
            handle->cur_pattern->cb(handle);
        } else {
            ESP_LOGW(TAG, "Pattern mode not found. Make sure this mode is registered with 'cnv_register_pattern_cb'");
            ESP_LOGW(TAG, "And calling the 'cnv_set_cur_pattern' function sets the current mode");
            handle->task_run = false;
        }
    }

    xEventGroupSetBits(handle->state_event, CNV_STOPPED_BIT);
    vTaskDelete(NULL);
}

esp_err_t cnv_run(cnv_handle_t *handle)
{
    handle->task_run = true;
    /* Create cnv task */
    esp_err_t ret = audio_thread_create(NULL, "cnv_task", cnv_task, handle, handle->task_stack, handle->task_prio, false, handle->task_core);
    return ret;
}

esp_err_t cnv_wait_for_stop(cnv_handle_t *handle)
{
    EventBits_t uxBits = xEventGroupWaitBits(handle->state_event, CNV_STOPPED_BIT, false, true, CNV_STOP_WAITTIME);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    if (uxBits & CNV_STOP_WAITTIME) {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t cnv_stop(cnv_handle_t *handle)
{
    /* Delete cnv task */
    handle->task_run = false;
    esp_err_t ret = cnv_wait_for_stop(handle);
    return ret;
}

esp_err_t cnv_set_source_data_cb(cnv_handle_t *handle, cnv_source_data_func fn)
{
    handle->source_data_cb = fn;
    return ESP_OK;
}

esp_err_t cnv_register_pattern_cb(cnv_handle_t *handle, cnv_pattern_func fn, const char *tag)
{
    cnv_pattern_func_t *pattern_item = (cnv_pattern_func_t *)audio_calloc(1, sizeof(cnv_pattern_func_t));
    AUDIO_NULL_CHECK(TAG, pattern_item, return ESP_FAIL);
    pattern_item->cb = fn;
    pattern_item->tag = tag;
    STAILQ_INSERT_TAIL(&handle->pattern_list, pattern_item, next);
    return ESP_OK;
}

esp_err_t cnv_unregister_pattern_cb(cnv_handle_t *handle, const char *pattern_tag)
{
    cnv_pattern_func_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &handle->pattern_list, next, tmp) {
        if (item->tag && strcasecmp(item->tag, pattern_tag) == 0) {
            STAILQ_REMOVE(&handle->pattern_list, item, cnv_pattern_func_s, next);
            audio_free(item);
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

cnv_pattern_func_t *cnv_get_pattern_by_tag(cnv_handle_t *handle, const char *pattern_tag)
{
    cnv_pattern_func_t *item;
    STAILQ_FOREACH(item, &handle->pattern_list, next) {
        if (item->tag && strcasecmp(item->tag, pattern_tag) == 0) {
            return item;
        }
    }
    return NULL;
}

esp_err_t cnv_get_pattern_count(cnv_handle_t *handle, uint16_t *out_count)
{
    cnv_pattern_func_t *item;
    uint16_t count = 0;
    STAILQ_FOREACH(item, &handle->pattern_list, next) {
        count ++;
    }
    *out_count = count;
    return ESP_OK;
}

esp_err_t cnv_set_cur_pattern(cnv_handle_t *handle, const char *pattern_tag)
{
    handle->cur_pattern = cnv_get_pattern_by_tag(handle, pattern_tag);
    return ESP_OK;
}

#endif
