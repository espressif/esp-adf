/* upgrade app and data partition Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "audio_common.h"
#include "board.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "tcpip_adapter.h"

#include "audio_mem.h"
#include "ota_service.h"
#include "ota_proc_default.h"

static const char *TAG = "HTTPS_OTA_EXAMPLE";
static EventGroupHandle_t events = NULL;

#define OTA_FINISH (BIT0)

typedef struct {
    int major_ver;
    int minor_ver;
} data_image_ver_t;

static bool ota_data_iamge_need_upgrade(void *handle, ota_node_attr_t *node)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, node->label);
    if (partition == NULL) {
        ESP_LOGE(TAG, "data partition [%s] not found", node->label);
        return false;
    }
    data_image_ver_t cur_ver = { 0 };
    data_image_ver_t incoming_ver = { 0 };
    if (esp_partition_read(partition, 0, (char *)&cur_ver, sizeof(data_image_ver_t)) != ESP_OK) {
        return false;
    }
    if (ota_data_image_stream_read(handle, (char *)&incoming_ver, sizeof(data_image_ver_t)) != ESP_OK) {
        return false;
    }
    ESP_LOGI(TAG, "current version %d.%d, the incoming image version %d.%d", cur_ver.major_ver, cur_ver.minor_ver, incoming_ver.major_ver, incoming_ver.minor_ver);
    if (cur_ver.major_ver != incoming_ver.major_ver || cur_ver.minor_ver != incoming_ver.minor_ver) {
        esp_err_t err = ESP_OK;
        if ((err = esp_partition_erase_range(partition, 0, partition->size)) != ESP_OK) {
            ESP_LOGE(TAG, "Erase [%s] failed and return %d", node->label, err);
            return false;
        }
        ota_data_partition_write(handle, (char *)&incoming_ver, sizeof(data_image_ver_t));
        return true;
    } else {
        return false;
    }
}

static esp_err_t ota_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    if (evt->type == OTA_SERV_EVENT_TYPE_RESULT) {
        ota_result_t *result_data = evt->data;
        if (result_data->result != ESP_OK) {
            ESP_LOGE(TAG, "List id: %d, OTA failed", result_data->id);
        } else {
            ESP_LOGI(TAG, "List id: %d, OTA success", result_data->id);
        }
    } else if (evt->type == OTA_SERV_EVENT_TYPE_FINISH) {
        xEventGroupSetBits(events, OTA_FINISH);
    }

    return ESP_OK;
}

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    tcpip_adapter_init();

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[1.2] Mount SDCard");
    audio_board_sdcard_init(set);

    ESP_LOGI(TAG, "[2.0] Create OTA service");
    ota_service_config_t ota_service_cfg = OTA_SERVICE_DEFAULT_CONFIG();
    ota_service_cfg.task_stack = 8 * 1024;
    ota_service_cfg.evt_cb = ota_service_cb;
    ota_service_cfg.cb_ctx = NULL;
    periph_service_handle_t ota_service = ota_service_create(&ota_service_cfg);
    events = xEventGroupCreate();

    ESP_LOGI(TAG, "[2.1] Set upgrade list");
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                CONFIG_DATA_PARTITION_LABEL,
                CONFIG_DATA_UPGRADE_URI,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            false,
            false
        },
        {
            {
                ESP_PARTITION_TYPE_APP,
                NULL,
                CONFIG_FIRMWARE_UPGRADE_URI,
                NULL
            },
            NULL,
            NULL,
            NULL,
            NULL,
            true,
            false
        }
    };

    ota_data_get_default_proc(&upgrade_list[0]);
    upgrade_list[0].need_upgrade = ota_data_iamge_need_upgrade;
    ota_app_get_default_proc(&upgrade_list[1]);

    ota_service_set_upgrade_param(ota_service, upgrade_list, sizeof(upgrade_list) / sizeof(ota_upgrade_ops_t));

    ESP_LOGI(TAG, "[2.2] Start OTA service");
    AUDIO_MEM_SHOW(TAG);
    periph_service_start(ota_service);

    EventBits_t bits = xEventGroupWaitBits(events, OTA_FINISH, true, false, portMAX_DELAY);
    if (bits & OTA_FINISH) {
        ESP_LOGI(TAG, "[2.3] Finish OTA service");
    }
    ESP_LOGI(TAG, "[2.4] Clear OTA service");
    periph_service_destroy(ota_service);
    vEventGroupDelete(events);
}
