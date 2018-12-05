/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"

#include "rom/queue.h"
#include "esp_log.h"
#include "audio_event_iface.h"

#include "audio_element.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_mutex.h"
#include "audio_error.h"

static const char *TAG = "AUDIO_ELEMENT";
#define DEFAULT_MAX_WAIT_TIME       (2000/portTICK_RATE_MS)
#define NUMBER_OF_MULTI_RINGBUF     1

/**
 *  Audio Element Abstract
 */
typedef struct audio_callback {
    stream_func                 cb;
    void                        *ctx;
} audio_callback_t;

typedef enum {
    IO_TYPE_RB = 1, /* I/O through ringbuffer */
    IO_TYPE_CB,     /* I/O through callback */
} io_type_t;

struct audio_element {
    /* Functions/RingBuffers */
    io_func                     open;
    io_func                     seek;
    process_func                process;
    io_func                     close;
    io_func                     destroy;
    io_type_t                   read_type;
    union {
        ringbuf_handle_t        input_rb;
        audio_callback_t        read_cb;
    } in;
    io_type_t                   write_type;
    union {
        ringbuf_handle_t        output_rb;
        audio_callback_t        write_cb;
    } out;

    ringbuf_handle_t            multi_in_rb[NUMBER_OF_MULTI_RINGBUF];
    ringbuf_handle_t            multi_out_rb[NUMBER_OF_MULTI_RINGBUF];

    /* Properties */
    bool                        is_open;
    audio_element_state_t       state;
    audio_event_iface_handle_t  event;

    int                         buf_size;
    char                        *buf;

    char                        *tag;
    int                         task_stack;
    int                         task_prio;
    int                         task_core;
    xSemaphoreHandle            lock;
    audio_element_info_t        info;

    /* PrivateData */
    void                        *data;
    EventGroupHandle_t          state_event;
    int                         input_wait_time;
    int                         output_wait_time;
    int                         out_buf_size_expect;
    int                         out_rb_size;
    bool                        is_running;
    bool                        task_run;
    bool                        enable_multi_io;
};

const static int STOPPED_BIT = BIT0;
const static int STARTED_BIT = BIT1;
const static int BUFFER_REACH_LEVEL_BIT = BIT2;
const static int TASK_CREATED_BIT = BIT3;
const static int TASK_DESTROYED_BIT = BIT4;
const static int PAUSED_BIT = BIT5;

static esp_err_t audio_element_force_set_state(audio_element_handle_t el, audio_element_state_t new_state)
{
    el->state = new_state;
    return ESP_OK;
}

static esp_err_t audio_element_cmd_send(audio_element_handle_t el, audio_element_state_t state)
{
    audio_event_iface_msg_t msg = {
        .source = el,
        .source_type = AUDIO_ELEMENT_TYPE_ELEMENT,
        .cmd = state,
    };
    ESP_LOGV(TAG, "[%s]evt internal cmd = %d", el->tag, msg.cmd);
    return audio_event_iface_cmd(el->event, &msg);
}

static esp_err_t audio_element_msg_sendout(audio_element_handle_t el, audio_event_iface_msg_t *msg)
{
    msg->source = el;
    msg->source_type = AUDIO_ELEMENT_TYPE_ELEMENT;
    return audio_event_iface_sendout(el->event, msg);
}

static esp_err_t audio_element_process_state_init(audio_element_handle_t el)
{
    if (el->open == NULL) {
        el->is_open = true;
        xEventGroupSetBits(el->state_event, STARTED_BIT);
        return ESP_OK;
    }
    el->is_open = true;
    if (el->open(el) == ESP_OK) {
        ESP_LOGD(TAG, "[%s] el opened", el->tag);
        audio_element_force_set_state(el, AEL_STATE_RUNNING);
        audio_element_report_status(el, AEL_STATUS_STATE_RUNNING);
        xEventGroupSetBits(el->state_event, STARTED_BIT);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "[%s] AEL_STATUS_ERROR_OPEN", el->tag);
    audio_element_report_status(el, AEL_STATUS_ERROR_OPEN);
    audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
    return ESP_FAIL;
}

static esp_err_t audio_element_on_cmd(audio_event_iface_msg_t *msg, void *context)
{
    audio_element_handle_t el = (audio_element_handle_t)context;

    if (msg->source_type != AUDIO_ELEMENT_TYPE_ELEMENT) {
        ESP_LOGE(TAG, "[%s] Invalid event type, this event should be ELEMENT type", el->tag);
        return ESP_FAIL;
    }
    //process an event
    switch (msg->cmd) {
        case AEL_MSG_CMD_ERROR:
            if (el->is_open && el->close) {
                ESP_LOGV(TAG, "[%s] will be closed, line %d", el->tag, __LINE__);
                el->close(el);
                el->is_open = false;
            }
            el->state = AEL_STATE_ERROR;
            audio_event_iface_set_cmd_waiting_timeout(el->event, portMAX_DELAY);
            audio_element_abort_input_ringbuf(el);
            audio_element_abort_output_ringbuf(el);
            el->is_running = false;
            xEventGroupSetBits(el->state_event, STOPPED_BIT);
            ESP_LOGE(TAG, "[%s] AEL_MSG_CMD_ERROR", el->tag);
            break;
        case AEL_MSG_CMD_FINISH:
            if (el->is_open && el->close) {
                ESP_LOGV(TAG, "[%s] will be closed, line %d", el->tag, __LINE__);
                el->close(el);
                el->is_open = false;
            }
            el->state = AEL_STATE_FINISHED;
            audio_event_iface_set_cmd_waiting_timeout(el->event, portMAX_DELAY);
            audio_element_report_status(el, AEL_STATUS_STATE_STOPPED);
            xEventGroupSetBits(el->state_event, STOPPED_BIT);
            el->is_running = false;
            ESP_LOGD(TAG, "[%s] AEL_MSG_CMD_FINISH", el->tag);
            break;
        case AEL_MSG_CMD_STOP:
            if ((el->state != AEL_STATE_FINISHED) && (el->state != AEL_STATE_FINISHED)) {
                if (el->is_open && el->close) {
                    ESP_LOGV(TAG, "[%s] will be closed, line %d", el->tag, __LINE__);
                    el->close(el);
                    el->is_open = false;
                }
                audio_element_abort_output_ringbuf(el);
                audio_element_abort_input_ringbuf(el);

                el->state = AEL_STATE_STOPPED;
                audio_event_iface_set_cmd_waiting_timeout(el->event, portMAX_DELAY);
                audio_element_report_status(el, AEL_STATUS_STATE_STOPPED);
                xEventGroupSetBits(el->state_event, STOPPED_BIT);
                el->is_running = false;
                ESP_LOGD(TAG, "[%s] AEL_MSG_CMD_STOP", el->tag);
            } else {
                // Change element state to AEL_STATE_STOPPED, even if AEL_STATE_ERROR or AEL_STATE_FINISHED.
                el->state = AEL_STATE_STOPPED;
                xEventGroupSetBits(el->state_event, STOPPED_BIT);
            }

            break;
        case AEL_MSG_CMD_PAUSE:
            el->state = AEL_STATE_PAUSED;
            if (el->is_open && el->close) {
                ESP_LOGV(TAG, "[%s] will be closed, line %d", el->tag, __LINE__);
                el->close(el);
                el->is_open = false;
            }
            audio_event_iface_set_cmd_waiting_timeout(el->event, portMAX_DELAY);
            audio_element_report_status(el, AEL_STATUS_STATE_PAUSED);
            el->is_running = false;
            ESP_LOGI(TAG, "[%s] AEL_MSG_CMD_PAUSE", el->tag);
            xEventGroupSetBits(el->state_event, PAUSED_BIT);
            break;
        case AEL_MSG_CMD_RESUME:
            ESP_LOGI(TAG, "[%s] AEL_MSG_CMD_RESUME,state:%d", el->tag, el->state);
            if (el->state == AEL_STATE_RUNNING) {
                el->is_running = true;
                break;
            }
            if (el->state != AEL_STATE_INIT && el->state != AEL_STATE_RUNNING && el->state != AEL_STATE_PAUSED) {
                audio_element_reset_output_ringbuf(el);
            }
            if (audio_element_process_state_init(el) != ESP_OK) {
                break;
            }

            audio_event_iface_set_cmd_waiting_timeout(el->event, 0);
            xEventGroupClearBits(el->state_event, STOPPED_BIT);
            el->is_running = true;
            break;
        case AEL_MSG_CMD_DESTROY:
            el->task_run = false;
            el->is_running = false;
            ESP_LOGD(TAG, "[%s] AEL_MSG_CMD_DESTROY", el->tag);
            return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t audio_element_process_state_running(audio_element_handle_t el)
{
    int process_len = -1;
    if (el->state < AEL_STATE_RUNNING || !el->is_open) {
        return ESP_ERR_INVALID_STATE;
    }
    process_len = el->process(el, el->buf, el->buf_size);
    if (process_len <= 0) {
        switch (process_len) {
            case AEL_IO_ABORT:
                ESP_LOGD(TAG, "[%s] ERROR_PROCESS, AEL_IO_ABORT", el->tag);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                // Re-open if reset_state function called
                if (audio_element_get_state(el) == AEL_STATE_INIT) {
                    el->is_open = false;
                    el->is_running = false;
                    audio_element_resume(el, 0, 0);
                    return ESP_OK;
                }
                audio_element_set_ringbuf_done(el);
                audio_element_cmd_send(el, AEL_MSG_CMD_FINISH);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "[%s] ERROR_PROCESS, AEL_IO_FAIL", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_PROCESS);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                ESP_LOGD(TAG, "[%s] ERROR_PROCESS, AEL_IO_TIMEOUT", el->tag);
                break;
            case AEL_PROCESS_FAIL:
                ESP_LOGE(TAG, "[%s] ERROR_PROCESS, AEL_PROCESS_FAIL", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_PROCESS);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
            default:
                ESP_LOGW(TAG, "[%s] Process return error,ret:%d", el->tag, process_len);
                break;
        }
    }
    return ESP_OK;
}

audio_element_err_t audio_element_input(audio_element_handle_t el, char *buffer, int wanted_size)
{
    int in_len = 0;
    if (el->read_type == IO_TYPE_CB) {
        if (el->in.read_cb.cb == NULL) {
            ESP_LOGE(TAG, "[%s] Read IO Type callback but callback not set", el->tag);
            return ESP_FAIL;
        }
        in_len = el->in.read_cb.cb(el, buffer, wanted_size, el->input_wait_time,
                                   el->in.read_cb.ctx);
    } else if (el->read_type == IO_TYPE_RB) {
        if (el->in.input_rb == NULL) {
            ESP_LOGE(TAG, "[%s] Read IO type ringbuf but ringbuf not set", el->tag);
            return ESP_FAIL;
        }
        in_len = rb_read(el->in.input_rb, buffer, wanted_size, el->input_wait_time);
    } else {
        ESP_LOGE(TAG, "[%s] Invalid read IO type", el->tag);
        return ESP_FAIL;
    }
    if (in_len <= 0) {
        switch (in_len) {
            case AEL_IO_ABORT:
                ESP_LOGW(TAG, "IN-[%s] AEL_IO_ABORT", el->tag);
                audio_element_set_ringbuf_done(el);
                audio_element_stop(el);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                ESP_LOGI(TAG, "IN-[%s] AEL_IO_DONE,%d", el->tag, in_len);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "IN-[%s] AEL_STATUS_ERROR_INPUT", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_INPUT);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                // ESP_LOGD(TAG, "IN-[%s] AEL_IO_TIMEOUT", el->tag);
                break;
            default:
                ESP_LOGE(TAG, "IN-[%s] Input return not support,ret:%d", el->tag, in_len);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
        }
    }
    return in_len;
}

audio_element_err_t audio_element_output(audio_element_handle_t el, char *buffer, int write_size)
{
    int output_len = 0;
    if (el->write_type == IO_TYPE_CB) {
        if (el->out.write_cb.cb && write_size) {
            output_len = el->out.write_cb.cb(el, buffer, write_size, el->output_wait_time,
                                             el->out.write_cb.ctx);
        }
    } else if (el->write_type == IO_TYPE_RB) {
        if (el->out.output_rb && write_size) {
            output_len = rb_write(el->out.output_rb, buffer, write_size, el->output_wait_time);
            if ((rb_bytes_filled(el->out.output_rb) > el->out_buf_size_expect) || (output_len < 0)) {
                xEventGroupSetBits(el->state_event, BUFFER_REACH_LEVEL_BIT);
            }
        }
    }
    if (output_len <= 0) {
        switch (output_len) {
            case AEL_IO_ABORT:
                ESP_LOGW(TAG, "OUT-[%s] AEL_IO_ABORT", el->tag);
                audio_element_set_ringbuf_done(el);
                audio_element_stop(el);
                break;
            case AEL_IO_DONE:
            case AEL_IO_OK:
                ESP_LOGI(TAG, "OUT-[%s] AEL_IO_DONE,%d", el->tag, output_len);
                break;
            case AEL_IO_FAIL:
                ESP_LOGE(TAG, "OUT-[%s] AEL_STATUS_ERROR_OUTPUT", el->tag);
                audio_element_report_status(el, AEL_STATUS_ERROR_OUTPUT);
                audio_element_cmd_send(el, AEL_MSG_CMD_ERROR);
                break;
            case AEL_IO_TIMEOUT:
                ESP_LOGW(TAG, "OUT-[%s] AEL_IO_TIMEOUT", el->tag);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
            default:
                ESP_LOGE(TAG, "OUT-[%s] Output return not support,ret:%d", el->tag, output_len);
                audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE);
                break;
        }
    }
    return output_len;
}
void audio_element_task(void *pv)
{
    audio_element_handle_t el = (audio_element_handle_t)pv;
    el->task_run = true;
    xEventGroupSetBits(el->state_event, TASK_CREATED_BIT);
    audio_element_force_set_state(el, AEL_STATE_INIT);
    audio_event_iface_set_cmd_waiting_timeout(el->event, portMAX_DELAY);
    if (el->buf_size > 0) {
        el->buf = audio_malloc(el->buf_size);
        AUDIO_MEM_CHECK(TAG, el->buf, {
            el->task_run = false;
            ESP_LOGE(TAG, "[%s] Error malloc element buffer", el->tag);
        });
    }
    xEventGroupClearBits(el->state_event, STOPPED_BIT);
    while (el->task_run) {
        if (audio_event_iface_waiting_cmd_msg(el->event) != ESP_OK) {
            break;
        }
        if (audio_element_process_state_running(el) != ESP_OK) {
            // continue;
        }
    }

    if (el->is_open && el->close) {
        ESP_LOGD(TAG, "[%s] el closed", el->tag);
        el->close(el);
        el->is_open = false;
    }
    audio_free(el->buf);
    ESP_LOGD(TAG, "[%s] el task deleted,%d", el->tag, uxTaskGetStackHighWaterMark(NULL));
    xEventGroupSetBits(el->state_event, TASK_DESTROYED_BIT);
    vTaskDelete(NULL);
}

esp_err_t audio_element_reset_state(audio_element_handle_t el)
{
    return audio_element_force_set_state(el, AEL_STATE_INIT);
}

audio_element_state_t audio_element_get_state(audio_element_handle_t el)
{
    return el->state;
}

QueueHandle_t audio_element_get_event_queue(audio_element_handle_t el)
{
    if (!el) {
        return NULL;
    }
    return audio_event_iface_get_queue_handle(el->event);
}

esp_err_t audio_element_setdata(audio_element_handle_t el, void *data)
{
    el->data = data;
    return ESP_OK;
}

void *audio_element_getdata(audio_element_handle_t el)
{
    return el->data;
}

esp_err_t audio_element_set_tag(audio_element_handle_t el, const char *tag)
{
    if (el->tag) {
        audio_free(el->tag);
        el->tag = NULL;
    }

    if (tag) {
        el->tag = strdup(tag);
        AUDIO_MEM_CHECK(TAG, el->tag, {
            return ESP_ERR_NO_MEM;
        });
    }
    return ESP_OK;
}

char *audio_element_get_tag(audio_element_handle_t el)
{
    return el->tag;
}

esp_err_t audio_element_set_uri(audio_element_handle_t el, const char *uri)
{
    if (el->info.uri) {
        audio_free(el->info.uri);
        el->info.uri = NULL;
    }

    if (uri) {
        el->info.uri = strdup(uri);
        AUDIO_MEM_CHECK(TAG, el->info.uri, {
            return ESP_ERR_NO_MEM;
        });
    }
    return ESP_OK;
}

char *audio_element_get_uri(audio_element_handle_t el)
{
    return el->info.uri;
}

esp_err_t audio_element_msg_set_listener(audio_element_handle_t el, audio_event_iface_handle_t listener)
{
    return audio_event_iface_set_listener(el->event, listener);
}

esp_err_t audio_element_msg_remove_listener(audio_element_handle_t el, audio_event_iface_handle_t listener)
{
    return audio_event_iface_remove_listener(listener, el->event);
}

esp_err_t audio_element_setinfo(audio_element_handle_t el, audio_element_info_t *info)
{
    if (info && el) {
        //FIXME: We will got reset if lock mutex here
        mutex_lock(el->lock);
        memcpy(&el->info, info, sizeof(audio_element_info_t));
        mutex_unlock(el->lock);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_getinfo(audio_element_handle_t el, audio_element_info_t *info)
{
    if (info && el) {
        mutex_lock(el->lock);
        memcpy(info, &el->info, sizeof(audio_element_info_t));
        mutex_unlock(el->lock);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_info(audio_element_handle_t el)
{
    if (el) {
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = AEL_MSG_CMD_REPORT_MUSIC_INFO;
        msg.data = NULL;
        ESP_LOGD(TAG, "REPORT_INFO,[%s]evt out cmd:%d,", el->tag, msg.cmd);
        audio_element_msg_sendout(el, &msg);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_codec_fmt(audio_element_handle_t el)
{
    if (el) {
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = AEL_MSG_CMD_REPORT_CODEC_FMT;
        msg.data = NULL;
        ESP_LOGD(TAG, "REPORT_FMT,[%s]evt out cmd:%d,", el->tag, msg.cmd);
        audio_element_msg_sendout(el, &msg);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_report_status(audio_element_handle_t el, audio_element_status_t status)
{
    audio_event_iface_msg_t msg = { 0 };
    msg.cmd = AEL_MSG_CMD_REPORT_STATUS;
    msg.data = (void *)status;
    msg.data_len = sizeof(status);
    ESP_LOGD(TAG, "REPORT_STATUS,[%s]evt out cmd = %d,status:%d", el->tag, msg.cmd, status);
    return audio_element_msg_sendout(el, &msg);
}

esp_err_t audio_element_reset_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    int ret = ESP_OK;
    if (el->in.input_rb) {
        ret |= rb_reset(el->in.input_rb);
        for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
            if (el->multi_in_rb[i]) {
                ret |= rb_reset(el->multi_in_rb[i]);
            }
        }
    }
    return ret;
}

esp_err_t audio_element_reset_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    int ret = ESP_OK;
    if (el->out.output_rb) {
        ret |= rb_reset(el->out.output_rb);
        if (el->enable_multi_io) {
            for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
                if (el->multi_out_rb[i]) {
                    ret |= rb_reset(el->multi_out_rb[i]);
                }
            }
        }

    }
    return ESP_OK;
}

esp_err_t audio_element_abort_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    int ret = ESP_OK;
    if (el->in.input_rb) {
        ret |= rb_abort(el->in.input_rb);
        if (el->enable_multi_io) {
            for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
                if (el->multi_in_rb[i]) {
                    ret |= rb_abort(el->multi_in_rb[i]);
                }
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_abort_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type != IO_TYPE_RB) {
        return ESP_FAIL;
    }
    int ret = ESP_OK;
    if (el->out.output_rb) {
        ret |= rb_abort(el->out.output_rb);
        if (el->enable_multi_io) {
            for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
                if (el->multi_out_rb[i]) {
                    ret |= rb_abort(el->multi_out_rb[i]);
                }
            }
        }
    }
    return ESP_OK;
}

esp_err_t audio_element_set_ringbuf_done(audio_element_handle_t el)
{
    int ret = ESP_OK;
    if (NULL == el) {
        return ESP_FAIL;
    }
    if (el->out.output_rb && el->write_type == IO_TYPE_RB) {
        ret |= rb_done_write(el->out.output_rb);
        if (el->enable_multi_io) {
            for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
                if (el->multi_out_rb[i]) {
                    ret |= rb_done_write(el->multi_out_rb[i]);
                }
            }
        }
    }
    return ret;
}

esp_err_t audio_element_set_input_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb)
{
    if (rb) {
        el->in.input_rb = rb;
        el->read_type = IO_TYPE_RB;
    }
    return ESP_OK;
}

ringbuf_handle_t audio_element_get_input_ringbuf(audio_element_handle_t el)
{
    if (el->read_type == IO_TYPE_RB) {
        return el->in.input_rb;
    } else {
        return NULL;
    }
}

esp_err_t audio_element_set_output_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb)
{
    if (rb) {
        el->out.output_rb = rb;
        el->write_type = IO_TYPE_RB;
    }
    return ESP_OK;
}

ringbuf_handle_t audio_element_get_output_ringbuf(audio_element_handle_t el)
{
    if (el->write_type == IO_TYPE_RB) {
        return el->out.output_rb;
    } else {
        return NULL;
    }
}

esp_err_t audio_element_set_input_timeout(audio_element_handle_t el, TickType_t timeout)
{
    if (el) {
        el->input_wait_time = timeout;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_set_output_timeout(audio_element_handle_t el, TickType_t timeout)
{
    if (el) {
        el->output_wait_time = timeout;
        return ESP_OK;
    }
    return ESP_FAIL;
}

int audio_element_get_output_ringbuf_size(audio_element_handle_t el)
{
    if (el) {
        return el->out_rb_size;
    }
    return 0;
}

esp_err_t audio_element_set_read_cb(audio_element_handle_t el, stream_func fn, void *context)
{
    if (el) {
        el->in.read_cb.cb = fn;
        el->in.read_cb.ctx = context;
        el->read_type = IO_TYPE_CB;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_set_write_cb(audio_element_handle_t el, stream_func fn, void *context)
{
    if (el) {
        el->out.write_cb.cb = fn;
        el->out.write_cb.ctx = context;
        el->write_type = IO_TYPE_CB;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t audio_element_wait_for_stop(audio_element_handle_t el)
{
    EventBits_t uxBits = xEventGroupWaitBits(el->state_event, STOPPED_BIT, false, true, DEFAULT_MAX_WAIT_TIME);
    esp_err_t ret = ESP_FAIL;
    if (uxBits & STOPPED_BIT) {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t audio_element_wait_for_buffer(audio_element_handle_t el, int size_expect, TickType_t timeout)
{
    int ret = ESP_FAIL;
    el->out_buf_size_expect = size_expect;
    if (el->out.output_rb) {
        xEventGroupClearBits(el->state_event, BUFFER_REACH_LEVEL_BIT);
        EventBits_t uxBits = xEventGroupWaitBits(el->state_event, BUFFER_REACH_LEVEL_BIT, false, true, timeout);
        if ((uxBits & BUFFER_REACH_LEVEL_BIT) != 0) {
            ret = ESP_OK;
        } else {
            ret = ESP_FAIL;
        }
    }
    return ret;
}

audio_element_handle_t audio_element_init(audio_element_cfg_t *config)
{
    audio_element_handle_t el = audio_calloc(1, sizeof(struct audio_element));

    AUDIO_MEM_CHECK(TAG, el, {
        return NULL;
    });

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt_cfg.on_cmd = audio_element_on_cmd;
    evt_cfg.context = el;
    evt_cfg.queue_set_size = 0; // Element have no queue_set by default.
    bool _success =
        (
            ((config->tag ? audio_element_set_tag(el, config->tag) : audio_element_set_tag(el, "unknown")) == ESP_OK) &&
            (el->lock           = mutex_create())                   &&
            (el->event          = audio_event_iface_init(&evt_cfg)) &&
            (el->state_event    = xEventGroupCreate())
        );

    AUDIO_MEM_CHECK(TAG, _success, goto _element_init_failed);

    el->open = config->open;
    el->process = config->process;
    el->close = config->close;
    el->destroy = config->destroy;
    el->enable_multi_io = config->enable_multi_io;
    if (config->task_stack > 0) {
        el->task_stack = config->task_stack;
    }
    if (config->task_prio) {
        el->task_prio = config->task_prio;
    } else {
        el->task_prio = DEFAULT_ELEMENT_TASK_PRIO;
    }
    if (config->task_core) {
        el->task_core = config->task_core;
    } else {
        el->task_core = DEFAULT_ELEMENT_TASK_CORE;
    }
    if (config->out_rb_size > 0) {
        el->out_rb_size = config->out_rb_size;
    } else {
        el->out_rb_size = DEFAULT_ELEMENT_RINGBUF_SIZE;
    }
    el->data = config ->data;

    el->state = AEL_STATE_INIT;
    el->buf_size = config->buffer_len;

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_setinfo(el, &info);
    audio_element_set_input_timeout(el, portMAX_DELAY);
    audio_element_set_output_timeout(el, portMAX_DELAY);

    if (config->read != NULL) {
        el->read_type = IO_TYPE_CB;
        el->in.read_cb.cb = config->read;
    } else {
        el->read_type = IO_TYPE_RB;
    }

    if (config->write != NULL) {
        el->write_type = IO_TYPE_CB;
        el->out.write_cb.cb = config->write;
    } else {
        el->write_type = IO_TYPE_RB;
    }

    return el;
_element_init_failed:
    if (el->lock) {
        mutex_destroy(el->lock);
    }
    if (el->state_event) {
        vEventGroupDelete(el->state_event);
    }
    if (el->event) {
        audio_event_iface_destroy(el->event);
    }
    if (el->tag) {
        audio_element_set_tag(el, NULL);
    }
    audio_element_set_uri(el, NULL);
    return NULL;
}

esp_err_t audio_element_deinit(audio_element_handle_t el)
{
    audio_element_stop(el);
    audio_element_wait_for_stop(el);
    audio_element_terminate(el);
    vEventGroupDelete(el->state_event);
    mutex_destroy(el->lock);
    audio_event_iface_destroy(el->event);
    if (el->destroy) {
        el->destroy(el);
    }
    audio_element_set_tag(el, NULL);
    audio_element_set_uri(el, NULL);
    audio_free(el);
    return ESP_OK;
}

esp_err_t audio_element_run(audio_element_handle_t el)
{
    char task_name[32];
    esp_err_t ret = ESP_FAIL;
    if (el->task_run) {
        ESP_LOGD(TAG, "[%s] Element already started", el->tag);
        return ESP_OK;
    }
    ESP_LOGV(TAG, "[%s] Element starting...", el->tag);
    snprintf(task_name, 32, "el-%s", el->tag);
    audio_event_iface_discard(el->event);
    xEventGroupClearBits(el->state_event, TASK_CREATED_BIT);
    xEventGroupClearBits(el->state_event, STOPPED_BIT);
    if (el->task_stack > 0) {
        if (xTaskCreatePinnedToCore(audio_element_task, task_name, el->task_stack, el, el->task_prio, NULL, el->task_core) != pdPASS) {
            ESP_LOGE(TAG, "[%s] Error create element task", el->tag);
            return ESP_FAIL;
        }
        EventBits_t uxBits = xEventGroupWaitBits(el->state_event, TASK_CREATED_BIT, false, true, DEFAULT_MAX_WAIT_TIME);
        if (uxBits & TASK_CREATED_BIT) {
            ret = ESP_OK;
        }
    } else {
        el->task_run = true;
        el->is_running = true;
        audio_element_force_set_state(el, AEL_STATE_RUNNING);
    }
    ESP_LOGI(TAG, "[%s] Element task created", el->tag);
    return ret;
}

esp_err_t audio_element_terminate(audio_element_handle_t el)
{
    if (!el->task_run) {
        ESP_LOGD(TAG, "[%s] Element has not create when AUDIO_ELEMENT_TERMINATE", el->tag);
        return ESP_OK;
    }
    if (el->task_stack <= 0) {
        el->task_run = false;
        el->is_running = false;
        return ESP_OK;
    }
    xEventGroupClearBits(el->state_event, TASK_DESTROYED_BIT);
    if (audio_element_cmd_send(el, AEL_MSG_CMD_DESTROY) != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Element destroy CMD failed", el->tag);
        return ESP_FAIL;
    }
    EventBits_t uxBits = xEventGroupWaitBits(el->state_event, TASK_DESTROYED_BIT, false, true, DEFAULT_MAX_WAIT_TIME);
    esp_err_t ret = ESP_FAIL;
    if (uxBits & TASK_DESTROYED_BIT) {
        ret = ESP_OK;
    }
    ESP_LOGD(TAG, "[%s] Element task destroyed", el->tag);
    return ret;
}

esp_err_t audio_element_pause(audio_element_handle_t el)
{
    if (!el->task_run) {
        ESP_LOGW(TAG, "[%s] Element has not create when AUDIO_ELEMENT_PAUSE", el->tag);
        return ESP_FAIL;
    }
    if ((el->state >= AEL_STATE_PAUSED)) {
        ESP_LOGD(TAG, "[%s] Element already paused, state:%d", el->tag, el->state);
        return ESP_OK;
    }
    xEventGroupClearBits(el->state_event, PAUSED_BIT);
    if (el->task_stack <= 0) {
        audio_element_force_set_state(el, AEL_STATE_PAUSED);
        return ESP_OK;
    }
    if (audio_element_cmd_send(el, AEL_MSG_CMD_PAUSE) != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Element send cmd error when AUDIO_ELEMENT_PAUSE", el->tag);
        return ESP_FAIL;
    }
    EventBits_t uxBits = xEventGroupWaitBits(el->state_event, PAUSED_BIT, false, true, DEFAULT_MAX_WAIT_TIME);
    esp_err_t ret = ESP_FAIL;
    if (uxBits & PAUSED_BIT) {
        ret = ESP_OK;
    }
    return ret;
}

esp_err_t audio_element_resume(audio_element_handle_t el, float wait_for_rb_threshold, TickType_t timeout)
{
    if (!el->task_run) {
        ESP_LOGW(TAG, "[%s] Element has not create when AUDIO_ELEMENT_RESUME", el->tag);
        return ESP_FAIL;
    }
    if ((el->is_running) || (el->state == AEL_STATE_RUNNING)) {
        ESP_LOGD(TAG, "[%s] RESUME: Element is already running, state:%d, task_run:%d", el->tag, el->state, el->task_run);
        return ESP_OK;
    }
    if (el->task_stack <= 0) {
        audio_element_force_set_state(el, AEL_STATE_RUNNING);
        return ESP_OK;
    }
    if (el->state == AEL_STATE_ERROR) {
        ESP_LOGE(TAG, "[%s] RESUME: Element error, state:%d", el->tag, el->state);
        return ESP_FAIL;
    }
    if (el->state == AEL_STATE_FINISHED) {
        ESP_LOGI(TAG, "[%s] RESUME: Element has finished, state:%d", el->tag, el->state);
        return ESP_OK;
    }
    if (wait_for_rb_threshold > 1 || wait_for_rb_threshold < 0) {
        return ESP_FAIL;
    }
    int ret =  ESP_OK;
    audio_element_cmd_send(el, AEL_MSG_CMD_RESUME);
    if (wait_for_rb_threshold != 0 && el->read_type == IO_TYPE_RB) {
        ret = audio_element_wait_for_buffer(el, rb_get_size(el->in.input_rb) * wait_for_rb_threshold, timeout);
    }
    return ret;
}

esp_err_t audio_element_stop(audio_element_handle_t el)
{
    if (!el->task_run) {
        ESP_LOGD(TAG, "[%s] Element has not create when AUDIO_ELEMENT_STOP", el->tag);
        return ESP_FAIL;
    }
    if (el->task_stack <= 0) {
        el->is_running = false;
        xEventGroupSetBits(el->state_event, STOPPED_BIT);
        return ESP_OK;
    }
    if (el->state == AEL_STATE_STOPPED) {
        ESP_LOGD(TAG, "[%s] Element already stoped", el->tag);
        return ESP_OK;
    }
    if (audio_element_cmd_send(el, AEL_MSG_CMD_STOP) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t audio_element_multi_input(audio_element_handle_t el, char *buffer, int wanted_size, int index, TickType_t ticks_to_wait)
{
    esp_err_t ret = ESP_OK;
    if (index >= NUMBER_OF_MULTI_RINGBUF) {
        return ESP_ERR_INVALID_ARG;
    }
    if (el->enable_multi_io) {
        if (el->multi_in_rb[index]) {
            ret = rb_read(el->multi_in_rb[index], buffer, wanted_size, ticks_to_wait);
        }
    }
    return ret;
}

esp_err_t audio_element_multi_output(audio_element_handle_t el, char *buffer, int wanted_size, TickType_t ticks_to_wait)
{
    esp_err_t ret = ESP_OK;
    if (el->enable_multi_io) {
        for (int i = 0; i < NUMBER_OF_MULTI_RINGBUF; ++i) {
            if (el->multi_out_rb[i]) {
                ret |= rb_write(el->multi_out_rb[i], buffer, wanted_size, ticks_to_wait);
            }
        }
    }
    return ret;
}

esp_err_t audio_element_set_multi_input_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb, int index)
{
    if ((index < NUMBER_OF_MULTI_RINGBUF) && rb) {
        el->multi_in_rb[index] = rb;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t audio_element_set_multi_output_ringbuf(audio_element_handle_t el, ringbuf_handle_t rb, int index)
{
    if ((index < NUMBER_OF_MULTI_RINGBUF) && rb) {
        el->multi_out_rb[index] = rb;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

ringbuf_handle_t audio_element_get_multi_input_ringbuf(audio_element_handle_t el, int index)
{
    if (index < NUMBER_OF_MULTI_RINGBUF) {
        return el->multi_in_rb[index];
    }
    return NULL;
}

ringbuf_handle_t audio_element_get_multi_output_ringbuf(audio_element_handle_t el, int index)
{
    if (index < NUMBER_OF_MULTI_RINGBUF) {
        return el->multi_out_rb[index];
    }
    return NULL;
}
