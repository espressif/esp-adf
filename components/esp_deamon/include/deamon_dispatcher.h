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

#ifndef __DEAMON_DISPATCHER_H__
#define __DEAMON_DISPATCHER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_deamon_def.h"

#define DEFAULT_DEAMON_DISPATCHER_STACK_SIZE      (4*1024)
#define DEFAULT_DEAMON_DISPATCHER_TASK_PRIO       (10)
#define DEFAULT_DEAMON_DISPATCHER_TASK_CORE       (0)

/**
 * @brief The dispatcher configuration
 */
typedef struct {
    int                         task_stack;             /*!< >0 Task stack; =0 with out task created */
    int                         task_prio;              /*!< Task priority (based on freeRTOS priority) */
    int                         task_core;              /*!< Task running in core (0 or 1) */
} deamon_dispatcher_config_t;

typedef struct deamon_dispatcher *deamon_dispatcher_handle_t;

#define DEAMON_DISPATCHER_CONFIG_DEFAULT() { \
    .task_stack = DEFAULT_DEAMON_DISPATCHER_STACK_SIZE, \
    .task_prio = DEFAULT_DEAMON_DISPATCHER_TASK_PRIO, \
    .task_core = DEFAULT_DEAMON_DISPATCHER_TASK_CORE, \
}

/**
 * brief      Create deamon dispatcher instance
 *
 * @param cfg   Configuration of the deamon dispatcher instance
 *
 * @return
 *     - NULL,  Fail
 *     - Others, Success
 */
deamon_dispatcher_handle_t deamon_dispatcher_create(deamon_dispatcher_config_t *cfg);

/**
 * brief      Destroy deamon dispatcher instance
 *
 * @param handle   The deamon dispatcher instance
 *
 * @return
 *     - ESP_OK
 *     - ESP_ERR_INVALID_ARG
 */
esp_err_t deamon_dispatcher_destroy(deamon_dispatcher_handle_t handle);

/**
 * brief      Register index of event and execution function to deamon dispatcher instance
 *
 * @param handle            The deamon dispatcher instance
 * @param exe_inst          The execution instance
 * @param sub_event_index   The index of event
 * @param func              The execution function
 *
 * @return
 *     - ESP_OK
 *     - ESP_ERR_ADF_ALREADY_EXISTS
 *     - ESP_ERR_INVALID_ARG
 */
esp_err_t deamon_dispatcher_reg_exe_func(deamon_dispatcher_handle_t handle, void *exe_inst, int sub_event_index, deamon_action_exe_func func);

/**
 * brief      Execution function with specific index of event.
 *            This is a synchronization interface.
 *
 * @param handle            The deamon dispatcher instance
 * @param sub_event_index   The index of event
 * @param arg               The arguments of execution function
 * @param result            The result of execution function
 *
 * @return
 *     - ESP_OK
 *     - ESP_ERR_INVALID_ARG
 *     - ESP_ERR_ADF_TIMEOUT, send request command timeout.
 *     - Others, execute function result.
 */
esp_err_t deamon_dispatcher_execute(deamon_dispatcher_handle_t handle, int sub_event_index,
                                    deamon_arg_t *arg, deamon_result_t *result);
#ifdef __cplusplus
}
#endif

#endif // End of __DEAMON_DISPATCHER_H__
