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
// Author: Chen Xihao (chenxihao@baidu.com)
//
// Description: Wrapper for semaphore

#ifndef BAIDU_LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_SEMAPHORE_H
#define BAIDU_LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_SEMAPHORE_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WAIT_FOR_EVER 0xffffffffu

typedef void* duer_semaphore_t;

/*
 * Create a semaphore object
 *
 * @Param count, number of available resources
 * @Return duer_semaphore_t, the created semaphore object
 */
DUER_INT duer_semaphore_t duer_semaphore_create(int count);

/*
 * Wait semaphore for specified time
 *
 * @Param semaphore, the semaphore object
 * @Param millisec, timeout value or 0 in case of no time-out
 * @Return duer_status_t, the operation result
 */
DUER_INT duer_status_t duer_semaphore_wait(duer_semaphore_t semaphore, duer_u32_t millisec);

/*
 * Release semaphore
 *
 * @Param semaphore, the semaphore object
 * @Return duer_status_t, the operation result
 */
DUER_INT duer_status_t duer_semaphore_release(duer_semaphore_t semaphore);

/*
 * Destroy semaphore object
 *
 * @Param semaphore, the semaphore object
 * @Return duer_status_t, the operation result
 */
DUER_INT duer_status_t duer_semaphore_destroy(duer_semaphore_t semaphore);

/*
 * The semaphore callbacks
 */
typedef duer_semaphore_t (*duer_semaphore_create_f)(int count);
typedef duer_status_t (*duer_semaphore_wait_f)(duer_semaphore_t semaphore, duer_u32_t millisec);
typedef duer_status_t (*duer_semaphore_release_f)(duer_semaphore_t semaphore);
typedef duer_status_t (*duer_semaphore_destroy_f)(duer_semaphore_t semaphore);

/*
 * Initial the semaphore callbacks
 *
 * @Param f_create, in, the function create semaphore context
 * @Param f_wait, in, the function wait semaphore
 * @Param f_release, in, the function release semaphore
 * @Param f_destroy, in, the function destroy semaphore context
 */
DUER_EXT void duer_semaphore_init(duer_semaphore_create_f f_create,
                                  duer_semaphore_wait_f f_wait,
                                  duer_semaphore_release_f f_release,
                                  duer_semaphore_destroy_f f_destroy);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_LIBDUER_DEVICE_FRAMEWORK_CORE_LIGHTDUER_SEMAPHORE_H
