/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_pm.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Wakeup and connection event bits shared by the example managers
 */
#define WAKEUP_TIMER_BIT    BIT(0)  /*!< Timer wakeup event bit */
#define WAKEUP_MQTT_BIT     BIT(1)  /*!< MQTT wakeup event bit */
#define WAKEUP_GPIO_BIT     BIT(2)  /*!< GPIO wakeup event bit */
#define WAKEUP_UART_BIT     BIT(3)  /*!< UART wakeup event bit */
#define MQTT_CONNECTED_BIT  BIT(4)  /*!< MQTT connected event bit */
#define WIFI_CONNECTED_BIT  BIT(5)  /*!< Wi-Fi connected event bit */
#define WIFI_FAIL_BIT       BIT(6)  /*!< Wi-Fi connection failure event bit */

/**
 * @brief  Shared runtime context for the power save example
 */
typedef struct {
    EventGroupHandle_t    wakeup_event;            /*!< Event group used for connection and wakeup events */
    esp_pm_lock_handle_t  audio_pm_lock;           /*!< PM lock held while audio resources are active */
    bool                  audio_pm_lock_acquired;  /*!< True when audio_pm_lock is currently acquired */
    volatile bool         waiting_for_wakeup;      /*!< True while the example is waiting for a wakeup event */
    EventBits_t           last_wakeup_bits;        /*!< Wakeup bits captured during the last idle wait */
} power_save_context_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
