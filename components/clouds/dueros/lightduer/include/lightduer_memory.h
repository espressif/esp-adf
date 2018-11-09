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
// Author: Su Hao (suhao@baidu.com)
//
// Description: The APIs for memory management.

#ifndef BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MEMORY_H
#define BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MEMORY_H

#include "lightduer_types.h"
#include "lightduer_lib.h"

#ifdef DUER_MEMORY_USAGE
DUER_INT void duer_memdbg_usage();
#define DUER_MEMDBG_USAGE(...)       duer_memdbg_usage()
#else
#define DUER_MEMDBG_USAGE(...)
#endif

#ifdef DUER_MEMORY_DEBUG
#define DUER_MALLOC(_s)          duer_malloc_ext(_s, __FILE__, __LINE__)
#define DUER_CALLOC(_s, _n)      duer_calloc_ext((_s) * (_n), __FILE__, __LINE__)
#define DUER_REALLOC(_p, _s)     duer_realloc_ext(_p, _s, __FILE__, __LINE__)
#define DUER_FREE(_p)            duer_free_ext(_p, __FILE__, __LINE__)
#else/*DUER_MEMORY_DEBUG*/
#define DUER_MALLOC(_s)          duer_malloc(_s)
#define DUER_CALLOC(_s, _n)      duer_calloc((_s) * (_n))
#define DUER_REALLOC(_p, _s)     duer_realloc(_p, _s)
#define DUER_FREE(_p)            duer_free(_p)
#endif/*DUER_MEMORY_DEBUG*/



#ifdef __cplusplus
extern "C" {
#endif

/*
 * Adapt the memory management functions.
 */
typedef void* (*duer_malloc_f)(duer_context context, duer_size_t size);
typedef void* (*duer_realloc_f)(duer_context context, void*, duer_size_t size);
typedef void (*duer_free_f)(duer_context context, void* p);

/*
 * Set the memory management functions.
 *
 * @Param ctx, duer_context, in, the memory context for user
 * @Param f_malloc, duer_malloc_f, the memory alloc function for user
 * @Param f_realloc, duer_realloc_f, the memory realloc function for user,
 * @Param f_free, duer_free_f, the memory free function for user
 *
 * If f_malloc or f_free set to NULL, we will use default libc instead all of it;
 * If only f_realloc set to NULL, we will realized it by f_malloc and f_free.
 */
DUER_EXT void baidu_ca_memory_init(duer_context context,
                                  duer_malloc_f f_malloc,
                                  duer_realloc_f f_realloc,
                                  duer_free_f f_free);

/*
 * malloc function
 *
 * @Param size, duer_size_t, the expected size of the memory
 * @Return the alloced memory pointer
 */
DUER_INT void* duer_malloc(duer_size_t size);

/*
 * cmalloc function zero-initialize the memory before return
 *
 * @Param size, duer_size_t, the expected size of the memory
 * @Return the alloced memory pointer
 */
DUER_INT void* duer_calloc(duer_size_t size);

/*
 * realloc function
 *
 * @Param ptr, void *, the old alloced memory
 * @Param size, duer_size_t, the expected size of the memory
 * @Return the new alloced memory pointer
 */
DUER_INT void* duer_realloc(void* ptr, duer_size_t size);

/*
 * free function
 *
 * @Param ptr, void *, the alloced memory
 */
DUER_INT void duer_free(void* ptr);

/*
 * malloc function
 *
 * @Param size, duer_size_t, the expected size of the memory
 * @Param file, const char *, the file name when alloc the memory
 * @Param line, duer_u32_t, the line number of the file when alloc the memory
 * @Return the alloced memory pointer
 */
DUER_INT void* duer_malloc_ext(duer_size_t size, const char* file, duer_u32_t line);

/*
 * malloc function, zero-initialize before return
 *
 * @Param size, duer_size_t, the expected size of the memory
 * @Param file, const char *, the file name when alloc the memory
 * @Param line, duer_u32_t, the line number of the file when alloc the memory
 * @Return the alloced memory pointer
 */
DUER_INT void* duer_calloc_ext(duer_size_t size, const char* file, duer_u32_t line);

/*
 * realloc function
 *
 * @Param ptr, void *, the old alloced memory
 * @Param size, duer_size_t, the expected size of the memory
 * @Param file, const char *, the file name when alloc the memory
 * @Param line, duer_u32_t, the line number of the file when alloc the memory
 * @Return the new alloced memory pointer
 */
DUER_INT void* duer_realloc_ext(void* ptr, duer_size_t size, const char* file,
                              duer_u32_t line);

/*
 * free function
 *
 * @Param ptr, void *, the alloced memory
 * @Param file, const char *, the file name when alloc the memory
 * @Param line, duer_u32_t, the line number of the file when alloc the memory
 */
DUER_INT void duer_free_ext(void* ptr, const char* file, duer_u32_t line);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MEMORY_H
