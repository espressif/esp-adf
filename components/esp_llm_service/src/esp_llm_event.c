/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_llm_service_internal.h"

static const char *TAG = "ESP_LLM_EVENT";

static const char *s_common_names[] = {
    [ESP_LLM_EVENT_NONE]         = "NONE",
    [ESP_LLM_EVENT_WAKEUP]       = "WAKEUP",
    [ESP_LLM_EVENT_VAD]          = "VAD",
    [ESP_LLM_EVENT_CONNECTED]    = "CONNECTED",
    [ESP_LLM_EVENT_DISCONNECTED] = "DISCONNECTED",
    [ESP_LLM_EVENT_MEDIA_READY]  = "MEDIA_READY",
    [ESP_LLM_EVENT_TEXT]         = "TEXT",
    [ESP_LLM_EVENT_ERROR]        = "ERROR",
    [ESP_LLM_EVENT_FINISHED]     = "FINISHED",
    [ESP_LLM_EVENT_BACKEND_RAW]  = "BACKEND_RAW",
};

static void release_event(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

const char *esp_llm_event_to_name(uint16_t event_id)
{
    return event_id < sizeof(s_common_names) / sizeof(s_common_names[0]) ?
           s_common_names[event_id] : NULL;
}

esp_err_t esp_llm_event_dispatch(esp_llm_service_t *service, const esp_llm_event_t *event)
{
    if (service == NULL || event == NULL ||
        (unsigned)event->common_id > ESP_LLM_EVENT_FINISHED ||
        ((event->payload == NULL) != (event->payload_len == 0))) {
        ESP_LOGE(TAG, "Dispatch event failed: invalid service or event");
        return ESP_ERR_INVALID_ARG;
    }

    size_t event_name_len = 0;
    if (event->common_id == ESP_LLM_EVENT_NONE && event->backend_event_name != NULL) {
        event_name_len = strlen(event->backend_event_name) + 1;
    }
    if (event->payload_len > SIZE_MAX - sizeof(*event) - event_name_len) {
        ESP_LOGE(TAG, "Dispatch event failed: payload is too large");
        return ESP_ERR_INVALID_SIZE;
    }

    size_t alloc_size = sizeof(*event) + event_name_len + event->payload_len;
    esp_llm_event_t *copy = calloc(1, alloc_size);
    if (copy == NULL) {
        ESP_LOGE(TAG, "Dispatch event failed: memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    *copy = *event;
    copy->backend_event_name = NULL;
    char *cursor = (char *)(copy + 1);
    if (event_name_len != 0) {
        memcpy(cursor, event->backend_event_name, event_name_len);
        copy->backend_event_name = cursor;
        cursor += event_name_len;
    }
    if (event->payload != NULL && event->payload_len != 0) {
        memcpy(cursor, event->payload, event->payload_len);
        copy->payload = cursor;
    } else {
        copy->payload = NULL;
        copy->payload_len = 0;
    }

    uint16_t event_id = copy->common_id == ESP_LLM_EVENT_NONE ?
                        ESP_LLM_EVENT_BACKEND_RAW : (uint16_t)copy->common_id;
    return esp_service_publish_event(ESP_SERVICE_BASE(service), event_id, copy,
                                     sizeof(*copy), release_event, NULL);
}

esp_err_t esp_llm_event_common_id_to_name(esp_llm_event_common_id_t id, const char **out_name)
{
    if (out_name == NULL || (unsigned)id > ESP_LLM_EVENT_FINISHED) {
        ESP_LOGE(TAG, "Get common event name failed: out_name is NULL or id is unknown");
        return ESP_ERR_INVALID_ARG;
    }
    *out_name = s_common_names[id];
    return ESP_OK;
}
