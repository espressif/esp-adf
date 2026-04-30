/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "adf_mem.h"
#include "esp_ota_service_source.h"
#include "esp_ota_service_checker_data_version.h"
#include "esp_ota_service_version_utils.h"

static const char *TAG = "OTA_CHK_DATA";

typedef struct {
    esp_ota_service_checker_t  base;
    uint32_t                   current_packed;
    char                       current_version[32];
} esp_ota_service_checker_data_obj_t;

static esp_err_t checker_data_check(esp_ota_service_checker_t *self, const char *uri,
                                    esp_ota_service_source_t *source, esp_ota_service_check_result_t *result)
{
    if (!self || !uri || !source || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ota_service_checker_data_obj_t *obj = (esp_ota_service_checker_data_obj_t *)self;

    esp_err_t ret = source->open(source, uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Source open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    int64_t total_size = source->get_size(source);

    uint8_t header[4];
    int bytes_read = 0;
    while (bytes_read < 4) {
        int r = source->read(source, header + bytes_read, 4 - bytes_read);
        if (r <= 0) {
            ESP_LOGE(TAG, "Failed to read 4-byte version header (got %d bytes)", bytes_read);
            source->close(source);
            return ESP_ERR_INVALID_SIZE;
        }
        bytes_read += r;
    }
    source->close(source);
    uint32_t incoming_packed = (uint32_t)header[0]
                               | ((uint32_t)header[1] << 8)
                               | ((uint32_t)header[2] << 16)
                               | ((uint32_t)header[3] << 24);

    memset(result, 0, sizeof(*result));
    result->image_size = (total_size >= 0 && total_size <= (int64_t)UINT32_MAX)
                             ? (uint32_t)total_size
                             : UINT32_MAX;

    char ver_str[12];
    if (esp_ota_service_version_unpack_semver(incoming_packed, ver_str, sizeof(ver_str)) == ESP_OK) {
        strncpy(result->version, ver_str, sizeof(result->version) - 1);
    } else {
        snprintf(result->version, sizeof(result->version), "0x%08" PRIx32, incoming_packed);
    }

    result->upgrade_available = (incoming_packed != UINT32_MAX) && (incoming_packed > obj->current_packed);

    ESP_LOGI(TAG, "Data version check: incoming=%s current=%s upgrade_available=%s",
             result->version, obj->current_version,
             result->upgrade_available ? "true" : "false");
    return ESP_OK;
}

static void checker_data_destroy(esp_ota_service_checker_t *self)
{
    if (self) {
        adf_free(self);
    }
}

esp_ota_service_checker_t *esp_ota_service_checker_data_version_create(const esp_ota_service_checker_data_cfg_t *cfg)
{
    if (!cfg || !cfg->current_version) {
        ESP_LOGE(TAG, "Invalid config: current_version is required");
        return NULL;
    }

    esp_ota_service_checker_data_obj_t *obj = adf_calloc(1, sizeof(*obj));
    if (!obj) {
        ESP_LOGE(TAG, "No memory for data checker");
        return NULL;
    }

    obj->base.check = checker_data_check;
    obj->base.destroy = checker_data_destroy;
    obj->base.priv = NULL;

    obj->current_packed = esp_ota_service_version_pack_semver(cfg->current_version);
    strncpy(obj->current_version, cfg->current_version, sizeof(obj->current_version) - 1);

    return &obj->base;
}
