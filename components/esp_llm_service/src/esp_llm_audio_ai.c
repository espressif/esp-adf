/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdlib.h>
#include <stdatomic.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_llm_service_internal.h"

static const char *TAG = "ESP_LLM_AUDIO_AI";

#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
static atomic_int s_fail_detach_remaining;
#endif

#define ESP_LLM_AUDIO_AI_CB_VAD   (1U << 0)
#define ESP_LLM_AUDIO_AI_CB_WN    (1U << 1)
#define ESP_LLM_AUDIO_AI_CB_DOA   (1U << 2)

struct esp_llm_audio_ai {
    esp_llm_service_t      *service;
    esp_capture_service_t  *capture;
    esp_llm_audio_ai_cfg_t  cfg;
    uint8_t                 attached;
    bool                    features_set;
    bool                    rollback_failed;
};

static bool capture_is_running(esp_capture_service_t *capture)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    return capture != NULL &&
           esp_service_get_state(ESP_SERVICE_BASE(capture), &state) == ESP_OK &&
           state == ESP_SERVICE_STATE_RUNNING;
}

static void emit_event(esp_llm_audio_ai_t *helper, esp_llm_event_common_id_t id,
                       const char *name, const void *payload, size_t payload_len)
{
    if (helper == NULL) {
        return;
    }

    esp_llm_event_t event = {
        .common_id = id,
        .backend_event_name = (id == ESP_LLM_EVENT_NONE) ? name : NULL,
        .payload_type = ESP_LLM_EVENT_PAYLOAD_TYPE_BINARY,
        .role = ESP_LLM_ROLE_USER,
        .last_payload = true,
        .payload = payload,
        .payload_len = payload_len,
    };
    esp_err_t ret = esp_llm_event_dispatch(helper->service, &event);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Dispatch AI audio event failed: %s", esp_err_to_name(ret));
    }
}

static void vad_callback(int state, void *ctx)
{
    emit_event((esp_llm_audio_ai_t *)ctx, ESP_LLM_EVENT_VAD,
               "VAD", &state, sizeof(state));
}

static void wakeup_callback(int state, int trigger_channel, void *ctx)
{
    esp_llm_audio_ai_t *helper = (esp_llm_audio_ai_t *)ctx;
    if (helper == NULL) {
        return;
    }
    esp_llm_wakeup_info_t info = {
        .state = state,
        .trigger_ch = trigger_channel,
    };
    emit_event(helper, ESP_LLM_EVENT_WAKEUP, state ? "WAKEUP" : "WAKEUP_END",
               &info, sizeof(info));
    if (state != 0 && atomic_load(&helper->service->wakeup_cancel_response)) {
        esp_err_t ret = esp_llm_service_cancel_response(helper->service);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Wake-up response cancellation failed: %s", esp_err_to_name(ret));
        }
    }
}

static void doa_callback(float doa_result, void *ctx)
{
    emit_event((esp_llm_audio_ai_t *)ctx, ESP_LLM_EVENT_NONE,
               "DOA", &doa_result, sizeof(doa_result));
}

static esp_err_t attach_callbacks(esp_llm_audio_ai_t *helper)
{
    uint32_t mask = helper->cfg.feature_mask;
    esp_err_t ret;

    if ((mask & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD) != 0) {
        ret = esp_capture_service_ai_audio_src_set_vad_cb(helper->capture, vad_callback, helper);
        if (ret != ESP_OK) {
            return ret;
        }
        helper->attached |= ESP_LLM_AUDIO_AI_CB_VAD;
    }
    if ((mask & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN) != 0) {
        ret = esp_capture_service_ai_audio_src_set_wn_cb(helper->capture, wakeup_callback, helper);
        if (ret != ESP_OK) {
            return ret;
        }
        helper->attached |= ESP_LLM_AUDIO_AI_CB_WN;
    }
    if ((mask & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_DOA) != 0) {
        ret = esp_capture_service_ai_audio_src_set_doa_cb(helper->capture, doa_callback, helper);
        if (ret != ESP_OK) {
            return ret;
        }
        helper->attached |= ESP_LLM_AUDIO_AI_CB_DOA;
    }
    return ESP_OK;
}

static esp_err_t detach_callbacks(esp_llm_audio_ai_t *helper)
{
    esp_err_t ret = ESP_OK;
    if ((helper->attached & ESP_LLM_AUDIO_AI_CB_VAD) != 0) {
        esp_err_t err = esp_capture_service_ai_audio_src_set_vad_cb(helper->capture, NULL, NULL);
        record_first_error(&ret, err);
        if (err == ESP_OK) {
            helper->attached &= ~ESP_LLM_AUDIO_AI_CB_VAD;
        }
    }
    if ((helper->attached & ESP_LLM_AUDIO_AI_CB_WN) != 0) {
        esp_err_t err = esp_capture_service_ai_audio_src_set_wn_cb(helper->capture, NULL, NULL);
        record_first_error(&ret, err);
        if (err == ESP_OK) {
            helper->attached &= ~ESP_LLM_AUDIO_AI_CB_WN;
        }
    }
    if ((helper->attached & ESP_LLM_AUDIO_AI_CB_DOA) != 0) {
        esp_err_t err = esp_capture_service_ai_audio_src_set_doa_cb(helper->capture, NULL, NULL);
        record_first_error(&ret, err);
        if (err == ESP_OK) {
            helper->attached &= ~ESP_LLM_AUDIO_AI_CB_DOA;
        }
    }
    return ret;
}

static esp_err_t clear_ai_features(esp_llm_audio_ai_t *helper)
{
    if (helper == NULL || helper->capture == NULL || !helper->features_set) {
        return ESP_OK;
    }
    esp_err_t ret = esp_capture_service_ai_audio_src_set_feature(helper->capture, 0, NULL);
    if (ret == ESP_OK) {
        helper->features_set = false;
    }
    return ret;
}

bool esp_llm_audio_ai_requested(const esp_llm_audio_ai_cfg_t *cfg)
{
    if (cfg == NULL) {
        return false;
    }
    return cfg->feature_mask != 0;
}

esp_err_t esp_llm_audio_ai_attach(esp_llm_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Attach failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (service->audio_ai != NULL) {
        if (service->audio_ai->rollback_failed) {
            ESP_LOGE(TAG, "Attach failed: a previous rollback left capture AI state attached");
            return ESP_ERR_INVALID_STATE;
        }
        return ESP_OK;
    }
    if (!esp_llm_audio_ai_requested(&service->cfg.audio_ai_cfg)) {
        return ESP_OK;
    }

    esp_capture_service_t *capture = service->cfg.audio_ai_cfg.capture;
    if (capture == NULL) {
        ESP_LOGE(TAG, "Attach failed: AI audio needs an audio capture");
        return ESP_ERR_INVALID_ARG;
    }
    if (capture_is_running(capture)) {
        ESP_LOGE(TAG, "Attach failed: capture is busy, another agent may still be running");
        return ESP_ERR_INVALID_STATE;
    }

    esp_llm_audio_ai_t *helper = calloc(1, sizeof(*helper));
    if (helper == NULL) {
        return ESP_ERR_NO_MEM;
    }
    helper->service = service;
    helper->capture = capture;
    helper->cfg = service->cfg.audio_ai_cfg;

    esp_err_t ret = attach_callbacks(helper);
    if (ret != ESP_OK) {
        goto fail;
    }

    if (helper->cfg.feature_mask != 0) {
        ret = esp_capture_service_ai_audio_src_set_feature(capture, helper->cfg.feature_mask,
                                                           &helper->cfg.feature_cfg);
        if (ret != ESP_OK) {
            goto fail;
        }
        helper->features_set = true;
    }

    service->audio_ai = helper;
    return ESP_OK;

fail:
    {
        esp_err_t detach_ret = detach_callbacks(helper);
        record_first_error(&detach_ret, clear_ai_features(helper));
        if (detach_ret != ESP_OK) {
            ESP_LOGE(TAG, "Attach rollback failed: callback detach returned %s",
                     esp_err_to_name(detach_ret));
            helper->rollback_failed = true;
            service->audio_ai = helper;
            return detach_ret;
        }
        free(helper);
        return ret;
    }
}

#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
void esp_llm_audio_ai_test_fail_next_detach(int count)
{
    atomic_store(&s_fail_detach_remaining, count > 0 ? count : 0);
}
#endif

esp_err_t esp_llm_audio_ai_detach(esp_llm_service_t *service)
{
#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
    int fail_remaining = atomic_load(&s_fail_detach_remaining);
    if (fail_remaining > 0) {
        atomic_store(&s_fail_detach_remaining, fail_remaining - 1);
        return ESP_FAIL;
    }
#endif
    if (service == NULL || service->audio_ai == NULL) {
        return ESP_OK;
    }

    esp_llm_audio_ai_t *helper = service->audio_ai;
    esp_err_t ret = detach_callbacks(helper);
    record_first_error(&ret, clear_ai_features(helper));
    if (helper->attached != 0 || helper->features_set) {
        ESP_LOGE(TAG, "Detach failed: capture AI state is still attached");
        return ret != ESP_OK ? ret : ESP_ERR_INVALID_STATE;
    }
    free(helper);
    service->audio_ai = NULL;
    return ret;
}

static esp_err_t audio_ai_get_capture(esp_llm_service_t *service, const char *what,
                                      esp_capture_service_t **out_capture)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "%s failed: service is NULL", what);
        return ESP_ERR_INVALID_ARG;
    }
    if (service->audio_ai == NULL || service->audio_ai->capture == NULL) {
        ESP_LOGE(TAG, "%s failed: audio AI is not attached", what);
        return ESP_ERR_INVALID_STATE;
    }
    *out_capture = service->audio_ai->capture;
    return ESP_OK;
}

esp_err_t esp_llm_audio_ai_trigger_wakeup(esp_llm_service_t *service)
{
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = audio_ai_get_capture(service, "Trigger wakeup", &capture);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_capture_service_ai_audio_src_trigger_wakeup(capture);
}

esp_err_t esp_llm_audio_ai_trigger_sleep(esp_llm_service_t *service)
{
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = audio_ai_get_capture(service, "Trigger sleep", &capture);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_capture_service_ai_audio_src_trigger_sleep(capture);
}

esp_err_t esp_llm_audio_ai_keep_awake(esp_llm_service_t *service, bool enable)
{
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = audio_ai_get_capture(service, "Keep awake", &capture);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_capture_service_ai_audio_src_keep_awake(capture, enable);
}
