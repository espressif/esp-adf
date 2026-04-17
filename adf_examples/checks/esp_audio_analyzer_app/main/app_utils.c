/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_gmf_oal_sys.h"

static const char *TAG = "APP_UTILS";

#define UTILS_TASK_STATUS_MONITOR_INTERVAL  (5000)

void app_utils_show_memory(void)
{
    ESP_LOGI(TAG, "Internal free heap size: %ld bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "Internal largest free block: %d bytes", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM  free heap size: %ld bytes", esp_get_free_heap_size() - esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "Total  free heap size: %ld bytes", esp_get_free_heap_size());
}

static void utils_task_status_monitor(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        esp_gmf_oal_sys_get_real_time_stats(UTILS_TASK_STATUS_MONITOR_INTERVAL, false);
        app_utils_show_memory();

        vTaskDelay(pdMS_TO_TICKS(UTILS_TASK_STATUS_MONITOR_INTERVAL));
    }
}

void app_utils_task_status_monitor_start(void)
{
    BaseType_t ret = xTaskCreate(utils_task_status_monitor, "task_status", (4 * 1024), NULL, 2, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task status monitor task");
    }
}
