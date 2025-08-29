/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file ota_service_mcp.c
 * @brief  MCP tool support for ota_service (mock)
 *
 *         This file is compiled only when CONFIG_ESP_MCP_ENABLE=y (see CMakeLists.txt).
 *         It has no effect on firmware size when MCP is disabled.
 *
 *         Exposed tools:
 *         ota_service_get_version   — return current and latest firmware version numbers
 *         ota_service_get_progress  — return download progress (percent, bytes written, total)
 *         ota_service_check_update  — check whether a newer firmware is available
 */

#include <string.h>
#include "esp_log.h"
#include "ota_service.h"
#include "ota_service_mcp.h"

static const char *TAG = "OTA_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_TXTFILES) */
extern const uint8_t ota_service_mcp_json_start[] asm("_binary_ota_service_mcp_json_start");

esp_err_t ota_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)ota_service_mcp_json_start;
    return ESP_OK;
}

static const char *ota_status_to_str(ota_status_t status)
{
    switch (status) {
        case OTA_STATUS_IDLE:
            return "IDLE";
        case OTA_STATUS_CHECKING:
            return "CHECKING";
        case OTA_STATUS_DOWNLOADING:
            return "DOWNLOADING";
        case OTA_STATUS_APPLYING:
            return "APPLYING";
        case OTA_STATUS_COMPLETED:
            return "COMPLETED";
        case OTA_STATUS_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

esp_err_t ota_service_tool_invoke(esp_service_t *service,
                                  const esp_service_tool_t *tool,
                                  const char *args,
                                  char *result,
                                  size_t result_size)
{
    ota_service_t *ota = (ota_service_t *)service;

    ESP_LOGI(TAG, "Tool_invoke: %s args=%s", tool->name, args ? args : "{}");

    if (strcmp(tool->name, "ota_service_get_version") == 0) {
        uint32_t current = 0;
        uint32_t latest = 0;
        ota_service_get_version(ota, &current, &latest);
        snprintf(result, result_size,
                 "{\"current_version\":%lu,\"latest_version\":%lu}",
                 (unsigned long)current, (unsigned long)latest);
        return ESP_OK;
    }

    if (strcmp(tool->name, "ota_service_get_progress") == 0) {
        ota_progress_t prog = {0};
        ota_service_get_progress(ota, &prog);

        ota_status_t status;
        ota_service_get_status(ota, &status);

        snprintf(result, result_size,
                 "{\"status\":\"%s\",\"partition\":\"%s\","
                 "\"percent\":%u,\"bytes_written\":%lu,\"total_bytes\":%lu}",
                 ota_status_to_str(status),
                 prog.partition_label,
                 (unsigned)prog.percent,
                 (unsigned long)prog.bytes_written,
                 (unsigned long)prog.total_bytes);
        return ESP_OK;
    }

    if (strcmp(tool->name, "ota_service_check_update") == 0) {
        ota_check_result_t check = {0};
        esp_err_t ret = ota_service_check_update(ota, &check);
        if (ret != ESP_OK) {
            snprintf(result, result_size,
                     "{\"update_available\":false,\"error\":%d}", ret);
            return ret;
        }
        snprintf(result, result_size,
                 "{\"update_available\":%s,\"new_version\":%lu}",
                 check.update_available ? "true" : "false",
                 (unsigned long)check.new_version);
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}
