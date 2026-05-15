/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_heap_caps.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_WIFI_SERVICE_INTERNAL_CAPS  (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define ESP_WIFI_SERVICE_EXTERNAL_CAPS  (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

#if CONFIG_SPIRAM
#define ESP_WIFI_SERVICE_CAPS  ESP_WIFI_SERVICE_EXTERNAL_CAPS
#else
#define ESP_WIFI_SERVICE_CAPS  ESP_WIFI_SERVICE_INTERNAL_CAPS
#endif  /* CONFIG_SPIRAM */

#if CONFIG_SPIRAM && CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM
#define ESP_WIFI_SERVICE_TASK_STACK_CAPS  ESP_WIFI_SERVICE_EXTERNAL_CAPS
#else
#define ESP_WIFI_SERVICE_TASK_STACK_CAPS  ESP_WIFI_SERVICE_INTERNAL_CAPS
#endif  /* CONFIG_SPIRAM && CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
