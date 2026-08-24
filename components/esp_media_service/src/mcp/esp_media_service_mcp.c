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
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_media_service_mcp.h"
#include "esp_service.h"

static const char *TAG = "MEDIA_SVC_MCP";

#define MEDIA_MCP_MAX_SERVICES  16

extern const uint8_t esp_media_service_mcp_json_start[] asm("_binary_esp_media_service_mcp_json_start");
extern const uint8_t esp_media_dummy_service_mcp_json_start[] asm("_binary_esp_media_dummy_service_mcp_json_start");

typedef struct {
    esp_service_t *service;
} media_mcp_entry_t;

static media_mcp_entry_t s_services[MEDIA_MCP_MAX_SERVICES];
static size_t s_service_count;
static SemaphoreHandle_t s_lock;
static bool s_registered;
static esp_service_t s_placeholder;
static bool s_placeholder_inited;

static void ensure_lock(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }
}

static void lock(void)
{
    ensure_lock();
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

esp_err_t esp_media_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_media_service_mcp_json_start;
    return ESP_OK;
}

esp_err_t esp_media_dummy_service_mcp_schema_get(const char **out_schema)
{
    if (out_schema == NULL) {
        ESP_LOGE(TAG, "Dummy schema get failed: out_schema is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_schema = (const char *)esp_media_dummy_service_mcp_json_start;
    return ESP_OK;
}

void esp_media_service_mcp_on_init(esp_service_t *service)
{
    if (service == NULL) {
        return;
    }
    lock();
    for (size_t i = 0; i < s_service_count; i++) {
        if (s_services[i].service == service) {
            unlock();
            return;
        }
    }
    if (s_service_count >= MEDIA_MCP_MAX_SERVICES) {
        unlock();
        ESP_LOGW(TAG, "Media MCP service table full");
        return;
    }
    s_services[s_service_count++].service = service;
    unlock();
}

void esp_media_service_mcp_on_deinit(esp_service_t *service)
{
    if (service == NULL) {
        return;
    }
    lock();
    for (size_t i = 0; i < s_service_count; i++) {
        if (s_services[i].service == service) {
            s_services[i] = s_services[s_service_count - 1];
            s_services[s_service_count - 1].service = NULL;
            s_service_count--;
            break;
        }
    }
    unlock();
}

static esp_service_t *find_service_by_name(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }
    lock();
    esp_service_t *found = NULL;
    for (size_t i = 0; i < s_service_count; i++) {
        const char *svc_name = NULL;
        if (esp_service_get_name(s_services[i].service, &svc_name) == ESP_OK &&
            svc_name != NULL && strcmp(svc_name, name) == 0) {
            found = s_services[i].service;
            break;
        }
    }
    unlock();
    return found;
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

static bool json_get_string(const cJSON *root, const char *name, const char **out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL || out_value == NULL) {
        return false;
    }
    *out_value = item->valuestring;
    return true;
}

static bool json_get_uint16(const cJSON *root, const char *name, uint16_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item || !cJSON_IsNumber(item) || !out_value) {
        return false;
    }
    if (item->valuedouble < 0 || item->valuedouble > UINT16_MAX ||
        (double)(uint16_t)item->valuedouble != item->valuedouble) {
        return false;
    }
    *out_value = (uint16_t)item->valuedouble;
    return true;
}

static bool json_get_optional_uint16(const cJSON *root, const char *name, uint16_t default_value, uint16_t *out_value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!item) {
        if (out_value) {
            *out_value = default_value;
        }
        return true;
    }
    return json_get_uint16(root, name, out_value);
}

static esp_err_t tool_link_or_unlink(bool do_link, const char *args, char *result, size_t result_size)
{
    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    const char *src_name = NULL;
    const char *sink_name = NULL;
    uint16_t src_stream = 0;
    uint16_t sink_stream = 0;
    if (!json_get_string(root, "src_name", &src_name) ||
        !json_get_string(root, "sink_name", &sink_name) ||
        !json_get_optional_uint16(root, "src_stream", 0, &src_stream) ||
        !json_get_optional_uint16(root, "sink_stream", 0, &sink_stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_t *src = find_service_by_name(src_name);
    esp_service_t *sink = find_service_by_name(sink_name);
    if (src == NULL || sink == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t op_ret = do_link ? esp_media_service_link(src, src_stream, sink, sink_stream) : esp_media_service_unlink(src, src_stream, sink, sink_stream);
    esp_err_t json_ret = write_simple_result(result, result_size,
                                             do_link ? "esp_media_service_link" : "esp_media_service_unlink",
                                             op_ret);
    cJSON_Delete(root);
    return (op_ret == ESP_OK) ? json_ret : op_ret;
}

esp_err_t esp_media_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                        char *result, size_t result_size)
{
    (void)service;
    if (!tool || !tool->name || !result || result_size == 0) {
        ESP_LOGE(TAG, "Tool invoke failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(tool->name, "esp_media_service_link") == 0) {
        return tool_link_or_unlink(true, args, result, result_size);
    }
    if (strcmp(tool->name, "esp_media_service_unlink") == 0) {
        return tool_link_or_unlink(false, args, result, result_size);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t esp_media_dummy_service_tool_invoke(esp_service_t *service, const esp_service_tool_t *tool, const char *args,
                                              char *result, size_t result_size)
{
    if (!service || !tool || !tool->name || !result || result_size == 0) {
        ESP_LOGE(TAG, "Dummy tool invoke failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(tool->name, "esp_media_dummy_service_start") == 0) {
        esp_err_t op_ret = esp_service_start(service);
        esp_err_t json_ret = write_simple_result(result, result_size, tool->name, op_ret);
        return (op_ret == ESP_OK) ? json_ret : op_ret;
    }
    if (strcmp(tool->name, "esp_media_dummy_service_stop") == 0) {
        esp_err_t op_ret = esp_service_stop(service);
        esp_err_t json_ret = write_simple_result(result, result_size, tool->name, op_ret);
        return (op_ret == ESP_OK) ? json_ret : op_ret;
    }
    if (strcmp(tool->name, "esp_media_dummy_service_get_stats") != 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *root = NULL;
    esp_err_t ret = parse_args_object(args, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t stream = 0;
    if (!json_get_optional_uint16(root, "stream", 0, &stream)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    esp_media_dummy_stream_stats_t stats = {0};
    esp_err_t op_ret = esp_media_dummy_service_get_stats((esp_media_dummy_service_t *)service, stream, &stats);
    if (op_ret != ESP_OK) {
        esp_err_t json_ret = write_simple_result(result, result_size, tool->name, op_ret);
        cJSON_Delete(root);
        return (json_ret == ESP_OK) ? op_ret : json_ret;
    }

    cJSON *out = cJSON_CreateObject();
    esp_err_t json_ret = ESP_OK;
    if (!out ||
        !cJSON_AddStringToObject(out, "operation", tool->name) ||
        !cJSON_AddBoolToObject(out, "ok", true) ||
        !cJSON_AddNumberToObject(out, "stream", stream) ||
        !cJSON_AddNumberToObject(out, "audio_frame_count", stats.audio_frame_count) ||
        !cJSON_AddNumberToObject(out, "audio_byte_count", stats.audio_byte_count) ||
        !cJSON_AddNumberToObject(out, "video_frame_count", stats.video_frame_count) ||
        !cJSON_AddNumberToObject(out, "video_byte_count", stats.video_byte_count)) {
        json_ret = ESP_ERR_NO_MEM;
    } else {
        json_ret = write_json_result(out, result, result_size);
    }
    cJSON_Delete(out);
    cJSON_Delete(root);
    return json_ret;
}

esp_err_t esp_media_service_mcp_register(esp_service_manager_t *mgr)
{
    if (mgr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_registered) {
        return ESP_OK;
    }

    if (!s_placeholder_inited) {
        esp_service_config_t cfg = ESP_SERVICE_CONFIG_DEFAULT();
        cfg.name = "esp_media_mcp";
        esp_err_t ret = esp_service_init(&s_placeholder, &cfg, NULL);
        if (ret != ESP_OK) {
            return ret;
        }
        s_placeholder_inited = true;
    }

    const char *schema = NULL;
    esp_err_t ret = esp_media_service_mcp_schema_get(&schema);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_service_registration_t reg = {
        .service = &s_placeholder,
        .category = "media",
        .flags = ESP_SERVICE_REG_FLAG_SKIP_BATCH_START | ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP,
        .tool_desc = schema,
        .tool_invoke = esp_media_service_tool_invoke,
    };
    ret = esp_service_manager_register(mgr, &reg);
    if (ret != ESP_OK) {
        return ret;
    }
    s_registered = true;
    ESP_LOGI(TAG, "Media link/unlink MCP tools registered");
    return ESP_OK;
}
