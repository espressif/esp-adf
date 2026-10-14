/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_tongyi_internal.h"
#include "esp_tongyi_utils.h"

static const char *TAG = "TONGYI";

#ifndef CONFIG_ESP_TONGYI_WS_SEND_TIMEOUT_MS
#define CONFIG_ESP_TONGYI_WS_SEND_TIMEOUT_MS 3000
#endif
#ifndef CONFIG_ESP_TONGYI_WS_RECONNECT_TIMEOUT_MS
#define CONFIG_ESP_TONGYI_WS_RECONNECT_TIMEOUT_MS 1000
#endif
#ifndef CONFIG_ESP_TONGYI_WS_CONNECT_TIMEOUT_MS
#define CONFIG_ESP_TONGYI_WS_CONNECT_TIMEOUT_MS 5000
#endif
#ifndef CONFIG_ESP_TONGYI_WS_BUFFER_SIZE
#define CONFIG_ESP_TONGYI_WS_BUFFER_SIZE 20480
#endif
#ifndef CONFIG_ESP_TONGYI_WS_NETWORK_TIMEOUT_MS
#define CONFIG_ESP_TONGYI_WS_NETWORK_TIMEOUT_MS 5000
#endif
#ifndef CONFIG_ESP_TONGYI_MAX_JSON_BYTES
#define CONFIG_ESP_TONGYI_MAX_JSON_BYTES 65536
#endif
#ifndef CONFIG_ESP_TONGYI_MAX_BINARY_BYTES
#define CONFIG_ESP_TONGYI_MAX_BINARY_BYTES 65536
#endif
#ifndef CONFIG_ESP_TONGYI_MAX_IMAGE_BYTES
#define CONFIG_ESP_TONGYI_MAX_IMAGE_BYTES 262144
#endif

static bool str_empty(const char *s)
{
    return s == NULL || s[0] == '\0';
}

static bool starts_with(const char *s, const char *prefix)
{
    return s != NULL && prefix != NULL && strncmp(s, prefix, strlen(prefix)) == 0;
}

static esp_err_t dup_str(char **dst, const char *src)
{
    *dst = NULL;
    if (src == NULL) {
        return ESP_OK;
    }
    *dst = strdup(src);
    return *dst != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t dup_or_generated_uuid(char **dst, const char *src, const char *what)
{
    if (!str_empty(src)) {
        *dst = strdup(src);
        if (*dst == NULL) {
            ESP_LOGE(TAG, "Create failed: %s allocation failed", what);
            return ESP_ERR_NO_MEM;
        }
        return ESP_OK;
    }

    char uuid[ESP_TONGYI_UUID_BUF_LEN];
    esp_err_t ret = esp_tongyi_generate_uuid(uuid, sizeof(uuid));
    if (ret != ESP_OK) {
        return ret;  /* esp_tongyi_generate_uuid already logged */
    }
    *dst = strdup(uuid);
    if (*dst == NULL) {
        ESP_LOGE(TAG, "Create failed: %s allocation failed", what);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static int resolve_positive(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

static size_t resolve_size(size_t value, size_t fallback)
{
    return value > 0 ? value : fallback;
}

static esp_err_t validate_biz_params(const char *json)
{
    if (str_empty(json)) {
        return ESP_OK;
    }
    cJSON *obj = cJSON_Parse(json);
    if (obj == NULL || !cJSON_IsObject(obj)) {
        cJSON_Delete(obj);
        ESP_LOGE(TAG, "Create failed: biz_params_json is not a JSON object");
        return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(obj);
    return ESP_OK;
}

static esp_err_t validate_cfg(const esp_tongyi_cfg_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *uri = cfg->transport.uri != NULL ? cfg->transport.uri : ESP_TONGYI_DEFAULT_WS_BASE_URL;
    if (!starts_with(uri, "ws://") && !starts_with(uri, "wss://")) {
        ESP_LOGE(TAG, "Create failed: uri must start with ws:// or wss://");
        return ESP_ERR_INVALID_ARG;
    }
    if (str_empty(cfg->api_key) || str_empty(cfg->workspace_id) || str_empty(cfg->app_id)) {
        ESP_LOGE(TAG, "Create failed: api_key, workspace_id, and app_id are required");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->keep_alive_time_sec > 3600U) {
        ESP_LOGE(TAG, "Create failed: keep_alive_time_sec out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->upstream.sample_rate < 0 || cfg->upstream.sample_rate > 48000 ||
        cfg->downstream.sample_rate < 0 || cfg->downstream.sample_rate > 48000) {
        ESP_LOGE(TAG, "Create failed: sample_rate out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->downstream.volume < 0 || cfg->downstream.volume > 100) {
        ESP_LOGE(TAG, "Create failed: volume out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->transport.buffer_size < 0 || cfg->transport.buffer_size > 131072) {
        ESP_LOGE(TAG, "Create failed: buffer_size out of range");
        return ESP_ERR_INVALID_ARG;
    }
    return validate_biz_params(cfg->biz_params_json);
}

void esp_tongyi_owned_cfg_deinit(esp_tongyi_owned_cfg_t *cfg)
{
    if (cfg == NULL) {
        return;
    }
    free(cfg->api_key);
    free(cfg->workspace_id);
    free(cfg->app_id);
    free(cfg->task_id);
    free(cfg->dialog_id);
    free(cfg->biz_params_json);
    free(cfg->uri);
    free(cfg->upstream.type);
    free(cfg->upstream.mode);
    free(cfg->upstream.audio_format);
    free(cfg->downstream.voice);
    free(cfg->downstream.audio_format);
    free(cfg->client.user_id);
    free(cfg->client.device_uuid);
    memset(cfg, 0, sizeof(*cfg));
}

static esp_err_t owned_cfg_init(esp_tongyi_owned_cfg_t *dst, const esp_tongyi_cfg_t *src)
{
    memset(dst, 0, sizeof(*dst));

#define DUP_OR_FAIL(field, src_field) do {                                 \
        if (dup_str(&(field), (src_field)) != ESP_OK) {                    \
            ESP_LOGE(TAG, "Create failed: string copy allocation failed"); \
            esp_tongyi_owned_cfg_deinit(dst);                              \
            return ESP_ERR_NO_MEM;                                         \
        }                                                                  \
    } while (0)

    DUP_OR_FAIL(dst->api_key, src->api_key);
    DUP_OR_FAIL(dst->workspace_id, src->workspace_id);
    DUP_OR_FAIL(dst->app_id, src->app_id);
    DUP_OR_FAIL(dst->dialog_id, src->dialog_id);
    DUP_OR_FAIL(dst->biz_params_json, src->biz_params_json);
    DUP_OR_FAIL(dst->uri, src->transport.uri != NULL ? src->transport.uri : ESP_TONGYI_DEFAULT_WS_BASE_URL);
    DUP_OR_FAIL(dst->upstream.type, src->upstream.type);
    DUP_OR_FAIL(dst->upstream.mode, src->upstream.mode);
    DUP_OR_FAIL(dst->upstream.audio_format, src->upstream.audio_format);
    DUP_OR_FAIL(dst->downstream.voice, src->downstream.voice);
    DUP_OR_FAIL(dst->downstream.audio_format, src->downstream.audio_format);
    DUP_OR_FAIL(dst->client.user_id, src->client.user_id);
    DUP_OR_FAIL(dst->client.device_uuid, src->client.device_uuid);

#undef DUP_OR_FAIL

    esp_err_t ret = dup_or_generated_uuid(&dst->task_id, src->task_id, "task_id");
    if (ret != ESP_OK) {
        esp_tongyi_owned_cfg_deinit(dst);
        return ret;
    }

    dst->keep_alive_time_sec = src->keep_alive_time_sec;
    dst->disconnect_after_stop = src->disconnect_after_stop;
    dst->event_cb = src->event_cb;
    dst->event_ctx = src->event_ctx;
    dst->buffer_size = resolve_positive(src->transport.buffer_size, CONFIG_ESP_TONGYI_WS_BUFFER_SIZE);
    dst->send_timeout_ms = resolve_positive(src->transport.send_timeout_ms, CONFIG_ESP_TONGYI_WS_SEND_TIMEOUT_MS);
    dst->connect_timeout_ms = resolve_positive(src->transport.connect_timeout_ms, CONFIG_ESP_TONGYI_WS_CONNECT_TIMEOUT_MS);
    dst->network_timeout_ms = resolve_positive(src->transport.network_timeout_ms, CONFIG_ESP_TONGYI_WS_NETWORK_TIMEOUT_MS);
    dst->reconnect_timeout_ms = resolve_positive(src->transport.reconnect_timeout_ms,
                                                 CONFIG_ESP_TONGYI_WS_RECONNECT_TIMEOUT_MS);
    dst->task = src->transport.task;
    if (dst->task.stack_size <= 0) {
        dst->task.stack_size = ESP_TONGYI_DEFAULT_TASK_STACK;
    }
    if (dst->task.priority <= 0) {
        dst->task.priority = ESP_TONGYI_DEFAULT_TASK_PRIO;
    }
    dst->reconnect = src->reconnect;
    if (dst->reconnect.delay_ms == 0) {
        dst->reconnect.delay_ms = ESP_TONGYI_DEFAULT_RECONNECT_DELAY_MS;
    }
    dst->limits.max_json_bytes = resolve_size(src->limits.max_json_bytes, CONFIG_ESP_TONGYI_MAX_JSON_BYTES);
    dst->limits.max_binary_bytes = resolve_size(src->limits.max_binary_bytes, CONFIG_ESP_TONGYI_MAX_BINARY_BYTES);
    dst->limits.max_image_bytes = resolve_size(src->limits.max_image_bytes, CONFIG_ESP_TONGYI_MAX_IMAGE_BYTES);
    dst->upstream.sample_rate = src->upstream.sample_rate;
    dst->downstream.sample_rate = src->downstream.sample_rate;
    dst->downstream.frame_size = src->downstream.frame_size;
    dst->downstream.volume = src->downstream.volume;
    dst->downstream.bit_rate = src->downstream.bit_rate;
    dst->downstream.transmit_rate_limit = src->downstream.transmit_rate_limit;
    return ESP_OK;
}

static void wait_callbacks(esp_tongyi_t *inst)
{
    for (int i = 0; i < 100 && atomic_load(&inst->in_callback) != 0; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void instance_free(esp_tongyi_t *inst)
{
    if (inst == NULL) {
        return;
    }
    atomic_store(&inst->closing, true);
    if (inst->event_group != NULL) {
        xEventGroupSetBits(inst->event_group, ESP_TONGYI_BIT_FAIL | ESP_TONGYI_BIT_DISCONNECTED);
    }
    esp_tongyi_session_ctrl_stop(inst);
    if (inst->session_sem != NULL) {
        xSemaphoreTake(inst->session_sem, portMAX_DELAY);
        xSemaphoreGive(inst->session_sem);
    }
    esp_tongyi_transport_destroy(inst);
    wait_callbacks(inst);
    if (inst->event_group != NULL) {
        vEventGroupDelete(inst->event_group);
    }
    if (inst->session_sem != NULL) {
        vSemaphoreDelete(inst->session_sem);
    }
    if (inst->lock != NULL) {
        vSemaphoreDelete(inst->lock);
    }
    free(inst->dialog_id);
    free(inst->saved_dialog_id);
    esp_tongyi_owned_cfg_deinit(&inst->cfg);
    free(inst);
}

esp_err_t esp_tongyi_create(const esp_tongyi_cfg_t *cfg, esp_tongyi_handle_t *out_handle)
{
    if (out_handle == NULL) {
        ESP_LOGE(TAG, "Create failed: out_handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = NULL;
    esp_err_t ret = validate_cfg(cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_tongyi_t *inst = (esp_tongyi_t *)calloc(1, sizeof(*inst));
    if (inst == NULL) {
        ESP_LOGE(TAG, "Create failed: allocation failed");
        return ESP_ERR_NO_MEM;
    }
    atomic_init(&inst->ready, false);
    atomic_init(&inst->closing, false);
    atomic_init(&inst->user_stopped, false);
    atomic_init(&inst->reconnect_pending, false);
    atomic_init(&inst->in_callback, 0);
    inst->state = ESP_TONGYI_STATE_IDLE;

    ret = owned_cfg_init(&inst->cfg, cfg);
    if (ret != ESP_OK) {
        free(inst);
        return ret;  /* owned_cfg_init already logged */
    }

    inst->lock = xSemaphoreCreateMutex();
    inst->session_sem = xSemaphoreCreateMutex();
    inst->event_group = xEventGroupCreate();
    if (inst->lock == NULL || inst->session_sem == NULL || inst->event_group == NULL) {
        ESP_LOGE(TAG, "Create failed: sync object allocation failed");
        instance_free(inst);
        return ESP_ERR_NO_MEM;
    }

    ret = esp_tongyi_transport_open(inst);
    if (ret != ESP_OK) {
        instance_free(inst);
        return ret;  /* transport_open already logged */
    }
    ret = esp_tongyi_session_ctrl_start(inst);
    if (ret != ESP_OK) {
        instance_free(inst);
        return ret;  /* session_ctrl_start already logged */
    }

    *out_handle = inst;
    ESP_LOGI(TAG, "Create 'tongyi': initialized");
    return ESP_OK;
}

esp_err_t esp_tongyi_destroy(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Destroy failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_tongyi_t *inst = handle;
    atomic_store(&inst->ready, false);
    instance_free(inst);
    ESP_LOGI(TAG, "Destroy 'tongyi': deinitialized");
    return ESP_OK;
}

esp_err_t esp_tongyi_start(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Start failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return esp_tongyi_session_start(handle);
}

esp_err_t esp_tongyi_stop(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Stop failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return esp_tongyi_session_stop(handle, true);
}

esp_err_t esp_tongyi_send_audio(esp_tongyi_handle_t handle, const uint8_t *audio_data, size_t audio_data_len)
{
    if (handle == NULL || audio_data == NULL) {
        ESP_LOGE(TAG, "Send audio failed: handle or audio_data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!atomic_load(&handle->ready) || atomic_load(&handle->closing)) {
        ESP_LOGE(TAG, "Send audio failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    return esp_tongyi_transport_send_bin(&handle->transport, audio_data, audio_data_len);
}

esp_err_t esp_tongyi_send_directive(esp_tongyi_handle_t handle, esp_tongyi_speech_action_t action)
{
    return esp_tongyi_session_send_directive(handle, action, false);
}

esp_err_t esp_tongyi_post_directive(esp_tongyi_handle_t handle, esp_tongyi_speech_action_t action)
{
    return esp_tongyi_session_send_directive(handle, action, true);
}

esp_err_t esp_tongyi_send_text(esp_tongyi_handle_t handle, esp_tongyi_text_kind_t kind, const char *text)
{
    if (handle == NULL || text == NULL) {
        ESP_LOGE(TAG, "Send text failed: handle or text is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!atomic_load(&handle->ready)) {
        ESP_LOGE(TAG, "Send text failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    char dialog_id[ESP_TONGYI_ID_MAX];
    esp_err_t ret = esp_tongyi_session_snapshot_dialog_id(handle, dialog_id, sizeof(dialog_id));
    if (ret != ESP_OK) {
        return ret;
    }
    char *json = esp_tongyi_proto_build_text(handle->cfg.task_id, dialog_id, text,
                                             kind == ESP_TONGYI_TEXT_TRANSCRIPT);
    return esp_tongyi_session_send_json(handle, json);
}

esp_err_t esp_tongyi_connect_video_channel(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Connect video channel failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!atomic_load(&handle->ready)) {
        ESP_LOGE(TAG, "Connect video channel failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    char dialog_id[ESP_TONGYI_ID_MAX];
    esp_err_t ret = esp_tongyi_session_snapshot_dialog_id(handle, dialog_id, sizeof(dialog_id));
    if (ret != ESP_OK) {
        return ret;
    }
    char *json = esp_tongyi_proto_build_video_channel(handle->cfg.task_id, dialog_id);
    return esp_tongyi_session_send_json(handle, json);
}

esp_err_t esp_tongyi_send_image(esp_tongyi_handle_t handle, const esp_tongyi_image_req_t *req)
{
    if (handle == NULL || req == NULL || req->data == NULL || req->data_len == 0) {
        ESP_LOGE(TAG, "Send image failed: invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    if (req->action == ESP_TONGYI_IMAGE_REQUEST_RESPONSE && req->prompt == NULL) {
        ESP_LOGE(TAG, "Send image failed: prompt is required");
        return ESP_ERR_INVALID_ARG;
    }
    if (req->data_len > handle->cfg.limits.max_image_bytes) {
        ESP_LOGE(TAG, "Send image failed: image_len=%zu exceeds max=%zu",
                 req->data_len, handle->cfg.limits.max_image_bytes);
        return ESP_ERR_INVALID_SIZE;
    }
    if (!atomic_load(&handle->ready)) {
        ESP_LOGE(TAG, "Send image failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }

    size_t base64_len = 0;
    esp_err_t ret = esp_tongyi_base64_encoded_size(req->data_len, &base64_len);
    if (ret != ESP_OK) {
        return ret;  /* esp_tongyi_base64_encoded_size already logged */
    }
    char *base64_image = (char *)malloc(base64_len);
    if (base64_image == NULL) {
        ESP_LOGE(TAG, "Send image failed: base64 buffer allocation failed, size=%zu", base64_len);
        return ESP_ERR_NO_MEM;
    }
    ret = esp_tongyi_base64_encode(req->data, req->data_len, base64_image, base64_len);
    if (ret != ESP_OK) {
        free(base64_image);
        return ret;  /* esp_tongyi_base64_encode already logged */
    }

    char dialog_id[ESP_TONGYI_ID_MAX];
    ret = esp_tongyi_session_snapshot_dialog_id(handle, dialog_id, sizeof(dialog_id));
    if (ret != ESP_OK) {
        free(base64_image);
        return ret;
    }
    char *json = esp_tongyi_proto_build_image(handle->cfg.task_id, dialog_id, base64_image,
                                              req->prompt,
                                              req->action == ESP_TONGYI_IMAGE_UPDATE_INFO);
    ret = esp_tongyi_session_send_json(handle, json);
    free(base64_image);
    return ret;
}

esp_err_t esp_tongyi_send_payload_json(esp_tongyi_handle_t handle, const char *payload_json)
{
    if (handle == NULL || str_empty(payload_json)) {
        ESP_LOGE(TAG, "Send payload failed: invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    if (!atomic_load(&handle->ready)) {
        ESP_LOGE(TAG, "Send payload failed: session is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    char *json = esp_tongyi_proto_build_payload_wrap(handle->cfg.task_id, payload_json);
    return esp_tongyi_session_send_json(handle, json);
}

static esp_err_t copy_str_to_buf(const char *src, char *buf, size_t buf_len)
{
    if (src == NULL) {
        ESP_LOGE(TAG, "Copy string failed: source is unavailable");
        return ESP_ERR_INVALID_STATE;
    }
    if (buf == NULL || buf_len == 0) {
        ESP_LOGE(TAG, "Copy string failed: buffer is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    size_t n = strlen(src);
    if (n + 1U > buf_len) {
        ESP_LOGE(TAG, "Copy string failed: buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(buf, src, n + 1U);
    return ESP_OK;
}

esp_err_t esp_tongyi_get_task_id(esp_tongyi_handle_t handle, char *buf, size_t buf_len)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Get task id failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return copy_str_to_buf(handle->cfg.task_id, buf, buf_len);
}

esp_err_t esp_tongyi_get_dialog_id(esp_tongyi_handle_t handle, char *buf, size_t buf_len)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Get dialog id failed: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = esp_tongyi_session_snapshot_dialog_id(handle, buf, buf_len);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Get dialog id failed: dialog id is unavailable");
    }
    return ret;
}

esp_err_t esp_tongyi_is_connected(esp_tongyi_handle_t handle, bool *out_connected)
{
    if (handle == NULL || out_connected == NULL) {
        ESP_LOGE(TAG, "Get connected failed: handle or output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return esp_tongyi_transport_is_connected(&handle->transport, out_connected);
}

esp_err_t esp_tongyi_is_ready(esp_tongyi_handle_t handle, bool *out_ready)
{
    if (handle == NULL || out_ready == NULL) {
        ESP_LOGE(TAG, "Get ready failed: handle or output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_ready = atomic_load(&handle->ready);
    return ESP_OK;
}

esp_tongyi_state_t esp_tongyi_get_state(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        return ESP_TONGYI_STATE_IDLE;
    }
    xSemaphoreTake(handle->lock, portMAX_DELAY);
    esp_tongyi_state_t state = handle->state;
    xSemaphoreGive(handle->lock);
    return state;
}

esp_tongyi_dialog_state_t esp_tongyi_get_dialog_state(esp_tongyi_handle_t handle)
{
    if (handle == NULL) {
        return ESP_TONGYI_DIALOG_STATE_IDLE;
    }
    return handle->dialog_state;
}
