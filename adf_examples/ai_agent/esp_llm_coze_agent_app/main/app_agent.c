/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_player_service_playback.h"

#include "coze_app_media.h"

#include "app_agent.h"

static const char *TAG = "APP_AGENT";

#define WAKEUP_PROMPT_URI              "file://spiffs/lai_lo.mp3"
#define APP_AGENT_LISTEN_TASK_STACK    (4 * 1024)
#define APP_AGENT_LISTEN_TASK_PRIO     5

/**
 * @brief  Private Coze agent application context
 */
struct app_agent {
    coze_app_media_t     *media;                  /*!< Application-owned media environment */
    esp_llm_service_t    *llm;                    /*!< Application-owned Coze LLM service */
    esp_player_service_t *player;                 /*!< Borrowed player for the wake-up prompt */
    atomic_bool           wakeup_prompt_pending;  /*!< Wake-up prompt is playing; open uplink when it ends */
};

static void record_first_error(esp_err_t *result, esp_err_t error)
{
    if (*result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

static esp_err_t validate_account_config(void)
{
    if (strlen(CONFIG_COZE_BOT_ID) == 0 || strlen(CONFIG_COZE_ACCESS_TOKEN) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Coze bot ID or access token is empty");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static uint32_t build_audio_feature_mask(void)
{
    uint32_t mask = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC |
                    ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS;
#if CONFIG_COZE_AUDIO_VAD
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD;
#endif
#if CONFIG_COZE_AUDIO_WN
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN;
#endif
    return mask;
}

static void fill_coze_config(esp_llm_coze_cfg_t *cfg)
{
    *cfg = ESP_LLM_COZE_CFG_DEFAULT();
    cfg->bot_id = CONFIG_COZE_BOT_ID;
    cfg->access_token = CONFIG_COZE_ACCESS_TOKEN;
    cfg->ws_base_url = strlen(CONFIG_COZE_WS_BASE_URL) > 0 ? CONFIG_COZE_WS_BASE_URL : NULL;
    cfg->conv_create_base_url = strlen(CONFIG_COZE_CONV_CREATE_BASE_URL) > 0 ?
                                CONFIG_COZE_CONV_CREATE_BASE_URL : NULL;
    cfg->user_id = strlen(CONFIG_COZE_USER_ID) > 0 ? CONFIG_COZE_USER_ID : NULL;
    cfg->voice_id = strlen(CONFIG_COZE_VOICE_ID) > 0 ? CONFIG_COZE_VOICE_ID : NULL;
    cfg->service.name = "coze_agent";
    cfg->service.audio_ai_cfg.feature_mask = build_audio_feature_mask();
}

#if CONFIG_COZE_AUDIO_WN
static bool stream_is_active(esp_player_service_t *player, esp_media_stream_id_t stream)
{
    esp_player_state_t state = ESP_PLAYER_STATE_IDLE;
    if (player == NULL ||
        esp_player_service_get_state(player, stream, &state) != ESP_OK) {
        return false;
    }
    return state == ESP_PLAYER_STATE_PREPARING ||
           state == ESP_PLAYER_STATE_PLAYING ||
           state == ESP_PLAYER_STATE_PAUSED;
}

static void open_uplink_after_wakeup_prompt(app_agent_t *agent)
{
    if (agent == NULL || agent->llm == NULL) {
        return;
    }
    if (!atomic_exchange(&agent->wakeup_prompt_pending, false)) {
        return;
    }
    ESP_LOGI(TAG, "Wake-up prompt finished, open uplink");
    esp_err_t ret = esp_llm_service_keep_awake(agent->llm, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Release keep-awake after wake-up prompt failed: %s", esp_err_to_name(ret));
    }
    ret = esp_llm_service_enable_uplink_track(agent->llm, ESP_MEDIA_TRACK_TYPE_AUDIO, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Open uplink after wake-up prompt failed: %s", esp_err_to_name(ret));
    }
}

static void open_uplink_task(void *arg)
{
    open_uplink_after_wakeup_prompt((app_agent_t *)arg);
    vTaskDelete(NULL);
}

static void request_open_uplink(app_agent_t *agent)
{
    if (agent == NULL) {
        return;
    }
    /* Player events run on the player thread and must not call control APIs. */
    if (xTaskCreate(open_uplink_task, "wk_listen", APP_AGENT_LISTEN_TASK_STACK, agent,
                    APP_AGENT_LISTEN_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Defer open-uplink failed");
    }
}

static void play_wakeup_prompt(app_agent_t *agent)
{
    if (agent == NULL || agent->player == NULL) {
        ESP_LOGW(TAG, "Wake-up prompt ignored: player is not ready");
        return;
    }
    if (stream_is_active(agent->player, COZE_APP_MEDIA_WAKEUP_STREAM)) {
        (void)esp_player_service_stop(agent->player, COZE_APP_MEDIA_WAKEUP_STREAM);
    }
    esp_err_t ret = esp_player_service_set_url(agent->player, COZE_APP_MEDIA_WAKEUP_STREAM,
                                               WAKEUP_PROMPT_URI);
    if (ret == ESP_OK) {
        ret = esp_player_service_play(agent->player, COZE_APP_MEDIA_WAKEUP_STREAM);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wake-up prompt start failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Play wake-up prompt: %s", WAKEUP_PROMPT_URI);
    }
}

static void handle_wakeup_start(app_agent_t *agent)
{
    if (agent == NULL || agent->llm == NULL) {
        return;
    }
    if (atomic_load(&agent->wakeup_prompt_pending)) {
        ESP_LOGI(TAG, "Wake-up prompt ignored: wake-up prompt is already playing");
        return;
    }

    atomic_store(&agent->wakeup_prompt_pending, true);
    esp_err_t ret = esp_llm_service_keep_awake(agent->llm, true);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Keep awake during wake-up prompt failed: %s", esp_err_to_name(ret));
    }
    ret = esp_llm_service_cancel_response(agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Interrupt response on wake-up failed: %s", esp_err_to_name(ret));
    }
    play_wakeup_prompt(agent);
    if (!stream_is_active(agent->player, COZE_APP_MEDIA_WAKEUP_STREAM)) {
        request_open_uplink(agent);
    }
}

static esp_err_t player_event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    app_agent_t *agent = (app_agent_t *)ctx;
    if (msg == NULL || agent == NULL || msg->stream != COZE_APP_MEDIA_WAKEUP_STREAM) {
        return ESP_OK;
    }
    if (msg->type == ESP_PLAYER_SERVICE_EVENT_FINISHED ||
        msg->type == ESP_PLAYER_SERVICE_EVENT_ERROR) {
        request_open_uplink(agent);
    }
    return ESP_OK;
}
#endif  /* CONFIG_COZE_AUDIO_WN */

static void llm_event_handler(const adf_event_t *hub_event, void *user_ctx)
{
    app_agent_t *agent = (app_agent_t *)user_ctx;
    if (hub_event == NULL || hub_event->payload == NULL ||
        hub_event->payload_len != sizeof(esp_llm_event_t)) {
        ESP_LOGW(TAG, "Handle LLM event: invalid event payload");
        return;
    }

    const esp_llm_event_t *event = (const esp_llm_event_t *)hub_event->payload;
    switch (event->common_id) {
        case ESP_LLM_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Agent event: connected");
            break;
        case ESP_LLM_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Agent event: disconnected");
            break;
        case ESP_LLM_EVENT_MEDIA_READY:
            ESP_LOGI(TAG, "Agent event: media ready");
            break;
        case ESP_LLM_EVENT_FINISHED:
            ESP_LOGI(TAG, "Agent event: chat completed");
            break;
        case ESP_LLM_EVENT_ERROR:
            if (event->payload != NULL && event->payload_len > 0) {
                ESP_LOGE(TAG, "Agent event: error %.*s",
                         (int)event->payload_len, (const char *)event->payload);
            } else {
                ESP_LOGE(TAG, "Agent event: error");
            }
            break;
        case ESP_LLM_EVENT_TEXT:
            if (event->payload != NULL && event->payload_len > 0) {
                ESP_LOGI(TAG, "Agent event: %s %.*s",
                         event->backend_event_name != NULL ? event->backend_event_name : "TEXT",
                         (int)event->payload_len, (const char *)event->payload);
            }
            break;
        case ESP_LLM_EVENT_VAD:
            if (event->payload != NULL && event->payload_len >= sizeof(int)) {
                int vad_state = 0;
                memcpy(&vad_state, event->payload, sizeof(vad_state));
                ESP_LOGI(TAG, "Agent event: VAD state %d", vad_state);
            }
            break;
        case ESP_LLM_EVENT_WAKEUP: {
            int state = 1;
            int trigger_ch = -1;
            if (event->payload != NULL && event->payload_len >= sizeof(esp_llm_wakeup_info_t)) {
                esp_llm_wakeup_info_t info = {0};
                memcpy(&info, event->payload, sizeof(info));
                state = info.state;
                trigger_ch = info.trigger_ch;
            }
            ESP_LOGI(TAG, "Agent event: WakeNet state=%d ch=%d", state, trigger_ch);
#if CONFIG_COZE_AUDIO_WN
            if (state != 0) {
                handle_wakeup_start(agent);
            } else if (agent != NULL && agent->llm != NULL) {
                atomic_store(&agent->wakeup_prompt_pending, false);
                (void)esp_llm_service_enable_uplink_track(agent->llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
            }
#else
            (void)agent;
            (void)state;
#endif
            break;
        }
        default:
            if (event->backend_event_name != NULL) {
                ESP_LOGI(TAG, "Agent event: %s", event->backend_event_name);
            }
            break;
    }
}

esp_err_t app_agent_create(app_agent_t **out_agent)
{
    if (out_agent == NULL || *out_agent != NULL) {
        ESP_LOGE(TAG, "Create failed: output is NULL or does not point to NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = validate_account_config();
    if (ret != ESP_OK) {
        return ret;  /* validate_account_config already logged */
    }

    app_agent_t *agent = calloc(1, sizeof(*agent));
    if (agent == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for agent context");
        return ESP_ERR_NO_MEM;
    }
    *out_agent = agent;

    esp_llm_coze_cfg_t coze_cfg;
    fill_coze_config(&coze_cfg);

    coze_app_media_cfg_t media_cfg = COZE_APP_MEDIA_CFG_DEFAULT();
    media_cfg.output_volume = CONFIG_COZE_OUTPUT_VOLUME;
    ret = coze_app_media_create(&coze_cfg, &media_cfg, &agent->media);
    if (ret != ESP_OK) {
        goto fail;  /* coze_app_media_create already logged */
    }
    esp_capture_service_t *capture = NULL;
    ret = coze_app_media_get_audio_capture(agent->media, &capture);
    if (ret != ESP_OK) {
        goto fail;  /* getter already logged */
    }
    ret = coze_app_media_get_player(agent->media, &agent->player);
    if (ret != ESP_OK) {
        goto fail;  /* getter already logged */
    }
#if CONFIG_COZE_AUDIO_WN
    ret = esp_player_service_set_event_cb(agent->player, player_event_cb, agent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe player events failed: %s", esp_err_to_name(ret));
        goto fail;
    }
#endif

    coze_cfg.service.audio_ai_cfg.capture = capture;
    ret = esp_llm_coze_create(&coze_cfg, &agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create Coze LLM service failed: %s", esp_err_to_name(ret));
        goto fail;
    }

#if CONFIG_COZE_AUDIO_WN
    ret = esp_llm_service_set_wakeup_cancel_response(agent->llm, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable wake-up response cancel failed: %s", esp_err_to_name(ret));
        goto fail;
    }
    ret = esp_llm_service_enable_uplink_track(agent->llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hold audio uplink until wake-up failed: %s", esp_err_to_name(ret));
        goto fail;
    }
#endif

    adf_event_subscribe_info_t subscription = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    subscription.handler = llm_event_handler;
    subscription.handler_ctx = agent;
    ret = esp_service_event_subscribe(ESP_SERVICE_BASE(agent->llm), &subscription);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe LLM events failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ESP_LOGI(TAG, "Create 'coze_agent'");
    return ESP_OK;

fail:
    {
        esp_err_t cleanup_ret = app_agent_destroy(out_agent);
        if (cleanup_ret != ESP_OK) {
            ESP_LOGW(TAG, "Create cleanup failed: %s", esp_err_to_name(cleanup_ret));
        }
    }
    return ret;
}

esp_err_t app_agent_start(app_agent_t *agent)
{
    if (agent == NULL) {
        ESP_LOGE(TAG, "Start failed: agent is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return coze_app_media_start(agent->media, agent->llm);
}

esp_err_t app_agent_stop(app_agent_t *agent)
{
    if (agent == NULL) {
        ESP_LOGE(TAG, "Stop failed: agent is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (agent->media == NULL) {
        return ESP_OK;
    }
    return coze_app_media_stop(agent->media);
}

esp_err_t app_agent_destroy(app_agent_t **agent)
{
    if (agent == NULL || *agent == NULL) {
        ESP_LOGE(TAG, "Destroy failed: agent handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_agent_t *ctx = *agent;
    esp_err_t result = app_agent_stop(ctx);
    if (ctx->llm != NULL) {
        (void)esp_service_event_unsubscribe(ESP_SERVICE_BASE(ctx->llm), NULL, ADF_EVENT_ANY_ID);
        record_first_error(&result, esp_llm_service_destroy(&ctx->llm));
        if (ctx->llm != NULL) {
            ESP_LOGW(TAG, "Destroy deferred: LLM service retained for cleanup retry");
            return result;
        }
    }
    if (ctx->player != NULL) {
        (void)esp_player_service_set_event_cb(ctx->player, NULL, NULL);
        ctx->player = NULL;
    }
    if (ctx->media != NULL) {
        record_first_error(&result, coze_app_media_destroy(&ctx->media));
        if (ctx->media != NULL) {
            ESP_LOGW(TAG, "Destroy deferred: media resources retained for cleanup retry");
            return result;
        }
    }

    free(ctx);
    *agent = NULL;
    ESP_LOGI(TAG, "Destroy 'coze_agent'");
    return result;
}
