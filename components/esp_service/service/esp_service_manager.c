/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_service_manager.h"

static const char *TAG = "SERVICE_MGR";

/**
 * @brief  Service entry in registry (internal)
 */
typedef struct service_entry {
    esp_service_t                *service;      /*!< Service instance */
    char                         *category;     /*!< Service category string (owned copy) */
    esp_service_tool_invoke_fn_t  tool_invoke;  /*!< Tool invocation callback; NULL = no tool support */
    esp_service_tool_t           *tools;        /*!< Array of parsed tools (NULL when tool_invoke is NULL) */
    uint16_t                      tool_count;   /*!< Number of entries in tools array */
    uint32_t                      flags;        /*!< ESP_SERVICE_REG_FLAG_xxx bits */
    struct service_entry         *next;         /*!< Next entry in linked list */
} service_entry_t;

/**
 * @brief  Service manager structure
 */
struct esp_service_manager {
    esp_service_manager_config_t  config;         /*!< Manager configuration */
    service_entry_t              *services;       /*!< Linked list of registered services */
    uint16_t                      service_count;  /*!< Number of registered services */
    SemaphoreHandle_t             mutex;          /*!< Mutex for thread-safe access */
};

static esp_err_t parse_mcp_tools(const char *json_str, service_entry_t *entry, uint16_t max_tools)
{
    if (!json_str || !entry) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    if (!cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "JSON is not an array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    uint16_t tool_count = cJSON_GetArraySize(root);
    if (tool_count == 0) {
        cJSON_Delete(root);
        return ESP_OK;  /* No tools, not an error */
    }

    if (tool_count > max_tools) {
        ESP_LOGW(TAG, "Tool count %d exceeds max %d, truncating", tool_count, max_tools);
        tool_count = max_tools;
    }

    entry->tools = calloc(tool_count, sizeof(esp_service_tool_t));
    if (!entry->tools) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    uint16_t idx = 0;
    cJSON *tool_obj = NULL;
    cJSON_ArrayForEach(tool_obj, root)
    {
        if (idx >= tool_count) {
            break;
        }

        cJSON *name = cJSON_GetObjectItem(tool_obj, "name");
        cJSON *desc = cJSON_GetObjectItem(tool_obj, "description");
        cJSON *input_schema = cJSON_GetObjectItem(tool_obj, "inputSchema");

        if (!name || !cJSON_IsString(name)) {
            ESP_LOGW(TAG, "Tool missing 'name' field, skipping");
            continue;
        }

        esp_service_tool_t *tool = &entry->tools[idx];
        tool->name = strdup(name->valuestring);
        if (!tool->name) {
            goto oom;
        }

        if (desc && cJSON_IsString(desc)) {
            tool->description = strdup(desc->valuestring);
            if (!tool->description) {
                goto oom;
            }
        }

        if (input_schema) {
            char *schema_str = cJSON_PrintUnformatted(input_schema);
            if (!schema_str) {
                goto oom;
            }
            tool->input_schema = schema_str;
        }

        idx++;
    }

    entry->tool_count = idx;
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Parsed %d tools from JSON", entry->tool_count);
    return ESP_OK;

oom:
    ESP_LOGE(TAG, "Out of memory while parsing tools");
    for (uint16_t i = 0; i <= idx && i < tool_count; i++) {
        if (entry->tools[i].name) {
            free(entry->tools[i].name);
        }
        if (entry->tools[i].description) {
            free(entry->tools[i].description);
        }
        if (entry->tools[i].input_schema) {
            cJSON_free(entry->tools[i].input_schema);
        }
    }
    free(entry->tools);
    entry->tools = NULL;
    entry->tool_count = 0;
    cJSON_Delete(root);
    return ESP_ERR_NO_MEM;
}

static void free_service_entry(service_entry_t *entry)
{
    if (!entry) {
        return;
    }

    if (entry->category) {
        free(entry->category);
    }

    if (entry->tools) {
        for (uint16_t i = 0; i < entry->tool_count; i++) {
            if (entry->tools[i].name) {
                free(entry->tools[i].name);
            }
            if (entry->tools[i].description) {
                free(entry->tools[i].description);
            }
            if (entry->tools[i].input_schema) {
                cJSON_free(entry->tools[i].input_schema);
            }
        }
        free(entry->tools);
    }

    free(entry);
}

esp_err_t esp_service_manager_create(const esp_service_manager_config_t *config, esp_service_manager_t **out_mgr)
{
    if (!out_mgr) {
        ESP_LOGE(TAG, "Create: out_mgr is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_manager_t *mgr = calloc(1, sizeof(esp_service_manager_t));
    if (!mgr) {
        ESP_LOGE(TAG, "Create: no memory for manager");
        return ESP_ERR_NO_MEM;
    }

    if (config) {
        mgr->config = *config;
    } else {
        mgr->config = (esp_service_manager_config_t)ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    }

    mgr->mutex = xSemaphoreCreateRecursiveMutex();
    if (!mgr->mutex) {
        free(mgr);
        ESP_LOGE(TAG, "Create: no memory for mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Service manager created (max_services=%d)", mgr->config.max_services);
    *out_mgr = mgr;
    return ESP_OK;
}

esp_err_t esp_service_manager_destroy(esp_service_manager_t *mgr)
{
    if (!mgr) {
        ESP_LOGE(TAG, "Destroy: mgr is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    service_entry_t *entry = mgr->services;
    while (entry) {
        service_entry_t *next = entry->next;
        free_service_entry(entry);
        entry = next;
    }

    if (mgr->mutex) {
        vSemaphoreDelete(mgr->mutex);
    }

    free(mgr);
    ESP_LOGI(TAG, "Service manager destroyed");
    return ESP_OK;
}

esp_err_t esp_service_manager_register(esp_service_manager_t *mgr, const esp_service_registration_t *reg)
{
    if (!mgr || !reg || !reg->service) {
        ESP_LOGE(TAG, "Register: invalid arg (mgr=%p, reg=%p)", (void *)mgr, (void *)reg);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Register: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    service_entry_t *curr = mgr->services;
    while (curr) {
        if (curr->service == reg->service) {
            xSemaphoreGiveRecursive(mgr->mutex);
            ESP_LOGW(TAG, "Service already registered");
            return ESP_ERR_INVALID_STATE;
        }
        curr = curr->next;
    }

    if (mgr->service_count >= mgr->config.max_services) {
        xSemaphoreGiveRecursive(mgr->mutex);
        ESP_LOGE(TAG, "Service registry full");
        return ESP_ERR_NO_MEM;
    }

    service_entry_t *entry = calloc(1, sizeof(service_entry_t));
    if (!entry) {
        xSemaphoreGiveRecursive(mgr->mutex);
        ESP_LOGE(TAG, "Register: no memory for service entry");
        return ESP_ERR_NO_MEM;
    }

    entry->service = reg->service;
    entry->flags = reg->flags;
    entry->tool_invoke = reg->tool_invoke;

    if (reg->category) {
        entry->category = strdup(reg->category);
        if (!entry->category) {
            free_service_entry(entry);
            xSemaphoreGiveRecursive(mgr->mutex);
            ESP_LOGE(TAG, "Register: no memory for category");
            return ESP_ERR_NO_MEM;
        }
    }

    if (reg->tool_desc && reg->tool_invoke) {
        esp_err_t ret = parse_mcp_tools(reg->tool_desc, entry, mgr->config.max_tools_per_service);
        if (ret == ESP_ERR_NO_MEM) {
            free_service_entry(entry);
            xSemaphoreGiveRecursive(mgr->mutex);
            ESP_LOGE(TAG, "Register: no memory parsing tools");
            return ESP_ERR_NO_MEM;
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to parse tools from JSON: %d", ret);
        }
    }

    entry->next = mgr->services;
    mgr->services = entry;
    mgr->service_count++;

    const char *name = NULL;
    esp_service_get_name(reg->service, &name);
    uint16_t tool_count = entry->tool_count;

    xSemaphoreGiveRecursive(mgr->mutex);

    ESP_LOGI(TAG, "Registered service '%s' with %d tools",
             name ? name : "unknown", tool_count);

    if (mgr->config.auto_start_services) {
        esp_service_start(reg->service);
    }

    return ESP_OK;
}

esp_err_t esp_service_manager_unregister(esp_service_manager_t *mgr, esp_service_t *service)
{
    if (!mgr || !service) {
        ESP_LOGE(TAG, "Unregister: invalid arg (mgr=%p, service=%p)", (void *)mgr, (void *)service);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Unregister: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    service_entry_t *prev = NULL;
    service_entry_t *curr = mgr->services;

    while (curr) {
        if (curr->service == service) {
            if (prev) {
                prev->next = curr->next;
            } else {
                mgr->services = curr->next;
            }

            mgr->service_count--;
            free_service_entry(curr);

            xSemaphoreGiveRecursive(mgr->mutex);
            ESP_LOGI(TAG, "Unregistered service");
            return ESP_OK;
        }
        prev = curr;
        curr = curr->next;
    }

    xSemaphoreGiveRecursive(mgr->mutex);
    ESP_LOGW(TAG, "Unregister: service not found");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_service_manager_find_by_name(esp_service_manager_t *mgr, const char *name, esp_service_t **out_service)
{
    if (!mgr || !name || !out_service) {
        ESP_LOGE(TAG, "Find by name: invalid arg");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Find by name: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    service_entry_t *curr = mgr->services;
    while (curr) {
        const char *svc_name = NULL;
        if (esp_service_get_name(curr->service, &svc_name) == ESP_OK) {
            if (svc_name && strcmp(svc_name, name) == 0) {
                *out_service = curr->service;
                xSemaphoreGiveRecursive(mgr->mutex);
                return ESP_OK;
            }
        }
        curr = curr->next;
    }

    xSemaphoreGiveRecursive(mgr->mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_service_manager_find_by_category(esp_service_manager_t *mgr,
                                               const char *category,
                                               uint16_t index,
                                               esp_service_t **out_service)
{
    if (!mgr || !category || !out_service) {
        ESP_LOGE(TAG, "Find by category: invalid arg");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Find by category: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t count = 0;
    service_entry_t *curr = mgr->services;

    while (curr) {
        if (curr->category && strcmp(curr->category, category) == 0) {
            if (count == index) {
                *out_service = curr->service;
                xSemaphoreGiveRecursive(mgr->mutex);
                return ESP_OK;
            }
            count++;
        }
        curr = curr->next;
    }

    xSemaphoreGiveRecursive(mgr->mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_service_manager_get_count(esp_service_manager_t *mgr, uint16_t *out_count)
{
    if (!mgr || !out_count) {
        ESP_LOGE(TAG, "Get count: invalid arg (mgr=%p, out=%p)", (void *)mgr, (void *)out_count);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Get count: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    *out_count = mgr->service_count;
    xSemaphoreGiveRecursive(mgr->mutex);
    return ESP_OK;
}

esp_err_t esp_service_manager_start_all(esp_service_manager_t *mgr)
{
    if (!mgr) {
        ESP_LOGE(TAG, "Start all: mgr is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Start all: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t count = mgr->service_count;
    esp_service_t **snapshot = count > 0 ? calloc(count, sizeof(*snapshot)) : NULL;
    uint32_t *flags_snapshot = count > 0 ? calloc(count, sizeof(*flags_snapshot)) : NULL;
    if (count > 0 && (!snapshot || !flags_snapshot)) {
        free(snapshot);
        free(flags_snapshot);
        xSemaphoreGiveRecursive(mgr->mutex);
        ESP_LOGE(TAG, "Start all: no memory for snapshot");
        return ESP_ERR_NO_MEM;
    }
    uint16_t idx = 0;
    for (service_entry_t *curr = mgr->services; curr && idx < count; curr = curr->next) {
        snapshot[idx] = curr->service;
        flags_snapshot[idx] = curr->flags;
        idx++;
    }
    xSemaphoreGiveRecursive(mgr->mutex);

    for (uint16_t i = 0; i < idx; i++) {
        if (!(flags_snapshot[i] & ESP_SERVICE_REG_FLAG_SKIP_BATCH_START)) {
            esp_service_start(snapshot[i]);
        } else {
            const char *name = NULL;
            esp_service_get_name(snapshot[i], &name);
            ESP_LOGD(TAG, "Skipped start for '%s' (SKIP_BATCH_START flag set)",
                     name ? name : "unknown");
        }
    }
    free(snapshot);
    free(flags_snapshot);
    ESP_LOGI(TAG, "Started all services");
    return ESP_OK;
}

esp_err_t esp_service_manager_stop_all(esp_service_manager_t *mgr)
{
    if (!mgr) {
        ESP_LOGE(TAG, "Stop all: mgr is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Stop all: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t count = mgr->service_count;
    esp_service_t **snapshot = count > 0 ? calloc(count, sizeof(*snapshot)) : NULL;
    uint32_t *flags_snapshot = count > 0 ? calloc(count, sizeof(*flags_snapshot)) : NULL;
    if (count > 0 && (!snapshot || !flags_snapshot)) {
        free(snapshot);
        free(flags_snapshot);
        xSemaphoreGiveRecursive(mgr->mutex);
        ESP_LOGE(TAG, "Stop all: no memory for snapshot");
        return ESP_ERR_NO_MEM;
    }
    uint16_t idx = 0;
    for (service_entry_t *curr = mgr->services; curr && idx < count; curr = curr->next) {
        snapshot[idx] = curr->service;
        flags_snapshot[idx] = curr->flags;
        idx++;
    }
    xSemaphoreGiveRecursive(mgr->mutex);

    for (uint16_t i = 0; i < idx; i++) {
        if (!(flags_snapshot[i] & ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP)) {
            esp_service_stop(snapshot[i]);
        } else {
            const char *name = NULL;
            esp_service_get_name(snapshot[i], &name);
            ESP_LOGD(TAG, "Skipped stop for '%s' (SKIP_BATCH_STOP flag set)",
                     name ? name : "unknown");
        }
    }
    free(snapshot);
    free(flags_snapshot);
    ESP_LOGI(TAG, "Stopped all services");
    return ESP_OK;
}

esp_err_t esp_service_manager_get_tools(esp_service_manager_t *mgr,
                                        const esp_service_tool_t **out_tools,
                                        uint16_t max_tools,
                                        uint16_t *out_count)
{
    if (!mgr || !out_tools || !out_count) {
        ESP_LOGE(TAG, "Get tools: invalid arg (mgr=%p, out_tools=%p, out_count=%p)",
                 (void *)mgr, (void *)out_tools, (void *)out_count);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Get tools: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    uint16_t count = 0;
    service_entry_t *curr = mgr->services;

    while (curr && count < max_tools) {
        for (uint16_t i = 0; i < curr->tool_count && count < max_tools; i++) {
            out_tools[count++] = &curr->tools[i];
        }
        curr = curr->next;
    }

    *out_count = count;
    xSemaphoreGiveRecursive(mgr->mutex);
    return ESP_OK;
}

void esp_service_manager_free_cloned_tools(esp_service_tool_t *tools, uint16_t count)
{
    if (tools == NULL) {
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        free(tools[i].name);
        free(tools[i].description);
        free(tools[i].input_schema);
    }
    free(tools);
}

esp_err_t esp_service_manager_clone_tools(esp_service_manager_t *mgr,
                                          esp_service_tool_t **out_tools,
                                          uint16_t *out_count)
{
    if (!mgr || !out_tools || !out_count) {
        ESP_LOGE(TAG, "Clone tools: invalid arg (mgr=%p, out_tools=%p, out_count=%p)",
                 (void *)mgr, (void *)out_tools, (void *)out_count);
        return ESP_ERR_INVALID_ARG;
    }

    *out_tools = NULL;
    *out_count = 0;

    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Clone tools: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    /* Cap aggregate count at UINT16_MAX to match the out_count type. */
    uint32_t total = 0;
    for (service_entry_t *curr = mgr->services; curr != NULL; curr = curr->next) {
        total += curr->tool_count;
        if (total >= 0xFFFFu) {
            total = 0xFFFFu;
            break;
        }
    }

    if (total == 0) {
        xSemaphoreGiveRecursive(mgr->mutex);
        return ESP_OK;
    }

    esp_service_tool_t *arr = calloc(total, sizeof(*arr));
    if (arr == NULL) {
        xSemaphoreGiveRecursive(mgr->mutex);
        ESP_LOGE(TAG, "Clone tools: no memory for %u entries", (unsigned)total);
        return ESP_ERR_NO_MEM;
    }

    uint16_t idx = 0;
    esp_err_t err = ESP_OK;
    for (service_entry_t *curr = mgr->services; curr != NULL && err == ESP_OK; curr = curr->next) {
        for (uint16_t i = 0; i < curr->tool_count && idx < total; i++) {
            const esp_service_tool_t *src = &curr->tools[i];
            if (src->name != NULL) {
                arr[idx].name = strdup(src->name);
                if (arr[idx].name == NULL) {
                    err = ESP_ERR_NO_MEM;
                    break;
                }
            }
            if (src->description != NULL) {
                arr[idx].description = strdup(src->description);
                if (arr[idx].description == NULL) {
                    err = ESP_ERR_NO_MEM;
                    break;
                }
            }
            if (src->input_schema != NULL) {
                arr[idx].input_schema = strdup(src->input_schema);
                if (arr[idx].input_schema == NULL) {
                    err = ESP_ERR_NO_MEM;
                    break;
                }
            }
            idx++;
        }
    }

    xSemaphoreGiveRecursive(mgr->mutex);

    if (err != ESP_OK) {
        esp_service_manager_free_cloned_tools(arr, (uint16_t)(idx + 1));
        return err;
    }

    *out_tools = arr;
    *out_count = idx;
    return ESP_OK;
}

esp_err_t esp_service_manager_invoke_tool(esp_service_manager_t *mgr,
                                          const char *tool_name,
                                          const char *args_json,
                                          char *result,
                                          size_t result_size)
{
    if (!mgr || !tool_name || !result) {
        ESP_LOGE(TAG, "Invoke tool: invalid arg (mgr=%p, tool_name=%p, result=%p)",
                 (void *)mgr, (void *)tool_name, (void *)result);
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTakeRecursive(mgr->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Invoke tool: mutex timeout");
        return ESP_ERR_TIMEOUT;
    }
    // Snapshot dispatch data under the lock so the callback runs unlocked.
    // Caller must not unregister the service during invocation (shallow copy).
    esp_service_tool_invoke_fn_t invoke_fn = NULL;
    esp_service_t *svc = NULL;
    esp_service_tool_t tool_snapshot = {0};
    bool found = false;

    for (service_entry_t *curr = mgr->services; curr != NULL && !found; curr = curr->next) {
        for (uint16_t i = 0; i < curr->tool_count; i++) {
            if (curr->tools[i].name && strcmp(curr->tools[i].name, tool_name) == 0) {
                invoke_fn = curr->tool_invoke;
                svc = curr->service;
                tool_snapshot = curr->tools[i];
                found = true;
                break;
            }
        }
    }
    xSemaphoreGiveRecursive(mgr->mutex);
    if (!found) {
        ESP_LOGW(TAG, "Invoke tool: '%s' not found", tool_name);
        return ESP_ERR_NOT_FOUND;
    }
    if (!invoke_fn) {
        ESP_LOGE(TAG, "Tool '%s' has no invoke function", tool_name);
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret = invoke_fn(svc, &tool_snapshot, args_json, result, result_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Tool '%s' invocation failed: %d", tool_name, ret);
    }
    return ret;
}

static esp_err_t provider_get_tools(void *ctx, const esp_service_tool_t **out_tools, uint16_t max_tools,
                                    uint16_t *out_count)
{
    return esp_service_manager_get_tools((esp_service_manager_t *)ctx, out_tools, max_tools, out_count);
}

static esp_err_t provider_invoke_tool(void *ctx, const char *tool_name, const char *args_json,
                                      char *result, size_t result_size)
{
    return esp_service_manager_invoke_tool((esp_service_manager_t *)ctx, tool_name, args_json, result, result_size);
}

esp_err_t esp_service_manager_as_tool_provider(esp_service_manager_t *mgr, esp_service_mcp_tool_provider_t *out_provider)
{
    if (!mgr || !out_provider) {
        return ESP_ERR_INVALID_ARG;
    }
    out_provider->get_tools = provider_get_tools;
    out_provider->invoke_tool = provider_invoke_tool;
    out_provider->ctx = mgr;
    return ESP_OK;
}
