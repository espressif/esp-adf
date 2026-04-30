/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"

#include "esp_ota_service.h"
#include "esp_ota_service_mcp.h"

static const char *TAG = "OTA_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_FILES) */
extern const uint8_t esp_ota_service_mcp_json_start[] asm("_binary_esp_ota_service_mcp_json_start");

static esp_ota_service_update_info_t s_cached_check_info;

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

static bool parse_version_triplet(const char *version, int out[3])
{
    if (!version || !out) {
        return false;
    }

    int found = 0;
    const char *p = version;
    while (*p != '\0' && found < 3) {
        while (*p != '\0' && (*p < '0' || *p > '9')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        char *end = NULL;
        long val = strtol(p, &end, 10);
        if (end == p || val < 0 || val > 1000000) {
            return false;
        }

        out[found++] = (int)val;
        p = end;
    }

    return (found == 3);
}

static int compare_version_triplet(const char *incoming, const char *current, bool *parsed_ok)
{
    int in_ver[3] = {0};
    int cur_ver[3] = {0};
    bool ok = parse_version_triplet(incoming, in_ver) && parse_version_triplet(current, cur_ver);
    if (parsed_ok) {
        *parsed_ok = ok;
    }
    if (!ok) {
        return 0;
    }

    for (int i = 0; i < 3; i++) {
        if (in_ver[i] != cur_ver[i]) {
            return (in_ver[i] > cur_ver[i]) ? 1 : -1;
        }
    }
    return 0;
}

static void normalize_upgrade_available(esp_ota_service_update_info_t *info)
{
    if (!info) {
        return;
    }

    const esp_app_desc_t *app = esp_app_get_description();
    bool parsed_ok = false;
    int ver_cmp = compare_version_triplet(info->version, app->version, &parsed_ok);

    if (parsed_ok) {
        info->upgrade_available = (ver_cmp > 0);
    } else {
        ESP_LOGW(TAG, "Version parse fallback: incoming=%s current=%s keep check_update result=%s", info->version,
                 app->version, info->upgrade_available ? "YES" : "NO");
    }
}

void esp_ota_service_mcp_cache_update_info(const esp_ota_service_update_info_t *info)
{
    if (!info) {
        return;
    }

    s_cached_check_info = *info;
    normalize_upgrade_available(&s_cached_check_info);
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
        const char *latest = s_cached_check_info.version[0] ? s_cached_check_info.version : app->version;

        snprintf(result, result_size, "{\"current_version\":\"%s\",\"latest_version\":\"%s\",\"upgrade_available\":%s}",
                 app->version, latest, s_cached_check_info.upgrade_available ? "true" : "false");
        return ESP_OK;
    }

    if (strcmp(tool->name, "esp_ota_service_get_progress") == 0) {
        uint32_t written = 0;
        uint32_t total = UINT32_MAX;
        int32_t percent = -1;
        esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;

        (void)esp_ota_service_get_progress(ota, &written, &total, &percent);
        (void)esp_service_get_state((const esp_service_t *)ota, &state);

        snprintf(result, result_size,
                 "{\"state\":\"%s\",\"bytes_written\":%" PRIu32 ",\"total_bytes\":%" PRIu32 ",\"percent\":%" PRId32 "}",
                 state_to_str_safe(state), written, total, percent);
        return ESP_OK;
    }

    if (strcmp(tool->name, "esp_ota_service_check_update") == 0) {
        esp_ota_service_update_info_t info = {0};
        esp_err_t ret = esp_ota_service_check_update(ota, 0, &info);
        if (ret != ESP_OK) {
            /* esp_ota_service_check_update already logged */
            snprintf(result, result_size, "{\"update_available\":false,\"error\":%d}", ret);
            return ret;
        }

        normalize_upgrade_available(&info);
        s_cached_check_info = info;

        snprintf(result, result_size,
                 "{\"update_available\":%s,\"version\":\"%s\",\"project\":\"%s\",\"size\":%" PRIu32 "}",
                 info.upgrade_available ? "true" : "false", info.version, info.label, info.image_size);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Tool invoke failed: unknown tool '%s'", tool->name);
    return ESP_ERR_NOT_SUPPORTED;
}
