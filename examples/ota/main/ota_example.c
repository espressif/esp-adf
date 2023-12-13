/* upgrade app and data partition Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "audio_common.h"
#include "board.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"

#include "audio_mem.h"
#include "ota_service.h"
#include "ota_proc_default.h"
#include "tone_partition.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "HTTPS_OTA_EXAMPLE";
static EventGroupHandle_t events = NULL;

#define OTA_FINISH (BIT0)

static ota_service_err_reason_t audio_tone_need_upgrade(void *handle, ota_node_attr_t *node)
{
    bool                need_write_desc = false;
    flash_tone_header_t cur_header      = { 0 };
    flash_tone_header_t incoming_header = { 0 };
    esp_app_desc_t      current_desc    = { 0 };
    esp_app_desc_t      incoming_desc   = { 0 };
    esp_err_t           err             = ESP_OK;

    /* try to get the tone partition*/
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, node->label);
    if (partition == NULL) {
        ESP_LOGE(TAG, "data partition [%s] not found", node->label);
        return OTA_SERV_ERR_REASON_PARTITION_NOT_FOUND;
    }

    tone_partition_handle_t tone = tone_partition_init(node->label, false);
    if (tone == NULL) {
        esp_partition_erase_range(partition, 0, partition->size);
        return OTA_SERV_ERR_REASON_SUCCESS;
    }

    /* Read tone header from partition */
    if (esp_partition_read(partition, 0, (char *)&cur_header, sizeof(flash_tone_header_t)) != ESP_OK) {
        return OTA_SERV_ERR_REASON_PARTITION_RD_FAIL;
    }

    /* Read tone header from incoming stream */
    if (ota_data_image_stream_read(handle, (char *)&incoming_header, sizeof(flash_tone_header_t)) != ESP_OK) {
        return OTA_SERV_ERR_REASON_STREAM_RD_FAIL;
    }
    if (incoming_header.header_tag != 0x2053) {
        ESP_LOGE(TAG, "not audio tone bin");
        return OTA_SERV_ERR_REASON_UNKNOWN;
    }
    ESP_LOGI(TAG, "format %" PRIu32 " : %" PRIu32, cur_header.format, incoming_header.format);
    /* upgrade the tone bin when incoming bin format is 0 */
    if (incoming_header.format == 0) {
        goto write_flash;
    }

    /* read the app desc from incoming stream when bin format is 1*/
    if (ota_data_image_stream_read(handle, (char *)&incoming_desc, sizeof(esp_app_desc_t)) != ESP_OK) {
        return OTA_SERV_ERR_REASON_STREAM_RD_FAIL;
    }
    ESP_LOGI(TAG, "imcoming magic_word %" PRIx32 ", project_name %s", incoming_desc.magic_word, incoming_desc.project_name);
    /* check the incoming app desc */
    if (incoming_desc.magic_word != FLASH_TONE_MAGIC_WORD) {
        return OTA_SERV_ERR_REASON_ERROR_MAGIC_WORD;
    }
    if (strstr(FLASH_TONE_PROJECT_NAME, incoming_desc.project_name) == NULL) {
        return OTA_SERV_ERR_REASON_ERROR_PROJECT_NAME;
    }

    /* compare current app desc with the incoming one if the current bin's format is 1*/
    if (cur_header.format == TONE_VERSION_1) {
        if (tone_partition_get_app_desc(tone, &current_desc) != ESP_OK) {
            return OTA_SERV_ERR_REASON_PARTITION_RD_FAIL;
        }
        if (ota_get_version_number(incoming_desc.version) < 0) {
            return OTA_SERV_ERR_REASON_ERROR_VERSION;
        }
        ESP_LOGI(TAG, "current version %s, incoming version %s", current_desc.version, incoming_desc.version);
        if (ota_get_version_number(incoming_desc.version) <= ota_get_version_number(current_desc.version)) {
            ESP_LOGW(TAG, "The incoming version is same as or lower than the running version");
            return OTA_SERV_ERR_REASON_NO_HIGHER_VERSION;
        }
    }

    /* incoming bin's format is 1, and the current bin's format is 0, upgrade the incoming one */
    need_write_desc = true;

write_flash:
    if ((err = esp_partition_erase_range(partition, 0, partition->size)) != ESP_OK) {
        ESP_LOGE(TAG, "Erase [%s] failed and return %d", node->label, err);
        return OTA_SERV_ERR_REASON_PARTITION_WT_FAIL;
    }
    ota_data_partition_erase_mark(handle);
    ota_data_partition_write(handle, (char *)&incoming_header, sizeof(flash_tone_header_t));
    if (need_write_desc) {
        ota_data_partition_write(handle, (char *)&incoming_desc, sizeof(esp_app_desc_t));
    }
    return OTA_SERV_ERR_REASON_SUCCESS;
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
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI(TAG, "[1.2] Mount SDCard");
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

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
    upgrade_list[0].need_upgrade = audio_tone_need_upgrade;
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
