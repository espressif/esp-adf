/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Platform abstraction for adf_event_hub.
 * On ESP-IDF: includes native FreeRTOS, esp_err, esp_log, esp_timer.
 * On POSIX/PC: uses upstream FreeRTOS-Kernel (POSIX port) + lightweight shims.
 */

#pragma once

#ifdef ESP_PLATFORM

/* Native ESP-IDF */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#else  /* POSIX / PC build */

/* Standard headers */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>

/* Upstream FreeRTOS-Kernel (no "freertos/" prefix) */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* esp_err_t */

typedef int esp_err_t;

#define ESP_OK                 0
#define ESP_FAIL               -1
#define ESP_ERR_NO_MEM         0x101
#define ESP_ERR_INVALID_ARG    0x102
#define ESP_ERR_INVALID_STATE  0x103
#define ESP_ERR_NOT_FOUND      0x105
#define ESP_ERR_TIMEOUT        0x107

/* esp_log */

#define ESP_LOGE(tag, fmt, ...)  fprintf(stderr, "E (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...)  fprintf(stderr, "W (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...)  printf("I (%s): " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...)  ((void)0)

/* esp_timer */

static inline int64_t esp_timer_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

/* portMUX compat: ESP-IDF spinlock API mapped to pthread mutex */

typedef pthread_mutex_t portMUX_TYPE;

#define portMUX_INITIALIZER_UNLOCKED  PTHREAD_MUTEX_INITIALIZER

static inline void portmux_compat_init(portMUX_TYPE *mux)
{
    pthread_mutex_init(mux, NULL);
}
#define portMUX_INITIALIZE(mux)       portmux_compat_init(mux)

#ifdef portENTER_CRITICAL
#undef portENTER_CRITICAL
#endif  /* portENTER_CRITICAL */
#define portENTER_CRITICAL(mux)  pthread_mutex_lock(mux)

#ifdef portEXIT_CRITICAL
#undef portEXIT_CRITICAL
#endif  /* portEXIT_CRITICAL */
#define portEXIT_CRITICAL(mux)  pthread_mutex_unlock(mux)

#ifdef portENTER_CRITICAL_ISR
#undef portENTER_CRITICAL_ISR
#endif  /* portENTER_CRITICAL_ISR */
#define portENTER_CRITICAL_ISR(mux)  pthread_mutex_lock(mux)

#ifdef portEXIT_CRITICAL_ISR
#undef portEXIT_CRITICAL_ISR
#endif  /* portEXIT_CRITICAL_ISR */
#define portEXIT_CRITICAL_ISR(mux)  pthread_mutex_unlock(mux)

#endif  /* ESP_PLATFORM */
