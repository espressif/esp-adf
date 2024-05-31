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
 * File: lightduer_queue_cache.h
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Provide the Cache util in the Queue.
 */

#ifndef BAIDU_DUER_LIGHTDUER_COMMON_LIGHTDUER_QUEUE_CACHE_H
#define BAIDU_DUER_LIGHTDUER_COMMON_LIGHTDUER_QUEUE_CACHE_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *duer_qcache_handler;

typedef void *duer_qcache_iterator;

typedef void (duer_qcache_visit_func)(void *);

duer_qcache_handler duer_qcache_create(void);

int duer_qcache_push(duer_qcache_handler cache, void *data);

int duer_qcache_insert(duer_qcache_handler cache, duer_qcache_iterator it, void *data);

void *duer_qcache_top(duer_qcache_handler cache);

void *duer_qcache_pop(duer_qcache_handler cache);

duer_qcache_iterator duer_qcache_head(duer_qcache_handler cache);

duer_qcache_iterator duer_qcache_next(duer_qcache_iterator it);

void *duer_qcache_data(duer_qcache_iterator it);

size_t duer_qcache_length(duer_qcache_handler cache);

void duer_qcache_destroy_traverse(duer_qcache_handler cache, duer_qcache_visit_func visit);

void duer_qcache_destroy(duer_qcache_handler cache);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_COMMON_LIGHTDUER_QUEUE_CACHE_H*/
