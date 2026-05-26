/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "unity.h"

#include "test_utils.h"

static const char *TAG = "esp_wifi_service_ut";

static void nvs_init_for_wifi(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void unity_worker(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(100));
    UNITY_BEGIN();
    unity_run_tests_by_tag("[wifi_service]", false);
    UNITY_END();
    ESP_LOGI(TAG, "<<< ESP_WIFI_SERVICE_UT_DONE >>>");
    fflush(stdout);
    vTaskDelete(NULL);
}

void app_main(void)
{
    nvs_init_for_wifi();
    xTaskCreatePinnedToCore(unity_worker, "wifi_svc_ut", UNITY_FREERTOS_STACK_SIZE, NULL,
                            UNITY_FREERTOS_PRIORITY, NULL, UNITY_FREERTOS_CPU);
}
