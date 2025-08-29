/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "unity.h"

#include "test_utils.h"

static const char *TAG = "adf_event_hub_ut";

static void unity_worker(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(100));
    UNITY_BEGIN();
    unity_run_menu();
    UNITY_END();
    ESP_LOGI(TAG, "<<< ADF_EVENT_HUB_UT_DONE >>>");
    fflush(stdout);
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreatePinnedToCore(unity_worker, "unity_ehut", UNITY_FREERTOS_STACK_SIZE, NULL,
                            UNITY_FREERTOS_PRIORITY, NULL, UNITY_FREERTOS_CPU);
}
