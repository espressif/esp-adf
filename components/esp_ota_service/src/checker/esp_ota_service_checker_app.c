/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "esp_log.h"

#include "adf_mem.h"
#include "esp_ota_service_source.h"
#include "esp_ota_service_app_desc_reader.h"
#include "esp_ota_service_checker_app.h"

static const char *TAG = "OTA_CHK_APP";

typedef struct {
    esp_ota_service_checker_t                  base;
    esp_ota_service_version_policy_t           policy;
    esp_ota_service_checker_should_upgrade_fn  should_upgrade;
    void                                      *should_upgrade_ctx;
} esp_ota_service_checker_app_obj_t;

static esp_err_t checker_app_check(esp_ota_service_checker_t *self, const char *uri,
                                   esp_ota_service_source_t *source, esp_ota_service_check_result_t *result)
{
    if (!self || !uri || !source || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ota_service_checker_app_obj_t *obj = (esp_ota_service_checker_app_obj_t *)self;

    esp_app_desc_t incoming;
    uint32_t image_size = UINT32_MAX;
    esp_err_t ret = esp_ota_service_app_desc_read_from_source(source, uri, &incoming, &image_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Remote descriptor fetch failed: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(result, 0, sizeof(*result));
    result->image_size = image_size;
    strncpy(result->version, incoming.version, sizeof(result->version) - 1);
    strncpy(result->label, incoming.project_name, sizeof(result->label) - 1);

    esp_app_desc_t cur_desc;
    const esp_app_desc_t *cur_ptr = NULL;
    if (esp_ota_service_app_desc_read_current(&cur_desc) == ESP_OK) {
        cur_ptr = &cur_desc;
    }

    if (cur_ptr == NULL) {
        ESP_LOGW(TAG, "Could not read running app descriptor — upgrade_available=false");
        result->upgrade_available = false;
    } else if (obj->should_upgrade != NULL) {
        result->upgrade_available = obj->should_upgrade(cur_ptr, &incoming, obj->should_upgrade_ctx);
    } else {
        esp_err_t cmp = esp_ota_service_version_compare_descriptors(cur_ptr, &incoming, &obj->policy);
        result->upgrade_available = (cmp == ESP_OK);
    }

    ESP_LOGI(TAG, "Check: incoming version='%s' project='%s' size=%" PRIu32 " upgrade_available=%s",
             result->version, result->label, result->image_size,
             result->upgrade_available ? "true" : "false");
    return ESP_OK;
}

static void checker_app_destroy(esp_ota_service_checker_t *self)
{
    if (self) {
        adf_free(self);
    }
}

esp_ota_service_checker_t *esp_ota_service_checker_app_create(const esp_ota_service_checker_app_cfg_t *cfg)
{
    esp_ota_service_checker_app_obj_t *obj = (esp_ota_service_checker_app_obj_t *)adf_calloc(1, sizeof(*obj));
    if (!obj) {
        ESP_LOGE(TAG, "Create failed: no memory for checker object");
        return NULL;
    }

    obj->base.check = checker_app_check;
    obj->base.destroy = checker_app_destroy;
    obj->base.priv = NULL;

    if (cfg != NULL) {
        obj->policy = cfg->version_policy;
        obj->should_upgrade = cfg->should_upgrade;
        obj->should_upgrade_ctx = cfg->should_upgrade_ctx;
    } else {
        obj->policy.require_higher_version = true;
        obj->policy.check_project_name = true;
    }

    return &obj->base;
}
