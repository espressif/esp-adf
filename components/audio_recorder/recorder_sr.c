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
#include "freertos/event_groups.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "audio_error.h"
#include "audio_mem.h"
#include "audio_thread.h"

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"

#ifdef CONFIG_USE_MULTINET
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#endif

#include "recorder_sr.h"

#define FEED_TASK_DESTROY  (BIT(0))
#define FETCH_TASK_DESTROY (BIT(1))
#define FEED_TASK_RUNNING  (BIT(2))
#define FETCH_TASK_RUNNING (BIT(3))
#define DEFAULT_OUT_Q_LEN  (10)

static const char *TAG = "RECORDER_SR";

#if CONFIG_AFE_MIC_NUM == (1)
#define RECORDER_CHANNEL_NUM (2)
static const esp_afe_sr_iface_t *esp_afe = &esp_afe_sr_1mic;
#else
#define RECORDER_CHANNEL_NUM (4)
static const esp_afe_sr_iface_t *esp_afe = &esp_afe_sr_2mic;
#endif

#ifdef CONFIG_USE_MULTINET
static const esp_mn_iface_t *multinet = &MULTINET_MODEL;
#endif

typedef struct {
    int16_t *data; /*!< Data pointer*/
    int      len;  /*!< Data length */
} data_pack_t;

typedef struct __recorder_sr {
    int                   feed_task_core;
    int                   feed_task_prio;
    int                   feed_task_stack;
    int                   fetch_task_core;
    int                   fetch_task_prio;
    int                   fetch_task_stack;
    QueueHandle_t         out_queue;
    EventGroupHandle_t    events;
    bool                  feed_running;
    bool                  fetch_running;
    recorder_data_read_t  read;
    void                  *read_ctx;
    esp_afe_sr_data_t     *afe_handle;
    recorder_sr_monitor_t afe_monitor;
    void                  *afe_monitor_ctx;
    bool                  wwe_enable;
    bool                  vad_enable;
#ifdef CONFIG_USE_MULTINET
    model_iface_data_t    *mn_handle;
    recorder_sr_monitor_t mn_monitor;
    void                  *mn_monitor_ctx;
    bool                  mn_enable;
#endif /* CONFIG_USE_MULTINET */
    int8_t                idx_ch0;
    int8_t                idx_ch1;
    int8_t                idx_ref;
} recorder_sr_t;

static esp_err_t recorder_sr_output(recorder_sr_t *recorder_sr, void *buffer, int len);

static int8_t recorder_sr_find_input_ch_idx(uint8_t *order, uint8_t target_ch)
{
    for (int8_t idx = 0; idx < RECORDER_SR_MAX_INPUT_CH; idx++) {
        if (order[idx] == target_ch) {
            return idx;
        }
    }
    ESP_LOGW(TAG, "Can't match the target channel %d", target_ch);
    return -1;
}

static void recorder_sr_input_ch_pick(recorder_sr_t *recorder_sr, int16_t *buffer, int len)
{
    if (RECORDER_CHANNEL_NUM == 4) {
        if (recorder_sr->idx_ch0 == 0 && recorder_sr->idx_ch1 == 1 && recorder_sr->idx_ref == 2) {
            ESP_LOGV(TAG, "Same as needed for 4ch");
            return;
        }

        for (int i = 0; i < (len / 8); i++) {
            int16_t ref = 0;
            if (recorder_sr->idx_ref != -1) {
                ref = buffer[4 * i + recorder_sr->idx_ref];
            }
            buffer[3 * i + 0] = buffer[4 * i + recorder_sr->idx_ch0];
            buffer[3 * i + 1] = buffer[4 * i + recorder_sr->idx_ch1];
            buffer[3 * i + 2] = ref;
        }
    } else if (RECORDER_CHANNEL_NUM == 2) {
        if (recorder_sr->idx_ch0 == 0 && recorder_sr->idx_ref == 1) {
            ESP_LOGV(TAG, "Same as needed for 2ch");
            return;
        }
        for (int i = 0; i < (len / 4); i++) {
            int16_t ref = 0;
            if (recorder_sr->idx_ref != -1) {
                ref = buffer[2 * i + recorder_sr->idx_ref];
            }
            buffer[2 * i] = buffer[2 * i + 1];
            buffer[2 * i + 1] = ref;
        }
    } else {
        ESP_LOGE(TAG, "unknown channel num");
    }
}

static inline int recorder_sr_afe_result_convert(recorder_sr_t *recorder_sr, int result)
{
    int ret = SR_RESULT_UNKNOW;
    switch (result) {
        case AFE_FETCH_CHANNEL_VERIFIED:
            ret = SR_RESULT_VERIFIED;
            break;
        case AFE_FETCH_NOISE:
            if (recorder_sr->vad_enable) {
                ret = SR_RESULT_NOISE;
            } else {
                ret = SR_RESULT_SPEECH;
            }
            break;
        case AFE_FETCH_SPEECH:
            ret = SR_RESULT_SPEECH;
            break;
        case AFE_FETCH_WWE_DETECTED:
            ret = SR_RESULT_WAKEUP;
            break;
        default:
            break;
    }
    return ret;
}

#ifdef CONFIG_USE_MULTINET
static esp_err_t recorder_mn_detect(recorder_sr_t *recorder_sr, int16_t *buffer, int afe_res)
{
    static int detect_flag = 0;
    int command_id = -3;
    if (!recorder_sr->mn_enable) {
        if (detect_flag == 1) {
            esp_afe->enable_wakenet(recorder_sr->afe_handle);
            esp_afe->enable_aec(recorder_sr->afe_handle);
            detect_flag = 0;
        }
        return ESP_OK;
    }
#if CONFIG_IDF_TARGET_ESP32
    if (afe_res == AFE_FETCH_WWE_DETECTED) {
        detect_flag = 1;
        esp_afe->disable_wakenet(recorder_sr->afe_handle);
        esp_afe->disable_aec(recorder_sr->afe_handle);
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    if (afe_res == AFE_FETCH_CHANNEL_VERIFIED) {
        detect_flag = 1;
        esp_afe->disable_wakenet(recorder_sr->afe_handle);
        esp_afe->disable_aec(recorder_sr->afe_handle);
    }
#endif
    if (detect_flag == 1) {
        command_id = multinet->detect(recorder_sr->mn_handle, buffer);
        if (command_id > -1) {
            if (recorder_sr->mn_monitor) {
                recorder_sr->mn_monitor(command_id, recorder_sr->mn_monitor_ctx);
            }
            esp_afe->enable_wakenet(recorder_sr->afe_handle);
            esp_afe->enable_aec(recorder_sr->afe_handle);
            detect_flag = 0;
        }

        if (command_id == -2) {
            esp_afe->enable_wakenet(recorder_sr->afe_handle);
            esp_afe->enable_aec(recorder_sr->afe_handle);
            detect_flag = 0;
            ESP_LOGI(TAG,"MN dect quit");
        }
    }
    return ESP_OK;
}
#endif /*CONFIG_USE_MULTINET*/

static void feed_task(void *parameters)
{
    recorder_sr_t *recorder_sr = (recorder_sr_t *)parameters;
    int chunksize = esp_afe->get_feed_chunksize(recorder_sr->afe_handle);
    int buf_size = chunksize * sizeof(int16_t) * RECORDER_CHANNEL_NUM;
    int16_t *buffer = audio_calloc(1, buf_size);
    assert(buffer);
    int fill_cnt = 0;

    recorder_sr->feed_running = true;

    while (recorder_sr->feed_running) {
        xEventGroupWaitBits(recorder_sr->events, FEED_TASK_RUNNING, false, true, portMAX_DELAY);

        int ret = recorder_sr->read((char *)buffer + fill_cnt, buf_size - fill_cnt, recorder_sr->read_ctx, portMAX_DELAY);
        fill_cnt += ret;
        if (fill_cnt == buf_size) {
            recorder_sr_input_ch_pick(recorder_sr, buffer, fill_cnt);
            esp_afe->feed(recorder_sr->afe_handle, buffer);
            fill_cnt -= buf_size;
        } else if (fill_cnt > buf_size) {
            ESP_LOGE(TAG, "fill cnt > buffer_size, there may be memory out of range");
            recorder_sr->feed_running = false;
        }
    }
    audio_free(buffer);
    xEventGroupClearBits(recorder_sr->events, FEED_TASK_RUNNING);
    xEventGroupSetBits(recorder_sr->events, FEED_TASK_DESTROY);
    vTaskDelete(NULL);
}

static void fetch_task(void *parameters)
{
    recorder_sr_t *recorder_sr = (recorder_sr_t *)parameters;
    int afe_chunksize = esp_afe->get_fetch_chunksize(recorder_sr->afe_handle);
    recorder_sr->fetch_running = true;

    while (recorder_sr->fetch_running) {
        xEventGroupWaitBits(recorder_sr->events, FETCH_TASK_RUNNING, false, true, portMAX_DELAY);

        int16_t *buffer = audio_calloc(1, afe_chunksize * sizeof(int16_t));
        assert(buffer);
        int res = esp_afe->fetch(recorder_sr->afe_handle, buffer);
#ifdef CONFIG_USE_MULTINET
        recorder_mn_detect(recorder_sr, buffer, res);
#endif
        if (recorder_sr->afe_monitor) {
            recorder_sr->afe_monitor(recorder_sr_afe_result_convert(recorder_sr, res), recorder_sr->afe_monitor_ctx);
        }
        recorder_sr_output(recorder_sr, buffer, afe_chunksize * sizeof(int16_t));
    }
    xEventGroupClearBits(recorder_sr->events, FETCH_TASK_RUNNING);
    xEventGroupSetBits(recorder_sr->events, FETCH_TASK_DESTROY);
    vTaskDelete(NULL);
}

static esp_err_t recorder_sr_output(recorder_sr_t *recorder_sr, void *buffer, int len)
{
    data_pack_t msg = { 0 };
    if (0 == uxQueueSpacesAvailable(recorder_sr->out_queue)) {
        xQueueReceive(recorder_sr->out_queue, &msg, 0);
        if (msg.data) {
            audio_free(msg.data);
        }
    }
    msg.data = buffer;
    msg.len = len;
    if (xQueueSend(recorder_sr->out_queue, &msg, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Fail to send data to out queue");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static int recorder_sr_fetch(void *handle, void *buf, int len, TickType_t ticks)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    data_pack_t msg = { 0 };
    if (xQueueReceive(recorder_sr->out_queue, &msg, ticks) == pdTRUE) {
        if (len < msg.len) {
            ESP_LOGE(TAG, "Buffer size[%d] < data size[%d], may be out of range", len, msg.len);
            return 0;
        }
        memcpy(buf, msg.data, msg.len);
        audio_free(msg.data);
        return msg.len;
    } else {
        ESP_LOGE(TAG, "sr fetch timeout");
        return 0;
    }
}

static esp_err_t recorder_sr_suspend(void *handle, bool suspend)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    if (suspend) {
        xEventGroupClearBits(recorder_sr->events, FEED_TASK_RUNNING);
        xEventGroupClearBits(recorder_sr->events, FETCH_TASK_RUNNING);
    } else {
        xEventGroupSetBits(recorder_sr->events, FEED_TASK_RUNNING);
        xEventGroupSetBits(recorder_sr->events, FETCH_TASK_RUNNING);
    }
    return ESP_OK;
}

static esp_err_t recorder_sr_enable(void *handle, bool enable)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    esp_err_t ret = ESP_OK;
    if (enable) {
        if (!recorder_sr->feed_running) {
            audio_thread_t feed_thread = NULL;
            ret |= audio_thread_create(&feed_thread, "feed_task",
                feed_task,
                (void *)recorder_sr,
                recorder_sr->feed_task_stack,
                recorder_sr->feed_task_prio,
                true,
                recorder_sr->feed_task_core);
        }
        if (!recorder_sr->fetch_running) {
            audio_thread_t fetch_thread = NULL;
            ret |= audio_thread_create(&fetch_thread, "fetch_task",
                fetch_task,
                (void *)recorder_sr,
                recorder_sr->fetch_task_stack,
                recorder_sr->fetch_task_prio,
                true,
                recorder_sr->fetch_task_core);
        }
        recorder_sr_suspend(handle, !recorder_sr->wwe_enable);
    } else {
        recorder_sr_suspend(handle, false);

        if (recorder_sr->fetch_running) {
            recorder_sr->fetch_running = false;
            EventBits_t bits = xEventGroupWaitBits(recorder_sr->events, FETCH_TASK_DESTROY, true, true, portMAX_DELAY);
            if (bits & FETCH_TASK_DESTROY) {
                ESP_LOGI(TAG, "Fetch task destroyed!");
            }
        }
        if (recorder_sr->feed_running) {
            recorder_sr->feed_running = false;
            EventBits_t bits = xEventGroupWaitBits(recorder_sr->events, FEED_TASK_DESTROY, true, true, portMAX_DELAY);
            if (bits & FEED_TASK_DESTROY) {
                ESP_LOGI(TAG, "Feed task destroyed!");
            }
        }
        if (recorder_sr->out_queue) {
            data_pack_t msg = { 0 };
            while (DEFAULT_OUT_Q_LEN != uxQueueSpacesAvailable(recorder_sr->out_queue)) {
                xQueueReceive(recorder_sr->out_queue, &msg, 0);
                if (msg.data) {
                    audio_free(msg.data);
                }
            }
        }
    }
    return ret == ESP_OK ? ESP_OK : ESP_FAIL;
}

static esp_err_t recorder_sr_get_state(void *handle, void *state)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    recorder_sr_state_t *sr_state = (recorder_sr_state_t *)state;
    EventBits_t bits = xEventGroupWaitBits(recorder_sr->events, FEED_TASK_RUNNING | FETCH_TASK_RUNNING, false, true, 0);
#ifdef CONFIG_USE_MULTINET
    sr_state->mn_enable = recorder_sr->mn_enable;
#else
    sr_state->mn_enable = false;
#endif
    sr_state->wwe_enable = recorder_sr->wwe_enable;
    sr_state->vad_enable = recorder_sr->vad_enable;
    if (!recorder_sr->feed_running || !recorder_sr->fetch_running) {
        sr_state->afe_state = DISABLED;
    } else if ((bits & (FEED_TASK_RUNNING | FETCH_TASK_RUNNING)) == (FEED_TASK_RUNNING | FETCH_TASK_RUNNING)) {
        sr_state->afe_state = RUNNING;
    } else {
        sr_state->afe_state = SUSPENDED;
    }

    return ESP_OK;
}

static esp_err_t recorder_sr_set_read_cb(void *handle, recorder_data_read_t read_cb, void *user_ctx)
{
    AUDIO_CHECK(TAG, handle, return ESP_FAIL, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    recorder_sr->read = read_cb;
    recorder_sr->read_ctx = user_ctx;

    return ESP_OK;
}

static esp_err_t recorder_sr_set_afe_monitor(void *handle, recorder_sr_monitor_t monitor, void *user_ctx)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    recorder_sr->afe_monitor = monitor;
    recorder_sr->afe_monitor_ctx = user_ctx;

    return ESP_OK;
}

static esp_err_t recorder_sr_set_mn_monitor(void *handle, recorder_sr_monitor_t monitor, void *user_ctx)
{
#ifdef CONFIG_USE_MULTINET
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    recorder_sr->mn_monitor = monitor;
    recorder_sr->mn_monitor_ctx = user_ctx;

    return ESP_OK;
#else
    ESP_LOGW(TAG, "Multinet is not enabled in SDKCONFIG");
    return ESP_FAIL;
#endif
}

static esp_err_t recorder_sr_wwe_enable(void *handle, bool enable)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    int ret = 0;

    if (enable) {
        if (!recorder_sr->wwe_enable) {
            ret = esp_afe->enable_wakenet(recorder_sr->afe_handle);
            recorder_sr->wwe_enable = true;
        }
    } else {
        if (recorder_sr->wwe_enable) {
            ret = esp_afe->disable_wakenet(recorder_sr->afe_handle);
            recorder_sr->wwe_enable = false;
        }
    }
    return ret == 1 ? ESP_OK : ESP_FAIL;
}

static esp_err_t recorder_sr_mn_enable(void *handle, bool enable)
{
#ifdef CONFIG_USE_MULTINET
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    recorder_sr->mn_enable = enable;
    if (!recorder_sr->mn_enable && recorder_sr->mn_handle) {
        multinet->destroy(recorder_sr->mn_handle);
        recorder_sr->mn_handle = NULL;
    }

    if (recorder_sr->mn_enable && !recorder_sr->mn_handle) {
        recorder_sr->mn_handle = multinet->create((const model_coeff_getter_t *)&MULTINET_COEFF, 5760);
        AUDIO_NULL_CHECK(TAG, recorder_sr->mn_handle, return ESP_FAIL);
    }
    return ESP_OK;
#else
    ESP_LOGW(TAG, "Multinet is not enabled in SDKCONFIG");
    return ESP_FAIL;
#endif
}

static recorder_sr_iface_t recorder_sr_iface = {
    .base.enable = recorder_sr_enable,
    .base.get_state = recorder_sr_get_state,
    .base.set_read_cb = recorder_sr_set_read_cb,
    .base.feed = NULL,
    .base.fetch = recorder_sr_fetch,
    .afe_suspend = recorder_sr_suspend,
    .set_afe_monitor = recorder_sr_set_afe_monitor,
    .set_mn_monitor = recorder_sr_set_mn_monitor,
    .wwe_enable = recorder_sr_wwe_enable,
    .mn_enable = recorder_sr_mn_enable,
};

static void recorder_sr_clear(void *handle)
{
    AUDIO_CHECK(TAG, handle, return, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    if (recorder_sr->afe_handle) {
        esp_afe->destroy(recorder_sr->afe_handle);
        recorder_sr->afe_handle = NULL;
    }
#ifdef CONFIG_USE_MULTINET
    if (recorder_sr->mn_handle) {
        multinet->destroy(recorder_sr->mn_handle);
        recorder_sr->mn_handle = NULL;
    }
#endif
    if (recorder_sr->out_queue) {
        vQueueDelete(recorder_sr->out_queue);
    }
    if (recorder_sr->events) {
        vEventGroupDelete(recorder_sr->events);
        recorder_sr->events = NULL;
    }
    if (recorder_sr) {
        audio_free(recorder_sr);
    }
}

recorder_sr_handle_t recorder_sr_create(recorder_sr_cfg_t *cfg, recorder_sr_iface_t **iface)
{
    AUDIO_NULL_CHECK(TAG, cfg, return NULL);

    recorder_sr_t *recorder_sr = audio_calloc(1, sizeof(recorder_sr_t));
    AUDIO_NULL_CHECK(TAG, recorder_sr, return NULL);
    recorder_sr->feed_task_core   = cfg->feed_task_core;
    recorder_sr->feed_task_prio   = cfg->feed_task_prio;
    recorder_sr->feed_task_stack  = cfg->feed_task_stack;
    recorder_sr->fetch_task_core  = cfg->fetch_task_core;
    recorder_sr->fetch_task_prio  = cfg->fetch_task_prio;
    recorder_sr->fetch_task_stack = cfg->fetch_task_stack;

    recorder_sr->idx_ch0 = recorder_sr_find_input_ch_idx(cfg->input_order, DAT_CH_0);
    AUDIO_CHECK(TAG, (recorder_sr->idx_ch0 != -1), goto _failed, "channel 0 not found");
#if CONFIG_AFE_MIC_NUM == (2)
    recorder_sr->idx_ch1 = recorder_sr_find_input_ch_idx(cfg->input_order, DAT_CH_1);
    AUDIO_CHECK(TAG, (recorder_sr->idx_ch1 != -1), goto _failed, "channel 1 not found");
#else
    recorder_sr->idx_ch1 = -1;
#endif
    recorder_sr->idx_ref = recorder_sr_find_input_ch_idx(cfg->input_order, DAT_CH_REF0);

    recorder_sr->afe_handle = esp_afe->create_from_config(&cfg->afe_cfg);
    AUDIO_NULL_CHECK(TAG, recorder_sr->afe_handle, goto _failed);
    if (cfg->afe_cfg.wakenet_init == true) {
        recorder_sr->wwe_enable = true;
    }
    if (cfg->afe_cfg.vad_init) {
        recorder_sr->vad_enable = true;
    }
#ifdef CONFIG_USE_MULTINET
    if (cfg->multinet_init) {
        recorder_sr->mn_handle = multinet->create((const model_coeff_getter_t *)&MULTINET_COEFF, 5760);
        recorder_sr->mn_enable = true;
        AUDIO_NULL_CHECK(TAG, recorder_sr->mn_handle, goto _failed);
    }
#endif
    recorder_sr->events = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, recorder_sr->events, goto _failed);
    recorder_sr->out_queue = xQueueCreate(DEFAULT_OUT_Q_LEN, sizeof(data_pack_t));
    AUDIO_NULL_CHECK(TAG, recorder_sr->out_queue, goto _failed);

    *iface = &recorder_sr_iface;

    return recorder_sr;

_failed:
    recorder_sr_clear(recorder_sr);
    return NULL;
}

esp_err_t recorder_sr_destroy(recorder_sr_handle_t handle)
{
    AUDIO_CHECK(TAG, handle, return ESP_FAIL, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;

    recorder_sr_enable(recorder_sr, false);
    recorder_sr_clear(recorder_sr);
    return ESP_OK;
}
