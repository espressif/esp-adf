/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_service.h"

static const char *TAG = "ESP_SERVICE";

static const char *s_state_names[] = {
    [ESP_SERVICE_STATE_UNINITIALIZED] = "UNINITIALIZED",
    [ESP_SERVICE_STATE_INITIALIZED]   = "INITIALIZED",
    [ESP_SERVICE_STATE_RUNNING]       = "RUNNING",
    [ESP_SERVICE_STATE_PAUSED]        = "PAUSED",
    [ESP_SERVICE_STATE_STOPPING]      = "STOPPING",
    [ESP_SERVICE_STATE_ERROR]         = "ERROR",
};

static inline const char *get_state_name(esp_service_state_t state)
{
    if (state < ESP_SERVICE_STATE_MAX) {
        return s_state_names[state];
    }
    return "UNKNOWN";
}

static esp_err_t get_state(const esp_service_t *service, esp_service_state_t *out_state)
{
    if (service == NULL || out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_state = atomic_load_explicit((atomic_int *)&service->state, memory_order_acquire);
    return ESP_OK;
}

static void set_state(esp_service_t *service, esp_service_state_t new_state)
{
    atomic_store_explicit((atomic_int *)&service->state, (int)new_state, memory_order_release);
}

static void free_event_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static void publish_core_state_changed_event(esp_service_t *service, esp_service_state_t old_state,
                                             esp_service_state_t new_state)
{
    if (service == NULL || service->event_hub == NULL || old_state == new_state) {
        return;
    }
    esp_service_state_changed_payload_t *payload = malloc(sizeof(*payload));
    if (payload == NULL) {
        ESP_LOGW(TAG, "[%s] Failed to allocate state changed payload", service->name);
        return;
    }
    payload->old_state = old_state;
    payload->new_state = new_state;

    adf_event_t evt = {
        .domain = service->name,
        .event_id = ESP_SERVICE_EVENT_STATE_CHANGED,
        .payload = payload,
        .payload_len = sizeof(*payload),
    };
    /* Ownership transfers to hub: free_event_payload handles free on every
     * path (success or any non-INVALID_ARG error). INVALID_ARG is unreachable
     * because service->event_hub and &evt are both non-NULL here. */
    esp_err_t ret = adf_event_hub_publish(service->event_hub, &evt, free_event_payload, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[%s] Publish state changed failed: %s", service->name, esp_err_to_name(ret));
    }
}

static esp_err_t set_state_internal(esp_service_t *service, esp_service_state_t new_state)
{
    esp_service_state_t old_state;
    esp_err_t ret = get_state(service, &old_state);
    if (ret != ESP_OK) {
        return ret;
    }
    set_state(service, new_state);
    publish_core_state_changed_event(service, old_state, new_state);
    ESP_LOGD(TAG, "[%s] State: %s -> %s", service->name,
             get_state_name(old_state), get_state_name(new_state));
    return ESP_OK;
}

static esp_err_t handle_start_sync(esp_service_t *service)
{
    esp_service_state_t state;
    get_state(service, &state);
    if (state != ESP_SERVICE_STATE_INITIALIZED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_start) {
        esp_err_t ret = service->ops->on_start(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_start failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            service->last_err = ret;
            set_state_internal(service, ESP_SERVICE_STATE_ERROR);
            return ret;
        }
    }
    service->last_err = ESP_OK;
    set_state_internal(service, ESP_SERVICE_STATE_RUNNING);
    ESP_LOGI(TAG, "[%s] Started", service->name);
    return ESP_OK;
}

static esp_err_t handle_stop_sync(esp_service_t *service)
{
    esp_service_state_t state;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_UNINITIALIZED ||
        state == ESP_SERVICE_STATE_INITIALIZED) {
        return ESP_OK;
    }
    if (service->ops && service->ops->on_stop) {
        esp_err_t ret = service->ops->on_stop(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_stop failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            service->last_err = ret;
            set_state_internal(service, ESP_SERVICE_STATE_ERROR);
            return ret;
        }
    }
    service->last_err = ESP_OK;
    set_state_internal(service, ESP_SERVICE_STATE_INITIALIZED);
    ESP_LOGI(TAG, "[%s] Stopped (sync)", service->name);
    return ESP_OK;
}

static esp_err_t handle_pause_sync(esp_service_t *service)
{
    esp_service_state_t state;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_INITIALIZED || state == ESP_SERVICE_STATE_PAUSED) {
        return ESP_OK;
    }
    if (state != ESP_SERVICE_STATE_RUNNING) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_pause) {
        esp_err_t ret = service->ops->on_pause(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_pause failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            service->last_err = ret;
            set_state_internal(service, ESP_SERVICE_STATE_ERROR);
            return ret;
        }
    }
    service->last_err = ESP_OK;
    set_state_internal(service, ESP_SERVICE_STATE_PAUSED);
    return ESP_OK;
}

static esp_err_t handle_resume_sync(esp_service_t *service)
{
    esp_service_state_t state;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_INITIALIZED || state == ESP_SERVICE_STATE_RUNNING) {
        return ESP_OK;
    }
    if (state != ESP_SERVICE_STATE_PAUSED) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_resume) {
        esp_err_t ret = service->ops->on_resume(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_resume failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            service->last_err = ret;
            set_state_internal(service, ESP_SERVICE_STATE_ERROR);
            return ret;
        }
    }
    service->last_err = ESP_OK;
    set_state_internal(service, ESP_SERVICE_STATE_RUNNING);
    return ESP_OK;
}

esp_err_t esp_service_init(esp_service_t *service, const esp_service_config_t *config, const esp_service_ops_t *ops)
{
    if (service == NULL || config == NULL) {
        ESP_LOGE(TAG, "Init: invalid arg (service=%p, config=%p)", (void *)service, (void *)config);
        return ESP_ERR_INVALID_ARG;
    }
    memset(service, 0, sizeof(esp_service_t));
    service->last_err = ESP_OK;
    service->ops = ops;
    service->user_data = config->user_data;

    esp_err_t ret;
    service->name = strdup(config->name ? config->name : "unnamed");
    if (service->name == NULL) {
        ESP_LOGE(TAG, "Init: no memory for service name");
        return ESP_ERR_NO_MEM;
    }

    ret = adf_event_hub_create(service->name, &service->event_hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Init: event hub create failed %s (0x%x)", service->name, esp_err_to_name(ret), ret);
        goto err_cleanup;
    }

    if (ops && ops->on_init) {
        ret = ops->on_init(service, config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] Init: on_init failed %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            goto err_cleanup;
        }
    }

    set_state(service, ESP_SERVICE_STATE_INITIALIZED);
    ESP_LOGI(TAG, "[%s] Initialized", service->name);
    return ESP_OK;

err_cleanup:
    if (service->event_hub) {
        adf_event_hub_destroy(service->event_hub);
        service->event_hub = NULL;
    }
    if (service->name) {
        free((void *)service->name);
        service->name = NULL;
    }
    return ret;
}

esp_err_t esp_service_deinit(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Deinit: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_stop(service);

    if (service->ops && service->ops->on_deinit) {
        service->ops->on_deinit(service);
    }
    set_state(service, ESP_SERVICE_STATE_UNINITIALIZED);
    if (service->event_hub) {
        adf_event_hub_destroy(service->event_hub);
        service->event_hub = NULL;
    }
    ESP_LOGI(TAG, "[%s] Deinitialized", service->name);
    free((void *)service->name);
    service->name = NULL;
    return ESP_OK;
}

esp_err_t esp_service_start(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Start: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state;
    esp_err_t ret = get_state(service, &state);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state != ESP_SERVICE_STATE_INITIALIZED) {
        ESP_LOGE(TAG, "[%s] Start: invalid state %s (expect INITIALIZED)", service->name, get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    return handle_start_sync(service);
}

esp_err_t esp_service_stop(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Stop: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state;
    esp_err_t ret = get_state(service, &state);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state == ESP_SERVICE_STATE_UNINITIALIZED || state == ESP_SERVICE_STATE_INITIALIZED) {
        return ESP_OK;
    }
    return handle_stop_sync(service);
}

esp_err_t esp_service_pause(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Pause: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state;
    esp_err_t ret = get_state(service, &state);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state == ESP_SERVICE_STATE_INITIALIZED || state == ESP_SERVICE_STATE_PAUSED) {
        ESP_LOGW(TAG, "[%s] Pause: the state is already %s", service->name, get_state_name(state));
        return ESP_OK;
    }
    if (state != ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGE(TAG, "[%s] Pause: invalid state %s (expect RUNNING)", service->name, get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    return handle_pause_sync(service);
}

esp_err_t esp_service_resume(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Resume: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state;
    esp_err_t ret = get_state(service, &state);
    if (ret != ESP_OK) {
        return ret;
    }
    if (state == ESP_SERVICE_STATE_INITIALIZED || state == ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGW(TAG, "[%s] Resume: the state is already %s", service->name, get_state_name(state));
        return ESP_OK;
    }
    if (state != ESP_SERVICE_STATE_PAUSED) {
        ESP_LOGE(TAG, "[%s] Resume: invalid state %s (expect PAUSED)", service->name, get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    return handle_resume_sync(service);
}

esp_err_t esp_service_get_state(const esp_service_t *service, esp_service_state_t *out_state)
{
    if (service == NULL || out_state == NULL) {
        ESP_LOGE(TAG, "Get state: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_state);
        return ESP_ERR_INVALID_ARG;
    }
    return get_state(service, out_state);
}

esp_err_t esp_service_is_running(const esp_service_t *service, bool *out_running)
{
    if (service == NULL || out_running == NULL) {
        ESP_LOGE(TAG, "Is running: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_running);
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state;
    esp_err_t ret = get_state(service, &state);
    if (ret != ESP_OK) {
        return ret;
    }
    *out_running = (state == ESP_SERVICE_STATE_RUNNING);
    return ESP_OK;
}

esp_err_t esp_service_publish_event(esp_service_t *service,
                                    uint16_t event_id,
                                    const void *payload,
                                    uint16_t payload_len,
                                    adf_event_payload_release_cb_t release_cb,
                                    void *release_ctx)
{
    if (service == NULL || event_id == 0) {
        ESP_LOGE(TAG, "Publish event: invalid arg (service=%p, event_id=%u)", (void *)service, (unsigned)event_id);
        if (release_cb) {
            release_cb(payload, release_ctx);
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (service->event_hub == NULL) {
        ESP_LOGE(TAG, "[%s] Publish event: no event hub bound", service->name);
        if (release_cb) {
            release_cb(payload, release_ctx);
        }
        return ESP_ERR_INVALID_STATE;
    }

    adf_event_t evt = {
        .domain = service->name,
        .event_id = event_id,
        .payload = payload,
        .payload_len = payload_len,
    };
    return adf_event_hub_publish(service->event_hub, &evt, release_cb, release_ctx);
}

esp_err_t esp_service_get_last_error(const esp_service_t *service, esp_err_t *out_err)
{
    if (service == NULL || out_err == NULL) {
        ESP_LOGE(TAG, "Get last error: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_err);
        return ESP_ERR_INVALID_ARG;
    }
    *out_err = service->last_err;
    return ESP_OK;
}

esp_err_t esp_service_lowpower_enter(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Lowpower enter: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    if (service->ops && service->ops->on_lowpower_enter) {
        esp_err_t ret = service->ops->on_lowpower_enter(service);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[%s] On_lowpower_enter failed: %s (0x%x)",
                     service->name, esp_err_to_name(ret), ret);
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_service_lowpower_exit(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Lowpower exit: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    if (service->ops && service->ops->on_lowpower_exit) {
        esp_err_t ret = service->ops->on_lowpower_exit(service);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[%s] On_lowpower_exit failed: %s (0x%x)",
                     service->name, esp_err_to_name(ret), ret);
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_service_get_name(const esp_service_t *service, const char **out_name)
{
    if (service == NULL || out_name == NULL) {
        ESP_LOGE(TAG, "Get name: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_name);
        return ESP_ERR_INVALID_ARG;
    }
    *out_name = service->name;
    return ESP_OK;
}

esp_err_t esp_service_get_user_data(const esp_service_t *service, void **out_data)
{
    if (service == NULL || out_data == NULL) {
        ESP_LOGE(TAG, "Get user data: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_data);
        return ESP_ERR_INVALID_ARG;
    }
    *out_data = service->user_data;
    return ESP_OK;
}

esp_err_t esp_service_set_user_data(esp_service_t *service, void *user_data)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set user data: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    service->user_data = user_data;
    return ESP_OK;
}

esp_err_t esp_service_set_event_hub(esp_service_t *service, adf_event_hub_t hub)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set event hub: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    service->event_hub = hub;
    return ESP_OK;
}

esp_err_t esp_service_event_subscribe(esp_service_t *service, const adf_event_subscribe_info_t *info)
{
    if (service == NULL || info == NULL) {
        ESP_LOGE(TAG, "Subscribe: invalid arg (service=%p, info=%p)", (void *)service, (void *)info);
        return ESP_ERR_INVALID_ARG;
    }
    if (service->event_hub == NULL) {
        ESP_LOGE(TAG, "[%s] Subscribe: no event hub bound", service->name);
        return ESP_ERR_INVALID_STATE;
    }
    return adf_event_hub_subscribe(service->event_hub, info);
}

esp_err_t esp_service_event_unsubscribe(esp_service_t *service, const char *domain, uint16_t event_id)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Unsubscribe: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    if (service->event_hub == NULL) {
        ESP_LOGE(TAG, "[%s] Unsubscribe: no event hub bound", service->name);
        return ESP_ERR_INVALID_STATE;
    }
    return adf_event_hub_unsubscribe(service->event_hub, domain, event_id);
}

esp_err_t esp_service_event_delivery_done(esp_service_t *service, adf_event_delivery_t *delivery)
{
    if (service == NULL || delivery == NULL) {
        ESP_LOGE(TAG, "Delivery done: invalid arg (service=%p, delivery=%p)", (void *)service, (void *)delivery);
        return ESP_ERR_INVALID_ARG;
    }
    if (service->event_hub == NULL) {
        ESP_LOGE(TAG, "[%s] Delivery done: no event hub bound", service->name);
        return ESP_ERR_INVALID_STATE;
    }
    return adf_event_hub_delivery_done(service->event_hub, delivery);
}

esp_err_t esp_service_get_event_hub(const esp_service_t *service, adf_event_hub_t *out_hub)
{
    if (service == NULL || out_hub == NULL) {
        ESP_LOGE(TAG, "Get event hub: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_hub);
        return ESP_ERR_INVALID_ARG;
    }
    *out_hub = service->event_hub;
    return ESP_OK;
}

esp_err_t esp_service_state_to_str(esp_service_state_t state, const char **out_str)
{
    if (out_str == NULL || state >= ESP_SERVICE_STATE_MAX) {
        ESP_LOGE(TAG, "State to str: invalid arg (out=%p, state=%d)", (void *)out_str, (int)state);
        return ESP_ERR_INVALID_ARG;
    }
    *out_str = s_state_names[state];
    return ESP_OK;
}

esp_err_t esp_service_get_event_name(const esp_service_t *service, uint16_t event_id, const char **out_name)
{
    if (service == NULL || out_name == NULL) {
        ESP_LOGE(TAG, "Get event name: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_name);
        return ESP_ERR_INVALID_ARG;
    }

    if (service->ops && service->ops->event_to_name) {
        const char *name = service->ops->event_to_name(event_id);
        if (name) {
            *out_name = name;
            return ESP_OK;
        }
    }

    switch (event_id) {
        case ESP_SERVICE_EVENT_STATE_CHANGED:
            *out_name = "STATE_CHANGED";
            return ESP_OK;
        default:
            break;
    }

    *out_name = NULL;
    return ESP_ERR_NOT_FOUND;
}
