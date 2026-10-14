/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_llm_adapter_utils.h"
#include "esp_llm_baidu_internal.h"

static const char *TAG = "ESP_LLM_BAIDU";

static esp_llm_baidu_ctx_t *s_active_ctx;
/* Context borrowed by esp_llm_baidu_backend_acquire(). Tracked separately from
   s_active_ctx because destroy clears s_active_ctx before it waits for the
   borrow to be returned. */
static esp_llm_baidu_ctx_t *s_borrowed_ctx;
static portMUX_TYPE s_active_ctx_lock = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t own_strings(esp_llm_baidu_ctx_t *ctx, const char ***fields, size_t count)
{
    size_t size = 0;
    for (size_t i = 0; i < count; i++) {
        if (*fields[i] != NULL) {
            size_t len = strlen(*fields[i]) + 1;
            if (len > SIZE_MAX - size) {
                return ESP_ERR_INVALID_SIZE;
            }
            size += len;
        }
    }
    if (size == 0) {
        return ESP_OK;
    }
    ctx->config_strings = malloc(size);
    if (ctx->config_strings == NULL) {
        return ESP_ERR_NO_MEM;
    }
    char *dst = ctx->config_strings;
    for (size_t i = 0; i < count; i++) {
        if (*fields[i] != NULL) {
            size_t len = strlen(*fields[i]) + 1;
            memcpy(dst, *fields[i], len);
            *fields[i] = dst;
            dst += len;
        }
    }
    return ESP_OK;
}

static esp_err_t own_config_strings(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx->cfg.mode == ESP_LLM_BAIDU_MODE_INTERCOM) {
        const char **fields[] = {
            &ctx->cfg.intercom.app_id, &ctx->cfg.intercom.server_url,
            &ctx->cfg.intercom.room_name, &ctx->cfg.intercom.user_id,
            &ctx->cfg.intercom.display_name, &ctx->cfg.intercom.token,
            &ctx->cfg.intercom.cer,
        };
        return own_strings(ctx, fields, sizeof(fields) / sizeof(fields[0]));
    }
    const char **fields[] = {
        &ctx->cfg.agent.account.app_id, &ctx->cfg.agent.account.user_id,
        &ctx->cfg.agent.account.license_key, &ctx->cfg.agent.account.server_url,
        &ctx->cfg.agent.account.workflow, &ctx->cfg.agent.account.llm,
        &ctx->cfg.agent.account.lang, &ctx->cfg.agent.engine.config_json_raw,
        &ctx->cfg.agent.engine.cer, &ctx->cfg.agent.engine.dfda,
        &ctx->cfg.agent.prompt.vision_prompt, &ctx->cfg.agent.prompt.vision_prompt_cmd,
        &ctx->cfg.agent.prompt.vision_model_type, &ctx->cfg.agent.greeting_text,
        &ctx->cfg.agent.wakeup_interrupt_cmd,
    };
    return own_strings(ctx, fields, sizeof(fields) / sizeof(fields[0]));
}

static esp_err_t validate_mode_config(const esp_llm_baidu_ctx_t *ctx)
{
    if (ctx->cfg.mode == ESP_LLM_BAIDU_MODE_AGENT) {
        if (baidu_str_or(ctx->cfg.agent.account.app_id, NULL) == NULL ||
            baidu_str_or(ctx->cfg.agent.account.user_id, NULL) == NULL ||
            baidu_str_or(ctx->cfg.agent.account.server_url, NULL) == NULL) {
            ESP_LOGE(TAG, "Adapter create failed: agent app_id, user_id, or server_url is empty");
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }
    if (baidu_str_or(ctx->cfg.intercom.app_id, NULL) == NULL ||
        baidu_str_or(ctx->cfg.intercom.server_url, NULL) == NULL ||
        baidu_str_or(ctx->cfg.intercom.room_name, NULL) == NULL ||
        baidu_str_or(ctx->cfg.intercom.cer, NULL) == NULL) {
        ESP_LOGE(TAG, "Adapter create failed: intercom app_id, server_url, room_name, or cer is empty");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t baidu_activate_ctx(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->evt_group == NULL) {
        ESP_LOGE(TAG, "Activate context failed: context or event group is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    portENTER_CRITICAL(&s_active_ctx_lock);
    if (s_active_ctx != NULL) {
        ret = ESP_ERR_INVALID_STATE;
    } else {
        ctx->callback_refs = 0;
        ctx->callbacks_enabled = true;
        s_active_ctx = ctx;
    }
    portEXIT_CRITICAL(&s_active_ctx_lock);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Activate context failed: another Baidu adapter instance is active");
    }
    return ret;
}

esp_llm_baidu_ctx_t *baidu_callback_enter(void)
{
    esp_llm_baidu_ctx_t *ctx = NULL;

    portENTER_CRITICAL(&s_active_ctx_lock);
    if (s_active_ctx != NULL && s_active_ctx->callbacks_enabled) {
        ctx = s_active_ctx;
        ctx->callback_refs++;
    }
    portEXIT_CRITICAL(&s_active_ctx_lock);
    return ctx;
}

void baidu_callback_exit(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    bool notify = false;
    portENTER_CRITICAL(&s_active_ctx_lock);
    if (ctx->callback_refs > 0) {
        ctx->callback_refs--;
    }
    notify = !ctx->callbacks_enabled && ctx->callback_refs == 0;
    portEXIT_CRITICAL(&s_active_ctx_lock);

    if (notify && ctx->evt_group != NULL) {
        xEventGroupSetBits(ctx->evt_group, BAIDU_CALLBACK_DONE_BIT);
    }
}

void baidu_deactivate_ctx(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->evt_group == NULL) {
        return;
    }

    xEventGroupClearBits(ctx->evt_group, BAIDU_CALLBACK_DONE_BIT);

    bool wait_callbacks = false;
    portENTER_CRITICAL(&s_active_ctx_lock);
    if (s_active_ctx == ctx) {
        ctx->callbacks_enabled = false;
        s_active_ctx = NULL;
    }
    wait_callbacks = ctx->callback_refs > 0;
    portEXIT_CRITICAL(&s_active_ctx_lock);

    if (wait_callbacks) {
        (void)xEventGroupWaitBits(ctx->evt_group, BAIDU_CALLBACK_DONE_BIT,
                                  pdTRUE, pdTRUE, portMAX_DELAY);
    }
}

bool baidu_engine_lock(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine_mux == NULL) {
        return false;
    }
    return xSemaphoreTakeRecursive(ctx->engine_mux, portMAX_DELAY) == pdTRUE;
}

void baidu_engine_unlock(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx != NULL && ctx->engine_mux != NULL) {
        (void)xSemaphoreGiveRecursive(ctx->engine_mux);
    }
}

esp_err_t baidu_enter_control(esp_llm_service_t *service,
                              esp_llm_baidu_mode_t require_mode,
                              esp_llm_baidu_ctx_t **out_ctx)
{
    if (service == NULL || out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ctx = NULL;

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK ||
        state != ESP_SERVICE_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_baidu_ctx_t *ctx = NULL;
    esp_err_t ret = esp_llm_adapter_ctx_get(service, ESP_LLM_ADAPTER_BAIDU, (void **)&ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ctx->cfg.mode != require_mode) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!baidu_engine_lock(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }
    bool backend_up = ctx->connected &&
                      (require_mode == ESP_LLM_BAIDU_MODE_AGENT ? ctx->engine != NULL
                                                                : ctx->rtc_client != NULL);
    if (!backend_up) {
        baidu_engine_unlock(ctx);
        return ESP_ERR_INVALID_STATE;
    }
    *out_ctx = ctx;
    return ESP_OK;
}

void baidu_exit_control(esp_llm_baidu_ctx_t *ctx)
{
    baidu_engine_unlock(ctx);
}

esp_err_t baidu_emit(esp_llm_baidu_ctx_t *ctx,
                     esp_llm_event_common_id_t common_id,
                     const char *backend_event_name,
                     esp_llm_event_payload_type_t payload_type,
                     esp_llm_role_t role,
                     const void *payload,
                     size_t payload_len)
{
    if (ctx == NULL || ctx->event_cb == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_event_t event = {
        .common_id = common_id,
        .backend_event_name = backend_event_name,
        .payload_type = payload_type,
        .role = role,
        .last_payload = true,
        .payload = payload,
        .payload_len = payload_len,
    };
    return ctx->event_cb(ctx->event_ctx, &event);
}

esp_err_t baidu_feed_tts(esp_llm_baidu_ctx_t *ctx, const uint8_t *data, size_t len, bool eos)
{
    if (ctx == NULL || ctx->downlink_frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!eos && (data == NULL || len == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .data = eos ? NULL : (void *)data,
        .size = eos ? 0 : len,
        .flags = 0,
        .dts = ESP_LLM_PTS_ABSENT,
    };
    if (eos) {
        frame.flags |= ESP_MEDIA_FRAME_FLAG_EOS;
    }
    if (ctx->tts_stream_start) {
        frame.flags |= ESP_MEDIA_FRAME_FLAG_KEY;
        ctx->tts_stream_start = false;
        ctx->pcm_bytes = 0;
    }
    uint8_t channel = ctx->downlink_channel != 0 ? ctx->downlink_channel : 1U;
    uint8_t bits = ctx->downlink_bits >= 8U ? ctx->downlink_bits : 16U;
    uint32_t rate = ctx->downlink_rate != 0 ? ctx->downlink_rate : BAIDU_TTS_PCM_RATE;
    frame.pts = esp_llm_pts_from_pcm(rate, channel, bits, &ctx->pcm_bytes, frame.size);
    return ctx->downlink_frame(ctx->downlink_frame_ctx, &frame);
}

void baidu_snapshot_reset_locked(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    free(ctx->uplink_jpeg);
    ctx->uplink_jpeg = NULL;
    ctx->uplink_jpeg_size = 0;
}

void baidu_snapshot_release(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->snap_mux == NULL) {
        baidu_snapshot_reset_locked(ctx);
        return;
    }
    if (xSemaphoreTake(ctx->snap_mux, pdMS_TO_TICKS(BAIDU_SNAPSHOT_LOCK_MS)) != pdTRUE) {
        /* The uplink path still owns the buffer while send_video() runs; freeing it
           here would pull it out from under the SDK. The next snapshot or stop
           reclaims it instead. */
        ESP_LOGW(TAG, "Snapshot release: lock busy, deferring reset");
        return;
    }
    baidu_snapshot_reset_locked(ctx);
    xSemaphoreGive(ctx->snap_mux);
}

void baidu_on_video_data(esp_llm_baidu_ctx_t *ctx, const uint8_t *data, size_t len,
                         int img_type, int width, int height)
{
    if (ctx == NULL || data == NULL || len == 0 ||
        !baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        return;
    }
    if (ctx->downlink_frame == NULL) {
        ESP_LOGW(TAG, "Video output ignored: downlink frame callback is not set");
        return;
    }

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = (void *)data,
        .size = len,
        .pts = esp_llm_pts_from_video_fps(ctx->downlink_fps, &ctx->video_frames),
        .dts = ESP_LLM_PTS_ABSENT,
    };
    ESP_LOGI(TAG, "[VIDEO] Received: %zu bytes, %dx%d type=%d", len, width, height, img_type);
    esp_err_t ret = ctx->downlink_frame(ctx->downlink_frame_ctx, &frame);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Video output failed: %s", esp_err_to_name(ret));
    }
}

void baidu_ctx_cleanup(esp_llm_baidu_ctx_t *ctx, bool release_active)
{
    if (ctx == NULL) {
        return;
    }

    ctx->started = false;
    if (release_active) {
        baidu_deactivate_ctx(ctx);
    }
    if (ctx->mode_ops != NULL && ctx->mode_ops->stop != NULL) {
        esp_err_t ret = ctx->mode_ops->stop(ctx);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Cleanup mode stop failed: %s", esp_err_to_name(ret));
        }
    }

    baidu_snapshot_release(ctx);

    if (ctx->evt_group != NULL) {
        vEventGroupDelete(ctx->evt_group);
        ctx->evt_group = NULL;
    }
    if (ctx->snap_mux != NULL) {
        vSemaphoreDelete(ctx->snap_mux);
        ctx->snap_mux = NULL;
    }
    if (ctx->engine_mux != NULL) {
        vSemaphoreDelete(ctx->engine_mux);
        ctx->engine_mux = NULL;
    }
    free(ctx->query_text);
    ctx->query_text = NULL;
    free(ctx->config_strings);
    ctx->config_strings = NULL;

    free(ctx);
}

static esp_err_t baidu_adapter_create(const void *adapter_cfg, size_t adapter_cfg_size,
                                      const esp_llm_adapter_cbs_t *cbs, void **out_ctx)
{
    if (adapter_cfg == NULL || adapter_cfg_size < sizeof(esp_llm_baidu_cfg_t) ||
        cbs == NULL || out_ctx == NULL || cbs->event_cb == NULL) {
        ESP_LOGE(TAG, "Adapter create failed: invalid config or callbacks");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_llm_baidu_cfg_t *cfg = (const esp_llm_baidu_cfg_t *)adapter_cfg;
    const esp_llm_baidu_mode_ops_t *mode_ops = NULL;
    switch (cfg->mode) {
        case ESP_LLM_BAIDU_MODE_INTERCOM:
            mode_ops = &baidu_intercom_mode_ops;
            break;
        case ESP_LLM_BAIDU_MODE_AGENT:
            mode_ops = &baidu_agent_mode_ops;
            break;
        default:
            ESP_LOGE(TAG, "Adapter create failed: unsupported mode=%d", (int)cfg->mode);
            return ESP_ERR_INVALID_ARG;
    }

    *out_ctx = NULL;

    esp_llm_baidu_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Adapter create failed: memory allocation failed");
        return ESP_ERR_NO_MEM;
    }

    ctx->event_cb = cbs->event_cb;
    ctx->event_ctx = cbs->event_ctx;
    ctx->downlink_frame = cbs->downlink_frame;
    ctx->downlink_frame_ctx = cbs->downlink_frame_ctx;
    ctx->mode_ops = mode_ops;
    memcpy(&ctx->cfg, cfg, sizeof(ctx->cfg));
    ctx->cfg.service.name = NULL;
    ctx->cfg.service.audio_ai_cfg = (esp_llm_audio_ai_cfg_t){0};
    ctx->cfg.service.downlink_cb = NULL;
    ctx->cfg.service.downlink_cb_ctx = NULL;

    if (ctx->cfg.connect_timeout_ms == 0) {
        ctx->cfg.connect_timeout_ms = ESP_LLM_BAIDU_DEFAULT_CONNECT_TIMEOUT_MS;
    }

    esp_err_t ret = validate_mode_config(ctx);
    if (ret != ESP_OK) {
        baidu_ctx_cleanup(ctx, false);
        return ret;
    }
    ret = own_config_strings(ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Adapter create failed: copy config strings returned %s",
                 esp_err_to_name(ret));
        baidu_ctx_cleanup(ctx, false);
        return ret;
    }

    if (ctx->cfg.mode == ESP_LLM_BAIDU_MODE_AGENT) {
        if (ctx->cfg.agent.engine_stack_size == 0) {
            ctx->cfg.agent.engine_stack_size = ESP_LLM_BAIDU_DEFAULT_ENGINE_STACK;
        }
        if (ctx->cfg.agent.engine_priority == 0) {
            ctx->cfg.agent.engine_priority = ESP_LLM_BAIDU_DEFAULT_ENGINE_PRIO;
        }
        if (ctx->cfg.agent.prompt.query_buf_size == 0) {
            ctx->cfg.agent.prompt.query_buf_size = ESP_LLM_BAIDU_DEFAULT_QUERY_BUF_SIZE;
        }
        if (ctx->cfg.vision.vision_expire_sec == 0) {
            ctx->cfg.vision.vision_expire_sec = ESP_LLM_BAIDU_DEFAULT_VISION_EXPIRE_SEC;
        }
        baidu_resolve_scene(ctx->cfg.agent.scene.scene_type, &ctx->resolved_scene);

        ctx->query_text = calloc(1, (size_t)ctx->cfg.agent.prompt.query_buf_size);
        if (ctx->query_text == NULL) {
            ESP_LOGE(TAG, "Adapter create failed: query buffer allocation failed");
            baidu_ctx_cleanup(ctx, false);
            return ESP_ERR_NO_MEM;
        }
    }

    ret = baidu_validate_streams(&ctx->cfg,
                                 ctx->cfg.mode == ESP_LLM_BAIDU_MODE_AGENT ?
                                 &ctx->resolved_scene : NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Adapter create failed: stream configuration is not supported");
        baidu_ctx_cleanup(ctx, false);
        return ret;
    }

    ctx->evt_group = xEventGroupCreate();
    ctx->snap_mux = xSemaphoreCreateMutex();
    ctx->engine_mux = xSemaphoreCreateRecursiveMutex();
    if (ctx->evt_group == NULL || ctx->snap_mux == NULL || ctx->engine_mux == NULL) {
        ESP_LOGE(TAG, "Adapter create failed: synchronization object allocation failed");
        baidu_ctx_cleanup(ctx, false);
        return ESP_ERR_NO_MEM;
    }

    baidu_apply_downlink_audio_profile(ctx);
    ctx->tts_session_open = true;
    ret = baidu_activate_ctx(ctx);
    if (ret != ESP_OK) {
        baidu_ctx_cleanup(ctx, false);
        return ret;
    }
    *out_ctx = ctx;
    ESP_LOGI(TAG, "Create '%s': adapter context created", ESP_LLM_ADAPTER_BAIDU);
    return ESP_OK;
}

static esp_err_t baidu_adapter_destroy(void *adapter_ctx)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Adapter destroy failed: context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    baidu_ctx_cleanup(ctx, true);
    ESP_LOGI(TAG, "Destroy '%s': adapter context destroyed", ESP_LLM_ADAPTER_BAIDU);
    return ESP_OK;
}

static esp_err_t baidu_adapter_start(void *adapter_ctx)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->mode_ops == NULL) {
        ESP_LOGE(TAG, "Adapter start failed: context or mode operations are NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->started) {
        return ESP_OK;
    }

    esp_err_t ret = ctx->mode_ops->start(ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Adapter start failed: mode=%d returned %s",
                 (int)ctx->cfg.mode, esp_err_to_name(ret));
        return ret;
    }

    ctx->started = true;
    return ESP_OK;
}

static esp_err_t baidu_adapter_stop(void *adapter_ctx)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Adapter stop failed: context is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->mode_ops == NULL) {
        ESP_LOGE(TAG, "Adapter stop failed: mode operations are NULL");
        return ESP_ERR_INVALID_STATE;
    }
    if (!ctx->started) {
        return ESP_OK;
    }

    ctx->started = false;
    esp_err_t ret = ctx->mode_ops->stop(ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Adapter stop failed: mode=%d returned %s",
                 (int)ctx->cfg.mode, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t baidu_adapter_send_text(void *adapter_ctx, const char *text)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->mode_ops == NULL || ctx->mode_ops->send_text == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return ctx->mode_ops->send_text(ctx, text);
}

static esp_err_t baidu_adapter_send_frame(void *adapter_ctx, const esp_media_frame_t *frame)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->mode_ops == NULL || ctx->mode_ops->send_frame == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return ctx->mode_ops->send_frame(ctx, frame);
}

static esp_err_t baidu_adapter_send_custom_data(void *adapter_ctx, const char *data)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->mode_ops == NULL || ctx->mode_ops->send_custom_data == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ctx->mode_ops->send_custom_data(ctx, data);
}

static esp_err_t baidu_adapter_cancel_response(void *adapter_ctx)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)adapter_ctx;
    if (ctx == NULL || ctx->mode_ops == NULL || ctx->mode_ops->cancel_response == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ctx->mode_ops->cancel_response(ctx);
}

static const esp_llm_adapter_ops_t s_baidu_ops = {
    .create           = baidu_adapter_create,
    .destroy          = baidu_adapter_destroy,
    .start            = baidu_adapter_start,
    .stop             = baidu_adapter_stop,
    .send_text        = baidu_adapter_send_text,
    .send_frame       = baidu_adapter_send_frame,
    .send_custom_data = baidu_adapter_send_custom_data,
    .cancel_response  = baidu_adapter_cancel_response,
};

static const esp_llm_adapter_desc_t s_baidu_desc = {
    .api_version     = "1.0",
    .name            = ESP_LLM_ADAPTER_BAIDU,
    .capabilities    = ESP_LLM_BAIDU_CAPABILITIES,
    .ops             = &s_baidu_ops,
};

esp_err_t esp_llm_baidu_create(const esp_llm_baidu_cfg_t *cfg,
                               esp_llm_service_t **out_service)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_llm_service_cfg_t service_cfg = cfg->service;
    return esp_llm_service_create(&s_baidu_desc, cfg, sizeof(*cfg),
                                  &service_cfg, out_service);
}

esp_err_t esp_llm_baidu_send_text_to_tts(esp_llm_service_t *service, const char *text)
{
    if (text == NULL || text[0] == '\0') {
        ESP_LOGE(TAG, "Send TTS failed: text is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_baidu_ctx_t *ctx = NULL;
    esp_err_t ret = baidu_enter_control(service, ESP_LLM_BAIDU_MODE_AGENT, &ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send TTS failed: %s", esp_err_to_name(ret));
        return ret;
    }
    baidu_chat_agent_engine_send_text_to_TTS(ctx->engine, text);
    baidu_exit_control(ctx);
    return ESP_OK;
}

esp_err_t esp_llm_baidu_send_function_call_result(esp_llm_service_t *service,
                                                  const char *id,
                                                  const char *result)
{
    if (result == NULL || result[0] == '\0') {
        ESP_LOGE(TAG, "Send function call result failed: result is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_baidu_ctx_t *ctx = NULL;
    esp_err_t ret = baidu_enter_control(service, ESP_LLM_BAIDU_MODE_AGENT, &ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send function call result failed: %s", esp_err_to_name(ret));
        return ret;
    }
    /* fc_id is written under the same mutex by the FUNCTION_CALL callback. */
    const char *call_id = (id != NULL && id[0] != '\0') ? id : ctx->fc_id;
    if (call_id[0] == '\0') {
        baidu_exit_control(ctx);
        ESP_LOGE(TAG, "Send function call result failed: no call id is available");
        return ESP_ERR_INVALID_STATE;
    }
    baidu_chat_agent_engine_send_functioncall_result(ctx->engine, call_id, result);
    baidu_exit_control(ctx);
    return ESP_OK;
}

esp_err_t esp_llm_baidu_set_enhance_query(esp_llm_service_t *service,
                                          esp_llm_baidu_enhance_query_t type,
                                          const char *pre_query,
                                          const char *post_query)
{
    if (type < ESP_LLM_BAIDU_ENHANCE_QUERY_NONE || type > ESP_LLM_BAIDU_ENHANCE_QUERY_BOTHWAY) {
        ESP_LOGE(TAG, "Set enhance query failed: type %d is out of range", (int)type);
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_baidu_ctx_t *ctx = NULL;
    esp_err_t ret = baidu_enter_control(service, ESP_LLM_BAIDU_MODE_AGENT, &ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set enhance query failed: %s", esp_err_to_name(ret));
        return ret;
    }
    baidu_chat_agent_engine_set_enhance_query(ctx->engine, (int)type,
                                              pre_query != NULL ? pre_query : "",
                                              post_query != NULL ? post_query : "");
    baidu_exit_control(ctx);
    return ESP_OK;
}

esp_err_t esp_llm_baidu_backend_acquire(esp_llm_service_t *service,
                                        esp_llm_baidu_backend_t *out_backend)
{
    if (out_backend == NULL) {
        ESP_LOGE(TAG, "Acquire backend failed: out_backend is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (service == NULL) {
        ESP_LOGE(TAG, "Acquire backend failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK ||
        state != ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGE(TAG, "Acquire backend failed: service is not running");
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_active_ctx_lock);
    bool busy = s_borrowed_ctx != NULL;
    portEXIT_CRITICAL(&s_active_ctx_lock);
    if (busy) {
        ESP_LOGE(TAG, "Acquire backend failed: a borrow is already outstanding");
        return ESP_ERR_INVALID_STATE;
    }

    /* Pin the context first so destroy cannot complete while it is borrowed,
       then take the engine mutex so stop and reconnect cannot run either. */
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Acquire backend failed: adapter context is not active");
        return ESP_ERR_INVALID_STATE;
    }
    if (!baidu_engine_lock(ctx)) {
        baidu_callback_exit(ctx);
        ESP_LOGE(TAG, "Acquire backend failed: engine lock is unavailable");
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_active_ctx_lock);
    s_borrowed_ctx = ctx;
    portEXIT_CRITICAL(&s_active_ctx_lock);

    out_backend->mode = ctx->cfg.mode;
    out_backend->engine = ctx->engine;
    out_backend->rtc_client = ctx->rtc_client;
    return ESP_OK;
}

esp_err_t esp_llm_baidu_backend_release(esp_llm_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Release backend failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&s_active_ctx_lock);
    esp_llm_baidu_ctx_t *ctx = s_borrowed_ctx;
    s_borrowed_ctx = NULL;
    portEXIT_CRITICAL(&s_active_ctx_lock);
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Release backend failed: no borrow is outstanding");
        return ESP_ERR_INVALID_STATE;
    }
    baidu_engine_unlock(ctx);
    baidu_callback_exit(ctx);
    return ESP_OK;
}
