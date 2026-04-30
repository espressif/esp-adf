/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * FreeRTOS configuration for POSIX (Linux/macOS) simulator.
 * Used only for PC-based unit testing of adf_event_hub.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdio.h>
#include <stdlib.h>

#define configUSE_PREEMPTION                     1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configTICK_RATE_HZ                       (1000)
#define configMINIMAL_STACK_SIZE                 ((unsigned short)4096)
#define configMAX_PRIORITIES                     (7)
#define configMAX_TASK_NAME_LEN                  (24)
#define configUSE_TRACE_FACILITY                 0
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                20
#define configUSE_QUEUE_SETS                     0
#define configUSE_TIME_SLICING                   1
#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configNUMBER_OF_CORES                    1
#define configUSE_NEWLIB_REENTRANT               0
#define configENABLE_BACKWARD_COMPATIBILITY      1
#define configUSE_TASK_NOTIFICATIONS             1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    3

#define configCHECK_FOR_STACK_OVERFLOW  \
    0
#define configUSE_MALLOC_FAILED_HOOK  \
    0
#define configUSE_TIMERS  \
    0

#define configASSERT(x)  if (!(x)) {                \
    fprintf(stderr, "ASSERT FAILED: %s [%s:%d]\n",  \
            #x, __FILE__, __LINE__);                \
    fflush(stderr);                                 \
    abort();                                        \
}

#define INCLUDE_vTaskPrioritySet             1
#define INCLUDE_uxTaskPriorityGet            1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_vTaskSuspend                 1
#define INCLUDE_vTaskDelayUntil              1
#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetSchedulerState       1
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_uxTaskGetStackHighWaterMark  0
#define INCLUDE_xTimerPendFunctionCall       0
#define INCLUDE_eTaskGetState                1

#endif  /* FREERTOS_CONFIG_H */
