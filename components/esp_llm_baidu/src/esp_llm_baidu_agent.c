/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "sys/lock.h"

#include "esp_log.h"

#include "esp_llm_baidu_internal.h"

static const char *TAG = "ESP_LLM_BAIDU_AGENT";

#define BAIDU_RECONNECT_TASK_STACK  (8192U)

static _lock_t s_reconnect_lock;

static bool jpeg_is_complete(const uint8_t *data, size_t size)
{
    return data != NULL && size >= 4 && data[0] == 0xFF && data[1] == 0xD8 &&
           data[size - 2] == 0xFF && data[size - 1] == 0xD9;
}

static int vision_mode_to_sdk(esp_llm_baidu_vision_mode_t mode)
{
    switch (mode) {
        case ESP_LLM_BAIDU_VISION_MODE_IMAGE:
            return VISION_MODE_IMAGE;
        case ESP_LLM_BAIDU_VISION_MODE_STREAM:
            return VISION_MODE_STREAM;
        default:
            return -1;
    }
}

static int effective_vision_sdk_mode(const esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->cfg.agent.scene.scene_type == ESP_LLM_BAIDU_SCENE_VOICE_CHAT) {
        return -1;
    }
    return vision_mode_to_sdk(ctx->cfg.vision.vision_mode);
}

static int effective_vision_expire_sec(const esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || effective_vision_sdk_mode(ctx) < 0) {
        return 0;
    }
    return ctx->cfg.vision.vision_expire_sec;
}

static Region region_to_sdk(esp_llm_baidu_region_t region)
{
    switch (region) {
        case ESP_LLM_BAIDU_REGION_BD_DEV:
            return REGION_BD_DEV;
        case ESP_LLM_BAIDU_REGION_EUROPE:
            return REGION_EUROPE;
        case ESP_LLM_BAIDU_REGION_AMERICA:
            return REGION_AMERICA;
        case ESP_LLM_BAIDU_REGION_OVERSEAS:
            return REGION_OVERSEAS;
        case ESP_LLM_BAIDU_REGION_GLOBAL:
            return REGION_GLOBAL;
        default:
            return REGION_MAINLAND;
    }
}

static void send_agent_event(esp_llm_baidu_ctx_t *ctx, const char *event)
{
    if (ctx == NULL || event == NULL || !baidu_engine_lock(ctx)) {
        return;
    }
    if (ctx->engine != NULL) {
        baidu_chat_agent_engine_send_event_to_agent(ctx->engine, event);
    }
    baidu_engine_unlock(ctx);
}

static esp_err_t update_vision_prompt(esp_llm_baidu_ctx_t *ctx, const char *prompt)
{
    if (ctx == NULL || ctx->query_text == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const char *cmd = ctx->cfg.agent.prompt.vision_prompt_cmd != NULL ?
                      ctx->cfg.agent.prompt.vision_prompt_cmd : ESP_LLM_BAIDU_VISION_PROMPT_CMD;
    const char *model = ctx->cfg.agent.prompt.vision_model_type != NULL ?
                        ctx->cfg.agent.prompt.vision_model_type : ESP_LLM_BAIDU_DEFAULT_VISION_MODEL_TYPE;
    const char *prompt_str = prompt != NULL ? prompt : "";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    int written = snprintf(ctx->query_text, (size_t)ctx->cfg.agent.prompt.query_buf_size, cmd,
                           AGENT_EVENT_UPDATE_SYSTEM_PROMPT, model, prompt_str);
#pragma GCC diagnostic pop
    if (written < 0 || written >= ctx->cfg.agent.prompt.query_buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    send_agent_event(ctx, ctx->query_text);
    return ESP_OK;
}

static esp_err_t setup_scene(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_llm_baidu_resolved_scene_t *scene = &ctx->resolved_scene;
    int sdk_mode = effective_vision_sdk_mode(ctx);
    int expire_sec = effective_vision_expire_sec(ctx);

    switch (ctx->cfg.agent.scene.scene_type) {
        case ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING: {
            const char *prompt = ctx->cfg.agent.prompt.vision_prompt != NULL ?
                                 ctx->cfg.agent.prompt.vision_prompt : ESP_LLM_BAIDU_OBJECT_VISION_PROMPT;
            esp_err_t ret = update_vision_prompt(ctx, prompt);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Scene setup failed: vision prompt update returned %s", esp_err_to_name(ret));
                return ret;
            }
            send_agent_event(ctx, AGENT_EVENT_DISABLE_MEDIA_GENERATE);
            if (!baidu_engine_lock(ctx)) {
                return ESP_ERR_INVALID_STATE;
            }
            if (ctx->engine == NULL) {
                baidu_engine_unlock(ctx);
                return ESP_ERR_INVALID_STATE;
            }
            baidu_chat_agent_engine_update_visual_mode(ctx->engine, sdk_mode, expire_sec);
            baidu_engine_unlock(ctx);
            break;
        }
        case ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION:
            if (!baidu_engine_lock(ctx)) {
                return ESP_ERR_INVALID_STATE;
            }
            if (ctx->engine == NULL) {
                baidu_engine_unlock(ctx);
                return ESP_ERR_INVALID_STATE;
            }
            baidu_chat_agent_engine_update_visual_mode(ctx->engine, sdk_mode, expire_sec);
            baidu_engine_unlock(ctx);
            send_agent_event(ctx, AGENT_EVENT_ENABLE_MEDIA_GENERATE);
            ESP_LOGI(TAG, "[SCENE] Image generation: ENABLE_MEDIA_GENERATE");
            break;
        case ESP_LLM_BAIDU_SCENE_MULTI_MODAL:
            if (!baidu_engine_lock(ctx)) {
                return ESP_ERR_INVALID_STATE;
            }
            if (ctx->engine == NULL) {
                baidu_engine_unlock(ctx);
                return ESP_ERR_INVALID_STATE;
            }
            baidu_chat_agent_engine_update_visual_mode(ctx->engine, sdk_mode, expire_sec);
            baidu_engine_unlock(ctx);
            if (scene->enable_image_generate) {
                send_agent_event(ctx, AGENT_EVENT_ENABLE_MEDIA_GENERATE);
            }
            break;
        case ESP_LLM_BAIDU_SCENE_VOICE_CHAT:
        default:
            send_agent_event(ctx, AGENT_EVENT_DISABLE_MEDIA_GENERATE);
            if (!baidu_engine_lock(ctx)) {
                return ESP_ERR_INVALID_STATE;
            }
            if (ctx->engine == NULL) {
                baidu_engine_unlock(ctx);
                return ESP_ERR_INVALID_STATE;
            }
            baidu_chat_agent_engine_update_visual_mode(ctx->engine, sdk_mode, expire_sec);
            baidu_engine_unlock(ctx);
            break;
    }
    return ESP_OK;
}

static int build_engine_config(char *buf, size_t buf_size, const esp_llm_baidu_ctx_t *ctx, bool enable_visual)
{
    const esp_llm_baidu_engine_cfg_t *eng = &ctx->cfg.agent.engine;
    const esp_llm_baidu_account_cfg_t *acc = &ctx->cfg.agent.account;

    if (eng->config_json_raw != NULL && eng->config_json_raw[0] != '\0') {
        return snprintf(buf, buf_size, "%s", eng->config_json_raw);
    }

    int width = ESP_LLM_BAIDU_DEFAULT_VIDEO_WIDTH;
    int height = ESP_LLM_BAIDU_DEFAULT_VIDEO_HEIGHT;
    if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)) {
        width = (int)baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video.width;
        height = (int)baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video.height;
    } else if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        width = (int)baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.width;
        height = (int)baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)->info.video.height;
    }
    const char *rtc_ac = baidu_agent_sdk_rtc_ac(&ctx->cfg);
    const char *fmt = baidu_rtc_ac_is_opus(rtc_ac) ? ESP_LLM_BAIDU_ENGINE_CONFIG_FORMAT_OPUS
                                                    : ESP_LLM_BAIDU_ENGINE_CONFIG_FORMAT;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    return snprintf(buf, buf_size, fmt,
                    baidu_str_or(acc->app_id, ""),
                    baidu_str_or(acc->llm, ESP_LLM_BAIDU_DEFAULT_LLM),
                    rtc_ac,
                    baidu_str_or(acc->lang, ESP_LLM_BAIDU_DEFAULT_LANG),
                    enable_visual ? "true" : "false",
                    baidu_str_or(eng->dfda, ESP_LLM_BAIDU_DEFAULT_DFDA),
                    width, height);
#pragma GCC diagnostic pop
}

static esp_err_t fill_engine_params(esp_llm_baidu_ctx_t *ctx, AgentEngineParams *params)
{
    const esp_llm_baidu_account_cfg_t *acc = &ctx->cfg.agent.account;
    const esp_llm_baidu_engine_cfg_t *eng = &ctx->cfg.agent.engine;
    /* Official 文生图 config sets enable_visual true so the cloud opens the
       picture downlink. ImageGeneration has enable_visual=false (no camera)
       but still needs this JSON flag, same as params->enable_video. */
    bool enable_visual = ctx->resolved_scene.enable_visual ||
                         ctx->resolved_scene.enable_image_generate;

    memset(params, 0, sizeof(*params));
    snprintf(params->agent_platform_url, sizeof(params->agent_platform_url), "%s",
             baidu_str_or(acc->server_url, ""));
    snprintf(params->appid, sizeof(params->appid), "%s", baidu_str_or(acc->app_id, ""));
    snprintf(params->userId, sizeof(params->userId), "%s", baidu_str_or(acc->user_id, ""));
    snprintf(params->workflow, sizeof(params->workflow), "%s",
             baidu_str_or(acc->workflow, ESP_LLM_BAIDU_DEFAULT_WORKFLOW));
    snprintf(params->llm, sizeof(params->llm), "%s", baidu_str_or(acc->llm, ESP_LLM_BAIDU_DEFAULT_LLM));
    snprintf(params->lang, sizeof(params->lang), "%s", baidu_str_or(acc->lang, ESP_LLM_BAIDU_DEFAULT_LANG));
    snprintf(params->AudioOutcodecType, sizeof(params->AudioOutcodecType), "%s",
             baidu_agent_sdk_rtc_ac(&ctx->cfg));
    /* AudioIn is the send_audio payload. rtc_ac / AudioOut is the wire codec:
       opus+opus passthrough, or pcm+g722 with G.722 inside the Baidu lib. */
    snprintf(params->AudioIncodecType, sizeof(params->AudioIncodecType), "%s",
             baidu_agent_sdk_uplink_codec(&ctx->cfg));
    snprintf(params->cer, sizeof(params->cer), "%s", baidu_str_or(eng->cer, ESP_LLM_BAIDU_DEFAULT_CER));
    snprintf(params->license_key, sizeof(params->license_key), "%s",
             acc->license_key != NULL ? acc->license_key : "");

    int needed = build_engine_config(params->config, sizeof(params->config), ctx, enable_visual);
    if (needed < 0 || (size_t)needed >= sizeof(params->config)) {
        return ESP_ERR_INVALID_SIZE;
    }

    params->instance_id = acc->instance_id;
    params->verbose = eng->verbose;
    params->enable_local_agent = eng->enable_local_agent;
    params->enable_voice_interrupt = eng->enable_voice_interrupt;
    params->disable_send_mute_data = eng->disable_send_mute_data;
    params->level_voice_interrupt = eng->level_voice_interrupt;
    params->AudioInChannel = 1;
    params->AudioInFrequency = BAIDU_TTS_PCM_RATE;
    if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)) {
        params->AudioInFrequency =
            (int)baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_AUDIO_UP)->info.audio.sample_rate;
    }
    params->region = region_to_sdk(eng->region);
    params->enable_video = enable_visual;
    params->enable_internal_device = false;
    ESP_LOGI(TAG, "Engine audio in=%s@%dHz rtc_ac/out=%s player_downlink=%d video=%d json_visual=%s",
             params->AudioIncodecType[0] != '\0' ? params->AudioIncodecType
                                                : ESP_LLM_BAIDU_DEFAULT_AUDIO_IN_CODEC,
             params->AudioInFrequency,
             params->AudioOutcodecType, (int)ctx->downlink_codec, params->enable_video,
             enable_visual ? "true" : "false");
    return ESP_OK;
}

static void on_error(int err_code, const char *err_msg)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
        ESP_LOGE(TAG, "Engine failed: code %d, %s", err_code, err_msg ? err_msg : "");
    if (ctx != NULL) {
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR, "ERROR",
                         ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_SYSTEM,
                         err_msg, err_msg != NULL ? strlen(err_msg) : 0);
        baidu_callback_exit(ctx);
    }
}

static void agent_request_reconnect(esp_llm_baidu_ctx_t *ctx, const char *reason);

static void on_call_state(AGentCallState state)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Call state changed: %d", state);
    if (state == AGENT_LOGIN_SUCCESS) {
        ctx->connected = true;
        if (ctx->evt_group != NULL) {
            xEventGroupSetBits(ctx->evt_group, BAIDU_JOIN_EVENT_BIT);
        }
        ESP_LOGI(TAG, "Login succeeded: uplink enabled when media is ready");
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_CONNECTED,
                         "CALL_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_NONE,
                         &state, sizeof(state));
    } else if (state == AGENT_LOGIN_FAIL) {
        ctx->connected = false;
        ctx->media_ready = false;
        ESP_LOGE(TAG, "Login failed");
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR,
                         "CALL_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_NONE,
                         &state, sizeof(state));
        agent_request_reconnect(ctx, "login_fail");
    } else if (state == AGENT_CALL_FAIL) {
        ctx->connected = false;
        ctx->media_ready = false;
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR,
                         "CALL_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_NONE,
                         &state, sizeof(state));
        agent_request_reconnect(ctx, "call_fail");
    }
    baidu_callback_exit(ctx);
}

static void on_conn_state(AGentConnectState state)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Connection state changed: %d", state);
    if (state == AGENT_CONNECTION_STATE_CONNECTED) {
        ctx->connected = true;
        ctx->reconnect_attempts = 0;
        ctx->reconnect_pending = false;
        ESP_LOGI(TAG, "Connected");
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_CONNECTED,
                         "CONNECTION_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY,
                         ESP_LLM_ROLE_NONE, &state, sizeof(state));
    } else if (state == AGENT_CONNECTION_STATE_RECONNECTING) {
        ESP_LOGI(TAG, "SDK reconnecting");
    } else if (state == AGENT_CONNECTION_STATE_DISCONNECTED) {
        ctx->connected = false;
        ctx->media_ready = false;
        ESP_LOGE(TAG, "Disconnect: session closed");
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_DISCONNECTED,
                         "CONNECTION_STATE", ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY,
                         ESP_LLM_ROLE_NONE, &state, sizeof(state));
        agent_request_reconnect(ctx, "disconnected");
    }
    baidu_callback_exit(ctx);
}

static void on_asr(const char *text, size_t text_len, bool final, const ASRExtInfo *info)
{
    (void)info;
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    if (text != NULL && text_len > 0) {
        ESP_LOGI(TAG, "[UPLINK] ASR: %.*s (final=%d)", (int)text_len, text, final);
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_TEXT, "ASR",
                         final ? ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT : ESP_LLM_EVENT_PAYLOAD_TYPE_JSON,
                         ESP_LLM_ROLE_USER, text, text_len);
    } else {
        ESP_LOGD(TAG, "[UPLINK] ASR callback (final=%d, empty text)", final);
    }
    baidu_callback_exit(ctx);
}

static void on_subtitle(const char *text, bool final)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL || text == NULL) {
        if (ctx != NULL) {
            baidu_callback_exit(ctx);
        }
        return;
    }
    ESP_LOGD(TAG, "AI agent subtitle: %s (final=%d)", text, final);
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_TEXT, "AGENT_SUBTITLE",
                     final ? ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT : ESP_LLM_EVENT_PAYLOAD_TYPE_JSON,
                     ESP_LLM_ROLE_ASSISTANT, text, strlen(text));
    baidu_callback_exit(ctx);
}

static void on_speaking(bool speaking)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "AI agent speaking: %s", speaking ? "Start" : "Stop");
    if (speaking) {
        ctx->tts_session_open = true;
        ctx->tts_stream_start = true;
    } else {
        ctx->tts_session_open = false;
        (void)baidu_feed_tts(ctx, NULL, 0, true);
    }
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE, "SPEAKING",
                     ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_ASSISTANT,
                     &speaking, sizeof(speaking));
    baidu_callback_exit(ctx);
}

static void on_function_call(const char *id, const char *params)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Function call, id: %s, params: %s", id ? id : "", params ? params : "");
    /* Cache the id so the application can answer with
       esp_llm_baidu_send_function_call_result(service, NULL, result). */
    if (baidu_engine_lock(ctx)) {
        snprintf(ctx->fc_id, sizeof(ctx->fc_id), "%s", id != NULL ? id : "");
        baidu_engine_unlock(ctx);
    }
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE, "FUNCTION_CALL",
                     ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, ESP_LLM_ROLE_NONE, params,
                     params != NULL ? strlen(params) : 0);
    baidu_callback_exit(ctx);
}

static void on_audio_player_op(const char *path, bool start)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Audio player op. path: %s, start: %d", path ? path : "", start);

    (void)baidu_emit(ctx, ESP_LLM_EVENT_NONE,
                     start ? "AUDIO_PLAYER_START" : "AUDIO_PLAYER_STOP",
                     ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE,
                     path, path != NULL ? strlen(path) : 0);
    baidu_callback_exit(ctx);
}

static void on_media_setup(void)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL || ctx->engine == NULL) {
        if (ctx != NULL) {
            baidu_callback_exit(ctx);
        }
        return;
    }
    ESP_LOGI(TAG, "[MEDIA] Setup completed");
    if (ctx->cfg.agent.greeting_text != NULL && ctx->cfg.agent.greeting_text[0] != '\0') {
        baidu_chat_agent_engine_send_text_to_TTS(ctx->engine, ctx->cfg.agent.greeting_text);
    }
    esp_err_t ret = setup_scene(ctx);
    ESP_LOGI(TAG, "[MEDIA] Scene setup result: %s, scene=%s",
             esp_err_to_name(ret),
             ctx->resolved_scene.scene_name != NULL ? ctx->resolved_scene.scene_name : "?");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Agent media setup failed: scene setup returned %s", esp_err_to_name(ret));
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR, "MEDIA_SETUP",
                         ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_SYSTEM,
                         esp_err_to_name(ret), strlen(esp_err_to_name(ret)));
        baidu_callback_exit(ctx);
        return;
    }
    ctx->media_ready = true;
    ESP_LOGI(TAG, "Agent media ready, uplink ASR path open");
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_MEDIA_READY, "MEDIA_SETUP",
                     ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT, ESP_LLM_ROLE_NONE, NULL, 0);
    baidu_callback_exit(ctx);
}

static void on_audio_data(const uint8_t *data, size_t len)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL || data == NULL || len == 0 || !ctx->tts_session_open) {
        if (ctx != NULL) {
            baidu_callback_exit(ctx);
        }
        return;
    }
    esp_err_t ret = baidu_feed_tts(ctx, data, len, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Agent audio output failed: %s", esp_err_to_name(ret));
    }
    baidu_callback_exit(ctx);
}

static void on_video_data_cb(const uint8_t *data, size_t len, RtcImageType img_type, int width, int height)
{
    if (img_type != RTC_IMAGE_TYPE_JPEG) {
        return;
    }
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        ESP_LOGW(TAG, "[VIDEO] Dropped: adapter callback not active");
        return;
    }
    baidu_on_video_data(ctx, data, len, (int)img_type, width, height);
    baidu_callback_exit(ctx);
}

static void on_vision_request(void)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    ESP_LOGI(TAG, "[VISION] Image request, scene=%s vision_mode=%d",
             ctx->resolved_scene.scene_name != NULL ? ctx->resolved_scene.scene_name : "?",
             (int)ctx->cfg.vision.vision_mode);
    if (ctx->cfg.vision.vision_mode != ESP_LLM_BAIDU_VISION_MODE_IMAGE ||
        !baidu_scene_needs_camera(&ctx->resolved_scene)) {
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE,
                         "VISION_IMAGE_REQUEST", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                         ESP_LLM_ROLE_NONE, NULL, 0);
        baidu_callback_exit(ctx);
        return;
    }
    if (ctx->cfg.agent.scene.scene_type == ESP_LLM_BAIDU_SCENE_MULTI_MODAL &&
        ctx->resolved_scene.enable_image_generate) {
        send_agent_event(ctx, AGENT_EVENT_DISABLE_MEDIA_GENERATE);
        ESP_LOGI(TAG, "[VISION] Disable media generate for snapshot upload");
    }
    ctx->pending_snapshot = true;
    ESP_LOGI(TAG, "[VISION] Waiting capture frame to upload");
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE,
                     "VISION_IMAGE_REQUEST", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                     ESP_LLM_ROLE_NONE, NULL, 0);
    baidu_callback_exit(ctx);
}

static void on_vision_ack(const char *name)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    ESP_LOGI(TAG, "[VISION] Image uploaded ack: %s", name ? name : "");
    if (ctx != NULL) {
        baidu_snapshot_release(ctx);
        if (ctx->cfg.agent.scene.scene_type == ESP_LLM_BAIDU_SCENE_MULTI_MODAL &&
            ctx->resolved_scene.enable_image_generate) {
            send_agent_event(ctx, AGENT_EVENT_ENABLE_MEDIA_GENERATE);
            ESP_LOGI(TAG, "[VISION] Re-enable media generate after snapshot ack");
        }
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE,
                         "VISION_IMAGE_ACK", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                         ESP_LLM_ROLE_NONE, name, name != NULL ? strlen(name) : 0);
        baidu_callback_exit(ctx);
    }
}

static void on_license(bool result)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    ESP_LOGI(TAG, "License result: %d", result);
    if (!result) {
        ESP_LOGE(TAG, "License validation failed");
    }
    if (ctx != NULL) {
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE, "LICENSE",
                         ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY, ESP_LLM_ROLE_NONE, &result, sizeof(result));
        baidu_callback_exit(ctx);
    }
}

static void on_media_generate_result(const char *result)
{
    if (result != NULL && result[0] != '\0') {
        ESP_LOGI(TAG, "[MEDIA-GEN] Result: %s", result);
        if (strstr(result, "\"error\":0") == NULL && strstr(result, "\"error\": 0") == NULL) {
            ESP_LOGW(TAG, "[MEDIA-GEN] Cloud generate failed; no JPEG/TTS until error is 0");
        }
    }
    /* Same as multimodal_assistant_app: JPEG arrives on onVideoData, not here. */
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx != NULL) {
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE,
                         "MEDIA_GENERATE_RESULT", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                         ESP_LLM_ROLE_NONE, "image", 5);
        baidu_callback_exit(ctx);
    }
}

static void on_media_generate_ack(const char *name)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    ESP_LOGD(TAG, "[MEDIA-GEN] Image uploaded ack: %s", name ? name : "");
    if (ctx != NULL) {
        (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE,
                         "MEDIA_GENERATE_ACK", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                         ESP_LLM_ROLE_NONE, name, name != NULL ? strlen(name) : 0);
        baidu_callback_exit(ctx);
    }
}

static void on_agent_event(const char *event_msg, size_t len)
{
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL) {
        return;
    }
    if (event_msg != NULL && len > 0) {
        ESP_LOGI(TAG, "[AGENT] Event updated: %.*s", (int)len, event_msg);
        if (strstr(event_msg, "MEDIA_GENERATE_START") != NULL) {
            ESP_LOGI(TAG, "[VIDEO] Media generate started");
        }
    }
    (void)baidu_emit(ctx,  ESP_LLM_EVENT_NONE, "AGENT_EVENT",
                     ESP_LLM_EVENT_PAYLOAD_TYPE_JSON, ESP_LLM_ROLE_NONE, event_msg, len);
    baidu_callback_exit(ctx);
}

static const BaiduChatAgentEvent s_agent_events = {
    .onError                 = on_error,
    .onCallStateChange       = on_call_state,
    .onConnectionStateChange = on_conn_state,
    .onUserAsrSubtitle       = on_asr,
    .onAIAgentSubtitle       = on_subtitle,
    .onAIAgentSpeaking       = on_speaking,
    .onFunctionCall          = on_function_call,
    .onAudioPlayerOp         = on_audio_player_op,
    .onMediaSetup            = on_media_setup,
    .onAudioData             = on_audio_data,
    .onVideoData             = on_video_data_cb,
    .onLicenseResult         = on_license,
    .onAgentEventUpdated     = on_agent_event,
    .onVisionImageRequest    = on_vision_request,
    .onVisionImageAck        = on_vision_ack,
    .onMediaGenerateResult   = on_media_generate_result,
    .onMediaGenerateAck      = on_media_generate_ack,
};

static esp_err_t start_agent_engine(esp_llm_baidu_ctx_t *ctx);

static void wait_engine_task_done(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx->engine_task == NULL || ctx->evt_group == NULL) {
        return;
    }
    /* The start task owns ctx->engine until it exits and destroys it on its own
       failure path, so callers must not destroy the engine before this returns. */
    (void)xEventGroupWaitBits(ctx->evt_group, BAIDU_ENGINE_DONE_BIT, pdFALSE, pdTRUE,
                              pdMS_TO_TICKS((uint32_t)ctx->cfg.connect_timeout_ms +
                                            BAIDU_ENGINE_START_EXTRA_MS));
}

static void engine_start_task(void *arg)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)arg;
    ctx->engine_start_ok = false;

    ctx->engine = baidu_create_chat_agent_engine(&s_agent_events);
    if (ctx->engine == NULL) {
        ESP_LOGE(TAG, "Create engine failed");
        goto done;
    }

    AgentEngineParams *params = calloc(1, sizeof(*params));
    if (params == NULL) {
        ESP_LOGE(TAG, "Engine start failed: params allocation failed");
        goto fail;
    }
    if (fill_engine_params(ctx, params) != ESP_OK) {
        free(params);
        goto fail;
    }

    int ret = baidu_chat_agent_engine_init(ctx->engine, params);
    free(params);
    if (ret != 200) {
        ESP_LOGE(TAG, "Engine init failed: code %d", ret);
        goto fail;
    }

    baidu_chat_agent_engine_call(ctx->engine);
    EventBits_t bits = xEventGroupWaitBits(ctx->evt_group, BAIDU_JOIN_EVENT_BIT,
                                           pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS(ctx->cfg.connect_timeout_ms));
    if ((bits & BAIDU_JOIN_EVENT_BIT) == 0) {
        ESP_LOGE(TAG, "Agent join timeout after %u ms", (unsigned)ctx->cfg.connect_timeout_ms);
        goto fail;
    }

    ctx->engine_start_ok = true;
    ESP_LOGI(TAG, "Baidu agent engine joined");
    goto done;

fail:
    baidu_chat_agent_engine_destroy(ctx->engine);
    ctx->engine = NULL;

done:
    /* Clear the handle before signalling so that anyone woken by the bit observes
       the task as gone and may take ownership of ctx->engine. */
    ctx->engine_task = NULL;
    if (ctx->evt_group != NULL) {
        xEventGroupSetBits(ctx->evt_group, BAIDU_ENGINE_DONE_BIT);
    }
    esp_gmf_oal_thread_delete(NULL);
}

static esp_err_t start_agent_engine(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx->engine_task != NULL) {
        ESP_LOGE(TAG, "Engine start rejected: a previous start is still running");
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupClearBits(ctx->evt_group, BAIDU_JOIN_EVENT_BIT | BAIDU_ENGINE_DONE_BIT);
    if (esp_gmf_oal_thread_create(&ctx->engine_task, "brtc_engine", engine_start_task, ctx,
                                  ctx->cfg.agent.engine_stack_size, ctx->cfg.agent.engine_priority,
                                  BAIDU_ENGINE_STACK_IN_EXT, BAIDU_ENGINE_CORE_ID) != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Engine start failed: GMF thread create failed");
        ctx->engine_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    EventBits_t bits = xEventGroupWaitBits(ctx->evt_group, BAIDU_ENGINE_DONE_BIT,
                                           pdTRUE, pdTRUE,
                                           pdMS_TO_TICKS((uint32_t)ctx->cfg.connect_timeout_ms +
                                                         BAIDU_ENGINE_START_EXTRA_MS));
    if ((bits & BAIDU_ENGINE_DONE_BIT) == 0 || !ctx->engine_start_ok || ctx->engine == NULL) {
        if (ctx->engine_task != NULL) {
            ESP_LOGW(TAG, "Engine start timed out, waiting for background task");
            (void)xEventGroupWaitBits(ctx->evt_group, BAIDU_ENGINE_DONE_BIT,
                                      pdTRUE, pdTRUE,
                                      pdMS_TO_TICKS(BAIDU_ENGINE_START_EXTRA_MS));
        }
        ctx->engine_start_ok = false;
        if (ctx->engine_task != NULL) {
            ESP_LOGE(TAG, "Engine start failed: task still running, engine left to it");
            return ESP_FAIL;
        }
        if (ctx->engine != NULL) {
            baidu_chat_agent_engine_destroy(ctx->engine);
            ctx->engine = NULL;
        }
        ESP_LOGE(TAG, "Engine start failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool reconnect_stop_requested(esp_llm_baidu_ctx_t *ctx)
{
    bool stop;

    _lock_acquire(&s_reconnect_lock);
    stop = ctx->reconnect_cancel || !ctx->started || !ctx->cfg.agent.auto_reconnect;
    _lock_release(&s_reconnect_lock);
    return stop;
}

static void reconnect_mark_done(esp_llm_baidu_ctx_t *ctx)
{
    /* Publish DONE before clearing the flag. stop waits on the bit, then
       observes the flag so it does not free ctx while this task still uses it. */
    if (ctx->evt_group != NULL) {
        xEventGroupSetBits(ctx->evt_group, BAIDU_RECONNECT_DONE_BIT);
    }
    _lock_acquire(&s_reconnect_lock);
    ctx->reconnect_in_progress = false;
    _lock_release(&s_reconnect_lock);
}

void baidu_agent_cancel_reconnect(esp_llm_baidu_ctx_t *ctx)
{
    bool wait = false;

    if (ctx == NULL) {
        return;
    }

    _lock_acquire(&s_reconnect_lock);
    ctx->reconnect_cancel = true;
    ctx->reconnect_pending = false;
    wait = ctx->reconnect_in_progress;
    _lock_release(&s_reconnect_lock);

    if (ctx->evt_group != NULL) {
        xEventGroupSetBits(ctx->evt_group, BAIDU_RECONNECT_CANCEL_BIT);
    }
    if (!wait) {
        return;
    }
    if (ctx->evt_group != NULL) {
        (void)xEventGroupWaitBits(ctx->evt_group, BAIDU_RECONNECT_DONE_BIT,
                                  pdTRUE, pdTRUE, portMAX_DELAY);
    }
    while (true) {
        bool alive;

        _lock_acquire(&s_reconnect_lock);
        alive = ctx->reconnect_in_progress;
        _lock_release(&s_reconnect_lock);
        if (!alive) {
            break;
        }
        vTaskDelay(1);
    }
}

static void agent_reconnect_task(void *arg)
{
    esp_llm_baidu_ctx_t *ctx = (esp_llm_baidu_ctx_t *)arg;

    while (ctx != NULL && !reconnect_stop_requested(ctx)) {
        uint32_t delay_ms = ctx->cfg.agent.reconnect_delay_ms;
        bool retry = false;
        uint32_t max;
        esp_err_t ret;

        if (delay_ms == 0) {
            delay_ms = ESP_LLM_BAIDU_DEFAULT_RECONNECT_DELAY_MS;
        }
        if (ctx->evt_group == NULL) {
            break;
        }
        (void)xEventGroupWaitBits(ctx->evt_group, BAIDU_RECONNECT_CANCEL_BIT,
                                  pdFALSE, pdTRUE, pdMS_TO_TICKS(delay_ms));
        if (reconnect_stop_requested(ctx)) {
            break;
        }

        ctx->connected = false;
        ctx->media_ready = false;
        ctx->engine_start_ok = false;
        ctx->tts_session_open = true;
        ctx->tts_stream_start = false;
        ctx->uplink_send_log_count = 0;

        wait_engine_task_done(ctx);
        if (ctx->engine != NULL) {
            ESP_LOGW(TAG, "Agent reconnect: destroy engine");
            baidu_chat_agent_engine_destroy(ctx->engine);
            ctx->engine = NULL;
        }

        ESP_LOGI(TAG, "Agent reconnect: restart engine (attempt %u)",
                 (unsigned)(ctx->reconnect_attempts + 1));
        ret = start_agent_engine(ctx);
        if (ret == ESP_OK) {
            ctx->reconnect_attempts = 0;
            ESP_LOGI(TAG, "Agent reconnect succeeded");
        } else {
            ctx->reconnect_attempts++;
            ESP_LOGE(TAG, "Agent reconnect failed (attempt %u)", (unsigned)ctx->reconnect_attempts);
            max = ctx->cfg.agent.reconnect_max_attempts;
            if (max == 0 || ctx->reconnect_attempts < max) {
                retry = true;
            } else {
                ESP_LOGE(TAG, "Agent reconnect gave up after %u attempts", (unsigned)max);
                (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR,
                                 "RECONNECT_FAILED", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                 ESP_LLM_ROLE_SYSTEM, NULL, 0);
            }
        }

        _lock_acquire(&s_reconnect_lock);
        if (ctx->reconnect_pending && ctx->started && !ctx->reconnect_cancel &&
            ctx->cfg.agent.auto_reconnect) {
            ctx->reconnect_pending = false;
            retry = true;
        }
        if (ctx->reconnect_cancel || !ctx->started || !ctx->cfg.agent.auto_reconnect) {
            retry = false;
        }
        max = ctx->cfg.agent.reconnect_max_attempts;
        if (retry && max > 0 && ctx->reconnect_attempts >= max) {
            retry = false;
        }
        _lock_release(&s_reconnect_lock);
        if (!retry) {
            break;
        }
    }

    if (ctx != NULL) {
        reconnect_mark_done(ctx);
    }
    esp_gmf_oal_thread_delete(NULL);
}

static void agent_request_reconnect(esp_llm_baidu_ctx_t *ctx, const char *reason)
{
    uint32_t max;
    uint32_t stack;
    uint8_t prio;
    bool skipped_max = false;
    bool already = false;

    if (ctx == NULL) {
        return;
    }
    max = ctx->cfg.agent.reconnect_max_attempts;

    _lock_acquire(&s_reconnect_lock);
    if (!ctx->started || ctx->reconnect_cancel || !ctx->cfg.agent.auto_reconnect) {
        _lock_release(&s_reconnect_lock);
        return;
    }
    if (max > 0 && ctx->reconnect_attempts >= max) {
        skipped_max = true;
    } else if (ctx->reconnect_in_progress) {
        ctx->reconnect_pending = true;
        already = true;
    } else {
        ctx->reconnect_in_progress = true;
    }
    _lock_release(&s_reconnect_lock);

    if (skipped_max) {
        ESP_LOGW(TAG, "Reconnect skipped: max attempts (%u) reached", (unsigned)max);
        return;
    }
    if (already) {
        ESP_LOGD(TAG, "Reconnect already in progress (%s)", reason ? reason : "");
        return;
    }

    ESP_LOGW(TAG, "Agent reconnect scheduled (%s)", reason ? reason : "unknown");
    if (ctx->evt_group != NULL) {
        xEventGroupClearBits(ctx->evt_group, BAIDU_RECONNECT_DONE_BIT);
    }

    stack = ctx->cfg.agent.engine_stack_size;
    if (stack < BAIDU_RECONNECT_TASK_STACK) {
        stack = BAIDU_RECONNECT_TASK_STACK;
    }
    prio = ctx->cfg.agent.engine_priority > 0 ? (uint8_t)(ctx->cfg.agent.engine_priority - 1) : 5;
    if (esp_gmf_oal_thread_create(NULL, "baidu_reconn", agent_reconnect_task, ctx,
                                  stack, prio, true, 0) != ESP_GMF_ERR_OK) {
        reconnect_mark_done(ctx);
        ESP_LOGE(TAG, "Create reconnect task failed: GMF thread create failed");
    }
}

static esp_err_t agent_mode_start(esp_llm_baidu_ctx_t *ctx)
{
    _lock_acquire(&s_reconnect_lock);
    ctx->reconnect_cancel = false;
    ctx->reconnect_pending = false;
    _lock_release(&s_reconnect_lock);
    if (ctx->evt_group != NULL) {
        xEventGroupClearBits(ctx->evt_group,
                             BAIDU_RECONNECT_CANCEL_BIT | BAIDU_RECONNECT_DONE_BIT);
    }
    return start_agent_engine(ctx);
}

static esp_err_t agent_mode_stop(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    baidu_agent_cancel_reconnect(ctx);
    wait_engine_task_done(ctx);
    if (ctx->engine != NULL) {
        baidu_chat_agent_engine_destroy(ctx->engine);
        ctx->engine = NULL;
    }
    ctx->connected = false;
    ctx->media_ready = false;
    ctx->engine_start_ok = false;
    ctx->reconnect_attempts = 0;
    return ESP_OK;
}

static esp_err_t agent_mode_send_text(esp_llm_baidu_ctx_t *ctx, const char *text)
{
    if (ctx == NULL || text == NULL || !ctx->connected || ctx->engine == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    baidu_chat_agent_engine_send_text(ctx->engine, text);
    return ESP_OK;
}

static esp_err_t agent_mode_send_frame(esp_llm_baidu_ctx_t *ctx, const esp_media_frame_t *frame)
{
    if (ctx == NULL || frame == NULL || frame->data == NULL || frame->size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        if (!ctx->connected || !ctx->media_ready || ctx->engine == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        baidu_chat_agent_engine_send_audio(ctx->engine, frame->data, frame->size);
        if (ctx->uplink_send_log_count < 3) {
            ESP_LOGI(TAG, "Uplink frame #%u size=%u in=%s",
                     (unsigned)(ctx->uplink_send_log_count + 1),
                     (unsigned)frame->size,
                     baidu_agent_sdk_uplink_codec(&ctx->cfg));
            ctx->uplink_send_log_count++;
        }
        return ESP_OK;
    }

    if (frame->type != ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!ctx->connected || ctx->engine == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    /* DVP JPEG is often truncated (NO-EOI). Seedream needs complete JPEGs.
       Skip the check for H.264 so NAL units are not dropped as incomplete JPEG. */
    if (baidu_stream_info(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP)->info.video.codec == ESP_FOURCC_MJPG &&
        !jpeg_is_complete(frame->data, frame->size)) {
        return ESP_OK;
    }

    if (ctx->cfg.vision.vision_mode == ESP_LLM_BAIDU_VISION_MODE_IMAGE &&
        ctx->pending_snapshot) {
        if (ctx->snap_mux == NULL ||
            xSemaphoreTake(ctx->snap_mux, pdMS_TO_TICKS(BAIDU_SNAPSHOT_LOCK_MS)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        esp_err_t ret = ESP_ERR_NO_MEM;
        uint8_t *copy = malloc(frame->size);
        if (copy != NULL) {
            memcpy(copy, frame->data, frame->size);
            baidu_snapshot_reset_locked(ctx);
            ctx->uplink_jpeg = copy;
            ctx->uplink_jpeg_size = frame->size;
            baidu_chat_agent_engine_send_video(ctx->engine, ctx->uplink_jpeg, ctx->uplink_jpeg_size);
            ctx->pending_snapshot = false;
            ret = ESP_OK;
        }
        xSemaphoreGive(ctx->snap_mux);
        return ret;
    }

    baidu_chat_agent_engine_send_video(ctx->engine, frame->data, frame->size);
    return ESP_OK;
}

static esp_err_t agent_mode_send_custom_data(esp_llm_baidu_ctx_t *ctx, const char *data)
{
    if (ctx == NULL || data == NULL || data[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctx->connected || ctx->engine == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    send_agent_event(ctx, data);
    return ESP_OK;
}

static esp_err_t agent_mode_cancel_response(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL || ctx->engine == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const char *cmd = baidu_str_or(ctx->cfg.agent.wakeup_interrupt_cmd, ESP_LLM_BAIDU_WAKEUP_INTERRUPT_CMD);
    if (cmd[0] != '\0') {
        baidu_chat_agent_engine_send_event_to_agent(ctx->engine, (char *)cmd);
        ESP_LOGI(TAG, "Wakeup interrupt: event=%s", cmd);
    }
    baidu_chat_agent_engine_interrupt(ctx->engine);
    return ESP_OK;
}

const esp_llm_baidu_mode_ops_t baidu_agent_mode_ops = {
    .start            = agent_mode_start,
    .stop             = agent_mode_stop,
    .send_text        = agent_mode_send_text,
    .send_frame       = agent_mode_send_frame,
    .send_custom_data = agent_mode_send_custom_data,
    .cancel_response  = agent_mode_cancel_response,
};
