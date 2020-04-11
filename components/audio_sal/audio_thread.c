/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "audio_mem.h"
#include "audio_thread.h"

static const char *TAG = "AUDIO_THREAD";

struct audio_thread {
    xTaskHandle                 task_handle;
    StackType_t                 *element_task_stack;
    StaticTask_t                *element_task_buf;
};

esp_err_t audio_thread_create(audio_thread_t *p_handle, const char *name, void(*main_func)(void *arg), void *arg,
                              uint32_t stack, int prio, bool stack_in_ext, int core_id)
{
    audio_thread_t handle = *p_handle;
    if (handle == NULL) {
        handle = (audio_thread_t) audio_calloc(1, sizeof (struct audio_thread));
    }
    if (handle == NULL) {
        ESP_LOGE(TAG, "allocation for handle failed");
        return ESP_FAIL;
    }

#if defined (CONFIG_SPIRAM_BOOT_INIT) && defined (CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY)
    ESP_LOGD(TAG, "The %s task allocate stack on external memory", name);
    if (stack_in_ext) {
        if (handle->element_task_stack == NULL) {
            handle->element_task_stack = (StackType_t *) audio_calloc(1, stack);
        }
        if (handle->element_task_stack == NULL) {
            ESP_LOGE(TAG, "Failed to allocate task_stack for %s", name);
            goto audio_thread_create_error;
        }
        if (handle->element_task_buf == NULL) {
            handle->element_task_buf = (StaticTask_t *) audio_calloc_inner(1, sizeof(StaticTask_t));
        }
        if (handle->element_task_buf == NULL) {
            ESP_LOGE(TAG, "Failed to allocate task_buf for %s", name);
            goto audio_thread_create_error;
        }
        handle->task_handle = xTaskCreateStaticPinnedToCore(main_func, name, stack, arg, prio, handle->element_task_stack,
                              handle->element_task_buf, core_id);
        if (handle->task_handle == NULL) {
            ESP_LOGE(TAG, "Error creating task %s", name);
            goto audio_thread_create_error;
        }
    } else
#endif
    {
        if (xTaskCreatePinnedToCore(main_func, name, stack, arg, prio, &handle->task_handle, core_id) != pdPASS) {
            ESP_LOGE(TAG, "Error creating task %s", name);
            goto audio_thread_create_error;
        }
    }
    *p_handle = handle;
    return ESP_OK;

audio_thread_create_error:
    if (handle->element_task_stack) {
        audio_free(handle->element_task_stack);
        handle->element_task_stack = NULL;
    }
    if (handle->element_task_buf) {
        audio_free(handle->element_task_buf);
        handle->element_task_buf = NULL;
    }
    return ESP_FAIL;
}

esp_err_t audio_thread_cleanup(audio_thread_t *p_handle)
{
    audio_thread_t handle = *p_handle;
    if (handle == NULL) {
        ESP_LOGE(TAG, "Thread is already clean");
        return ESP_FAIL; /* Invalid argument */
    }
    if (handle->element_task_stack) {
        audio_free(handle->element_task_stack);
        handle->element_task_stack = NULL;
    }
    /**
     * Sometimes FreeRTOS freeing of task memory will still be delegated to the Idle Task.
     * The TCB buffer handle->element_task_buf should be free after task really delete.
     * Delay 100 ms to wait for Idle task free the deleted task memory.
     */
    vTaskDelay(100 / portTICK_RATE_MS);
    if (handle->element_task_buf) {
        audio_free(handle->element_task_buf);
        handle->element_task_buf = NULL;
    }
    audio_free(handle);
    *p_handle = NULL;
    return ESP_OK;
}

esp_err_t audio_thread_delete_task(audio_thread_t *p_handle)
{
    audio_thread_t handle = *p_handle;
    if (handle == NULL) {
        ESP_LOGE(TAG, "Thread is already clean");
        return ESP_FAIL; /* Invalid argument */
    }
    xTaskHandle task_handle = handle->task_handle;
    if (task_handle == NULL) {
        ESP_LOGE(TAG, "Task is already destroyed!");
        return ESP_FAIL;
    }
    handle->task_handle = NULL;
    vTaskDelete(task_handle);
    return ESP_OK; /* Control never reach here if this is self delete */
}
