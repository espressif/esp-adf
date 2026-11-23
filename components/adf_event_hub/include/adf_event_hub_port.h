/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Platform abstraction for adf_event_hub.
 * On ESP-IDF: includes native FreeRTOS, esp_err and esp_log.
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
#define ESP_ERR_INVALID_SIZE   0x104
#define ESP_ERR_NOT_FOUND      0x105
#define ESP_ERR_TIMEOUT        0x107
#define ESP_ERR_NOT_FINISHED   0x10C

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

#endif  /* ESP_PLATFORM */
