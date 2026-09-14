/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_extractor.h"
#include "esp_log.h"

#include "esp_extractor_service.h"
#include "esp_extractor_service_err.h"
#include "esp_extractor_service_mcp.h"
#include "esp_extractor_service_ops.h"
#include "esp_service.h"

static const char *TAG = "EXTRACT_MCP";

extern const uint8_t esp_extractor_service_mcp_json_start[] asm("_binary_esp_extractor_service_mcp_json_start");

esp_err_t esp_extractor_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Schema get failed: out_schema is NULL");
    }
    *out_schema = (const char *)esp_extractor_service_mcp_json_start;
    return ESP_OK;
}

static esp_extractor_service_t *as_extractor(esp_service_t *service)
{
    return (esp_extractor_service_t *)service;
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

static bool parse_mask(const char *text, uint8_t *out_mask)
{
    if (!text || !out_mask) {
        return false;
    }
    if (strcasecmp(text, "audio") == 0 || strcmp(text, "1") == 0) {
        *out_mask = ESP_EXTRACT_MASK_AUDIO;
        return true;
    }
    if (strcasecmp(text, "video") == 0 || strcmp(text, "2") == 0) {
        *out_mask = ESP_EXTRACT_MASK_VIDEO;
        return true;
    }
    if (strcasecmp(text, "av") == 0 || strcasecmp(text, "audio_video") == 0 || strcmp(text, "3") == 0) {
        *out_mask = ESP_EXTRACT_MASK_AV;
        return true;
    }
    return false;
}

static esp_err_t tool_set_mask(esp_extractor_service_t *extractor, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *mask_text = NULL;
    if (!json_get_optional_string(root, "mask", &mask_text) || mask_text == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mask = 0;
    if (!parse_mask(mask_text, &mask)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_extractor_service_set_extract_mask(extractor, mask);
    ret = finish_op(result, result_size, "esp_extractor_service_set_extract_mask", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_pool(esp_extractor_service_t *extractor, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    uint32_t size = 0;
    if (!json_get_optional_uint32(root, "out_pool_size", 0, &size)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t op_ret = esp_extractor_service_set_out_pool_size(extractor, size);
    ret = finish_op(result, result_size, "esp_extractor_service_set_out_pool_size", op_ret);
    cJSON_Delete(root);
    return ret;
}

static esp_err_t tool_set_url(esp_extractor_service_t *extractor, const char *args, char *result, size_t result_size)
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
    esp_err_t op_ret = esp_extractor_service_set_url(extractor, url);
    ret = finish_op(result, result_size, "esp_extractor_service_set_url", op_ret);
    cJSON_Delete(root);
    return ret;
}

esp_err_t esp_extractor_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool,
                                            const char *args, char *result, size_t result_size)
{
    esp_extractor_service_t *extractor = as_extractor(service);
    if (!extractor || !tool || !tool->name || !result || result_size == 0) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Tool invoke failed: invalid argument");
    }
    if (strcmp(tool->name, "esp_extractor_service_set_extract_mask") == 0) {
        return tool_set_mask(extractor, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_extractor_service_set_out_pool_size") == 0) {
        return tool_set_pool(extractor, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_extractor_service_set_url") == 0) {
        return tool_set_url(extractor, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_extractor_service_start") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_start(ESP_SERVICE_BASE(extractor)));
    }
    if (strcmp(tool->name, "esp_extractor_service_stop") == 0) {
        return finish_op(result, result_size, tool->name, esp_service_stop(ESP_SERVICE_BASE(extractor)));
    }
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "Unknown tool %s", tool->name);
}
