// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_OSAL_VOLC_OSAL_H__
#define __CONV_AI_OSAL_VOLC_OSAL_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* volc_osal_malloc(size_t size);
void* volc_osal_calloc(size_t num, size_t size);
void* volc_osal_realloc(void* ptr, size_t new_size);
void volc_osal_free(void* ptr);
#define HAL_SAFE_FREE(ptr) do { if (ptr) { volc_osal_free(ptr); ptr = NULL; } } while (0)

typedef void* volc_osal_mutex_t;
volc_osal_mutex_t volc_osal_mutex_create(void);
void volc_osal_mutex_lock(volc_osal_mutex_t mutex);
void volc_osal_mutex_unlock(volc_osal_mutex_t mutex);
void volc_osal_mutex_destroy(volc_osal_mutex_t mutex);

typedef void* volc_osal_sem_t;
volc_osal_sem_t volc_osal_sem_create(void);
void volc_osal_sem_wait(volc_osal_sem_t sem);
void volc_osal_sem_post(volc_osal_sem_t sem);
void volc_osal_sem_destroy(volc_osal_sem_t sem);

uint64_t volc_osal_get_time_ms(void);

int volc_osal_get_uuid(char* uuid, size_t size);

#define THREAD_NAME_MAX_LEN 16
typedef void* volc_osal_tid_t;
typedef struct {
    char name[THREAD_NAME_MAX_LEN];
    int priority;
    int stack_size;
    int bind_cpu;
    int stack_in_ext;
} volc_osal_thread_param_t;

#if defined(PLATFORM_MACOS)|| (PLATFORM_LINUX)
int volc_osal_thread_create(volc_osal_tid_t* thread, const volc_osal_thread_param_t* param, void* (*start_routine)(void *), void* args);
#else
int volc_osal_thread_create(volc_osal_tid_t* thread, const volc_osal_thread_param_t* param, void (*start_routine)(void *), void* args);
#endif
int volc_osal_thread_detach(volc_osal_tid_t thread);
void volc_osal_thread_exit(volc_osal_tid_t thread);
void volc_osal_thread_sleep(int time_ms);
void volc_osal_thread_destroy(volc_osal_tid_t thread);

int volc_osal_get_platform_info(char* info, size_t size);
int volc_osal_fill_random(uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_OSAL_VOLC_OSAL_H__ */
