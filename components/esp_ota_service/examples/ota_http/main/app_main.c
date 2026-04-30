/**
 * HTTP OTA Upgrade Example
 *
 * Downloads firmware from a local HTTP server and performs OTA upgrade.
 *
 * Usage (run from components/esp_ota_service/tools/):
 *   1. Build + publish a versioned payload:
 *        ./build_firmware.py http -v 1.0.1
 *   2. Start the HTTP server (serves ota_http_v1.0.1.bin at /firmware.bin):
 *        ./ota_http_serve.py --update-manifest-url
 *   3. Set ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL in ota_http_example.c to the URL printed above,
 *      flash the device, bring WiFi up, then esp_service_start().
 *      OTA service runs a version check first; if the server image is not
 *      newer it emits ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK with upgrade_available=false
 *      and the service continues with the next item.
 *   4. Bump version.txt, rerun build_firmware.py http to publish, repeat.
 *
 * Upgrade is considered OK only after: download + reboot into the new slot,
 * WiFi comes up on the new image, then esp_ota_service_confirm_update() marks the
 * image valid (rollback disabled). If WiFi fails while the image is still
 * PENDING_VERIFY, the device rolls back to the previous firmware.
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"

#include "esp_ota_service.h"
#include "esp_ota_service_http_example.h"

static const char *TAG = "OTA_HTTP_APP";

void app_main(void)
{
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Project:  %s", app->project_name);
    ESP_LOGI(TAG, "  Version:  %s", app->version);
    ESP_LOGI(TAG, "  Built:    %s %s", app->date, app->time);
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "  Partition: %s (0x%08" PRIx32 ")",
             running ? running->label : "<unknown>",
             running ? running->address : 0);
    ESP_LOGI(TAG, "========================================");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    esp_ota_img_states_t img_state = ESP_OTA_IMG_UNDEFINED;
    if (running != NULL) {
        (void)esp_ota_get_state_partition(running, &img_state);
    }
    ESP_LOGI(TAG, "Running partition OTA state: %d (1=PENDING_VERIFY after OTA reboot)", (int)img_state);

    ESP_LOGI(TAG, "Connecting to WiFi ...");
    err = esp_ota_service_http_example_wifi_connect();
    if (err != ESP_OK) {
        if (img_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGE(TAG, "App_main failed: WiFi unavailable while image pending verify — rolling back (%s)",
                     esp_err_to_name(err));
            (void)esp_ota_service_rollback();
        } else {
            /* WiFi failure already logged by esp_ota_service_http_example_wifi_connect() */
        }
        goto idle;
    }

    (void)esp_ota_service_http_example_confirm_if_pending_verify(img_state);

    (void)esp_ota_service_http_example_run_ota_session();
    /* OTA session outcome already logged by esp_ota_service_http_example_run_ota_session() */

idle:
    ESP_LOGI(TAG, "Running version %s — waiting for next upgrade ...", app->version);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "[%s] Still running on %s", app->version,
                 running ? running->label : "<unknown>");
    }
}
