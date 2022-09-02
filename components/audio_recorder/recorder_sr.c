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

#include "ringbuf.h"

#include "esp_afe_sr_models.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"

#ifdef CONFIG_USE_MULTINET
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_process_sdkconfig.h"
#endif

#include "recorder_sr.h"
#include "ch_sort.h"

#define FEED_TASK_DESTROY  (BIT(0))
#define FETCH_TASK_DESTROY (BIT(1))
#define FEED_TASK_RUNNING  (BIT(2))
#define FETCH_TASK_RUNNING (BIT(3))

static const char *TAG = "RECORDER_SR";

static const esp_afe_sr_iface_t *esp_afe = &ESP_AFE_SR_HANDLE;
#if CONFIG_AFE_MIC_NUM == (1)
#define RECORDER_CHANNEL_NUM (2)
#else
#define RECORDER_CHANNEL_NUM (4)
#endif

#ifdef CONFIG_USE_MULTINET
static const esp_mn_iface_t *multinet = NULL;
#endif

typedef struct __recorder_sr {
    int                   feed_task_core;
    int                   feed_task_prio;
    int                   feed_task_stack;
    int                   fetch_task_core;
    int                   fetch_task_prio;
    int                   fetch_task_stack;
    ringbuf_handle_t      out_rb;
    int                   rb_size;
    EventGroupHandle_t    events;
    bool                  feed_running;
    bool                  fetch_running;
    recorder_data_read_t  read;
    void                  *read_ctx;
    srmodel_list_t        *models;
    esp_afe_sr_data_t     *afe_handle;
    recorder_sr_monitor_t afe_monitor;
    void                  *afe_monitor_ctx;
    bool                  wwe_enable;
    bool                  vad_enable;
    char                  *partition_label;
    bool                  aec_enable;
#ifdef CONFIG_USE_MULTINET
    model_iface_data_t    *mn_handle;
    recorder_sr_monitor_t mn_monitor;
    void                  *mn_monitor_ctx;
    bool                  mn_enable;
    char                  *mn_language;
#endif /* CONFIG_USE_MULTINET */
    int8_t                input_order[DAT_CH_MAX];
} recorder_sr_t;

static esp_err_t recorder_sr_output(recorder_sr_t *recorder_sr, void *buffer, int len);

static inline int recorder_sr_afe_result_convert(recorder_sr_t *recorder_sr, afe_fetch_result_t *result)
{
    int ret = SR_RESULT_UNKNOW;
    ESP_LOGV(TAG, "wake %d, vad %d", result->wakeup_state, result->vad_state);
    switch (result->wakeup_state) {
        case WAKENET_CHANNEL_VERIFIED:
            ret = SR_RESULT_VERIFIED;
            break;
        case WAKENET_NO_DETECT:
            if (recorder_sr->vad_enable) {
                if (result->vad_state == AFE_VAD_SILENCE) {
                    ret = SR_RESULT_NOISE;
                } else if (result->vad_state == AFE_VAD_SPEECH) {
                    ret = SR_RESULT_SPEECH;
                } else {
                    ESP_LOGE(TAG, "vad state error");
                }
            } else {
                ret = SR_RESULT_SPEECH;
            }
            break;
        case WAKENET_DETECTED:
            ret = SR_RESULT_WAKEUP;
            break;
        default:
            break;
    }
    return ret;
}

#ifdef CONFIG_USE_MULTINET

#ifdef CONFIG_IDF_TARGET_ESP32
static void recorder_sr_enable_wakenet_aec(recorder_sr_t *recorder_sr)
{
    esp_afe->enable_wakenet(recorder_sr->afe_handle);
    if (recorder_sr->aec_enable) {
        esp_afe->enable_aec(recorder_sr->afe_handle);
    }
}

static void recorder_sr_disable_wakenet_aec(recorder_sr_t *recorder_sr)
{
    esp_afe->disable_wakenet(recorder_sr->afe_handle);
    if (recorder_sr->aec_enable) {
        esp_afe->disable_aec(recorder_sr->afe_handle);
    }
}
#endif

static esp_err_t recorder_mn_detect(recorder_sr_t *recorder_sr, int16_t *buffer, int afe_res)
{
    static int detect_flag = 0;

    if (!recorder_sr->mn_enable) {
        if (detect_flag == 1) {
#if CONFIG_IDF_TARGET_ESP32
            recorder_sr_enable_wakenet_aec(recorder_sr);
#endif
            detect_flag = 0;
        }
        return ESP_OK;
    }
#if CONFIG_IDF_TARGET_ESP32
    if (afe_res == WAKENET_DETECTED) {
        detect_flag = 1;
        recorder_sr_disable_wakenet_aec(recorder_sr);
    }
#elif CONFIG_IDF_TARGET_ESP32S3
    if (afe_res == WAKENET_CHANNEL_VERIFIED) {
        detect_flag = 1;
    }
#endif
    if (detect_flag == 1) {
        esp_mn_state_t mn_state = multinet->detect(recorder_sr->mn_handle, buffer);

        if (mn_state == ESP_MN_STATE_DETECTING) {
            return ESP_OK;
        }
        if (mn_state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn_result = multinet->get_results(recorder_sr->mn_handle);
            if (recorder_sr->mn_monitor) {
                recorder_sr->mn_monitor(mn_result->command_id[0], recorder_sr->mn_monitor_ctx);
            }
#if CONFIG_IDF_TARGET_ESP32
            recorder_sr_enable_wakenet_aec(recorder_sr);
#endif
            detect_flag = 0;
        }

        if (mn_state == ESP_MN_STATE_TIMEOUT) {
#if CONFIG_IDF_TARGET_ESP32
            recorder_sr_enable_wakenet_aec(recorder_sr);
#endif
            detect_flag = 0;
            ESP_LOGI(TAG, "MN dect quit");
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
    int16_t *i_buf = audio_calloc(1, buf_size);
    assert(i_buf);
    int16_t *o_buf = audio_calloc(1, buf_size);;
    assert(o_buf);

    int fill_cnt = 0;

    recorder_sr->feed_running = true;

    while (recorder_sr->feed_running) {
        xEventGroupWaitBits(recorder_sr->events, FEED_TASK_RUNNING, false, true, portMAX_DELAY);

        int ret = recorder_sr->read((char *)i_buf + fill_cnt, buf_size - fill_cnt, recorder_sr->read_ctx, portMAX_DELAY);
        fill_cnt += ret;
        if (fill_cnt == buf_size) {
#if RECORDER_CHANNEL_NUM == 2
            ch_sort_16bit_2ch(i_buf, o_buf, fill_cnt, recorder_sr->input_order);
#else /* RECORDER_CHANNEL_NUM == 2 */
            ch_sort_16bit_4ch(i_buf, o_buf, fill_cnt, recorder_sr->input_order);
#endif /* RECORDER_CHANNEL_NUM == 2 */
            esp_afe->feed(recorder_sr->afe_handle, o_buf);
            fill_cnt -= buf_size;
        } else if (fill_cnt > buf_size) {
            ESP_LOGE(TAG, "fill cnt > buffer_size, there may be memory out of range");
            recorder_sr->feed_running = false;
        }
    }
    audio_free(i_buf);
    audio_free(o_buf);
    xEventGroupClearBits(recorder_sr->events, FEED_TASK_RUNNING);
    xEventGroupSetBits(recorder_sr->events, FEED_TASK_DESTROY);
    vTaskDelete(NULL);
}

static void fetch_task(void *parameters)
{
    recorder_sr_t *recorder_sr = (recorder_sr_t *)parameters;
    recorder_sr->fetch_running = true;

    while (recorder_sr->fetch_running) {
        xEventGroupWaitBits(recorder_sr->events, FETCH_TASK_RUNNING, false, true, portMAX_DELAY);

        afe_fetch_result_t *res = esp_afe->fetch(recorder_sr->afe_handle);
#ifdef CONFIG_USE_MULTINET
        recorder_mn_detect(recorder_sr, res->data, res->wakeup_state);
#endif
        if (recorder_sr->afe_monitor) {
            recorder_sr->afe_monitor(recorder_sr_afe_result_convert(recorder_sr, res), recorder_sr->afe_monitor_ctx);
        }
        recorder_sr_output(recorder_sr, res->data, res->data_size);
    }
    xEventGroupClearBits(recorder_sr->events, FETCH_TASK_RUNNING);
    xEventGroupSetBits(recorder_sr->events, FETCH_TASK_DESTROY);
    vTaskDelete(NULL);
}

static esp_err_t recorder_sr_output(recorder_sr_t *recorder_sr, void *buffer, int len)
{
    if (rb_bytes_available(recorder_sr->out_rb) < len) {
        rb_read(recorder_sr->out_rb, NULL, len, 0);
    }
    int ret = rb_write(recorder_sr->out_rb, buffer, len, 0);
    return ret == len ? ESP_OK : ESP_FAIL;
}

static int recorder_sr_fetch(void *handle, void *buf, int len, TickType_t ticks)
{
    AUDIO_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Handle is NULL");
    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    return rb_read(recorder_sr->out_rb, buf, len, ticks);
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

        if (recorder_sr->out_rb) {
            rb_reset(recorder_sr->out_rb);
        }
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
        if (recorder_sr->out_rb) {
            rb_done_write(recorder_sr->out_rb);
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
        char *mn_name = esp_srmodel_filter(recorder_sr->models, ESP_MN_PREFIX, recorder_sr->mn_language);
        multinet = esp_mn_handle_from_name(mn_name);
        recorder_sr->mn_handle = multinet->create(mn_name, 5760);
        AUDIO_NULL_CHECK(TAG, recorder_sr->mn_handle, return ESP_FAIL);
        esp_mn_commands_update_from_sdkconfig((esp_mn_iface_t *)multinet, recorder_sr->mn_handle);
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
    if (recorder_sr->models) {
        free(recorder_sr->models);
        recorder_sr->models = NULL;
    }
    if (recorder_sr->out_rb) {
        rb_reset(recorder_sr->out_rb);
        rb_destroy(recorder_sr->out_rb);
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
    recorder_sr->rb_size          = cfg->rb_size;
    recorder_sr->partition_label  = cfg->partition_label;
    recorder_sr->aec_enable       = cfg->afe_cfg.aec_init;
#ifdef CONFIG_USE_MULTINET
    recorder_sr->mn_language      = cfg->mn_language;
#endif

    memcpy(recorder_sr->input_order, cfg->input_order, DAT_CH_MAX);

    recorder_sr->models = esp_srmodel_init(recorder_sr->partition_label);
    char *wn_name = esp_srmodel_filter(recorder_sr->models, ESP_WN_PREFIX, NULL);
    AUDIO_NULL_CHECK(TAG, wn_name, goto _failed);
    cfg->afe_cfg.wakenet_model_name = wn_name;
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
        char *mn_name = esp_srmodel_filter(recorder_sr->models, ESP_MN_PREFIX, recorder_sr->mn_language);
        AUDIO_NULL_CHECK(TAG, mn_name, goto _failed);
        multinet = esp_mn_handle_from_name(mn_name);
        recorder_sr->mn_handle = multinet->create(mn_name, 5760);
        AUDIO_NULL_CHECK(TAG, recorder_sr->mn_handle, goto _failed);
        recorder_sr->mn_enable = true;
        esp_mn_commands_update_from_sdkconfig((esp_mn_iface_t *)multinet, recorder_sr->mn_handle);
    }
#endif
    recorder_sr->events = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, recorder_sr->events, goto _failed);
    recorder_sr->out_rb = rb_create(recorder_sr->rb_size, 1);
    AUDIO_NULL_CHECK(TAG, recorder_sr->out_rb, goto _failed);

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


esp_err_t recorder_sr_reset_speech_cmd(recorder_sr_handle_t handle, char *command_str, char *err_phrase_id)
{
#ifdef CONFIG_USE_MULTINET
    AUDIO_CHECK(TAG, handle, return ESP_FAIL, "Handle is NULL");
    AUDIO_CHECK(TAG, command_str, return ESP_FAIL, "command_str is NULL");

    recorder_sr_t *recorder_sr = (recorder_sr_t *)handle;
    esp_err_t err = ESP_OK;
    char *buf = audio_calloc(1, strlen(command_str) + 1);
    AUDIO_NULL_CHECK(TAG, buf, return ESP_ERR_NO_MEM);
    char *tmp = buf;

    esp_mn_commands_clear();

    uint8_t cmd_id = 0;
    char *cmd_str = NULL;
    char *phrase = NULL;
    strcpy(buf, command_str);
    char *p = NULL;
    while ((p = strtok_r(buf, ";", &cmd_str)) != NULL) {
        buf = p;
        while ((p = strtok_r(buf, ",", &phrase)) != NULL) {
            buf = NULL;
            ESP_LOGI(TAG, "cmd [%d : %s]", cmd_id, p);
            err = esp_mn_commands_add(cmd_id, p);
            if (err != ESP_OK) {
                strcpy(err_phrase_id, p);
                free(tmp);
                return err;
            }
        }
        buf = NULL;
        if (++cmd_id > ESP_MN_MAX_PHRASE_NUM) {
            break;
        }
    }
    free(tmp);
    esp_mn_commands_print();
    esp_mn_error_t *mn_err = esp_mn_commands_update(multinet, recorder_sr->mn_handle);
    return mn_err == NULL ? ESP_OK : ESP_FAIL;
#else
    ESP_LOGW(TAG, "Multinet is not enabled");
    return ESP_FAIL;
#endif
}
