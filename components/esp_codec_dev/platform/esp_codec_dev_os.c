/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define TICK_PER_MS portTICK_PERIOD_MS
#else
#define TICK_PER_MS portTICK_RATE_MS
#endif

void esp_codec_dev_sleep(int ms)
{
    vTaskDelay(ms / TICK_PER_MS);
}
