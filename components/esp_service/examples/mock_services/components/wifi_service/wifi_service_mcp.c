/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * @file wifi_service_mcp.c
 * @brief  MCP tool support for wifi_service
 *
 *         This file is compiled only when CONFIG_ESP_MCP_ENABLE=y (see CMakeLists.txt).
 *
 *         Exposed tools:
 *         wifi_service_get_status   — return current connection status string
 *         wifi_service_get_ssid     — return configured/connected AP SSID
 *         wifi_service_get_rssi     — return signal strength (dBm) of connected AP
 *         wifi_service_reconnect    — disconnect then connect to specified SSID/password
 */

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"
#include "wifi_service.h"
#include "wifi_service_mcp.h"
#include "../mock_mcp_util.h"

static const char *TAG = "WIFI_MCP";

/* Embedded JSON schema (via CMakeLists.txt EMBED_TXTFILES) */
extern const uint8_t wifi_service_mcp_json_start[] asm("_binary_wifi_service_mcp_json_start");

const char *wifi_service_mcp_schema(void)
{
    return (const char *)wifi_service_mcp_json_start;
}

static const char *wifi_status_to_str(wifi_svc_status_t status)
{
    switch (status) {
        case WIFI_SVC_STATUS_IDLE:
            return "IDLE";
        case WIFI_SVC_STATUS_SCANNING:
            return "SCANNING";
        case WIFI_SVC_STATUS_CONNECTING:
            return "CONNECTING";
        case WIFI_SVC_STATUS_CONNECTED:
            return "CONNECTED";
        case WIFI_SVC_STATUS_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

esp_err_t wifi_service_tool_invoke(esp_service_t *service,
                                   const esp_service_tool_t *tool,
                                   const char *args,
                                   char *result,
                                   size_t result_size)
{
    wifi_service_t *wifi = (wifi_service_t *)service;

    ESP_LOGD(TAG, "Tool invoke: %s", tool->name);

    if (strcmp(tool->name, "wifi_service_get_status") == 0) {
        wifi_svc_status_t status;
        wifi_service_get_status(wifi, &status);
        snprintf(result, result_size,
                 "{\"status\":\"%s\"}", wifi_status_to_str(status));
        return ESP_OK;
    }

    if (strcmp(tool->name, "wifi_service_get_ssid") == 0) {
        const char *ssid = NULL;
        wifi_service_get_ssid(wifi, &ssid);
        cJSON *json = cJSON_CreateObject();
        if (!json || !cJSON_AddStringToObject(json, "ssid", ssid ? ssid : "")) {
            cJSON_Delete(json);
            return ESP_ERR_NO_MEM;
        }
        return mock_mcp_write_json(json, result, result_size);
    }

    if (strcmp(tool->name, "wifi_service_get_rssi") == 0) {
        wifi_svc_status_t status = WIFI_SVC_STATUS_IDLE;
        int8_t rssi = 0;
        wifi_service_get_status(wifi, &status);
        wifi_service_get_rssi(wifi, &rssi);
        snprintf(result, result_size,
                 "{\"rssi\":%d,\"connected\":%s}",
                 (int)rssi,
                 status == WIFI_SVC_STATUS_CONNECTED ? "true" : "false");
        return ESP_OK;
    }

    if (strcmp(tool->name, "wifi_service_reconnect") == 0) {
        cJSON *root = cJSON_Parse(args);
        if (!cJSON_IsObject(root)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
        if (!cJSON_IsString(ssid_item) || strlen(ssid_item->valuestring) == 0 || strlen(ssid_item->valuestring) > 32) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }

        const char *ssid = ssid_item->valuestring;
        const char *password = NULL;
        cJSON *pw_item = cJSON_GetObjectItemCaseSensitive(root, "password");
        if (pw_item && (!cJSON_IsString(pw_item) || strlen(pw_item->valuestring) > 64)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        if (pw_item) {
            password = pw_item->valuestring;
        }

        esp_err_t ret = wifi_service_reconnect(wifi, ssid, password);
        cJSON *json = cJSON_CreateObject();
        if (!json || !cJSON_AddBoolToObject(json, "connected", ret == ESP_OK) ||
            !cJSON_AddStringToObject(json, "ssid", ssid)) {
            cJSON_Delete(json);
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        if (ret == ESP_OK) {
            int8_t rssi = 0;
            wifi_service_get_rssi(wifi, &rssi);
            if (!cJSON_AddNumberToObject(json, "rssi", rssi)) {
                cJSON_Delete(json);
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
        } else {
            if (!cJSON_AddNumberToObject(json, "error", ret)) {
                cJSON_Delete(json);
                cJSON_Delete(root);
                return ESP_ERR_NO_MEM;
            }
        }

        cJSON_Delete(root);
        esp_err_t write_ret = mock_mcp_write_json(json, result, result_size);
        return write_ret == ESP_OK ? ret : write_ret;
    }

    return ESP_ERR_NOT_SUPPORTED;
}
