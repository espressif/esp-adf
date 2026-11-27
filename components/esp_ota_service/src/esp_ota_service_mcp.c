/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "cJSON.h"

#include "esp_ota_service.h"
#include "esp_ota_service_mcp.h"

static const char *TAG = "OTA_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_TXTFILES) */
extern const uint8_t esp_ota_service_mcp_json_start[] asm("_binary_esp_ota_service_mcp_json_start");

static esp_ota_service_update_info_t s_cached_check_info;
static portMUX_TYPE s_cache_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t esp_ota_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_ota_service_mcp_json_start;
    return ESP_OK;
}

static const char *state_to_str_safe(esp_service_state_t state)
{
    const char *name = "UNKNOWN";
    if (esp_service_state_to_str(state, &name) != ESP_OK) {
        return "UNKNOWN";
    }
    return name;
}

static void cache_store(const esp_ota_service_update_info_t *info)
{
    taskENTER_CRITICAL(&s_cache_lock);
    s_cached_check_info = *info;
    taskEXIT_CRITICAL(&s_cache_lock);
}

/* Untrusted version/label strings go through cJSON so quotes and control
 * characters cannot break out of the JSON result. */
static esp_err_t emit_json(cJSON *root, char *result, size_t result_size)
{
    if (root == NULL) {
        ESP_LOGE(TAG, "Emit JSON failed: out of memory");
        return ESP_ERR_NO_MEM;
    }
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (text == NULL) {
        ESP_LOGE(TAG, "Emit JSON failed: cJSON_PrintUnformatted returned NULL");
        return ESP_ERR_NO_MEM;
    }
    size_t len = strlen(text);
    esp_err_t ret = ESP_OK;
    if (len >= result_size) {
        ESP_LOGE(TAG, "Result buffer too small: need %u, have %u", (unsigned)(len + 1), (unsigned)result_size);
        ret = ESP_ERR_INVALID_SIZE;
    } else {
        memcpy(result, text, len + 1);
    }
    cJSON_free(text);
    return ret;
}

void esp_ota_service_mcp_cache_update_info(const esp_ota_service_update_info_t *info)
{
    if (!info) {
        return;
    }
    cache_store(info);
}

esp_err_t esp_ota_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                      char *result, size_t result_size)
{
    (void)args;

    esp_ota_service_t *ota = (esp_ota_service_t *)service;
    if (!ota || !tool || !result || result_size == 0) {
        ESP_LOGE(TAG, "Tool invoke failed: invalid argument (ota=%p tool=%p result=%p size=%zu)", (void *)ota,
                 (void *)tool, (void *)result, result_size);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Tool invoke: %s args=%s", tool->name, args ? args : "{}");

    if (strcmp(tool->name, "esp_ota_service_get_version") == 0) {
        const esp_app_desc_t *app = esp_app_get_description();
        esp_ota_service_update_info_t cached;
        taskENTER_CRITICAL(&s_cache_lock);
        cached = s_cached_check_info;
        taskEXIT_CRITICAL(&s_cache_lock);
        const char *latest = cached.version[0] ? cached.version : app->version;

        cJSON *root = cJSON_CreateObject();
        if (!root ||
            !cJSON_AddStringToObject(root, "current_version", app->version) ||
            !cJSON_AddStringToObject(root, "latest_version", latest) ||
            !cJSON_AddBoolToObject(root, "upgrade_available", cached.upgrade_available)) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Emit JSON failed: out of memory");
            return ESP_ERR_NO_MEM;
        }
        return emit_json(root, result, result_size);
    }

    if (strcmp(tool->name, "esp_ota_service_get_progress") == 0) {
        uint32_t written = 0;
        uint32_t total = UINT32_MAX;
        int32_t percent = -1;
        esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;

        (void)esp_ota_service_get_progress(ota, &written, &total, &percent);
        (void)esp_service_get_state((const esp_service_t *)ota, &state);

        cJSON *root = cJSON_CreateObject();
        if (!root ||
            !cJSON_AddStringToObject(root, "state", state_to_str_safe(state)) ||
            !cJSON_AddNumberToObject(root, "bytes_written", (double)written) ||
            !cJSON_AddNumberToObject(root, "total_bytes", (total == UINT32_MAX) ? -1.0 : (double)total) ||
            !cJSON_AddNumberToObject(root, "percent", (double)percent)) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Emit JSON failed: out of memory");
            return ESP_ERR_NO_MEM;
        }
        return emit_json(root, result, result_size);
    }

    if (strcmp(tool->name, "esp_ota_service_check_update") == 0) {
        esp_ota_service_update_info_t info = {0};
        esp_err_t ret = esp_ota_service_check_update(ota, 0, &info);
        if (ret != ESP_OK) {
            /* esp_ota_service_check_update already logged */
            snprintf(result, result_size, "{\"update_available\":false,\"error\":%d}", ret);
            return ret;
        }

        /* The checker's policy result is authoritative (project name, data partitions). */
        cache_store(&info);

        cJSON *root = cJSON_CreateObject();
        if (!root ||
            !cJSON_AddBoolToObject(root, "update_available", info.upgrade_available) ||
            !cJSON_AddStringToObject(root, "version", info.version) ||
            !cJSON_AddStringToObject(root, "project", info.label) ||
            !cJSON_AddNumberToObject(root, "size",
                                     (info.image_size == UINT32_MAX) ? -1.0 : (double)info.image_size)) {
            cJSON_Delete(root);
            ESP_LOGE(TAG, "Emit JSON failed: out of memory");
            return ESP_ERR_NO_MEM;
        }
        return emit_json(root, result, result_size);
    }

    ESP_LOGE(TAG, "Tool invoke failed: unknown tool '%s'", tool->name);
    return ESP_ERR_NOT_SUPPORTED;
}
