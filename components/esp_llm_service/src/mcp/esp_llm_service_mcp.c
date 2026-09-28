/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

#include "esp_llm_service_mcp.h"
#include "esp_llm_service_internal.h"

static const char *TAG = "ESP_LLM_MCP";

extern const uint8_t esp_llm_service_mcp_json_start[] asm("_binary_esp_llm_service_mcp_json_start");

typedef struct {
    const char *name;
    uint32_t    flag;
} esp_llm_cap_desc_t;

static const char *session_instance_name(esp_llm_service_t *service);

static const esp_llm_cap_desc_t s_cap_desc[] = {
    { "text_input", ESP_LLM_CAP_TEXT_INPUT },
    { "audio_input", ESP_LLM_CAP_AUDIO_INPUT },
    { "video_input", ESP_LLM_CAP_VIDEO_INPUT },
    { "text_output", ESP_LLM_CAP_TEXT_OUTPUT },
    { "audio_output", ESP_LLM_CAP_AUDIO_OUTPUT },
    { "video_output", ESP_LLM_CAP_VIDEO_OUTPUT },
    { "cancel_response", ESP_LLM_CAP_CANCEL_RESPONSE },
    { "function_call", ESP_LLM_CAP_FUNCTION_CALL },
    { "vision", ESP_LLM_CAP_VISION },
    { "custom_data", ESP_LLM_CAP_CUSTOM_DATA },
};

static esp_err_t write_json_result(cJSON *json, char *result, size_t result_size)
{
    if (json == NULL || result == NULL || result_size == 0) {
        ESP_LOGE(TAG, "Write result failed: JSON or result buffer is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    char *payload = cJSON_PrintUnformatted(json);
    if (payload == NULL) {
        ESP_LOGE(TAG, "Write result failed: JSON serialization failed");
        return ESP_ERR_NO_MEM;
    }

    size_t payload_len = strlen(payload);
    if (payload_len >= result_size) {
        cJSON_free(payload);
        ESP_LOGE(TAG, "Write result failed: result buffer is too small");
        return ESP_ERR_NO_MEM;
    }
    memcpy(result, payload, payload_len + 1);
    cJSON_free(payload);
    return ESP_OK;
}

static esp_err_t write_operation_result(char *result, size_t result_size,
                                        const char *operation, esp_err_t operation_ret)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Write operation result failed: JSON allocation failed");
        return ESP_ERR_NO_MEM;
    }

    bool ok = operation_ret == ESP_OK;
    esp_err_t ret = ESP_OK;
    if (!cJSON_AddStringToObject(root, "operation", operation != NULL ? operation : "") ||
        !cJSON_AddBoolToObject(root, "ok", ok) ||
        !cJSON_AddNumberToObject(root, "error", operation_ret) ||
        !cJSON_AddStringToObject(root, "error_name", esp_err_to_name(operation_ret))) {
        ESP_LOGE(TAG, "Write operation result failed: JSON construction failed");
        ret = ESP_ERR_NO_MEM;
    } else {
        ret = write_json_result(root, result, result_size);
    }
    cJSON_Delete(root);
    return ret;
}

static esp_err_t parse_args(const char *args, cJSON **out_root)
{
    if (out_root == NULL) {
        ESP_LOGE(TAG, "Parse arguments failed: out_root is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *json = args != NULL && args[0] != '\0' ? args : "{}";
    cJSON *root = cJSON_Parse(json);
    if (root == NULL || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Parse arguments failed: arguments are not a JSON object");
        return ESP_ERR_INVALID_ARG;
    }
    *out_root = root;
    return ESP_OK;
}

static esp_err_t resolve_llm_service(esp_llm_service_t *self, const cJSON *root,
                                     esp_llm_service_t **out_service, uint16_t *out_index)
{
    if (self == NULL || out_service == NULL) {
        ESP_LOGE(TAG, "Resolve session failed: service or out_service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_service = NULL;
    if (out_index != NULL) {
        *out_index = 0;
    }

    const cJSON *name_item = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "name") : NULL;
    if (cJSON_IsString(name_item) && name_item->valuestring != NULL && name_item->valuestring[0] != '\0') {
        if (strcmp(name_item->valuestring, session_instance_name(self)) != 0) {
            ESP_LOGE(TAG, "Resolve session failed: name '%s' is not this service",
                     name_item->valuestring);
            return ESP_ERR_NOT_FOUND;
        }
        *out_service = self;
        return ESP_OK;
    }

    const cJSON *index_item = root != NULL ? cJSON_GetObjectItemCaseSensitive(root, "index") : NULL;
    if (!cJSON_IsNumber(index_item) || index_item->valuedouble != 0) {
        ESP_LOGE(TAG, "Resolve session failed: name or index is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    *out_service = self;
    return ESP_OK;
}

static cJSON *capability_names_create(esp_llm_capability_t caps)
{
    cJSON *names = cJSON_CreateArray();
    if (names == NULL) {
        ESP_LOGE(TAG, "Create capability names failed: JSON allocation failed");
        return NULL;
    }
    for (size_t i = 0; i < sizeof(s_cap_desc) / sizeof(s_cap_desc[0]); ++i) {
        if ((caps & s_cap_desc[i].flag) == 0) {
            continue;
        }
        if (!cJSON_AddItemToArray(names, cJSON_CreateString(s_cap_desc[i].name))) {
            cJSON_Delete(names);
            return NULL;
        }
    }
    return names;
}

static const char *session_instance_name(esp_llm_service_t *service)
{
    const char *name = NULL;
    if (service != NULL &&
        esp_service_get_name(ESP_SERVICE_BASE(service), &name) == ESP_OK &&
        name != NULL) {
        return name;
    }
    return "";
}

static esp_err_t append_session_fields(cJSON *obj, uint16_t index,
                                       esp_llm_service_t *service, esp_llm_capability_t capabilities)
{
    cJSON *cap_names = capability_names_create(capabilities);
    if (cap_names == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const char *adapter_name = esp_llm_adapter_name(service);
    const char *instance_name = session_instance_name(service);
    if (!cJSON_AddNumberToObject(obj, "index", index) ||
        !cJSON_AddStringToObject(obj, "name", instance_name) ||
        !cJSON_AddStringToObject(obj, "backend", adapter_name) ||
        !cJSON_AddNumberToObject(obj, "capabilities", capabilities) ||
        !cJSON_AddItemToObject(obj, "capability_names", cap_names)) {
        cJSON_Delete(cap_names);
        ESP_LOGE(TAG, "Append session fields failed: JSON construction failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t tool_list(esp_llm_service_t *service, char *result, size_t result_size)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "List sessions failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *services = cJSON_CreateArray();
    if (root == NULL || services == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(services);
        ESP_LOGE(TAG, "List sessions failed: JSON allocation failed");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    esp_llm_capability_t capabilities = (esp_llm_capability_t)0;
    if (esp_llm_service_get_capabilities(service, &capabilities) != ESP_OK) {
        ESP_LOGW(TAG, "List sessions: session could not be queried");
    } else {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            ret = ESP_ERR_NO_MEM;
        } else {
            ret = append_session_fields(entry, 0, service, capabilities);
            if (ret != ESP_OK) {
                cJSON_Delete(entry);
            } else if (!cJSON_AddItemToArray(services, entry)) {
                cJSON_Delete(entry);
                ret = ESP_ERR_NO_MEM;
            }
        }
    }

    if (ret == ESP_OK &&
        (!cJSON_AddBoolToObject(root, "ok", true) ||
         !cJSON_AddItemToObject(root, "services", services))) {
        ret = ESP_ERR_NO_MEM;
    }
    if (ret == ESP_OK) {
        ret = write_json_result(root, result, result_size);
    }
    if (cJSON_GetObjectItemCaseSensitive(root, "services") != services) {
        cJSON_Delete(services);
    }
    cJSON_Delete(root);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "List sessions failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t tool_send_string(esp_llm_service_t *self, const char *args,
                                  char *result, size_t result_size,
                                  const char *field, const char *log_what,
                                  const char *operation,
                                  esp_err_t (*fn)(esp_llm_service_t *service, const char *value))
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, field);
    esp_llm_service_t *service = NULL;
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        cJSON_Delete(root);
        ESP_LOGE(TAG, "%s failed: %s is invalid", log_what, field);
        return ESP_ERR_INVALID_ARG;
    }
    ret = resolve_llm_service(self, root, &service, NULL);
    if (ret == ESP_OK) {
        ret = fn(service, item->valuestring);
    }
    esp_err_t json_ret = write_operation_result(result, result_size, operation, ret);
    cJSON_Delete(root);
    return ret == ESP_OK ? json_ret : ret;
}

static esp_err_t tool_send_text(esp_llm_service_t *self, const char *args,
                                char *result, size_t result_size)
{
    return tool_send_string(self, args, result, result_size, "text", "Send text tool",
                            "esp_llm_service_send_text", esp_llm_service_send_text);
}

static esp_err_t start_service(esp_llm_service_t *service)
{
    return esp_service_start(ESP_SERVICE_BASE(service));
}

static esp_err_t stop_service(esp_llm_service_t *service)
{
    return esp_service_stop(ESP_SERVICE_BASE(service));
}

static esp_err_t tool_session_control(esp_llm_service_t *self, const char *args,
                                      char *result, size_t result_size,
                                      const char *operation,
                                      esp_err_t (*fn)(esp_llm_service_t *service))
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_llm_service_t *service = NULL;
    ret = resolve_llm_service(self, root, &service, NULL);
    if (ret == ESP_OK) {
        ret = fn(service);
    }
    esp_err_t json_ret = write_operation_result(result, result_size, operation, ret);
    cJSON_Delete(root);
    return ret == ESP_OK ? json_ret : ret;
}

static esp_err_t tool_send_custom_data(esp_llm_service_t *self, const char *args,
                                       char *result, size_t result_size)
{
    return tool_send_string(self, args, result, result_size, "data", "Send custom data tool",
                            "esp_llm_service_send_custom_data", esp_llm_service_send_custom_data);
}

static esp_err_t tool_get_session(esp_llm_service_t *self, const char *args,
                                  char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_llm_service_t *service = NULL;
    uint16_t index = 0;
    ret = resolve_llm_service(self, root, &service, &index);
    if (ret != ESP_OK) {
        goto fail;
    }

    esp_llm_capability_t capabilities = (esp_llm_capability_t)0;
    ret = esp_llm_service_get_capabilities(service, &capabilities);
    if (ret != ESP_OK) {
        goto fail;
    }

    cJSON *response = cJSON_CreateObject();
    if (response == NULL ||
        !cJSON_AddBoolToObject(response, "ok", true)) {
        cJSON_Delete(response);
        cJSON_Delete(root);
        ESP_LOGE(TAG, "Get session tool failed: JSON construction failed");
        return ESP_ERR_NO_MEM;
    }
    ret = append_session_fields(response, index, service, capabilities);
    if (ret == ESP_OK) {
        ret = write_json_result(response, result, result_size);
    }
    cJSON_Delete(response);
    cJSON_Delete(root);
    return ret;

fail:
    {
        esp_err_t json_ret = write_operation_result(result, result_size,
                                                    "esp_llm_service_get_session", ret);
        cJSON_Delete(root);
        return json_ret == ESP_OK ? ret : json_ret;
    }
}

esp_err_t esp_llm_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Get schema failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_llm_service_mcp_json_start;
    return ESP_OK;
}

esp_err_t esp_llm_service_tool_invoke(esp_service_t *service,
                                      const esp_service_tool_t *tool,
                                      const char *args,
                                      char *result,
                                      size_t result_size)
{
    if (tool == NULL || tool->name == NULL || result == NULL || result_size == 0) {
        ESP_LOGE(TAG, "Invoke tool failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_t *self = (esp_llm_service_t *)service;
    if (strcmp(tool->name, "esp_llm_service_list") == 0) {
        return tool_list(self, result, result_size);
    }
    if (strcmp(tool->name, "esp_llm_service_get_session") == 0) {
        return tool_get_session(self, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_llm_service_start") == 0) {
        return tool_session_control(self, args, result, result_size, "esp_llm_service_start",
                                    start_service);
    }
    if (strcmp(tool->name, "esp_llm_service_stop") == 0) {
        return tool_session_control(self, args, result, result_size, "esp_llm_service_stop",
                                    stop_service);
    }
    if (strcmp(tool->name, "esp_llm_service_send_text") == 0) {
        return tool_send_text(self, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_llm_service_send_custom_data") == 0) {
        return tool_send_custom_data(self, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_llm_service_cancel_response") == 0) {
        return tool_session_control(self, args, result, result_size,
                                    "esp_llm_service_cancel_response",
                                    esp_llm_service_cancel_response);
    }
    ESP_LOGW(TAG, "Invoke tool: tool '%s' is not supported", tool->name);
    return ESP_ERR_NOT_SUPPORTED;
}
