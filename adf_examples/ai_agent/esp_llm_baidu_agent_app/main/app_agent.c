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

#include "esp_gmf_oal_thread.h"
#include "esp_log.h"
#include "esp_player_service_playback.h"

#include "baidu_app_media.h"

#include "app_agent.h"
#include "app_agent_utils.h"

static const char *TAG = "APP_AGENT";

#define WAKEUP_PROMPT_URI              "file://spiffs/lai_lo.mp3"
/* Baidu chat-agent custom-data BEGIN. Adapter default
   ESP_LLM_BAIDU_WAKEUP_INTERRUPT_CMD is "[B]:[BEGIN]:2000" (skip 2 s).
   After the local ding this sends BEGIN:0 so listening starts immediately. */
#define WAKEUP_LISTEN_CMD              "[B]:[BEGIN]:0"
#define APP_AGENT_LISTEN_TASK_STACK    (8 * 1024)
#define APP_AGENT_LISTEN_TASK_PRIO     5

/**
 * @brief  Private Baidu agent application context
 */
struct app_agent {
    baidu_app_media_t     *media;                  /*!< Application-owned media environment */
    esp_llm_service_t     *llm;                    /*!< Application-owned Baidu LLM service */
    esp_player_service_t  *player;                /*!< Borrowed player for music and wake-up mix */
    int                    output_volume;          /*!< Last applied player volume, 0 through 100 */
    atomic_bool            wakeup_prompt_pending;  /*!< Wake-up prompt is playing; start listen when it ends */
};

static void record_first_error(esp_err_t *result, esp_err_t error)
{
    if (*result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

static esp_err_t validate_account_config(void)
{
    if (strlen(CONFIG_BAIDU_APP_ID) == 0 || strlen(CONFIG_BAIDU_USER_ID) == 0 ||
        strlen(CONFIG_BAIDU_SERVER_URL) == 0 || strlen(CONFIG_BAIDU_WORKFLOW) == 0 ||
        CONFIG_BAIDU_INSTANCE_ID <= 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Baidu account values are incomplete");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static uint32_t build_audio_feature_mask(void)
{
    uint32_t mask = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC |
                    ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD;
#if CONFIG_BAIDU_AUDIO_NS
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS;
#endif
#if CONFIG_BAIDU_AUDIO_WN
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN;
#endif
    return mask;
}

static void fill_baidu_config(esp_llm_baidu_cfg_t *cfg)
{
    *cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
    cfg->agent.account.app_id = CONFIG_BAIDU_APP_ID;
    cfg->agent.account.user_id = CONFIG_BAIDU_USER_ID;
    cfg->agent.account.license_key = CONFIG_BAIDU_LICENSE_KEY;
    cfg->agent.account.server_url = CONFIG_BAIDU_SERVER_URL;
    cfg->agent.account.workflow = CONFIG_BAIDU_WORKFLOW;
    cfg->agent.account.instance_id = CONFIG_BAIDU_INSTANCE_ID;
#if CONFIG_BAIDU_APP_SCENE_IMAGE_UNDERSTANDING
    cfg->agent.scene = ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING_CFG_DEFAULT();
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;
#elif CONFIG_BAIDU_APP_SCENE_IMAGE_GENERATION
    cfg->agent.scene = ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION_CFG_DEFAULT();
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = true;
#elif CONFIG_BAIDU_APP_SCENE_MULTI_MODAL
    cfg->agent.scene = ESP_LLM_BAIDU_SCENE_MULTI_MODAL_CFG_DEFAULT();
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = true;
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = true;
#else
    cfg->agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
#endif
#if CONFIG_BAIDU_APP_SCENE_IMAGE_UNDERSTANDING || CONFIG_BAIDU_APP_SCENE_MULTI_MODAL
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.width =
        (uint16_t)(CONFIG_BAIDU_CAMERA_WIDTH & ~1);
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.height =
        (uint16_t)(CONFIG_BAIDU_CAMERA_HEIGHT & ~1);
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].info.info.video.fps =
        (uint16_t)CONFIG_BAIDU_CAMERA_FPS;
#if CONFIG_BAIDU_VISION_MODE_STREAM
    cfg->vision.vision_mode = ESP_LLM_BAIDU_VISION_MODE_STREAM;
#else
    cfg->vision.vision_mode = ESP_LLM_BAIDU_VISION_MODE_IMAGE;
#endif
#endif
    cfg->agent.auto_reconnect = true;
    cfg->agent.wakeup_interrupt_cmd = WAKEUP_LISTEN_CMD;
    cfg->service.name = "baidu_agent";
    cfg->service.audio_ai_cfg.feature_mask = build_audio_feature_mask();
    /* Prompt playback would eat a 2000 ms BEGIN skip. The app interrupts TTS
       on wake-up, then sends BEGIN after the ding so listening starts then. */
}

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

static void arm_cloud_listen(app_agent_t *agent)
{
    if (agent == NULL || agent->llm == NULL) {
        return;
    }
    esp_err_t ret = esp_llm_service_send_custom_data(agent->llm, WAKEUP_LISTEN_CMD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Arm cloud listen failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Arm cloud listen: %s", WAKEUP_LISTEN_CMD);
    }
}

static void start_listen_after_wakeup_prompt(app_agent_t *agent)
{
    if (agent == NULL || agent->llm == NULL) {
        return;
    }
    if (!atomic_exchange(&agent->wakeup_prompt_pending, false)) {
        return;
    }
    ESP_LOGI(TAG, "Wake-up prompt finished, start listen");
    esp_err_t ret = esp_llm_service_cancel_response(agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Start listen after wake-up prompt failed: %s", esp_err_to_name(ret));
    }
    ret = esp_llm_service_keep_awake(agent->llm, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Release keep-awake after wake-up prompt failed: %s", esp_err_to_name(ret));
    }
}

static void start_listen_task(void *arg)
{
    start_listen_after_wakeup_prompt((app_agent_t *)arg);
    esp_gmf_oal_thread_delete(NULL);
}

static void request_start_listen(app_agent_t *agent)
{
    if (agent == NULL) {
        return;
    }
    /* Player events run on player_cmd; Baidu cancel_response needs more stack. */
    esp_gmf_err_t err = esp_gmf_oal_thread_create(NULL, "wk_listen", start_listen_task, agent,
                                                  APP_AGENT_LISTEN_TASK_STACK, APP_AGENT_LISTEN_TASK_PRIO, true, 0);
    if (err != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Defer start-listen failed: %d", (int)err);
    }
}

static void play_url_on_stream(app_agent_t *agent, esp_media_stream_id_t stream, const char *url)
{
    if (agent == NULL || agent->player == NULL) {
        ESP_LOGW(TAG, "Prompt playback ignored: player is not ready");
        return;
    }
    if (url == NULL || url[0] == '\0') {
        ESP_LOGW(TAG, "Prompt playback ignored: URL is empty");
        return;
    }
    if (stream_is_active(agent->player, stream)) {
        (void)esp_player_service_stop(agent->player, stream);
    }
    esp_err_t ret = esp_player_service_set_url(agent->player, stream, url);
    if (ret == ESP_OK) {
        ret = esp_player_service_play(agent->player, stream);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Prompt playback start failed on stream %d: %s",
                 (int)stream, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Play URL mix on stream %d: %s", (int)stream, url);
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
    /* Stop cloud TTS. Mixer EXCLUSIVE on the wake-up stream pauses URL and drops TTS. */
    ret = esp_llm_service_cancel_response(agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Interrupt response on wake-up failed: %s", esp_err_to_name(ret));
    }
    play_url_on_stream(agent, BAIDU_APP_MEDIA_WAKEUP_STREAM, WAKEUP_PROMPT_URI);
    if (!stream_is_active(agent->player, BAIDU_APP_MEDIA_WAKEUP_STREAM)) {
        request_start_listen(agent);
    }
}

static esp_err_t player_event_cb(const esp_player_service_event_msg_t *msg, void *ctx)
{
    app_agent_t *agent = (app_agent_t *)ctx;
    if (msg == NULL || agent == NULL || msg->stream != BAIDU_APP_MEDIA_WAKEUP_STREAM) {
        return ESP_OK;
    }
    if (msg->type == ESP_PLAYER_SERVICE_EVENT_FINISHED ||
        msg->type == ESP_PLAYER_SERVICE_EVENT_ERROR) {
        request_start_listen(agent);
    }
    return ESP_OK;
}

static void handle_backend_audio_player(app_agent_t *agent, const esp_llm_event_t *event, bool start)
{
    if (agent == NULL || agent->player == NULL) {
        ESP_LOGW(TAG, "Backend audio request ignored: player is not ready");
        return;
    }
    if (!start) {
        if (stream_is_active(agent->player, BAIDU_APP_MEDIA_MUSIC_STREAM)) {
            (void)esp_player_service_stop(agent->player, BAIDU_APP_MEDIA_MUSIC_STREAM);
        }
        return;
    }
    if (event->payload == NULL || event->payload_len == 0) {
        ESP_LOGW(TAG, "Backend audio request ignored: URL is empty");
        return;
    }

    char *url = malloc(event->payload_len + 1);
    if (url == NULL) {
        ESP_LOGE(TAG, "Backend audio request ignored: no memory");
        return;
    }
    memcpy(url, event->payload, event->payload_len);
    url[event->payload_len] = '\0';
    play_url_on_stream(agent, BAIDU_APP_MEDIA_MUSIC_STREAM, url);
    free(url);
}

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
        case ESP_LLM_EVENT_MEDIA_READY:
            ESP_LOGI(TAG, "Agent event: media ready");
            break;
        case ESP_LLM_EVENT_TEXT:
            if (event->payload != NULL && event->payload_len > 0) {
                ESP_LOGI(TAG, "Agent event: %s %.*s",
                         event->backend_event_name != NULL ? event->backend_event_name : "TEXT",
                         (int)event->payload_len, (const char *)event->payload);
            }
            break;
        case ESP_LLM_EVENT_WAKEUP:
            if (event->payload != NULL && event->payload_len >= sizeof(esp_llm_wakeup_info_t)) {
                esp_llm_wakeup_info_t info = {0};
                memcpy(&info, event->payload, sizeof(info));
                ESP_LOGI(TAG, "Agent event: wake-up state %d channel %d", info.state, info.trigger_ch);
                if (info.state != 0) {
                    handle_wakeup_start(agent);
                }
            } else {
                ESP_LOGI(TAG, "Agent event: wake-up");
                handle_wakeup_start(agent);
            }
            break;
        case ESP_LLM_EVENT_VAD:
            if (event->payload != NULL && event->payload_len >= sizeof(int)) {
                int vad_state = 0;
                memcpy(&vad_state, event->payload, sizeof(vad_state));
                ESP_LOGI(TAG, "Agent event: VAD state %d", vad_state);
            }
            break;
        case ESP_LLM_EVENT_NONE:
            if (event->backend_event_name == NULL) {
                break;
            }
            if (strcmp(event->backend_event_name, "AUDIO_PLAYER_START") == 0) {
                handle_backend_audio_player(agent, event, true);
            } else if (strcmp(event->backend_event_name, "AUDIO_PLAYER_STOP") == 0) {
                handle_backend_audio_player(agent, event, false);
            } else if (strcmp(event->backend_event_name, "SPEAKING") == 0) {
#if !CONFIG_BAIDU_AUDIO_WN
                /* WakeNet is off, so BEGIN is never sent from the ding path.
                   The cloud listen window opens after each TTS turn. */
                if (event->payload != NULL && event->payload_len >= sizeof(bool)) {
                    bool speaking = false;
                    memcpy(&speaking, event->payload, sizeof(speaking));
                    if (!speaking && !atomic_load(&agent->wakeup_prompt_pending)) {
                        arm_cloud_listen(agent);
                    }
                }
#endif
            } else if (strcmp(event->backend_event_name, "FUNCTION_CALL") == 0) {
                if (event->payload == NULL || event->payload_len == 0) {
                    ESP_LOGW(TAG, "Function call ignored: payload is empty");
                    break;
                }
                char *json = malloc(event->payload_len + 1);
                if (json == NULL) {
                    ESP_LOGE(TAG, "Function call ignored: no memory");
                    break;
                }
                memcpy(json, event->payload, event->payload_len);
                json[event->payload_len] = '\0';
                ESP_LOGI(TAG, "Agent event: FUNCTION_CALL %s", json);
                app_agent_parse_fc_volume_adjust(agent->llm, agent->player,
                                                 &agent->output_volume, json);
                free(json);
            }
            break;
        default:
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
    agent->output_volume = CONFIG_BAIDU_OUTPUT_VOLUME;

    esp_llm_baidu_cfg_t baidu_cfg;
    fill_baidu_config(&baidu_cfg);

    baidu_app_media_cfg_t media_cfg = BAIDU_APP_MEDIA_CFG_DEFAULT();
    ret = baidu_app_media_create(&baidu_cfg, &media_cfg, &agent->media);
    if (ret != ESP_OK) {
        goto fail;  /* baidu_app_media_create already logged */
    }
    esp_capture_service_t *capture = NULL;
    ret = baidu_app_media_get_audio_capture(agent->media, &capture);
    if (ret != ESP_OK) {
        goto fail;  /* getter already logged */
    }
    ret = baidu_app_media_get_player(agent->media, &agent->player);
    if (ret != ESP_OK) {
        goto fail;  /* getter already logged */
    }
    ret = esp_player_service_set_event_cb(agent->player, player_event_cb, agent);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe player events failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    baidu_cfg.service.audio_ai_cfg.capture = capture;
    ret = esp_llm_baidu_create(&baidu_cfg, &agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create Baidu LLM service failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    adf_event_subscribe_info_t subscription = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    subscription.handler = llm_event_handler;
    subscription.handler_ctx = agent;
    ret = esp_service_event_subscribe(ESP_SERVICE_BASE(agent->llm), &subscription);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe LLM events failed: %s", esp_err_to_name(ret));
        goto fail;
    }

    ESP_LOGI(TAG, "Create 'baidu_agent'");
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
    return baidu_app_media_start(agent->media, agent->llm);
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
    return baidu_app_media_stop(agent->media);
}

esp_err_t app_agent_destroy(app_agent_t **agent)
{
    if (agent == NULL || *agent == NULL) {
        ESP_LOGE(TAG, "Destroy failed: agent handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    app_agent_t *ctx = *agent;
    esp_err_t result = app_agent_stop(ctx);
    if (ctx->player != NULL) {
        (void)esp_player_service_set_event_cb(ctx->player, NULL, NULL);
        ctx->player = NULL;
    }
    if (ctx->llm != NULL) {
        (void)esp_service_event_unsubscribe(ESP_SERVICE_BASE(ctx->llm), NULL, ADF_EVENT_ANY_ID);
        record_first_error(&result, esp_llm_service_destroy(&ctx->llm));
        if (ctx->llm != NULL) {
            ESP_LOGW(TAG, "Destroy deferred: LLM service retained for cleanup retry");
            return result;
        }
    }
    if (ctx->media != NULL) {
        record_first_error(&result, baidu_app_media_destroy(&ctx->media));
        if (ctx->media != NULL) {
            ESP_LOGW(TAG, "Destroy deferred: media resources retained for cleanup retry");
            return result;
        }
    }

    free(ctx);
    *agent = NULL;
    ESP_LOGI(TAG, "Destroy 'baidu_agent'");
    return result;
}
