/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "esp_rtmp_service.h"
#include "esp_rtmp_service_err.h"
#include "esp_rtmp_service_mcp.h"
#include "esp_rtmp_service_ops.h"
#include "esp_service.h"

static const char *TAG = "RTMP_MCP";

extern const uint8_t esp_rtmp_service_mcp_json_start[] asm("_binary_esp_rtmp_service_mcp_json_start");

esp_err_t esp_rtmp_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Schema get failed: out_schema is NULL");
    }
    *out_schema = (const char *)esp_rtmp_service_mcp_json_start;
    return ESP_OK;
}

static esp_rtmp_service_t *as_rtmp(esp_service_t *service)
{
    return (esp_rtmp_service_t *)service;
}

static esp_err_t write_json_result(cJSON *json, char *result, size_t result_size)
{
    if (!json || !result || result_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }
    size_t len = strlen(payload);
    if (len >= result_size) {
        cJSON_free(payload);
        return ESP_ERR_NO_MEM;
    }
    memcpy(result, payload, len + 1);
    cJSON_free(payload);
    return ESP_OK;
}

static esp_err_t write_simple_result(char *result, size_t result_size, const char *operation, esp_err_t op_ret)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t ret = ESP_OK;
    if (!cJSON_AddStringToObject(root, "operation", operation ? operation : "") ||
        !cJSON_AddBoolToObject(root, "ok", op_ret == ESP_OK) ||
        !cJSON_AddNumberToObject(root, "error", op_ret) ||
        !cJSON_AddStringToObject(root, "error_name", esp_err_to_name(op_ret))) {
        ret = ESP_ERR_NO_MEM;
    } else {
        ret = write_json_result(root, result, result_size);
    }
    cJSON_Delete(root);
    return ret;
}

static esp_err_t finish_op(char *result, size_t result_size, const char *op, esp_err_t op_ret)
{
    esp_err_t json_ret = write_simple_result(result, result_size, op, op_ret);
    return (op_ret == ESP_OK) ? json_ret : op_ret;
}

static esp_err_t parse_args_object(const char *args, cJSON **out_root)
{
    if (!out_root) {
        return ESP_ERR_INVALID_ARG;
    }
    const char *json = (args && args[0] != '\0') ? args : "{}";
    cJSON *root = cJSON_Parse(json);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static bool json_get_optional_string(const cJSON *root, const char *name, const char **out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = NULL;
        }
        return true;
    }
    if (!cJSON_IsString(item)) {
        return false;
    }
    if (out_value) {
        *out_value = item->valuestring;
    }
    return true;
}

static bool json_get_optional_uint32(const cJSON *root, const char *name, uint32_t default_value, uint32_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    if (!cJSON_IsNumber(item) || !out_value || item->valuedouble < 0 || item->valuedouble > UINT32_MAX) {
        return false;
    }
    *out_value = (uint32_t)item->valuedouble;
    return true;
}

static esp_err_t tool_setup(esp_rtmp_service_t *rtmp, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_rtmp_service_setup_t setup = {0};
    uint32_t chunk_size = 0;
    uint32_t port = 0;
    uint32_t max_clients = 0;
    uint32_t cache_size = 0;
    uint32_t audio_cache = 0;
    uint32_t video_cache = 0;
    const char *app_name = NULL;
    if (!json_get_optional_uint32(root, "chunk_size", 0, &chunk_size) ||
        !json_get_optional_uint32(root, "port", 0, &port) ||
        !json_get_optional_uint32(root, "max_clients", 0, &max_clients) ||
        !json_get_optional_uint32(root, "cache_size", 0, &cache_size) ||
        !json_get_optional_uint32(root, "audio_cache_size", 0, &audio_cache) ||
        !json_get_optional_uint32(root, "video_cache_size", 0, &video_cache) ||
        !json_get_optional_string(root, "app_name", &app_name)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    setup.chunk_size = chunk_size;
    setup.server.port = (uint16_t)port;
    setup.server.app_name = (app_name && app_name[0] != '\0') ? app_name : NULL;
    setup.server.max_clients = (uint8_t)max_clients;
    setup.src.cache_size = cache_size;
    setup.src.audio_cache_size = audio_cache;
    setup.src.video_cache_size = video_cache;

    esp_err_t op_ret = esp_rtmp_service_setup(rtmp, &setup);
    ret = finish_op(result, result_size, "esp_rtmp_service_setup", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_url(esp_rtmp_service_t *rtmp, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *url = NULL;
    if (!json_get_optional_string(root, "url", &url) || url == NULL || url[0] == '\0') {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_rtmp_service_set_url(rtmp, url);
    ret = finish_op(result, result_size, "esp_rtmp_service_set_url", op_ret);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_rtmp_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                       const char *args, char *result, size_t result_size)
{
    esp_rtmp_service_t *rtmp = as_rtmp(service);
    if (!rtmp || !tool || !tool->name || !result || result_size == 0) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Tool invoke failed: invalid argument");
    }
    if (strcmp(tool->name, "esp_rtmp_service_setup") == 0) {
        return tool_setup(rtmp, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_rtmp_service_set_url") == 0) {
        return tool_set_url(rtmp, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_rtmp_service_start") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_start(ESP_SERVICE_BASE(rtmp)));
    }
    if (strcmp(tool->name, "esp_rtmp_service_stop") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_stop(ESP_SERVICE_BASE(rtmp)));
    }
    if (strcmp(tool->name, "esp_rtmp_service_query") == 0) {
        return finish_op(result, result_size, tool->name, esp_rtmp_service_query(rtmp));
    }
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "Unknown tool %s", tool->name);
}
