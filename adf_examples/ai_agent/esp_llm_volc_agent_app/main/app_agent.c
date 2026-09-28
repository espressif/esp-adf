/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_log.h"

#include "volc_app_media.h"

#include "app_agent.h"

static const char *TAG = "APP_AGENT";

/**
 * @brief  Private Volc agent application context
 */
struct app_agent {
    volc_app_media_t  *media;  /*!< Application-owned media environment */
    esp_llm_service_t *llm;    /*!< Application-owned Volc LLM service */
};

static void record_first_error(esp_err_t *result, esp_err_t error)
{
    if (*result == ESP_OK && error != ESP_OK) {
        *result = error;
    }
}

static esp_err_t validate_account_config(void)
{
    if (strlen(CONFIG_VOLC_IOT_INSTANCE_ID) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Volc IoT instance ID is empty");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(CONFIG_VOLC_IOT_PRODUCT_KEY) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Volc IoT product key is empty");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(CONFIG_VOLC_IOT_PRODUCT_SECRET) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Volc IoT product secret is empty");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(CONFIG_VOLC_IOT_DEVICE_NAME) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Volc IoT device name is empty");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(CONFIG_VOLC_BOT_ID) == 0) {
        ESP_LOGE(TAG, "Validate configuration failed: Volc bot ID is empty");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static uint32_t build_audio_feature_mask(void)
{
    uint32_t mask = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC |
                    ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_NS;
#if CONFIG_VOLC_AUDIO_VAD
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD;
#endif
#if CONFIG_VOLC_AUDIO_WN
    mask |= ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN;
#endif
    return mask;
}

static void fill_volc_config(esp_llm_volc_cfg_t *cfg)
{
    cfg->iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
    cfg->iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
    cfg->iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
    cfg->iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
    cfg->iot.bot_id = CONFIG_VOLC_BOT_ID;
    cfg->service.name = "volc_agent";
    cfg->service.audio_ai_cfg.feature_mask = build_audio_feature_mask();
    /* volc_app_media is audio-only, so keep video off even when the component's
       Kconfig defaults publish or subscribe it; nothing would link those streams. */
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = false;
    cfg->service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = false;
}

static void llm_event_handler(const adf_event_t *hub_event, void *user_ctx)
{
    app_agent_t *agent = (app_agent_t *)user_ctx;
    if (hub_event == NULL) {
        return;
    }
    /* Wildcard subscribe also sees ESP_SERVICE_EVENT_STATE_CHANGED. */
    if (hub_event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED) {
        return;
    }
    if (hub_event->payload == NULL ||
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
#if CONFIG_VOLC_AUDIO_WN
            if (agent != NULL && agent->llm != NULL) {
                (void)esp_llm_service_enable_uplink_track(agent->llm, ESP_MEDIA_TRACK_TYPE_AUDIO,
                                                         state != 0);
            }
#else
            (void)agent;
#endif
            break;
        }
        default:
            if (event->backend_event_name != NULL &&
                strcmp(event->backend_event_name, "CONV_STATUS") == 0 &&
                event->payload != NULL && event->payload_len > 0) {
                ESP_LOGI(TAG, "Agent event: Conv %.*s",
                         (int)event->payload_len, (const char *)event->payload);
            } else if (event->backend_event_name != NULL) {
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

    esp_llm_volc_cfg_t volc_cfg = ESP_LLM_VOLC_CFG_DEFAULT();
    fill_volc_config(&volc_cfg);

    volc_app_media_cfg_t media_cfg = VOLC_APP_MEDIA_CFG_DEFAULT();
    media_cfg.output_volume = CONFIG_VOLC_OUTPUT_VOLUME;
    ret = volc_app_media_create(&volc_cfg, &media_cfg, &agent->media);
    if (ret != ESP_OK) {
        goto fail;  /* volc_app_media_create already logged */
    }
    esp_capture_service_t *capture = NULL;
    ret = volc_app_media_get_audio_capture(agent->media, &capture);
    if (ret != ESP_OK) {
        goto fail;  /* getter already logged */
    }

    volc_cfg.service.audio_ai_cfg.capture = capture;
    ret = esp_llm_volc_create(&volc_cfg, &agent->llm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create Volc LLM service failed: %s", esp_err_to_name(ret));
        goto fail;
    }

#if CONFIG_VOLC_AUDIO_WN
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

    ESP_LOGI(TAG, "Create 'volc_agent'");
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
    return volc_app_media_start(agent->media, agent->llm);
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
    return volc_app_media_stop(agent->media);
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
    if (ctx->media != NULL) {
        record_first_error(&result, volc_app_media_destroy(&ctx->media));
        if (ctx->media != NULL) {
            ESP_LOGW(TAG, "Destroy deferred: media resources retained for cleanup retry");
            return result;
        }
    }

    free(ctx);
    *agent = NULL;
    ESP_LOGI(TAG, "Destroy 'volc_agent'");
    return result;
}
