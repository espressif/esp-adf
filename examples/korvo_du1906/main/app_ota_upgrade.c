/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "audio_error.h"
#include "esp_partition.h"
#include "ota_service.h"
#include "ota_proc_default.h"

static const char *TAG = "APP_OTA_UPGRADE";

static EventGroupHandle_t OTA_FLAG;
#define OTA_FINISH (BIT0)
#define OTA_SERVICE_STACK_SIZE  (4 * 1024)

static bool ota_sdcard_image_need_upgrade(void *handle, ota_node_attr_t *node)
{
    AUDIO_NULL_CHECK(TAG, node->uri, return false);
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, node->label);
    if (partition == NULL) {
        ESP_LOGE(TAG, "data partition [%s] not found", node->label);
        return false;
    }

    char *uri = strstr(node->uri, "/sdcard/");
    if (access(uri, 0) == 0) {
        ESP_LOGI(TAG, "Found ota file in sdcard, uri: %s", uri);
        esp_err_t err = ESP_OK;
        if ((err = esp_partition_erase_range(partition, 0, partition->size)) != ESP_OK) {
            ESP_LOGE(TAG, "Erase [%s] failed and return %d", node->label, err);
            return false;
        }
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
            ESP_LOGI(TAG, "List id: %d, OTA sucessed", result_data->id);
        }
    } else if (evt->type == OTA_SERV_EVENT_TYPE_FINISH) {
        xEventGroupSetBits(OTA_FLAG, OTA_FINISH);
    }
    return ESP_OK;
}

void app_ota_start()
{
    ESP_LOGI(TAG, "Create OTA service");
    OTA_FLAG = xEventGroupCreate();
    ota_service_config_t ota_service_cfg = OTA_SERVICE_DEFAULT_CONFIG();
    ota_service_cfg.task_stack = OTA_SERVICE_STACK_SIZE;
    ota_service_cfg.evt_cb = ota_service_cb;
    ota_service_cfg.cb_ctx = NULL;
    periph_service_handle_t ota_service = ota_service_create(&ota_service_cfg);
    ota_upgrade_ops_t upgrade_list[] = {
        {
            {
                ESP_PARTITION_TYPE_DATA,
                "flash_tone",
                "file://sdcard/flash_tone.bin",
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
                ESP_PARTITION_TYPE_DATA,
                "profile",
                "file://sdcard/profile.bin",
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
                "https://github.com/withdra/my_ota_test/blob/master/dul1906.bin?raw=true",
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

    for (int i = 0; i < sizeof(upgrade_list) / sizeof(upgrade_list[0]); i++) {
        if (upgrade_list[i].node.type == ESP_PARTITION_TYPE_DATA) {
            ota_data_get_default_proc(&upgrade_list[i]);
            upgrade_list[i].need_upgrade = ota_sdcard_image_need_upgrade;
        } else if (upgrade_list[i].node.type == ESP_PARTITION_TYPE_APP) {
            ota_app_get_default_proc(&upgrade_list[i]);
        } else {
            ESP_LOGE(TAG, "unknown type");
        }
    }
    ota_service_set_upgrade_param(ota_service, upgrade_list, sizeof(upgrade_list) / sizeof(upgrade_list[0]));
    ESP_LOGI(TAG, "Start OTA service");
    periph_service_start(ota_service);
    EventBits_t bits = xEventGroupWaitBits(OTA_FLAG, OTA_FINISH, true, false, portMAX_DELAY);
    if (bits & OTA_FINISH) {
        ESP_LOGW(TAG, "upgrade finished, Please check the result of OTA");
    }
    vEventGroupDelete(OTA_FLAG);
    periph_service_destroy(ota_service);
}
