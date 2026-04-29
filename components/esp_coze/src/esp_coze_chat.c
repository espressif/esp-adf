/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"

#include "esp_coze_chat_priv.h"

static const char *TAG = ESP_COZE_TAG;

void esp_coze_post_event(coze_chat_t *chat, int32_t event_id)
{
    if (chat == NULL || chat->config.event_callback == NULL) {
        return;
    }
    chat->config.event_callback((esp_coze_chat_event_t)event_id, NULL,
                                chat->config.event_callback_ctx);
}

void esp_coze_post_event_text(coze_chat_t *chat, int32_t event_id, const char *text)
{
    if (chat == NULL || text == NULL || chat->config.event_callback == NULL) {
        return;
    }
    chat->config.event_callback((esp_coze_chat_event_t)event_id, (void *)text,
                                chat->config.event_callback_ctx);
}

void esp_coze_post_event_blob(coze_chat_t *chat, int32_t event_id,
                              const void *data, size_t size)
{
    if (chat == NULL || data == NULL || size == 0) {
        return;
    }

    if (event_id == ESP_COZE_CHAT_EVENT_AUDIO_DATA &&
        size >= sizeof(esp_coze_chat_audio_data_t)) {
        const esp_coze_chat_audio_data_t *audio = (const esp_coze_chat_audio_data_t *)data;
        /* Routing contract (esp_coze_chat.h): audio events go to audio_callback
         * exclusively when one is configured; never silently fall back to
         * event_callback, even on an empty/zero-length frame. */
        if (chat->config.audio_callback != NULL) {
            if (audio->len > 0) {
                chat->config.audio_callback((const uint8_t *)audio->data, audio->len,
                                            chat->config.audio_callback_ctx);
            }
            return;
        }
        if (chat->config.event_callback != NULL) {
            chat->config.event_callback((esp_coze_chat_event_t)event_id, (void *)data,
                                        chat->config.event_callback_ctx);
        }
        return;
    }

    if (chat->config.event_callback != NULL) {
        chat->config.event_callback((esp_coze_chat_event_t)event_id, (void *)data,
                                    chat->config.event_callback_ctx);
    }
}

static char *sdk_strdup(const char *str)
{
    return str ? strdup(str) : NULL;
}

static char **dup_string_array(const char **src)
{
    if (src == NULL) {
        return NULL;
    }
    int n = 0;
    while (src[n] != NULL) {
        n++;
    }
    char **out = (char **)calloc((size_t)n + 1, sizeof(char *));
    if (out == NULL) {
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        out[i] = sdk_strdup(src[i]);
        if (out[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(out[j]);
            }
            free(out);
            return NULL;
        }
    }
    return out;
}

static void free_string_array(char **arr)
{
    if (arr == NULL) {
        return;
    }
    for (int i = 0; arr[i] != NULL; i++) {
        free(arr[i]);
    }
    free(arr);
}

static void clear_parameters(coze_chat_t *chat)
{
    if (chat->chat_parameters.items == NULL) {
        return;
    }
    for (int i = 0; i < chat->chat_parameters.count; i++) {
        free(chat->chat_parameters.items[i].key);
        free(chat->chat_parameters.items[i].value);
    }
    free(chat->chat_parameters.items);
    chat->chat_parameters.items = NULL;
    chat->chat_parameters.count = 0;
}

static void destroy_chat(coze_chat_t *chat)
{
    if (chat == NULL) {
        return;
    }
    if (chat->transport) {
        coze_transport_destroy(chat->transport);
    }
    if (chat->router) {
        coze_router_destroy(chat->router);
    }
    if (chat->lock) {
        vSemaphoreDelete(chat->lock);
    }
    clear_parameters(chat);
    free_string_array(chat->subscribe_event);
    free(chat->ws_base_url);
    free(chat->conv_create_base_url);
    free(chat->bot_id);
    free(chat->access_token);
    free(chat->user_id);
    free(chat->voice_id);
    free(chat->conversation_id);
    free(chat);
}

esp_err_t esp_coze_chat_init(esp_coze_chat_config_t *config, esp_coze_chat_handle_t *chat_hd)
{
    esp_err_t ret = ESP_OK;
    coze_chat_t *chat = NULL;
    char *uri = NULL;

    ESP_RETURN_ON_FALSE(config != NULL && chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    ESP_RETURN_ON_FALSE(config->bot_id != NULL && config->access_token != NULL && config->voice_id != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "Fields bot_id, access_token, and voice_id are required");
    ESP_RETURN_ON_FALSE(config->ws_base_url != NULL, ESP_ERR_INVALID_ARG, TAG, "Field ws_base_url is required");

    chat = (coze_chat_t *)calloc(1, sizeof(coze_chat_t));
    ESP_RETURN_ON_FALSE(chat != NULL, ESP_ERR_NO_MEM, TAG, "No memory for chat instance");

    memcpy(&chat->config, config, sizeof(esp_coze_chat_config_t));
    chat->config.websocket_connect_timeout = CONFIG_ESP_COZE_WS_CONNECT_TIMEOUT_MS;
    chat->config.ws_base_url = NULL;
    chat->config.conv_create_base_url = NULL;
    chat->config.bot_id = NULL;
    chat->config.access_token = NULL;
    chat->config.user_id = NULL;
    chat->config.voice_id = NULL;
    chat->config.subscribe_event = NULL;

    chat->ws_base_url = sdk_strdup(config->ws_base_url);
    chat->conv_create_base_url = sdk_strdup(config->conv_create_base_url);
    chat->bot_id = sdk_strdup(config->bot_id);
    chat->access_token = sdk_strdup(config->access_token);
    chat->user_id = sdk_strdup(config->user_id);
    chat->voice_id = sdk_strdup(config->voice_id);
    chat->subscribe_event = dup_string_array(config->subscribe_event);

    ESP_GOTO_ON_FALSE(chat->ws_base_url && chat->bot_id && chat->access_token && chat->voice_id &&
                          (config->user_id == NULL || chat->user_id != NULL) &&
                          (config->conv_create_base_url == NULL || chat->conv_create_base_url != NULL) &&
                          (config->subscribe_event == NULL || chat->subscribe_event != NULL),
                      ESP_ERR_NO_MEM, fail, TAG, "Out of memory while duplicating configuration strings");

    chat->lock = xSemaphoreCreateMutex();
    ESP_GOTO_ON_FALSE(chat->lock != NULL, ESP_ERR_NO_MEM, fail, TAG, "Failed to create chat mutex");

    /* Build URI: "<ws_base_url>?bot_id=<bot_id>" */
    size_t uri_len = strlen(chat->ws_base_url) + strlen("?bot_id=") + strlen(chat->bot_id) + 1;
    uri = (char *)malloc(uri_len);
    ESP_GOTO_ON_FALSE(uri != NULL, ESP_ERR_NO_MEM, fail, TAG, "No memory for WebSocket URI");
    snprintf(uri, uri_len, "%s?bot_id=%s", chat->ws_base_url, chat->bot_id);

    const char *ws_task_name = CONFIG_ESP_COZE_WS_TASK_NAME;
#if CONFIG_ESP_COZE_WS_TASK_CORE_ID_SET
    const bool ws_core_set = true;
    const int ws_core_id = CONFIG_ESP_COZE_WS_TASK_CORE_ID;
#else
    const bool ws_core_set = false;
    const int ws_core_id = 0;
#endif  /* CONFIG_ESP_COZE_WS_TASK_CORE_ID_SET */

    coze_transport_cfg_t tcfg = {
        .uri = uri,
        .bearer_token = chat->access_token,
        .buffer_size = CONFIG_ESP_COZE_WS_BUFFER_SIZE,
        .reconnect_timeout_ms = CONFIG_ESP_COZE_WS_RECONNECT_TIMEOUT_MS,
        .network_timeout_ms = CONFIG_ESP_COZE_WS_NETWORK_TIMEOUT_MS,
        .task_prio = CONFIG_ESP_COZE_WS_TASK_PRIO,
        .task_stack = CONFIG_ESP_COZE_WS_TASK_STACK,
        .task_name = (ws_task_name[0] != '\0') ? ws_task_name : NULL,
        .task_core_id_set = ws_core_set,
        .task_core_id = ws_core_id,
        .use_crt_bundle = true,
    };
    ESP_GOTO_ON_ERROR(coze_transport_create(&tcfg, &chat->transport), fail, TAG,
                      "transport create failed");
    free(uri);
    uri = NULL;

    ESP_GOTO_ON_ERROR(coze_router_create(&chat->router), fail, TAG, "router create failed");

    coze_transport_set_router(chat->transport, chat->router);
    coze_transport_set_observer(chat->transport, esp_coze_chat_transport_observer, chat);

    ESP_GOTO_ON_ERROR(esp_coze_chat_event_register(chat), fail, TAG, "register chat events failed");

    *chat_hd = chat;
    ESP_LOGI(TAG, "Create 'coze_chat': initialized");
    return ESP_OK;

fail:
    free(uri);
    destroy_chat(chat);
    return ret;
}

esp_err_t esp_coze_chat_deinit(esp_coze_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    destroy_chat((coze_chat_t *)chat_hd);
    return ESP_OK;
}

esp_err_t esp_coze_chat_start(esp_coze_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;

    ESP_RETURN_ON_ERROR(coze_transport_start(chat->transport), TAG, "WebSocket client start failed");

    esp_err_t err = coze_transport_wait_connected(chat->transport,
                                                  chat->config.websocket_connect_timeout);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WebSocket connection timed out");
        coze_transport_stop(chat->transport);
        return err;
    }
    ESP_LOGI(TAG, "WebSocket session connected");
    return esp_coze_proto_send_update(chat);
}

esp_err_t esp_coze_chat_stop(esp_coze_chat_handle_t chat_hd)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;
    return coze_transport_stop(chat->transport);
}

esp_err_t esp_coze_set_chat_config_voice_id(esp_coze_chat_handle_t chat_hd, const char *voice_id)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL && voice_id != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;
    char *copy = strdup(voice_id);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "No memory for voice_id copy");
    ESP_COZE_LOCK(chat);
    free(chat->voice_id);
    chat->voice_id = copy;
    ESP_COZE_UNLOCK(chat);
    return ESP_OK;
}

esp_err_t esp_coze_set_chat_config_parameters(esp_coze_chat_handle_t chat_hd,
                                              esp_coze_parameters_kv_t *key_val)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL && key_val != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;

    int count = 0;
    for (esp_coze_parameters_kv_t *p = key_val; p->key != NULL; p++) {
        ESP_RETURN_ON_FALSE(p->value != NULL, ESP_ERR_INVALID_ARG, TAG, "Parameter '%s' has NULL value",
                            p->key);
        count++;
    }

    coze_kv_owned_t *items = NULL;
    if (count > 0) {
        items = (coze_kv_owned_t *)calloc((size_t)count, sizeof(coze_kv_owned_t));
        ESP_RETURN_ON_FALSE(items != NULL, ESP_ERR_NO_MEM, TAG, "No memory for parameters list");
        for (int i = 0; i < count; i++) {
            items[i].key = strdup(key_val[i].key);
            items[i].value = strdup(key_val[i].value);
            if (items[i].key == NULL || items[i].value == NULL) {
                ESP_LOGE(TAG, "Set chat parameters failed: key/value strdup returned NULL");
                for (int j = 0; j <= i; j++) {
                    free(items[j].key);
                    free(items[j].value);
                }
                free(items);
                return ESP_ERR_NO_MEM;
            }
        }
    }

    ESP_COZE_LOCK(chat);
    clear_parameters(chat);
    chat->chat_parameters.items = items;
    chat->chat_parameters.count = count;
    ESP_COZE_UNLOCK(chat);
    return ESP_OK;
}

esp_err_t esp_coze_set_chat_config_conversation_id(esp_coze_chat_handle_t chat_hd, const char *conversation_id)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL && conversation_id != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;
    char *copy = strdup(conversation_id);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "No memory for conversation_id copy");
    ESP_COZE_LOCK(chat);
    free(chat->conversation_id);
    chat->conversation_id = copy;
    chat->conversation_id_set = true;
    ESP_COZE_UNLOCK(chat);
    return ESP_OK;
}

esp_err_t esp_coze_chat_get_config_conversation_id(esp_coze_chat_handle_t chat_hd, char **conversation_id)
{
    ESP_RETURN_ON_FALSE(chat_hd != NULL && conversation_id != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    coze_chat_t *chat = (coze_chat_t *)chat_hd;
    *conversation_id = NULL;
    ESP_COZE_LOCK(chat);
    if (chat->conversation_id == NULL) {
        ESP_COZE_UNLOCK(chat);
        return ESP_ERR_NOT_FOUND;
    }
    char *copy = strdup(chat->conversation_id);
    ESP_COZE_UNLOCK(chat);
    if (copy == NULL) {
        ESP_LOGE(TAG, "Get conversation_id failed: strdup returned NULL");
        return ESP_ERR_NO_MEM;
    }
    *conversation_id = copy;
    return ESP_OK;
}

esp_err_t esp_coze_chat_send_audio_data(esp_coze_chat_handle_t chat_hd, char *data, int len)
{
    return esp_coze_proto_send_audio((coze_chat_t *)chat_hd, data, len);
}

esp_err_t esp_coze_chat_audio_data_clearup(esp_coze_chat_handle_t chat_hd)
{
    return esp_coze_proto_send_audio_clear((coze_chat_t *)chat_hd);
}

esp_err_t esp_coze_chat_send_audio_complete(esp_coze_chat_handle_t chat_hd)
{
    return esp_coze_proto_send_audio_complete((coze_chat_t *)chat_hd);
}

esp_err_t esp_coze_chat_send_audio_cancel(esp_coze_chat_handle_t chat_hd)
{
    return esp_coze_proto_send_audio_cancel((coze_chat_t *)chat_hd);
}

esp_err_t esp_coze_chat_update_chat(esp_coze_chat_handle_t chat_hd)
{
    return esp_coze_proto_send_update((coze_chat_t *)chat_hd);
}

esp_err_t esp_coze_chat_send_customer_data(esp_coze_chat_handle_t chat_hd, const char *data)
{
    ESP_RETURN_ON_FALSE(data != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid data pointer");
    return esp_coze_proto_send_raw((coze_chat_t *)chat_hd, data, (int)strlen(data));
}
