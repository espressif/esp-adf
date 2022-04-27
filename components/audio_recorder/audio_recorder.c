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
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "audio_error.h"
#include "audio_mem.h"
#include "audio_recorder.h"
#include "audio_sys.h"
#include "audio_thread.h"

#define DEFAULT_READ_BUFFER_SIZE (2048)
#define DEFAULT_CMD_Q_LEN        (3)
#define RECORDER_DESTROYED       (BIT0)

typedef enum __audio_recorder_state {
    RECORDER_ST_IDLE,
    RECORDER_ST_WAKEUP,
    RECORDER_ST_WAIT_FOR_SPEECH,
    RECORDER_ST_SPEECHING,
    RECORDER_ST_WAIT_FOR_SILENCE,
    RECORDER_ST_WAIT_FOR_SLEEP,
} audio_recorder_state_t;

typedef enum __audio_recorder_event {
    RECORDER_EVENT_NOISE_DECT,
    RECORDER_EVENT_SPEECH_DECT,
    RECORDER_EVENT_WWE_DECT,
    RECORDER_EVENT_WAKEUP_TIMER_EXPIRED,
    RECORDER_EVENT_VAD_TIMER_EXPIRED
} audio_recorder_event_t;

typedef struct __recorder_data_pack {
    uint8_t *data; /*!< Data pointer*/
    int      len;  /*!< Data length */
} recorder_data_pack_t;

typedef struct __recorder_message {
    enum {
        RECORDER_CMD_UPDATE_STATE,
        RECORDER_CMD_TRIGGER_START,
        RECORDER_CMD_TRIGGER_STOP,
        RECORDER_CMD_VAD_CTRL,
        RECORDER_CMD_WWE_CTRL,
        RECORDER_CMD_MN_CTRL,
        RECORDER_CMD_MN_DECT,
        RECORDER_CMD_DESTROY,
    } id;
    void *data;
    int  data_len;
} recorder_msg_t;

/**
 * @brief Container of recorder
 */
typedef struct __audio_recorder {
    rec_event_cb_t            event_cb;
    void                      *user_data;
    recorder_data_read_t      read;
    void                      *sr_handle;
    recorder_sr_iface_t       *sr_iface;
    int                       wakeup_time;
    int                       vad_start;
    int                       vad_off;
    int                       wakeup_end;
    void                      *encoder_handle;
    recorder_encoder_iface_t  *encoder_iface;
    audio_thread_t            task_handle;
    EventGroupHandle_t        sync_evt;
    QueueHandle_t             cmd_queue;
    esp_timer_handle_t        wakeup_timer;
    bool                      vad_check;
    esp_timer_handle_t        vad_timer;
    audio_recorder_state_t    state;
} audio_recorder_t;

static void audio_recorder_reset(audio_recorder_t *recorder);

static const char *TAG = "AUDIO_RECORDER";

static esp_err_t audio_recorder_send_msg(audio_recorder_t *recorder, int msg_id, void *data, int len)
{
    AUDIO_NULL_CHECK(TAG, recorder, return ESP_FAIL);

    recorder_msg_t msg = { 0 };
    msg.id = msg_id;
    msg.data = data;
    msg.data_len = len;
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "recorder event send failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t audio_recorder_notify_events(audio_recorder_t *recorder, int event)
{
    AUDIO_NULL_CHECK(TAG, recorder, return ESP_FAIL);

    return audio_recorder_send_msg(recorder, RECORDER_CMD_UPDATE_STATE, (void *)event, 0);
}

static inline esp_err_t audio_recorder_update_state_2_user(audio_recorder_t *recorder, int evt)
{
    AUDIO_NULL_CHECK(TAG, recorder, return ESP_FAIL);
    recorder->event_cb(evt, recorder->user_data);
    return ESP_OK;
}

static esp_err_t audio_recorder_wakeup_timer_start(audio_recorder_t *recorder, int state)
{
    int interval = 0;
    if (state == RECORDER_ST_WAKEUP) {
        interval = recorder->wakeup_time;
    } else if (state == RECORDER_ST_WAIT_FOR_SLEEP) {
        interval = recorder->wakeup_end;
    }

    esp_timer_stop(recorder->wakeup_timer);
    if (interval > 0) {
        return esp_timer_start_once(recorder->wakeup_timer, interval * 1000);
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t audio_recorder_vad_timer_start(audio_recorder_t *recorder, int state)
{
    int interval = 0;
    if (state == RECORDER_ST_WAIT_FOR_SPEECH) {
        interval = recorder->vad_start;
    } else if (state == RECORDER_ST_WAIT_FOR_SILENCE) {
        interval = recorder->vad_off;
    }

    esp_timer_stop(recorder->vad_timer);
    if (interval > 0) {
        return esp_timer_start_once(recorder->vad_timer, interval * 1000);
    } else {
        return ESP_FAIL;
    }
}

static void audio_recorder_wakeup_timer_expired(void *arg)
{
    AUDIO_NULL_CHECK(TAG, arg, return );
    audio_recorder_t *recorder = (audio_recorder_t *)arg;

    audio_recorder_notify_events(recorder, RECORDER_EVENT_WAKEUP_TIMER_EXPIRED);
}

static void audio_recorder_vad_timer_expired(void *arg)
{
    AUDIO_NULL_CHECK(TAG, arg, return );
    audio_recorder_t *recorder = (audio_recorder_t *)arg;

    audio_recorder_notify_events(recorder, RECORDER_EVENT_VAD_TIMER_EXPIRED);
}

static esp_err_t audio_recorder_afe_monitor(recorder_sr_result_t result, void *user_ctx)
{
    AUDIO_NULL_CHECK(TAG, user_ctx, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)user_ctx;
    switch (result) {
        case SR_RESULT_VERIFIED:
            break;
        case SR_RESULT_NOISE:
            audio_recorder_notify_events(recorder, RECORDER_EVENT_NOISE_DECT);
            break;
        case SR_RESULT_WAKEUP:
            audio_recorder_notify_events(recorder, RECORDER_EVENT_WWE_DECT);
            break;
        case SR_RESULT_SPEECH:
            audio_recorder_notify_events(recorder, RECORDER_EVENT_SPEECH_DECT);
            break;
        case SR_RESULT_COMMAND:
            break;
        default:
            ESP_LOGE(TAG, "unkown afe result");
            break;
    }
    return ESP_OK;
}

static esp_err_t audio_recorder_mn_monitor(recorder_sr_result_t result, void *user_ctx)
{
    if (result >= 0) {
        return audio_recorder_send_msg(user_ctx, RECORDER_CMD_MN_DECT, (void *)result, 0);
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t audio_recorder_encoder_enable(audio_recorder_t *recorder, bool enable)
{
    if (recorder->encoder_handle) {
        return recorder->encoder_iface->base.enable(recorder->encoder_handle, enable);
    }
    return ESP_OK;
}

static void audio_recorder_update_state(audio_recorder_t *recorder, int event)
{
    AUDIO_NULL_CHECK(TAG, recorder, return );
    static int last_event = -1;
    if (last_event != event) {
        ESP_LOGV(TAG, "Recorder update state, cur %d, event %d", recorder->state, event);
        last_event = event;
    }

    if (event == RECORDER_EVENT_WWE_DECT && recorder->state != RECORDER_ST_IDLE) {
        audio_recorder_reset(recorder);
    }

    switch (recorder->state) {
        case RECORDER_ST_IDLE: {
            if (event == RECORDER_EVENT_WWE_DECT) {
                recorder_sr_state_t sr_st = { 0 };
                if (recorder->sr_handle) {
                    recorder->sr_iface->base.get_state(recorder->sr_handle, &sr_st);
                }

                if (sr_st.wwe_enable) {
                    recorder->state = RECORDER_ST_WAKEUP;
                    audio_recorder_wakeup_timer_start(recorder, recorder->state);
                    audio_recorder_update_state_2_user(recorder, AUDIO_REC_WAKEUP_START);
                } else {
                    recorder->state = RECORDER_ST_SPEECHING;
                    audio_recorder_update_state_2_user(recorder, AUDIO_REC_WAKEUP_START);
                    audio_recorder_update_state_2_user(recorder, AUDIO_REC_VAD_START);
                    audio_recorder_encoder_enable(recorder, true);
                }
            }
            break;
        }
        case RECORDER_ST_WAKEUP: {
            if (event == RECORDER_EVENT_SPEECH_DECT) {
                if (recorder->vad_check) {
                    recorder->state = RECORDER_ST_WAIT_FOR_SPEECH;
                    audio_recorder_vad_timer_start(recorder, recorder->state);
                } else {
                    recorder->state = RECORDER_ST_SPEECHING;
                    audio_recorder_update_state_2_user(recorder, AUDIO_REC_VAD_START);
                    audio_recorder_encoder_enable(recorder, true);
                }
            } else if (event == RECORDER_EVENT_WAKEUP_TIMER_EXPIRED) {
                recorder->state = RECORDER_ST_IDLE;
                esp_timer_stop(recorder->vad_timer);
                audio_recorder_update_state_2_user(recorder, AUDIO_REC_WAKEUP_END);
            }
            break;
        }
        case RECORDER_ST_WAIT_FOR_SPEECH: {
            if (event == RECORDER_EVENT_NOISE_DECT) {
                recorder->state = RECORDER_ST_WAKEUP;
            } else if (event == RECORDER_EVENT_VAD_TIMER_EXPIRED) {
                recorder->state = RECORDER_ST_SPEECHING;
                esp_timer_stop(recorder->wakeup_timer);
                audio_recorder_update_state_2_user(recorder, AUDIO_REC_VAD_START);
                audio_recorder_encoder_enable(recorder, true);
            } else if (event == RECORDER_EVENT_WAKEUP_TIMER_EXPIRED) {
                recorder->state = RECORDER_ST_IDLE;
                esp_timer_stop(recorder->vad_timer);
                audio_recorder_update_state_2_user(recorder, AUDIO_REC_WAKEUP_END);
            }
            break;
        }
        case RECORDER_ST_SPEECHING: {
            if (event == RECORDER_EVENT_NOISE_DECT) {
                if (recorder->vad_check) {
                    recorder->state = RECORDER_ST_WAIT_FOR_SILENCE;
                    audio_recorder_vad_timer_start(recorder, recorder->state);
                } else {
                    recorder->state = RECORDER_ST_WAIT_FOR_SLEEP;
                    audio_recorder_wakeup_timer_start(recorder, recorder->state);
                    audio_recorder_update_state_2_user(recorder, AUDIO_REC_VAD_END);
                    audio_recorder_encoder_enable(recorder, false);
                }
            }
            break;
        }
        case RECORDER_ST_WAIT_FOR_SILENCE: {
            if (event == RECORDER_EVENT_SPEECH_DECT) {
                recorder->state = RECORDER_ST_SPEECHING;
                esp_timer_stop(recorder->vad_timer);
            } else if (event == RECORDER_EVENT_VAD_TIMER_EXPIRED) {
                recorder->state = RECORDER_ST_WAIT_FOR_SLEEP;
                audio_recorder_wakeup_timer_start(recorder, recorder->state);
                audio_recorder_update_state_2_user(recorder, AUDIO_REC_VAD_END);
                audio_recorder_encoder_enable(recorder, false);
            }
            break;
        }
        case RECORDER_ST_WAIT_FOR_SLEEP: {
            if (event == RECORDER_EVENT_SPEECH_DECT) {
                recorder->state = RECORDER_ST_WAIT_FOR_SPEECH;
                audio_recorder_vad_timer_start(recorder, recorder->state);
            } else if (event == RECORDER_EVENT_WAKEUP_TIMER_EXPIRED) {
                recorder->state = RECORDER_ST_IDLE;
                esp_timer_stop(recorder->vad_timer);
                audio_recorder_update_state_2_user(recorder, AUDIO_REC_WAKEUP_END);
            }
            break;
        }
        default:
            break;
    }
}

static void audio_recorder_reset(audio_recorder_t *recorder)
{
    esp_timer_stop(recorder->wakeup_timer);
    esp_timer_stop(recorder->vad_timer);
    audio_recorder_encoder_enable(recorder, false);
    recorder->state = RECORDER_ST_IDLE;
}

static void audio_recorder_task(void *parameters)
{
    audio_recorder_t *recorder = (audio_recorder_t *)parameters;
    recorder_msg_t msg;
    bool running = true;

    while (running) {
        xQueueReceive(recorder->cmd_queue, &msg, portMAX_DELAY);
        switch (msg.id) {
            case RECORDER_CMD_UPDATE_STATE:
                audio_recorder_update_state(recorder, (int)msg.data);
                break;
            case RECORDER_CMD_TRIGGER_START: {
                ESP_LOGI(TAG, "RECORDER_CMD_TRIGGER_START");
                if (recorder->sr_handle && recorder->sr_iface) {
                    recorder_sr_state_t sr_st = { 0 };
                    recorder->sr_iface->base.get_state(recorder->sr_handle, &sr_st);
                    if (sr_st.afe_state == SUSPENDED) {
                        recorder->sr_iface->afe_suspend(recorder->sr_handle, false);
                    }
                }
                audio_recorder_update_state(recorder, RECORDER_EVENT_WWE_DECT);
                break;
            }
            case RECORDER_CMD_TRIGGER_STOP: {
                ESP_LOGI(TAG, "RECORDER_CMD_TRIGGER_STOP [state %d]", recorder->state);
                if ((recorder->state >= RECORDER_ST_SPEECHING) && (recorder->state <= RECORDER_ST_WAIT_FOR_SILENCE)) {
                    recorder->event_cb(AUDIO_REC_VAD_END, recorder->user_data);
                    audio_recorder_encoder_enable(recorder, false);
                }
                if (recorder->state != RECORDER_ST_IDLE) {
                    recorder->event_cb(AUDIO_REC_WAKEUP_END, recorder->user_data);
                }
                audio_recorder_reset(recorder);
                if (recorder->sr_handle && recorder->sr_iface) {
                    recorder_sr_state_t sr_st = { 0 };
                    recorder->sr_iface->base.get_state(recorder->sr_handle, &sr_st);
                    if (sr_st.wwe_enable == false) {
                        recorder->sr_iface->afe_suspend(recorder->sr_handle, true);
                    }
                }
                break;
            }
            case RECORDER_CMD_DESTROY:
                if (recorder->sr_handle && recorder->sr_iface) {
                    recorder->sr_iface->base.enable(recorder->sr_handle, false);
                }
                running = false;
                break;
            case RECORDER_CMD_VAD_CTRL:
                audio_recorder_reset(recorder);
                recorder->vad_check = (bool)msg.data;
                ESP_LOGI(TAG, "recorder vad %d", recorder->vad_check);
                break;
            case RECORDER_CMD_WWE_CTRL:
                audio_recorder_reset(recorder);
                if (recorder->sr_handle && recorder->sr_iface) {
                    recorder->sr_iface->wwe_enable(recorder->sr_handle, (bool)msg.data);
                }
                break;
            case RECORDER_CMD_MN_CTRL:
                if (recorder->sr_handle && recorder->sr_iface) {
                    recorder->sr_iface->mn_enable(recorder->sr_handle, (bool)msg.data);
                }
                break;
            case RECORDER_CMD_MN_DECT:
                audio_recorder_update_state_2_user(recorder, (int)msg.data);
                break;
            default:
                break;
        }
    }

    recorder->task_handle = NULL;
    xEventGroupSetBits(recorder->sync_evt, RECORDER_DESTROYED);
    vTaskDelete(NULL);
}

static int audio_recorder_read_from_sr(void *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    AUDIO_NULL_CHECK(TAG, user_ctx, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)user_ctx;
    int ret = 0;
    if (recorder->sr_handle) {
        recorder_subproc_iface_t *subproc = &recorder->sr_iface->base;
        ret = subproc->fetch(recorder->sr_handle, buffer, buf_sz, ticks);
    }

    return ret;
}

static void audio_recorder_free(audio_recorder_t *recorder)
{
    AUDIO_NULL_CHECK(TAG, recorder, return;);

    esp_timer_stop(recorder->wakeup_timer);
    esp_timer_delete(recorder->wakeup_timer);

    esp_timer_stop(recorder->vad_timer);
    esp_timer_delete(recorder->vad_timer);

    if (recorder->cmd_queue) {
        vQueueDelete(recorder->cmd_queue);
    }
    if (recorder->sync_evt) {
        vEventGroupDelete(recorder->sync_evt);
    }
    free(recorder);
}

audio_rec_handle_t audio_recorder_create(audio_rec_cfg_t *config)
{
    AUDIO_NULL_CHECK(TAG, config, return NULL);
    AUDIO_NULL_CHECK(TAG, config->event_cb, return NULL);
    AUDIO_NULL_CHECK(TAG, config->read, return NULL);

    audio_recorder_t *recorder = audio_calloc(1, sizeof(audio_recorder_t));
    AUDIO_NULL_CHECK(TAG, recorder, return NULL);
    recorder->state = RECORDER_ST_IDLE;

    recorder->event_cb       = config->event_cb;
    recorder->user_data      = config->user_data;
    recorder->read           = config->read;
    recorder->sr_handle      = config->sr_handle;
    recorder->sr_iface       = config->sr_iface;
    recorder->wakeup_time    = config->wakeup_time;
    recorder->vad_start      = config->vad_start;
    recorder->vad_off        = config->vad_off;
    recorder->wakeup_end     = config->wakeup_end;
    recorder->encoder_handle = config->encoder_handle;
    recorder->encoder_iface  = config->encoder_iface;

    recorder->vad_check = true;
    recorder->sync_evt = xEventGroupCreate();
    AUDIO_NULL_CHECK(TAG, recorder->sync_evt, goto _failed);
    recorder->cmd_queue = xQueueCreate(DEFAULT_CMD_Q_LEN, sizeof(recorder_msg_t));
    AUDIO_NULL_CHECK(TAG, recorder->cmd_queue, goto _failed);

    esp_timer_create_args_t wakeup_timer_cfg = {
        .callback = audio_recorder_wakeup_timer_expired,
        .arg = recorder,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wakeup_timer",
    };
    AUDIO_CHECK(TAG, (esp_timer_create(&wakeup_timer_cfg, &recorder->wakeup_timer) == ESP_OK), goto _failed, "Wakeup timer create failed");

    esp_timer_create_args_t vad_timer_cfg = {
        .callback = audio_recorder_vad_timer_expired,
        .arg = recorder,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "vad_timer",
    };
    AUDIO_CHECK(TAG, (esp_timer_create(&vad_timer_cfg, &recorder->vad_timer) == ESP_OK), goto _failed, "Vad timer create failed");

    if (recorder->sr_handle) {
        recorder_sr_iface_t *sr_iface = recorder->sr_iface;
        AUDIO_NULL_CHECK(TAG, sr_iface, goto _failed);

        sr_iface->set_afe_monitor(recorder->sr_handle, audio_recorder_afe_monitor, recorder);
        sr_iface->set_mn_monitor(recorder->sr_handle, audio_recorder_mn_monitor, recorder);
        sr_iface->base.set_read_cb(recorder->sr_handle, recorder->read, NULL);
        sr_iface->base.enable(recorder->sr_handle, true);
    }

    if (recorder->encoder_handle) {
        recorder_encoder_iface_t *encoder_iface = recorder->encoder_iface;

        if (recorder->sr_handle) {
            encoder_iface->base.set_read_cb(recorder->encoder_handle, audio_recorder_read_from_sr, recorder);
        } else {
            encoder_iface->base.set_read_cb(recorder->encoder_handle, recorder->read, NULL);
        }
    }

    if (audio_thread_create(&recorder->task_handle, "recorder_task", audio_recorder_task, (void *)recorder, config->task_size, config->task_prio, true, config->pinned_core) != ESP_OK) {
        ESP_LOGE(TAG, "Fail to create recorder_task");
        goto _failed;
    }
    return recorder;

_failed:
    audio_recorder_free(recorder);
    recorder = NULL;
    return NULL;
}

esp_err_t audio_recorder_destroy(audio_rec_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    audio_recorder_reset(recorder);

    recorder_msg_t msg = {
        .id = RECORDER_CMD_DESTROY,
        .data = NULL,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder destroy failed");
        return ESP_FAIL;
    }
    EventBits_t bits = xEventGroupWaitBits(recorder->sync_evt, RECORDER_DESTROYED, true, true, portMAX_DELAY);
    if (bits & RECORDER_DESTROYED) {
        ESP_LOGI(TAG, "Recorder task destroyed");
    }
    audio_recorder_free(recorder);
    ESP_LOGI(TAG, "Recorder destroyed");
    return ESP_OK;
}

esp_err_t audio_recorder_trigger_start(audio_rec_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    recorder_msg_t msg = {
        .id = RECORDER_CMD_TRIGGER_START,
        .data = NULL,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder start failed");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}

esp_err_t audio_recorder_trigger_stop(audio_rec_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    recorder_msg_t msg = {
        .id = RECORDER_CMD_TRIGGER_STOP,
        .data = NULL,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder stop failed");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}

esp_err_t audio_recorder_wakenet_enable(audio_rec_handle_t handle, bool enable)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    recorder_msg_t msg = {
        .id = RECORDER_CMD_WWE_CTRL,
        .data = (void *)enable,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder wwe ctrl failed");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}

esp_err_t audio_recorder_multinet_enable(audio_rec_handle_t handle, bool enable)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    recorder_msg_t msg = {
        .id = RECORDER_CMD_MN_CTRL,
        .data = (void *)enable,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder mn ctrl failed");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}

esp_err_t audio_recorder_vad_check_enable(audio_rec_handle_t handle, bool enable)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_FAIL);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    recorder_msg_t msg = {
        .id = RECORDER_CMD_VAD_CTRL,
        .data = (void *)enable,
        .data_len = 0
    };
    if (xQueueSend(recorder->cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "The recorder vad ctrl failed");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}

int audio_recorder_data_read(audio_rec_handle_t handle, void *buffer, int length, TickType_t ticks)
{
    AUDIO_NULL_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;
    recorder_subproc_iface_t *subproc = NULL;
    int ret = 0;
    if (recorder->state != RECORDER_ST_SPEECHING && recorder->state != RECORDER_ST_WAIT_FOR_SILENCE) {
        ESP_LOGW(TAG, "Not in speeching, return 0");
        return 0;
    }
    if (recorder->encoder_handle) {
        subproc = &recorder->encoder_iface->base;
        ret = subproc->fetch(recorder->encoder_handle, buffer, length, ticks);
    } else if (recorder->sr_handle) {
        subproc = &recorder->sr_iface->base;
        ret = subproc->fetch(recorder->sr_handle, buffer, length, ticks);
    } else {
        ret = recorder->read(buffer, length, recorder, ticks);
    }

    return ret;
}

bool audio_recorder_get_wakeup_state(audio_rec_handle_t handle)
{
    AUDIO_NULL_CHECK(TAG, handle, return false);
    audio_recorder_t *recorder = (audio_recorder_t *)handle;

    return recorder->state >= RECORDER_ST_WAKEUP ? true : false;
}