// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_osal.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void* volc_osal_malloc(size_t size) {
    return heap_caps_malloc(size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void* volc_osal_calloc(size_t num, size_t size) {
    return heap_caps_calloc(num,size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void* volc_osal_realloc(void* ptr, size_t new_size) {
    return heap_caps_realloc(ptr,new_size,MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
}

void volc_osal_free(void* ptr) {
    heap_caps_free(ptr);
}

volc_osal_mutex_t volc_osal_mutex_create(void) {
    pthread_mutex_t* p_mutex = NULL;
    pthread_mutexattr_t attr;

    p_mutex = (pthread_mutex_t *)volc_osal_calloc(1, sizeof(pthread_mutex_t));
    if (NULL == p_mutex) {
        return NULL;
    }

    if (0 != pthread_mutexattr_init(&attr) ||
        0 != pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL) ||
        0 != pthread_mutex_init(p_mutex, &attr)) {
        volc_osal_free(p_mutex);
        return NULL;
    }

    return (volc_osal_mutex_t)p_mutex;
}
void volc_osal_mutex_lock(volc_osal_mutex_t mutex) {
    pthread_mutex_lock((pthread_mutex_t *)mutex);
}

void volc_osal_mutex_unlock(volc_osal_mutex_t mutex) {
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

void volc_osal_mutex_destroy(volc_osal_mutex_t mutex) {
    pthread_mutex_t* p_mutex = (pthread_mutex_t *)mutex;
    if (NULL == p_mutex) {
        return;
    }
    pthread_mutex_destroy(p_mutex);
    volc_osal_free(p_mutex);
}

volc_osal_sem_t volc_osal_sem_create() {
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (NULL == sem) {
        return NULL;
    }
    return (volc_osal_sem_t)sem;
}

void volc_osal_sem_wait(volc_osal_sem_t sem) {
    if (NULL == sem) {
        return;
    }
    xSemaphoreTake((SemaphoreHandle_t)sem, portMAX_DELAY);
}

void volc_osal_sem_post(volc_osal_sem_t sem) {
    if (NULL == sem) {
        return;
    }
    xSemaphoreGive((SemaphoreHandle_t)sem);
}

void volc_osal_sem_destroy(volc_osal_sem_t sem) {
    if (NULL == sem) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)sem);
}

uint64_t volc_osal_get_time_ms(void) {
    struct timespec now_time;
    clock_gettime(CLOCK_REALTIME, &now_time);
    return (uint64_t)now_time.tv_sec * 1000 + (uint64_t)now_time.tv_nsec / 1000000;
}

int volc_osal_get_uuid(char* uuid, size_t size) {
    esp_netif_t *netif = NULL;

    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        uint8_t hwaddr[6] = {0};
        esp_netif_get_mac(netif, hwaddr);
        snprintf(uuid, size,"%02X%02X%02X%02X%02X%02X",hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
    }
    return 0;
}

int volc_osal_thread_create(volc_osal_tid_t* thread, const volc_osal_thread_param_t* param, void (*start_routine)(void *), void* args) {
    int ret = 0;
    int stack_size = 0;
    int priority = 0;
    BaseType_t core_id = 0;
    TaskHandle_t* handle = NULL;
    if (NULL == thread || NULL == start_routine) {
        return -1;
    }

    if (NULL != param) {
        stack_size = param->stack_size <= 0 ? 8192 : param->stack_size;
        priority = param->priority <= 0 ? 3 : param->priority;
        core_id = 1;
    } else {
        stack_size = 8192;
        priority = 3;
        core_id = tskNO_AFFINITY;
    }
    handle = (TaskHandle_t *)volc_osal_calloc(1, sizeof(TaskHandle_t));
    if (NULL == handle) {
        return -1;
    }
    *thread = (volc_osal_tid_t *)handle;

    if(param->stack_in_ext) {
        ret = xTaskCreatePinnedToCoreWithCaps(start_routine, param->name, stack_size, args, priority, handle, core_id, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    } else {
        ret = xTaskCreatePinnedToCore(start_routine, param->name, stack_size, args, priority, handle, core_id);
    }
    if (pdPASS != ret) {
        return -1;
    }
    return 0;
}
int volc_osal_thread_detach(volc_osal_tid_t thread) {
    return pthread_detach((pthread_t)thread);
}

void volc_osal_thread_exit(volc_osal_tid_t thread) {
    vTaskDelete(NULL);
}

void volc_osal_thread_sleep(int time_ms) {
    vTaskDelay(pdMS_TO_TICKS(time_ms));
}

void volc_osal_thread_destroy(volc_osal_tid_t thread) {
    if (NULL == thread) {
        return;
    }
    volc_osal_free(thread);
}

int volc_osal_get_platform_info(char* info, size_t size) {
    if (NULL == info || size <= 0) {
        return -1;
    }
    snprintf(info, size, "esp32");
    return 0;
}

int volc_osal_fill_random(uint8_t* data, size_t size) {
    if (NULL == data || size <= 0) {
        return -1;
    }
    esp_fill_random((void *)data, size);
    return 0;
}
