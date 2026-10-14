/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

#include "esp_llm_adapter_utils.h"
#include "esp_llm_service_internal.h"

static const char *TAG = "ESP_LLM_SVC";

static esp_err_t validate_adapter_desc(const esp_llm_adapter_desc_t *adapter)
{
    if (adapter == NULL || adapter->name == NULL || adapter->name[0] == '\0' ||
        adapter->ops == NULL || adapter->ops->create == NULL ||
        adapter->ops->destroy == NULL || adapter->ops->start == NULL ||
        adapter->ops->stop == NULL) {
        ESP_LOGE(TAG, "Create failed: invalid adapter descriptor");
        return ESP_ERR_INVALID_ARG;
    }
    if ((adapter->capabilities &
         (ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_VIDEO_INPUT | ESP_LLM_CAP_VISION)) != 0 &&
        adapter->ops->send_frame == NULL) {
        ESP_LOGE(TAG, "Create failed: adapter advertises media input without send_frame");
        return ESP_ERR_INVALID_ARG;
    }
    if ((adapter->capabilities & ESP_LLM_CAP_TEXT_INPUT) != 0 &&
        adapter->ops->send_text == NULL) {
        ESP_LOGE(TAG, "Create failed: adapter advertises text input without send_text");
        return ESP_ERR_INVALID_ARG;
    }
    if ((adapter->capabilities & ESP_LLM_CAP_CUSTOM_DATA) != 0 &&
        adapter->ops->send_custom_data == NULL) {
        ESP_LOGE(TAG, "Create failed: adapter advertises custom data without send_custom_data");
        return ESP_ERR_INVALID_ARG;
    }
    if ((adapter->capabilities & ESP_LLM_CAP_CANCEL_RESPONSE) != 0 &&
        adapter->ops->cancel_response == NULL) {
        ESP_LOGE(TAG, "Create failed: adapter advertises cancel without cancel_response");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t validate_create(const esp_llm_adapter_desc_t *adapter,
                                 const void *adapter_cfg, size_t adapter_cfg_size,
                                 const esp_llm_service_cfg_t *cfg)
{
    esp_err_t ret = validate_adapter_desc(adapter);
    if (ret != ESP_OK) {
        return ret;  /* helper already logged */
    }
    if (cfg == NULL || ((adapter_cfg == NULL) != (adapter_cfg_size == 0))) {
        ESP_LOGE(TAG, "Create failed: invalid service config");
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->downlink_cb != NULL &&
        (adapter->capabilities & (ESP_LLM_CAP_AUDIO_OUTPUT | ESP_LLM_CAP_VIDEO_OUTPUT)) == 0) {
        ESP_LOGE(TAG, "Create failed: downlink callback set but adapter has no media output");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t validate_downlink_callbacks(const esp_llm_service_t *service)
{
    if (service->cfg.downlink_cb != NULL &&
        !service->paths[ESP_LLM_MEDIA_STREAM_AUDIO_DN].enabled &&
        !service->paths[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled) {
        ESP_LOGE(TAG, "Create failed: downlink callback set but AUDIO_DN and VIDEO_DN are disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t validate_audio_ai(esp_llm_service_t *service)
{
    if (!esp_llm_audio_ai_requested(&service->cfg.audio_ai_cfg)) {
        return ESP_OK;
    }
    if (service->cfg.audio_ai_cfg.capture == NULL) {
        ESP_LOGE(TAG, "Create failed: audio_ai_cfg requires a borrowed audio capture");
        return ESP_ERR_INVALID_ARG;
    }
    if ((service->effective_capabilities & ESP_LLM_CAP_AUDIO_INPUT) == 0) {
        ESP_LOGE(TAG, "Create failed: audio AI requires AUDIO_UP");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_llm_capability_t effective_caps(const esp_llm_service_t *service)
{
    uint32_t caps = service->adapter_desc != NULL ?
                    (uint32_t)service->adapter_desc->capabilities : 0;
    static const uint32_t stream_caps[ESP_LLM_MEDIA_STREAM_COUNT] = {
        ESP_LLM_CAP_AUDIO_INPUT,
        ESP_LLM_CAP_VIDEO_INPUT,
        ESP_LLM_CAP_AUDIO_OUTPUT,
        ESP_LLM_CAP_VIDEO_OUTPUT,
    };
    for (int i = 0; i < ESP_LLM_MEDIA_STREAM_COUNT; i++) {
        if (!service->paths[i].enabled) {
            caps &= ~stream_caps[i];
        }
    }
    return (esp_llm_capability_t)caps;
}

static esp_err_t adapter_precheck(esp_llm_service_t *service, bool op_available,
                                  esp_llm_capability_t cap, const char *what)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Send %s failed: service is NULL", what);
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_state_t state;
    esp_err_t ret = esp_service_get_state(ESP_SERVICE_BASE(service), &state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send %s failed: get state returned %s", what, esp_err_to_name(ret));
        return ret;
    }
    if (state != ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGE(TAG, "Send %s failed: service state is %s",
                 what, esp_llm_service_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    if ((esp_llm_service_caps(service) & cap) == 0) {
        ESP_LOGW(TAG, "Send %s failed: backend '%s' does not support %s",
                 what, esp_llm_adapter_name(service), what);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!op_available) {
        ESP_LOGW(TAG, "Send %s failed: backend '%s' does not implement %s",
                 what, esp_llm_adapter_name(service), what);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static esp_err_t require_running(esp_llm_service_t *service, const char *what)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "%s failed: service is NULL", what);
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_state_t state;
    esp_err_t ret = esp_service_get_state(ESP_SERVICE_BASE(service), &state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: get state returned %s", what, esp_err_to_name(ret));
        return ret;
    }
    if (state != ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGE(TAG, "%s failed: service is not running", what);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t service_event_cb(void *ctx, const esp_llm_event_t *event)
{
    return esp_llm_event_dispatch((esp_llm_service_t *)ctx, event);
}

static esp_err_t service_write_frame(void *ctx, const esp_media_frame_t *frame)
{
    esp_llm_service_t *service = (esp_llm_service_t *)ctx;

    if (service == NULL || frame == NULL) {
        ESP_LOGE(TAG, "Write frame failed: service or frame is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO && frame->type != ESP_MEDIA_TRACK_TYPE_VIDEO) {
        ESP_LOGE(TAG, "Write frame failed: unsupported track type %d", (int)frame->type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return esp_llm_media_write(service, frame);
}

static esp_err_t destroy_adapter_ctx(esp_llm_service_t *service)
{
    if (service->adapter_ctx == NULL) {
        return ESP_OK;
    }
    esp_err_t ret = service->adapter_desc->ops->destroy(service->adapter_ctx);
    if (ret != ESP_OK) {
        return ret;
    }
    service->adapter_ctx = NULL;
    return ESP_OK;
}

static esp_err_t service_on_start(esp_service_t *base)
{
    esp_llm_service_t *service = (esp_llm_service_t *)base;
    if (service == NULL) {
        ESP_LOGE(TAG, "Start failed: service base is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const char *name = esp_llm_adapter_name(service);
    esp_err_t ret = esp_llm_audio_ai_attach(service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start '%s' failed: AI audio attach returned %s",
                 name, esp_err_to_name(ret));
        goto detach_audio_ai;
    }

    ret = esp_llm_media_prepare(service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start '%s' failed: media prepare returned %s",
                 name, esp_err_to_name(ret));
        goto detach_audio_ai;
    }

    ret = service->adapter_desc->ops->start(service->adapter_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start '%s' failed: adapter returned %s", name, esp_err_to_name(ret));
        goto detach_audio_ai;
    }

    atomic_store(&service->uplink_send_allowed, true);

    ret = esp_llm_media_start_uplink(service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start '%s' failed: uplink start returned %s",
                 name, esp_err_to_name(ret));
        if (esp_llm_uplink_running(service)) {
            atomic_store(&service->uplink_send_allowed, false);
            return ret;
        }
        goto stop_adapter;
    }
    return ESP_OK;

stop_adapter:
    {
        esp_err_t stop_ret = service->adapter_desc->ops->stop(service->adapter_ctx);
        if (stop_ret != ESP_OK) {
            ESP_LOGW(TAG, "Start '%s' rollback: adapter stop returned %s",
                     name, esp_err_to_name(stop_ret));
        }
    }
detach_audio_ai:
    {
        esp_err_t detach_ret = esp_llm_audio_ai_detach(service);
        if (detach_ret != ESP_OK) {
            ESP_LOGW(TAG, "Start '%s' rollback: AI audio detach returned %s",
                     name, esp_err_to_name(detach_ret));
        }
    }
    return ret;
}

static esp_err_t service_on_stop(esp_service_t *base)
{
    esp_llm_service_t *service = (esp_llm_service_t *)base;
    if (service == NULL) {
        ESP_LOGE(TAG, "Stop failed: service base is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_llm_media_stop_uplink(service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop '%s': uplink stop returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(ret));
    }
    if (esp_llm_uplink_running(service)) {
        /* The adapter may still be called by a consume task. Keep it and the
           downlink alive so a later stop/destroy can retry safely. */
        return ret != ESP_OK ? ret : ESP_ERR_TIMEOUT;
    }

    /* Capture is stopped; release AFE before adapter/downlink teardown so
       another instance can attach even if this stop later fails. */
    esp_err_t detach_ret = esp_llm_audio_ai_detach(service);
    if (detach_ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop '%s': AI audio detach returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(detach_ret));
        record_first_error(&ret, detach_ret);
    }

    esp_err_t adapter_ret = service->adapter_desc->ops->stop(service->adapter_ctx);
    if (adapter_ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop '%s' failed: adapter returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(adapter_ret));
        record_first_error(&ret, adapter_ret);
        /* Keep the downlink alive until adapter callbacks stop. */
        return ret;
    }
    esp_err_t downlink_ret = esp_llm_media_stop_downlink(service);
    if (downlink_ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop '%s': downlink stop returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(downlink_ret));
        record_first_error(&ret, downlink_ret);
    }
    return ret;
}

static esp_err_t release_service_resources(esp_llm_service_t *service)
{
    esp_err_t ret = esp_llm_audio_ai_detach(service);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = destroy_adapter_ctx(service);
    if (ret != ESP_OK) {
        return ret;
    }
    service->adapter_desc = NULL;
    esp_llm_media_destroy_downlink(service);
    return ESP_OK;
}

static esp_err_t service_on_deinit(esp_service_t *base)
{
    esp_llm_service_t *service = (esp_llm_service_t *)base;
    if (service == NULL) {
        ESP_LOGE(TAG, "Deinitialize failed: service base is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    return release_service_resources(service);
}

static const esp_service_ops_t s_service_ops = {
    .on_start = service_on_start,
    .on_stop = service_on_stop,
    .on_deinit = service_on_deinit,
    .event_to_name = esp_llm_event_to_name,
};

static void destroy_uplink_semaphores(esp_llm_service_t *service)
{
    for (int i = ESP_LLM_MEDIA_STREAM_AUDIO_UP; i <= ESP_LLM_MEDIA_STREAM_VIDEO_UP; i++) {
        esp_llm_media_path_t *path = &service->paths[i];
        if (path->up.done != NULL) {
            vSemaphoreDelete(path->up.done);
            path->up.done = NULL;
        }
    }
}

esp_err_t esp_llm_service_create(const esp_llm_adapter_desc_t *adapter,
                                 const void *adapter_cfg,
                                 size_t adapter_cfg_size,
                                 const esp_llm_service_cfg_t *cfg,
                                 esp_llm_service_t **out_service)
{
    esp_err_t ret = validate_create(adapter, adapter_cfg, adapter_cfg_size, cfg);
    if (ret != ESP_OK) {
        return ret;
    }
    if (out_service == NULL) {
        ESP_LOGE(TAG, "Create failed: out_service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    *out_service = NULL;

    esp_llm_service_t *service = (esp_llm_service_t *)calloc(1, sizeof(*service));
    if (service == NULL) {
        ESP_LOGE(TAG, "Create '%s' failed: memory allocation failed", adapter->name);
        return ESP_ERR_NO_MEM;
    }

    service->adapter_desc = adapter;
    service->cfg = *cfg;
    ret = esp_llm_media_resolve_paths(service);
    if (ret != ESP_OK) {
        goto fail;
    }
    service->effective_capabilities = effective_caps(service);

    ret = validate_downlink_callbacks(service);
    if (ret != ESP_OK) {
        goto fail;
    }

    atomic_init(&service->audio_uplink_enabled, true);
    atomic_init(&service->video_uplink_enabled, true);
    atomic_init(&service->uplink_send_allowed, false);
    atomic_init(&service->wakeup_cancel_response, false);

    ret = validate_audio_ai(service);
    if (ret != ESP_OK) {
        goto fail;
    }

    ret = esp_llm_media_create_downlink(service);
    if (ret != ESP_OK) {
        goto fail;
    }

    esp_llm_adapter_cbs_t cbs = {
        .event_cb = service_event_cb,
        .event_ctx = service,
        .downlink_frame = service_write_frame,
        .downlink_frame_ctx = service,
    };
    ret = service->adapter_desc->ops->create(
        adapter_cfg, adapter_cfg_size, &cbs, &service->adapter_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create '%s' failed: adapter create returned %s",
                 adapter->name, esp_err_to_name(ret));
        goto fail;
    }
    esp_media_service_config_t media_cfg = {
        .name = cfg->name != NULL ? cfg->name : adapter->name,
        .service_ops = &s_service_ops,
        .media_ops = esp_llm_media_ops(),
    };
    ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create '%s' failed: esp_media_service_init returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(ret));
        goto fail;
    }
    *out_service = service;
    ESP_LOGI(TAG, "Create '%s': service created, adapter version %s",
             esp_llm_adapter_name(service),
             adapter->api_version != NULL ? adapter->api_version : "unknown");
    return ESP_OK;

fail:
    (void)destroy_adapter_ctx(service);
    esp_llm_media_destroy_downlink(service);
    destroy_uplink_semaphores(service);
    free(service);
    return ret;
}

esp_err_t esp_llm_service_destroy(esp_llm_service_t **service)
{
    if (service == NULL || *service == NULL) {
        ESP_LOGE(TAG, "Destroy failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_service_t *svc = *service;
    if (esp_llm_uplink_running(svc)) {
        ESP_LOGE(TAG, "Destroy '%s' failed: uplink task is still running; call esp_service_stop() first",
                 esp_llm_adapter_name(svc));
        return ESP_ERR_INVALID_STATE;
    }

    const char *adapter_name = esp_llm_adapter_name(svc);
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    esp_err_t state_ret = esp_service_get_state(ESP_SERVICE_BASE(svc), &state);
    if (state_ret != ESP_OK) {
        ESP_LOGE(TAG, "Destroy '%s' failed: get state returned %s",
                 adapter_name, esp_err_to_name(state_ret));
        return state_ret;
    }

    esp_err_t release_ret = ESP_OK;
    if (state != ESP_SERVICE_STATE_UNINITIALIZED) {
        /* esp_service_deinit() ignores on_deinit errors. adapter_ctx is checked below. */
        release_ret = esp_media_service_deinit(ESP_SERVICE_BASE(svc));
    } else if (svc->adapter_ctx != NULL) {
        /* A previous destroy already deinited the base and kept this handle. */
        release_ret = release_service_resources(svc);
    }
    if (release_ret != ESP_OK || svc->adapter_ctx != NULL) {
        esp_err_t ret = release_ret != ESP_OK ? release_ret : ESP_FAIL;
        ESP_LOGE(TAG, "Destroy '%s' failed: adapter release returned %s; service retained for retry",
                 adapter_name, esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Destroy '%s': service destroyed", adapter_name);
    destroy_uplink_semaphores(svc);
    free(svc);
    *service = NULL;
    return ESP_OK;
}

esp_err_t esp_llm_service_send_text(esp_llm_service_t *service, const char *text)
{
    if (text == NULL || text[0] == '\0') {
        ESP_LOGE(TAG, "Send text failed: text is empty");
        return ESP_ERR_INVALID_ARG;
    }
    const esp_llm_adapter_ops_t *ops = esp_llm_adapter_ops(service);
    esp_err_t ret = adapter_precheck(service, ops != NULL && ops->send_text != NULL,
                                     ESP_LLM_CAP_TEXT_INPUT, "text");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ops->send_text(service->adapter_ctx, text);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send text failed: backend '%s' returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_llm_service_send_frame(esp_llm_service_t *service, const esp_media_frame_t *frame)
{
    if (service == NULL || frame == NULL) {
        ESP_LOGE(TAG, "Send frame failed: service or frame is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (frame->size == 0 || frame->data == NULL) {
        ESP_LOGE(TAG, "Send frame failed: frame payload is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_llm_capability_t cap = (esp_llm_capability_t)0;
    const char *kind = "media";
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        cap = ESP_LLM_CAP_AUDIO_INPUT;
        kind = "audio";
    } else if (frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        cap = (esp_llm_capability_t)(ESP_LLM_CAP_VIDEO_INPUT | ESP_LLM_CAP_VISION);
        kind = "video/vision";
    } else {
        ESP_LOGE(TAG, "Send frame failed: unsupported track type %d", (int)frame->type);
        return ESP_ERR_INVALID_ARG;
    }

    const esp_llm_adapter_ops_t *ops = esp_llm_adapter_ops(service);
    esp_err_t ret = adapter_precheck(service, ops != NULL && ops->send_frame != NULL, cap, kind);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!esp_llm_uplink_type_enabled(service, frame->type)) {
        return ESP_OK;
    }
    ret = ops->send_frame(service->adapter_ctx, frame);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE && ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGE(TAG, "Send %s failed: backend '%s' returned %s",
                 kind, esp_llm_adapter_name(service), esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_llm_service_send_custom_data(esp_llm_service_t *service, const char *data)
{
    if (data == NULL || data[0] == '\0') {
        ESP_LOGE(TAG, "Send custom data failed: data is empty");
        return ESP_ERR_INVALID_ARG;
    }
    const esp_llm_adapter_ops_t *ops = esp_llm_adapter_ops(service);
    esp_err_t ret = adapter_precheck(service, ops != NULL && ops->send_custom_data != NULL,
                                     ESP_LLM_CAP_CUSTOM_DATA, "custom data");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ops->send_custom_data(service->adapter_ctx, data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send custom data failed: backend '%s' returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_llm_service_cancel_response(esp_llm_service_t *service)
{
    const esp_llm_adapter_ops_t *ops = esp_llm_adapter_ops(service);
    esp_err_t ret = adapter_precheck(service, ops != NULL && ops->cancel_response != NULL,
                                     ESP_LLM_CAP_CANCEL_RESPONSE, "cancel response");
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ops->cancel_response(service->adapter_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cancel response failed: backend '%s' returned %s",
                 esp_llm_adapter_name(service), esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_llm_service_enable_uplink_track(esp_llm_service_t *service,
                                             esp_media_track_type_t type,
                                             bool enabled)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Enable uplink track failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        atomic_store(&service->audio_uplink_enabled, enabled);
        return ESP_OK;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        atomic_store(&service->video_uplink_enabled, enabled);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Enable uplink track failed: type %d is not audio or video", (int)type);
    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_llm_service_trigger_wakeup(esp_llm_service_t *service)
{
    esp_err_t ret = require_running(service, "Trigger wakeup");
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_llm_audio_ai_trigger_wakeup(service);
}

esp_err_t esp_llm_service_trigger_sleep(esp_llm_service_t *service)
{
    esp_err_t ret = require_running(service, "Trigger sleep");
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_llm_audio_ai_trigger_sleep(service);
}

esp_err_t esp_llm_service_keep_awake(esp_llm_service_t *service, bool enable)
{
    esp_err_t ret = require_running(service, "Keep awake");
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_llm_audio_ai_keep_awake(service, enable);
}

esp_err_t esp_llm_service_set_wakeup_cancel_response(esp_llm_service_t *service, bool enable)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set wakeup cancel response failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_llm_audio_ai_requested(&service->cfg.audio_ai_cfg) ||
        (service->cfg.audio_ai_cfg.feature_mask & ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN) == 0) {
        ESP_LOGE(TAG, "Set wakeup cancel response failed: WakeNet is not enabled");
        return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&service->wakeup_cancel_response, enable);
    return ESP_OK;
}

esp_err_t esp_llm_service_set_downlink_cache_size(esp_llm_service_t *service,
                                                  esp_media_track_type_t type,
                                                  uint32_t cache_size)
{
    if (service == NULL ||
        (type != ESP_MEDIA_TRACK_TYPE_AUDIO && type != ESP_MEDIA_TRACK_TYPE_VIDEO)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    esp_err_t ret = esp_service_get_state(ESP_SERVICE_BASE(service), &state);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state == ESP_SERVICE_STATE_RUNNING || service->downlink.provider_exported) {
        return ESP_ERR_INVALID_STATE;
    }

    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        service->downlink.audio_cache_size = cache_size;
    } else {
        service->downlink.video_cache_size = cache_size;
    }
    esp_llm_media_destroy_downlink(service);
    return esp_llm_media_create_downlink(service);
}

esp_err_t esp_llm_service_get_capabilities(esp_llm_service_t *service, esp_llm_capability_t *out_caps)
{
    if (service == NULL || out_caps == NULL) {
        ESP_LOGE(TAG, "Get capabilities failed: service or out_caps is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_caps = esp_llm_service_caps(service);
    return ESP_OK;
}

esp_err_t esp_llm_service_get_stream_info(esp_llm_service_t *service,
                                          esp_llm_media_stream_t stream,
                                          esp_media_track_info_t *out_info)
{
    if (service == NULL || out_info == NULL || stream >= ESP_LLM_MEDIA_STREAM_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!service->paths[stream].enabled) {
        return ESP_ERR_NOT_FOUND;
    }
    *out_info = service->paths[stream].info;
    return ESP_OK;
}

esp_err_t esp_llm_adapter_ctx_get(esp_llm_service_t *service,
                                  const char *adapter_name,
                                  void **out_ctx)
{
    if (service == NULL || adapter_name == NULL || out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ctx = NULL;

    if (strcmp(esp_llm_adapter_name(service), adapter_name) != 0) {
        ESP_LOGE(TAG, "Adapter context lookup failed: '%s' requested on backend '%s'",
                 adapter_name, esp_llm_adapter_name(service));
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (service->adapter_ctx == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    *out_ctx = service->adapter_ctx;
    return ESP_OK;
}
