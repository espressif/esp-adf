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
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "rom/queue.h"
#include "esp_log.h"

#include "esp_dispatcher.h"
#include "audio_mutex.h"
#include "audio_mem.h"
#include "audio_error.h"

static const char *TAG = "DISPATCHER";
#define  ESP_DISPATCHER_EVENT_SIZE                              3

#define ESP_DISPATCHER_TASK_DESTROY_BIT                 BIT(4)
#define ESP_DISPATCHER_UPDATE_EXE_FUNC_RETURN_BIT       BIT(5)

typedef enum {
    ESP_DISPCH_EVENT_TYPE_UNKNOWN,
    ESP_DISPCH_EVENT_TYPE_CMD,
    ESP_DISPCH_EVENT_TYPE_EXE,
} esp_dispatcher_event_type_t;

typedef struct {
    esp_dispatcher_event_type_t             type;
    int                                     sub_index;
    action_arg_t                            arg;
} esp_dispatcher_info_t;

typedef struct evt_exe_item {
    STAILQ_ENTRY(evt_exe_item)              entries;
    int                                     sub_index;
    esp_action_exe                          exe_func;
    void*                                   exe_instance;
} esp_action_exe_item_t;

typedef struct esp_dispatcher {
    QueueHandle_t                                  exe_que;
    QueueHandle_t                                  result_que;
    SemaphoreHandle_t                              mutex;
    TaskHandle_t                                   task_handle;
    STAILQ_HEAD(action_exe_list, evt_exe_item)     exe_list;
} esp_dispatcher_t;


static esp_err_t execute_index_exist(esp_dispatcher_handle_t h, int idx)
{
    esp_dispatcher_t *impl = (esp_dispatcher_t *)h;
    esp_action_exe_item_t *item;
    STAILQ_FOREACH(item, &impl->exe_list, entries) {
        if (idx == item->sub_index) {
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

static esp_action_exe_item_t *found_exe_func(esp_dispatcher_handle_t h, int idx)
{
    esp_dispatcher_t *impl = (esp_dispatcher_t *)h;
    esp_action_exe_item_t *item;
    STAILQ_FOREACH(item, &impl->exe_list, entries) {
        if (idx == item->sub_index) {
            return item;
        }
    }
    return NULL;
}

static void dispatcher_event_task(void *parameters)
{
    esp_dispatcher_t *dispch = (esp_dispatcher_t *)parameters;
    esp_dispatcher_info_t msg = {0};
    action_result_t result = {0};
    esp_action_exe_item_t *exe_item = NULL;
    bool task_run = true;
    ESP_LOGI(TAG, "%s is running...", __func__);
    while (task_run) {
        if (xQueueReceive(dispch->exe_que, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.type == ESP_DISPCH_EVENT_TYPE_EXE) {
                exe_item = found_exe_func(dispch, msg.sub_index);
                if (exe_item) {
                    result.err = ESP_OK;
                    result.data = 0;
                    result.len = 0;
                    ESP_LOGD(TAG, "EXE type:%d, index:%x,%p,%d",
                             msg.type, msg.sub_index, msg.arg.data, msg.arg.len);
                    result.err = exe_item->exe_func(exe_item->exe_instance, &msg.arg, &result);
                    if (xQueueSend(dispch->result_que, &result, 0 / portTICK_PERIOD_MS) == pdFALSE) {
                        ESP_LOGW(TAG, "Sending result failed, index:%x", msg.sub_index);
                    }
                } else {
                    result.err = ESP_ERR_ADF_NOT_SUPPORT;
                    result.data = 0;
                    result.len = 0;
                    ESP_LOGW(TAG, "Not found index:%x", msg.sub_index);
                    if (xQueueSend(dispch->result_que, &result, 0 / portTICK_PERIOD_MS) == pdFALSE) {
                        ESP_LOGW(TAG, "Send result failed, index:%x", msg.sub_index);
                    }
                }
            } else if (msg.type == ESP_DISPCH_EVENT_TYPE_CMD) {
                task_run = false;
            }
        }
    }
    result.err = ESP_OK;
    result.data = 0;
    result.len = 0;
    xQueueSend(dispch->result_que, &result, 0 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}

esp_err_t esp_dispatcher_reg_exe_func(esp_dispatcher_handle_t dh, void *exe_inst, int sub_event_index, esp_action_exe func)
{
    esp_dispatcher_t *impl = (esp_dispatcher_t *)dh;
    AUDIO_NULL_CHECK(TAG, impl, return ESP_ERR_INVALID_ARG);
    if (execute_index_exist(dh, sub_event_index) == ESP_OK) {
        ESP_LOGW(TAG, "The %x index of function already exists", sub_event_index);
        return ESP_ERR_ADF_ALREADY_EXISTS;
    }
    esp_action_exe_item_t *item = audio_calloc(1, sizeof(esp_action_exe_item_t));
    item->sub_index = sub_event_index;
    item->exe_func = func;
    item->exe_instance = exe_inst;

    STAILQ_INSERT_TAIL(&impl->exe_list, item, entries);
    return ESP_OK;
}

esp_err_t esp_dispatcher_execute(esp_dispatcher_handle_t dh, int sub_event_index,
                                    action_arg_t *in_para, action_result_t *out_result)
{
    esp_dispatcher_t *impl = (esp_dispatcher_t *)dh;
    AUDIO_NULL_CHECK(TAG, impl, return ESP_ERR_INVALID_ARG);
    esp_dispatcher_info_t info = {0};
    info.type = ESP_DISPCH_EVENT_TYPE_EXE;
    info.sub_index = sub_event_index;
    if (in_para) {
        memcpy(&info.arg, in_para, sizeof(action_arg_t));
    }
    ESP_LOGI(TAG, "EXE IN, cmd type:%d, index:%x, data:%p, len:%d",
             info.type, info.sub_index, info.arg.data, info.arg.len);

    mutex_lock(impl->mutex);
    if (xQueueSend(impl->exe_que, &info, 5000 / portTICK_PERIOD_MS) == pdFALSE) {
        ESP_LOGE(TAG, "Send timeout type:%d, index:%x", info.type, info.sub_index);
        mutex_unlock(impl->mutex);
        return ESP_ERR_ADF_TIMEOUT;
    }
    action_result_t ret = {0};
    xQueueReceive(impl->result_que, &ret, portMAX_DELAY);
    mutex_unlock(impl->mutex);
    if (out_result) {
        memcpy(out_result, &ret, sizeof(action_result_t));
    }
    ESP_LOGI(TAG, "EXE OUT,result type:%d, index:%x, ret:%x, data:%p, len:%d", info.type, info.sub_index, ret.err, ret.data, ret.len);
    return ret.err;
}

esp_dispatcher_handle_t esp_dispatcher_create(esp_dispatcher_config_t *cfg)
{
    AUDIO_NULL_CHECK(TAG, cfg, return NULL);
    esp_dispatcher_handle_t impl = audio_calloc(1, sizeof(esp_dispatcher_t));
    AUDIO_MEM_CHECK(TAG, impl, return NULL);
    impl->result_que = xQueueCreate(1, sizeof(action_result_t));
    AUDIO_MEM_CHECK(TAG, impl->result_que, goto _failed;);
    impl->exe_que = xQueueCreate(ESP_DISPATCHER_EVENT_SIZE, sizeof(esp_dispatcher_info_t));
    AUDIO_MEM_CHECK(TAG, impl->exe_que, goto _failed;);
    impl->mutex = mutex_create();
    AUDIO_MEM_CHECK(TAG, impl->mutex, goto _failed;);
    STAILQ_INIT(&impl->exe_list);
    ESP_LOGE(TAG, "exe first list: %p", STAILQ_FIRST(&impl->exe_list));

    if (pdPASS != xTaskCreatePinnedToCore(dispatcher_event_task,
                                          "esp_dispatcher",
                                          cfg->task_stack,
                                          impl,
                                          cfg->task_prio,
                                          &impl->task_handle,
                                          cfg->task_core)) {
        ESP_LOGE(TAG, "Create task failed on %s", __func__);
        goto _failed;
    }
    return impl;
_failed:
    if (impl->result_que) {
        vQueueDelete(impl->result_que);
        impl->result_que = NULL;
    }
    if (impl->exe_que) {
        vQueueDelete(impl->exe_que);
        impl->exe_que = NULL;
    }
    if (impl->mutex) {
        mutex_destroy(impl->mutex);
        impl->mutex = NULL;
    }
    free(impl);
    impl = NULL;
    return impl;
}

esp_err_t esp_dispatcher_destroy(esp_dispatcher_handle_t dh)
{
    esp_dispatcher_t *impl = (esp_dispatcher_t *)dh;
    AUDIO_NULL_CHECK(TAG, impl, return ESP_ERR_INVALID_ARG);
    esp_dispatcher_info_t info = {0};
    action_result_t ret = {0};
    info.type = ESP_DISPCH_EVENT_TYPE_CMD;
    xQueueSend(impl->exe_que, &info, portMAX_DELAY);
    xQueueReceive(impl->result_que, &ret, portMAX_DELAY);
    esp_action_exe_item_t *item;
    STAILQ_FOREACH(item, &impl->exe_list, entries) {
        STAILQ_REMOVE(&impl->exe_list, item, evt_exe_item, entries);
        free(item);
    }

    if (impl->result_que) {
        vQueueDelete(impl->result_que);
        impl->result_que = NULL;
    }
    if (impl->exe_que) {
        vQueueDelete(impl->exe_que);
        impl->exe_que = NULL;
    }
    if (impl->mutex) {
        mutex_destroy(impl->mutex);
        impl->mutex = NULL;
    }
    free(impl);
    impl = NULL;
    return ESP_OK;
}
