/**
 * BLE OTA using esp_ota_service with the official ESP BLE OTA APP protocol.
 *
 * Uses esp_ota_service_source_ble_create() (official ESP BLE OTA APP protocol) so the device can be
 * upgraded with the official ESP BLE OTA Android/iOS APP.
 *
 * Usage:
 *   1. Flash this firmware.
 *   2. Open the ESP BLE OTA APP, scan and connect to the device.
 *   3. Select a firmware .bin and start OTA.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "esp_ota_service_ble_example.h"

static const char *TAG = "esp_ota_svc_ble_ex";

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  BLE OTA (esp_ota_service + APP protocol)");
    ESP_LOGI(TAG, "  Project:  %s", app->project_name);
    ESP_LOGI(TAG, "  Version:  %s", app->version);
    ESP_LOGI(TAG, "  Partition: %s", running->label);
    ESP_LOGI(TAG, "  Use the ESP BLE OTA APP to scan, connect and push firmware.");
    ESP_LOGI(TAG, "========================================");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    (void)esp_ota_service_ble_example_confirm_running_image();
    /* Outcome already logged by esp_ota_service_ble_example_confirm_running_image() */

    (void)esp_ota_service_ble_example_run_ota();
    /* Outcome already logged by esp_ota_service_ble_example_run_ota() */

    ESP_LOGI(TAG, "Idle. Restart to try BLE OTA again.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
