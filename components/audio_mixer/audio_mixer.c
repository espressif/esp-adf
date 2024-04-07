/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "audio_thread.h"
#include "audio_error.h"
#include "audio_mixer.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "esp_downmix.h"
#include "sys/queue.h"

static const char *TAG = "AUDIO_MIXER";

#define CHECK_OUT_OF_RANGE(x, a, b)    (((x)<(a)) || ((x)>(b)))
#define DEFUALT_AUDIO_MIXER_SAMOLE_NUM (512)
#define MIXER_DATA_READY_BIT           BIT(0)
#define MIXER_DESTROY_BIT              BIT(1)
#define MIXER_DESTROYED                BIT(2)
#define PREVIOUS_RETURN_VALID          (1)

typedef struct slot_info {
    STAILQ_ENTRY(slot_info)     next;
    uint8_t                     id;
    mixer_io_callback_t         read;
    void                        *ctx;
    int                         prv_ret;
} slot_info_t;

typedef STAILQ_HEAD(slot_list, slot_info) slot_list_t;

typedef struct {
    audio_mixer_state_t state;
    slot_list_t         slot_in;
    mixer_io_callback_t out;
    void               *out_ctx;
    EventGroupHandle_t  evt_sync;
    int                 evt_ticks;
    uint8_t             max_in_slot;
    int                 max_sample_num;
    esp_downmix_info_t  downmix_info;
    unsigned char     **inbuf;
    unsigned char      *outbuf;
    void               *downmix_handle;
    bool                reopen;
    bool                process_run;
    bool                destroy;
    bool                is_open;
    int                 channel;
} audio_mixer_t;

static esp_err_t audio_mixer_close(void *handle);

static esp_err_t audio_mixer_destroy(void *handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (mixer->downmix_info.source_info) {
        audio_free(mixer->downmix_info.source_info);
    }
    audio_free(mixer);
    return ESP_OK;
}

static esp_err_t audio_mixer_open(void *handle)
{
    ESP_LOGI(TAG, "Open an audio mixer");
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    mixer->inbuf = (unsigned char **)audio_calloc(1, mixer->max_in_slot * sizeof(unsigned char *));
    AUDIO_MEM_CHECK(TAG, mixer->inbuf, return ESP_ERR_NO_MEM);
    for (int i = 0; i < mixer->max_in_slot; i++) {
        mixer->inbuf[i] = (unsigned char *)audio_calloc(1, mixer->max_sample_num * mixer->channel * sizeof(short));
        AUDIO_MEM_CHECK(TAG, mixer->inbuf[i], goto _exit);
    }
    mixer->outbuf = (unsigned char *)audio_calloc(1, mixer->max_sample_num * mixer->channel * sizeof(short));
    AUDIO_NULL_CHECK(TAG, mixer->outbuf, goto _exit);

    mixer->reopen = true;
    mixer->downmix_handle = esp_downmix_open(&mixer->downmix_info);
    if (mixer->downmix_handle == NULL) {
        ESP_LOGE(TAG, "Downmix open failed");
        goto _exit;
    }
    return ESP_OK;
_exit:
    audio_mixer_close(handle);
    return ESP_FAIL;
}

static esp_err_t audio_mixer_close(void *handle)
{
    ESP_LOGD(TAG, "Close an audio mixer, h:%p", handle);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (mixer->downmix_handle != NULL) {
        esp_downmix_close(mixer->downmix_handle);
    }
    for (int i = 0; i < mixer->max_in_slot; i++) {
        if (mixer->inbuf[i] != NULL) {
            memset(mixer->inbuf[i], 0, mixer->max_sample_num * 2 * sizeof(short));
            audio_free(mixer->inbuf[i]);
            mixer->inbuf[i] = NULL;
        }
    }
    if (mixer->inbuf != NULL) {
        audio_free(mixer->inbuf);
        mixer->inbuf = NULL;
    }
    if (mixer->outbuf != NULL) {
        audio_free(mixer->outbuf);
        mixer->outbuf = NULL;
    }
    return ESP_OK;
}

static int audio_mixer_reset_downmix(void *handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (mixer->downmix_handle != NULL) {
        esp_downmix_close(mixer->downmix_handle);
    }
    mixer->downmix_handle = esp_downmix_open(&mixer->downmix_info);
    if (mixer->downmix_handle == NULL) {
        ESP_LOGE(TAG, "Downmix reset failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int audio_mixer_process(void *handle)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    int r_size = mixer->max_sample_num * sizeof(short);
    int ret = 0;
    if (mixer->reopen == true) {
        ret = audio_mixer_reset_downmix(mixer);
        if (ret != ESP_OK) {
            return ESP_ERR_MIXER_FAIL;
        }
        mixer->reopen = false;
        ESP_LOGI(TAG, "Reset the audio mixer");
        return ESP_ERR_MIXER_OK;
    }
    slot_info_t *ch_list, *tmp;
    int k = 0;
    uint8_t i = 0;
    STAILQ_FOREACH_SAFE(ch_list, &mixer->slot_in, next, tmp) {
        if (ch_list->read) {
            if (ch_list->prv_ret > 0) {
                memset(mixer->inbuf[i], 0, mixer->max_sample_num * mixer->channel * sizeof(short));
                ret = ch_list->read((uint8_t *)mixer->inbuf[i], r_size * mixer->channel, ch_list->ctx);
                ch_list->prv_ret = ret;
            }
        } else {
            ret = ESP_ERR_MIXER_FAIL;
        }
        if (ret > 0) {
            k++;
        }
        i++;
    }
    if (k == 0) { 
        mixer->out(mixer->outbuf, 0, mixer->out_ctx);
        return ESP_ERR_MIXER_NO_DATA;
    }
    ESP_LOGD(TAG, "valid_cnt:%d, I:%d", k, i);
    ret = esp_downmix_process(mixer->downmix_handle, mixer->inbuf, mixer->outbuf, mixer->max_sample_num, mixer->downmix_info.mode);
    if (ret < 0) {
        ESP_LOGE(TAG, "Downmix process fail");
        ret = ESP_ERR_MIXER_FAIL;
    }

    ret = mixer->out(mixer->outbuf, ret, mixer->out_ctx);
    if (ret >= 0) {
        ret = ESP_ERR_MIXER_OK;
    }
    return ret;
}

static void mixer_task(void *params)
{
    audio_mixer_t *mixer = (audio_mixer_t *)params;
    mixer->evt_ticks = portMAX_DELAY;
    bool wait_run = true;
    int ret = 0;
    mixer->is_open = false;

    while (wait_run) {
        mixer->state = AUDIO_MIXER_STATE_IDLE;
        EventBits_t uxBits = xEventGroupWaitBits(mixer->evt_sync, MIXER_DATA_READY_BIT | MIXER_DESTROY_BIT, true, false, mixer->evt_ticks);
        if (mixer->is_open == false) {
            ret = audio_mixer_open(mixer);
            if (ret != ESP_OK) {
                wait_run = false;
                break;
            }
            mixer->is_open = true;
        }
        if ((uxBits & MIXER_DATA_READY_BIT) == MIXER_DATA_READY_BIT) {
            mixer->process_run = true;
            mixer->state = AUDIO_MIXER_STATE_RUNNING;
            ESP_LOGI(TAG, "Going into mixing");
        }
        if ((uxBits & MIXER_DESTROY_BIT) == MIXER_DESTROY_BIT) {
            ESP_LOGW(TAG, "The task destroy bit has been set");
            mixer->destroy = false;
            wait_run = false;
            mixer->process_run = false;
            mixer->state = AUDIO_MIXER_STATE_IDLE;
        }
        while (mixer->process_run) {
            ret = audio_mixer_process(mixer);
            if (ret != ESP_OK) {
                if (ret == ESP_ERR_MIXER_NO_DATA) {
                    mixer->state = AUDIO_MIXER_STATE_IDLE;
                    ESP_LOGI(TAG, "No input data, will be blocked");
                } else {
                    mixer->state = AUDIO_MIXER_STATE_ERROR;
                    ESP_LOGE(TAG, "The audio_mixer_process got error, ret:%d", ret);
                }
                mixer->process_run = false;
                break;
            }
            if (mixer->destroy) {
                ESP_LOGW(TAG, "The task destroy bit has been set");
                wait_run = false;
                mixer->destroy = false;
                mixer->process_run = false;
                mixer->state = AUDIO_MIXER_STATE_IDLE;
                break;
            }
        }
    }
    if (mixer->is_open) {
        audio_mixer_close(mixer);
        mixer->is_open = false;
    }
    audio_mixer_destroy(mixer);
    xEventGroupSetBits(mixer->evt_sync, MIXER_DESTROYED);
    vTaskDelete(NULL);
}

esp_err_t audio_mixer_init(audio_mixer_config_t *config, void **handle)
{
    AUDIO_NULL_CHECK(TAG, config, return ESP_ERR_INVALID_ARG);
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    if (CHECK_OUT_OF_RANGE(config->channel, 1, 2)) {
        ESP_LOGE(TAG, "The channel number[%d] does not support", config->channel);
        return ESP_ERR_INVALID_ARG;
    }
    if (CHECK_OUT_OF_RANGE(config->sample_rate, SAMPLERATE_MIN, SAMPLERATE_MAX)) {
        ESP_LOGE(TAG, "Unsupported sample rate, %d", config->sample_rate);
        return ESP_ERR_INVALID_ARG;
    }
    if (CHECK_OUT_OF_RANGE(config->max_in_slot, 0, SOURCE_NUM_MAX)) {
        ESP_LOGE(TAG, "The maximum number of input stream out of range, %d", config->max_in_slot);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->max_in_slot <= 0) {
        ESP_LOGE(TAG, "The number of source stream is less than 1");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->max_sample_num <= 0) {
        config->max_sample_num = 512;
    }

    esp_err_t ret = ESP_OK;
    audio_mixer_t *mixer = audio_calloc(1, sizeof(audio_mixer_t));
    AUDIO_MEM_CHECK(TAG, mixer, return ESP_ERR_INVALID_ARG);
    mixer->evt_sync = xEventGroupCreate();
    AUDIO_MEM_CHECK(TAG, mixer->evt_sync, goto __mixer_init;);
    mixer->downmix_info.source_info = audio_calloc(config->max_in_slot, sizeof(esp_downmix_input_info_t));
    if (mixer->downmix_info.source_info == NULL) {
        ESP_LOGE(TAG, "Allocate downmix source_info failed");
        goto __mixer_init;
    }
    if (config->bits != 16) {
        ESP_LOGE(TAG, "The bits number of downmix source stream must be 16 bits. (line %d)", __LINE__);
        goto __mixer_init;
    }
    for (int i = 0; i < config->max_in_slot; ++i) {
        mixer->downmix_info.source_info[i].samplerate = config->sample_rate;
        mixer->downmix_info.source_info[i].channel = config->channel;
        mixer->downmix_info.source_info[i].bits_num = config->bits;
        if (config->channel > 0) {
            mixer->channel = config->channel;
        }
    }
    mixer->downmix_info.out_ctx = ESP_DOWNMIX_OUT_CTX_NORMAL;
    mixer->downmix_info.output_type = mixer->channel;
    mixer->downmix_info.mode = ESP_DOWNMIX_WORK_MODE_SWITCH_ON;
    mixer->downmix_info.source_num = config->max_in_slot;

    mixer->max_in_slot = config->max_in_slot;
    mixer->max_sample_num = config->max_sample_num;
    STAILQ_INIT(&mixer->slot_in);
    ret = audio_thread_create(NULL, "audio_mixer", mixer_task, mixer,
                              config->task_stack, config->task_prio, true, config->task_core);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Audio Mixer task thread create failed");
        goto __mixer_init;
    }
    mixer->state = AUDIO_MIXER_STATE_UNKNOWN;
    *handle = mixer;
    ESP_LOGI(TAG, "Audio mixer initialized, %p", mixer);
    return ESP_OK;
__mixer_init:
    if (mixer->evt_sync) {
        vEventGroupDelete(mixer->evt_sync);
    }
    if (mixer) {
        audio_free(mixer);
    }
    return ret;
}

esp_err_t audio_mixer_deinit(void *handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    mixer->destroy = false;
    xEventGroupSetBits(mixer->evt_sync, MIXER_DESTROY_BIT);
    xEventGroupWaitBits(mixer->evt_sync, MIXER_DESTROYED, true, false, portMAX_DELAY);
    if (mixer->evt_sync) {
        vEventGroupDelete(mixer->evt_sync);
    }
    return ESP_OK;
}

esp_err_t audio_mixer_configure_in(void *handle, audio_mixer_slot_info_t *input_slot, int num_of_slot)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (num_of_slot != mixer->max_in_slot) {
        ESP_LOGE(TAG, "The in slot number[%d] is not equal maximum of slot number[%d]",
                 num_of_slot, mixer->max_in_slot);
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < num_of_slot; ++i) {
        if (CHECK_OUT_OF_RANGE(input_slot[i].origin_gain_db, (float)GAIN_MIN, (float)GAIN_MAX)) {
            ESP_LOGE(TAG, "The origin gain [%d, 100] out of range [%d, %d]",
                     input_slot[i].origin_gain_db, GAIN_MIN, GAIN_MAX);
            return ESP_ERR_INVALID_ARG;
        }
        if (CHECK_OUT_OF_RANGE(input_slot[i].goal_gain_db, (float)GAIN_MIN, (float)GAIN_MAX)) {
            ESP_LOGE(TAG, "The goal gain [-100, %d] out of range [%d, %d]",
                     input_slot[i].goal_gain_db, GAIN_MIN, GAIN_MAX);
            return ESP_ERR_INVALID_ARG;
        }
        if (input_slot[i].transfer_time_ms < 0) {
            ESP_LOGE(TAG, "The transfer time [%d] need gather than 0", input_slot[i].transfer_time_ms);
            return ESP_ERR_INVALID_ARG;
        }
    }
    float cumulative_gain = 0;
    for (int i = 0; i < num_of_slot; i++) {
        mixer->downmix_info.source_info[i].gain[0] = input_slot[i].origin_gain_db;
        mixer->downmix_info.source_info[i].gain[1] = input_slot[i].goal_gain_db;
        mixer->downmix_info.source_info[i].transit_time = input_slot[i].transfer_time_ms;

        cumulative_gain += input_slot[i].goal_gain_db;
        slot_info_t *item = audio_calloc(1, sizeof(slot_info_t));
        AUDIO_MEM_CHECK(TAG, item, return ESP_ERR_NO_MEM);
        item->read = input_slot[i].read_cb;

        item->ctx = input_slot[i].cb_ctx;
        item->id = i;
        item->prv_ret = PREVIOUS_RETURN_VALID;
        STAILQ_INSERT_TAIL(&mixer->slot_in, item, next);
    }
    if (cumulative_gain > 0) {
        ESP_LOGE(TAG, "The cumulative gain is %f, which is larger than 0", cumulative_gain);
    }
    return ESP_OK;
}

esp_err_t audio_mixer_configure_out(void *handle, mixer_io_callback_t write_cb, void *ctx)
{
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    AUDIO_NULL_CHECK(TAG, mixer, return ESP_ERR_INVALID_ARG);
    AUDIO_NULL_CHECK(TAG, write_cb, return ESP_ERR_INVALID_ARG);
    mixer->out = write_cb;
    mixer->out_ctx = ctx;
    return ESP_OK;
}

esp_err_t audio_mixer_data_is_ready(void *handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    xEventGroupSetBits(mixer->evt_sync, MIXER_DATA_READY_BIT);
    return ESP_OK;
}

esp_err_t audio_mixer_get_state(void *handle, audio_mixer_state_t *state)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (state) {
        *state = mixer->state;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t audio_mixer_set_read_cb(void *handle, audio_mixer_slot_t slot, mixer_io_callback_t read, void *ctx)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (slot >= mixer->max_in_slot) {
        ESP_LOGE(TAG, "The ID of source number is out of range, line %d", __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    slot_info_t *ch_list, *tmp;
    STAILQ_FOREACH_SAFE(ch_list, &mixer->slot_in, next, tmp) {
        if (ch_list->id == slot) {
            ch_list->read = read;
            ch_list->ctx = ctx;
            ch_list->prv_ret = PREVIOUS_RETURN_VALID;
            break;
        }
    }
    return ESP_OK;
}

esp_err_t audio_mixer_reset_slot(void *handle, audio_mixer_slot_t slot)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (slot >= mixer->max_in_slot) {
        ESP_LOGE(TAG, "The index of source slot is out of range, idx:%d, line:%d", slot, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    slot_info_t *ch_list, *tmp;
    STAILQ_FOREACH_SAFE(ch_list, &mixer->slot_in, next, tmp) {
        if (ch_list->id == slot) {
            ch_list->prv_ret = PREVIOUS_RETURN_VALID;
            break;
        }
    }
    return ESP_OK;
}

esp_err_t audio_mixer_set_channel(void *handle, audio_mixer_slot_t slot, int channel_num)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (slot >= mixer->max_in_slot) {
        ESP_LOGE(TAG, "The index of source slot is out of range, idx:%d, line:%d", slot, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    if (CHECK_OUT_OF_RANGE(mixer->downmix_info.source_info[slot].channel, 1, 2)) {
        ESP_LOGE(TAG, "The channel number[%d] does not support", mixer->downmix_info.source_info[slot].channel);
        return ESP_ERR_INVALID_ARG;
    }
    if (mixer->downmix_info.source_info[slot].channel != channel_num) {
        mixer->downmix_info.source_info[slot].channel = channel_num;
        mixer->reopen = true;
    }
    return ESP_OK;
}

esp_err_t audio_mixer_set_gain(void *handle, audio_mixer_slot_t slot, float gain0, float gain1)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (slot >= mixer->max_in_slot) {
        ESP_LOGE(TAG, "The index of source slot is out of range, idx:%d, line:%d", slot, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    if (gain0 < GAIN_MIN || gain0 > GAIN_MAX || gain1 < GAIN_MIN || gain1 > GAIN_MAX) {
        ESP_LOGE(TAG, "The gain is out of range [%d, %d]", GAIN_MIN, GAIN_MAX);
        return ESP_ERR_INVALID_ARG;
    }
    if ((mixer->downmix_info.source_info[slot].gain[0] - gain0 < 0.01)
        && (mixer->downmix_info.source_info[slot].gain[0] - gain0 > 0.01)
        && (mixer->downmix_info.source_info[slot].gain[1] - gain1 < 0.01)
        && (mixer->downmix_info.source_info[slot].gain[1] - gain1 > 0.01)) {
        return ESP_OK;
    }
    mixer->downmix_info.source_info[slot].gain[0] = gain0;
    mixer->downmix_info.source_info[slot].gain[1] = gain1;
    mixer->reopen = true;
    return ESP_OK;
}

esp_err_t audio_mixer_set_transit_time(void *handle, audio_mixer_slot_t slot, int transit_time)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_mixer_t *mixer = (audio_mixer_t *)handle;
    if (slot >= mixer->max_in_slot) {
        ESP_LOGE(TAG, "The index of source slot is out of range, idx:%d, line:%d", slot, __LINE__);
        return ESP_ERR_INVALID_ARG;
    }
    if (transit_time < 0) {
        ESP_LOGE(TAG, "The set transit_time must be greater than or equal to zero (%d)", transit_time);
        return ESP_ERR_INVALID_ARG;
    }
    if (mixer->downmix_info.source_info[slot].transit_time != transit_time) {
        mixer->downmix_info.source_info[slot].transit_time = transit_time;
        mixer->reopen = true;
    }
    return ESP_OK;
}
