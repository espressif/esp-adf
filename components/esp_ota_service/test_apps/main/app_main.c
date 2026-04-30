/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "unity.h"

#include "test_utils.h"

static const char *TAG = "esp_ota_service_ut";

/* Ensure NVS is usable before the first test runs. Resume-path tests save/load
 * @c esp_ota_service_resume_point_t records through NVS; the esp_ota_service component does
 * not call nvs_flash_init() on its own, so the application owns that step. */
static void nvs_init_for_tests(void)
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
    unity_run_menu();
    UNITY_END();
    ESP_LOGI(TAG, "<<< ESP_OTA_SERVICE_UT_DONE >>>");
    fflush(stdout);
    vTaskDelete(NULL);
}

void app_main(void)
{
    nvs_init_for_tests();
    /* Same pattern as IDF unit-test-app: heap/leak hooks via test_utils setUp/tearDown. */
    xTaskCreatePinnedToCore(unity_worker, "unity_otaut", UNITY_FREERTOS_STACK_SIZE, NULL,
                            UNITY_FREERTOS_PRIORITY, NULL, UNITY_FREERTOS_CPU);
}
