/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_llm_adapter_utils.h"
#include "esp_log.h"

#include "esp_llm_mock.h"

static const char *TAG = "ESP_LLM_MOCK";

typedef struct {
    esp_llm_event_dispatch_cb_t     event_cb;
    void                           *event_ctx;
    esp_llm_downlink_frame_cb_t     downlink_frame;
    void                           *downlink_frame_ctx;
    esp_llm_mock_cfg_t              cfg;
    bool                            started;
    uint64_t                        pcm_bytes;
} esp_llm_mock_ctx_t;

static esp_err_t mock_emit_common(esp_llm_mock_ctx_t *ctx,
                                  esp_llm_event_common_id_t common_id,
                                  esp_llm_event_payload_type_t payload_type,
                                  esp_llm_role_t role,
                                  const char *backend_event_name,
                                  const void *payload,
                                  size_t payload_len,
                                  bool final)
{
    if (ctx->event_cb == NULL) {
        return ESP_OK;
    }
    esp_llm_event_t event = {
        .common_id = common_id,
        .backend_event_name = backend_event_name,
        .payload_type = payload_type,
        .role = role,
        .last_payload = final,
        .payload = payload,
        .payload_len = payload_len,
    };
    return ctx->event_cb(ctx->event_ctx, &event);
}

static esp_err_t mock_emit_raw(esp_llm_mock_ctx_t *ctx,
                               const char *backend_event_name,
                               esp_llm_event_payload_type_t payload_type,
                               esp_llm_role_t role,
                               const void *payload,
                               size_t payload_len)
{
    if (ctx->event_cb == NULL) {
        return ESP_OK;
    }
    esp_llm_event_t event = {
        .common_id = ESP_LLM_EVENT_NONE,
        .backend_event_name = backend_event_name,
        .payload_type = payload_type,
        .role = role,
        .last_payload = true,
        .payload = payload,
        .payload_len = payload_len,
    };
    return ctx->event_cb(ctx->event_ctx, &event);
}

static esp_err_t mock_emit_audio(esp_llm_mock_ctx_t *ctx)
{
    static const uint8_t s_audio_bytes[] = {
        0x00, 0x00, 0x18, 0x00, 0x30, 0x00, 0x18, 0x00,
    };
    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = (void *)s_audio_bytes,
        .size = sizeof(s_audio_bytes),
        .flags = ESP_MEDIA_FRAME_FLAG_KEY,
        .dts = ESP_LLM_PTS_ABSENT,
    };
    ctx->pcm_bytes = 0;
    frame.pts = esp_llm_pts_from_pcm(16000, 1, 16, &ctx->pcm_bytes, frame.size);
    if (ctx->downlink_frame != NULL) {
        return ctx->downlink_frame(ctx->downlink_frame_ctx, &frame);
    }
    return mock_emit_raw(ctx, "AUDIO_DATA", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY,
                            ESP_LLM_ROLE_ASSISTANT, frame.data, frame.size);
}

static esp_err_t mock_create(const void *adapter_cfg, size_t adapter_cfg_size,
                             const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(esp_llm_mock_cfg_t) ||
        cbs == NULL || out_ctx == NULL) {
        ESP_LOGE(TAG, "Create failed: invalid config or callbacks");
        return ESP_ERR_INVALID_ARG;
    }
    if (cbs->event_cb == NULL) {
        ESP_LOGE(TAG, "Create failed: event callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_mock_ctx_t *ctx = (esp_llm_mock_ctx_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Create failed: context allocation failed");
        return ESP_ERR_NO_MEM;
    }

    ctx->event_cb = cbs->event_cb;
    ctx->event_ctx = cbs->event_ctx;
    ctx->downlink_frame = cbs->downlink_frame;
    ctx->downlink_frame_ctx = cbs->downlink_frame_ctx;
    memcpy(&ctx->cfg, adapter_cfg, sizeof(ctx->cfg));
    ctx->cfg.service = (esp_llm_service_cfg_t){0};

    *out_ctx = ctx;
    return ESP_OK;
}

static esp_err_t mock_destroy(void *ctx)
{
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Destroy failed: context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    free(ctx);
    return ESP_OK;
}

static esp_err_t mock_start(void *ctx)
{
    esp_llm_mock_ctx_t *mock = (esp_llm_mock_ctx_t *)ctx;
    if (mock == NULL) {
        ESP_LOGE(TAG, "Start failed: context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    mock->started = true;

    if (!mock->cfg.emit_start_events) {
        return ESP_OK;
    }

    esp_err_t ret = mock_emit_common(mock, ESP_LLM_EVENT_CONNECTED, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                     ESP_LLM_ROLE_NONE, "CONNECTED", NULL, 0, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: connected event returned %s", esp_err_to_name(ret));
        return ret;
    }

    const char *text = "mock assistant text";
    ret = mock_emit_common(mock, ESP_LLM_EVENT_TEXT, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                           ESP_LLM_ROLE_ASSISTANT, "TEXT",
                           text, strlen(text), true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: text event returned %s", esp_err_to_name(ret));
        return ret;
    }

    const char *function_call = "{\"name\":\"mock_tool\",\"arguments\":{\"value\":1}}";
    ret = mock_emit_raw(mock, "FUNCTION_CALL",
                        ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, ESP_LLM_ROLE_TOOL,
                        function_call, strlen(function_call));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: raw event returned %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mock_emit_audio(mock);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: audio event returned %s", esp_err_to_name(ret));
        return ret;
    }

    return mock_emit_common(mock, ESP_LLM_EVENT_FINISHED, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                            ESP_LLM_ROLE_ASSISTANT, "FINISHED", NULL, 0, true);
}

static esp_err_t mock_stop(void *ctx)
{
    esp_llm_mock_ctx_t *mock = (esp_llm_mock_ctx_t *)ctx;
    if (mock == NULL) {
        ESP_LOGE(TAG, "Stop failed: context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    mock->started = false;
    return mock_emit_common(mock, ESP_LLM_EVENT_DISCONNECTED, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                            ESP_LLM_ROLE_NONE, "DISCONNECTED", NULL, 0, true);
}

static esp_err_t mock_send_text(void *ctx, const char *text)
{
    esp_llm_mock_ctx_t *mock = (esp_llm_mock_ctx_t *)ctx;
    if (mock == NULL || text == NULL) {
        ESP_LOGE(TAG, "Send text failed: context or text is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = mock_emit_common(mock, ESP_LLM_EVENT_TEXT, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                     ESP_LLM_ROLE_USER, "SEND_TEXT",
                                     text, strlen(text), true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send text failed: user event returned %s", esp_err_to_name(ret));
        return ret;
    }
    const char *reply = "mock echo reply";
    return mock_emit_common(mock, ESP_LLM_EVENT_TEXT, ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                            ESP_LLM_ROLE_ASSISTANT, "ECHO_TEXT",
                            reply, strlen(reply), true);
}

static esp_err_t mock_send_frame(void *ctx, const esp_media_frame_t *frame)
{
    esp_llm_mock_ctx_t *mock = (esp_llm_mock_ctx_t *)ctx;
    if (mock == NULL || frame == NULL) {
        ESP_LOGE(TAG, "Send frame failed: context or frame is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO) {
        ESP_LOGW(TAG, "Send frame: only audio loopback is supported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(TAG, "Send frame: loopback %u bytes", (unsigned)frame->size);
    if (mock->downlink_frame != NULL) {
        esp_media_frame_t out = *frame;
        if (out.pts == ESP_LLM_PTS_ABSENT) {
            out.pts = esp_llm_pts_from_pcm(16000, 1, 16, &mock->pcm_bytes, out.size);
        }
        out.dts = ESP_LLM_PTS_ABSENT;
        return mock->downlink_frame(mock->downlink_frame_ctx, &out);
    }
    return mock_emit_raw(mock, "AUDIO_DATA", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY,
                         ESP_LLM_ROLE_ASSISTANT, frame->data, frame->size);
}

#define ESP_LLM_MOCK_CAPABILITIES  ((esp_llm_capability_t)(    \
                                    ESP_LLM_CAP_TEXT_INPUT |   \
                                    ESP_LLM_CAP_TEXT_OUTPUT |  \
                                    ESP_LLM_CAP_AUDIO_INPUT |  \
                                    ESP_LLM_CAP_AUDIO_OUTPUT | \
                                    ESP_LLM_CAP_FUNCTION_CALL))

static const esp_llm_adapter_ops_t s_mock_ops = {
    .create = mock_create,
    .destroy = mock_destroy,
    .start = mock_start,
    .stop = mock_stop,
    .send_text = mock_send_text,
    .send_frame = mock_send_frame,
    .send_custom_data = NULL,
};

static const esp_llm_adapter_desc_t s_mock_desc = {
    .api_version = "1.0",
    .name = ESP_LLM_ADAPTER_MOCK,
    .capabilities = ESP_LLM_MOCK_CAPABILITIES,
    .ops = &s_mock_ops,
};

esp_err_t esp_llm_mock_create(const esp_llm_mock_cfg_t *cfg,
                              esp_llm_service_t **out_service)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_cfg_t service_cfg = cfg->service;
    return esp_llm_service_create(&s_mock_desc, cfg, sizeof(*cfg),
                                  &service_cfg, out_service);
}
