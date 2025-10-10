/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_idf_version.h"
#include "esp_codec_dev_os.h"

void esp_codec_dev_sleep(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

esp_codec_dev_mutex_handle_t esp_codec_dev_mutex_create(void)
{
    return (esp_codec_dev_mutex_handle_t)xSemaphoreCreateMutex();
}

int esp_codec_dev_mutex_lock(esp_codec_dev_mutex_handle_t mutex, int timeout_ms)
{
    if (mutex == NULL) {
        return -1;
    }
    return xSemaphoreTake((SemaphoreHandle_t)mutex, pdMS_TO_TICKS(timeout_ms)) ? 0 : -1;
}

int esp_codec_dev_mutex_unlock(esp_codec_dev_mutex_handle_t mutex)
{
    if (mutex == NULL) {
        return -1;
    }
    return xSemaphoreGive((SemaphoreHandle_t)mutex) ? 0 : -1;
}

void esp_codec_dev_mutex_destroy(esp_codec_dev_mutex_handle_t mutex)
{
    if (mutex == NULL) {
        return;
    }
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
}
