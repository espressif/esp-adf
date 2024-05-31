/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_thread.h
 * Auth: Chang Li (changli@baidu.com)
 * Desc: Provide the thread APIs.
 */

#ifndef LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_THREAD_H
#define LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_THREAD_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CORE_0,
    CORE_1,
    CORE_NONE
} cpu_core_id_t;

typedef void * duer_thread_t;

typedef struct {
    duer_thread_t p_thread;
    void *args;
} duer_thread_entry_args_t;

typedef void (*duer_thread_entry_f_t)(duer_thread_entry_args_t *);

/*
 * @Desc Create the specific thread object.
 *
 * @Param thread name.
 * @Param thread priority.
 * @Param thread stack size in bytes.
 * @Param thread stack buffer is static or dynamic.
 * @Param cpu_core_id_t, thread is created on which core.
 *
 * @Ret thread object.
 */
duer_thread_t duer_thread_create(char *name, duer_u8_t priority, duer_u32_t stack_size, duer_bool is_static, cpu_core_id_t core_id);

/*
 * @Desc Start the specific thread.
 *
 * @Param duer_thread_t object.
 * @Param thread entry function.
 * @Param arguments passed to the entry function.
 *
 * @Ret Success: DUER_OK, Failure: DUER_ERR_FAILED.
 */
duer_status_t duer_thread_start(duer_thread_t thread, duer_thread_entry_f_t entry_func, void *args);

/*
 * @Desc Thread exit function. Note: Currently it only be called by the thread its self!!!
 *
 * @Param duer_thread_entry_args_t object.
 * @Param is_released=true, resources can be released automatically, without
 * calling duer_thread_destroy.
 *
 * @Ret Success: DUER_OK, Failure: DUER_ERR_FAILED.
 */
duer_status_t duer_thread_exit(duer_thread_entry_args_t *entry_args, duer_bool is_released);

/*
 * @Desc Destroy thread object and release resources.
 *
 * @Param duer_thread_t object.
 *
 * @Ret Success: DUER_OK, Failure: DUER_ERR_FAILED.
 */
duer_status_t duer_thread_destroy(duer_thread_t thread);

/*
 * @Desc Get the thread id. For FreeRTOS, it is TaskHandle_t. TODO: for 64 bit OS.
 *
 * @Param duer_thread_t object.
 *
 * @Ret thread id.
 */
duer_u32_t duer_thread_get_id(duer_thread_t thread);

/*
 * @Desc Dump the stack usage of thread
 *
 * @Param thread, the thread object
 * @Return duer_status_t, the operation result
 */
duer_status_t duer_thread_get_stack_info(duer_thread_t thread);

/*
 * The thread callbacks
 */
typedef duer_thread_t (*duer_thread_create_f_t)(char *, duer_u8_t, duer_u32_t, duer_bool, cpu_core_id_t core_id);
typedef duer_status_t (*duer_thread_start_f_t)(duer_thread_t, duer_thread_entry_f_t, void *);
typedef duer_status_t (*duer_thread_exit_f_t)(duer_thread_entry_args_t *, duer_bool is_released);
typedef duer_status_t (*duer_thread_destroy_f_t)(duer_thread_t);
typedef duer_u32_t (*duer_thread_get_id_f_t)(duer_thread_t);
typedef duer_status_t (*duer_thread_get_stack_info_f)(duer_thread_t thread);

/*
 * Initial the thread callbacks for lightduer
 *
 * @Param _f_create implementation.
 * @Param _f_start implementation.
 * @Param _f_exit implementation.
 * @Param _f_destroy implementation.
 * @Param _f_get_id implementation.
 */
DUER_EXT void duer_thread_init(
        duer_thread_create_f_t _f_create,
        duer_thread_start_f_t _f_start,
        duer_thread_exit_f_t _f_exit,
        duer_thread_destroy_f_t _f_destroy,
        duer_thread_get_id_f_t _f_get_id,
        duer_thread_get_stack_info_f f_get_stack_info);

#ifdef __cplusplus
}
#endif

#endif // LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_THREAD_H
