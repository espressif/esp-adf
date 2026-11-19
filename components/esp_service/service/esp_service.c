/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
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

static inline const char *svc_name(const esp_service_t *service)
{
    return service->name ? service->name : "?";
}

static esp_err_t get_state(const esp_service_t *service, esp_service_state_t *out_state)
{
    if (service == NULL || out_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_state = atomic_load_explicit(&service->state, memory_order_acquire);
    return ESP_OK;
}

static void set_state(esp_service_t *service, esp_service_state_t new_state)
{
    atomic_store_explicit(&service->state, new_state, memory_order_seq_cst);
}

static void free_event_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

/* Check admission and pin the hub under the same lock that detaches it.
 * Only publishers need the domain; on_init may set up subscriptions. */
static esp_err_t event_begin(esp_service_t *service, adf_event_hub_t *hub, const char **domain)
{
    SemaphoreHandle_t lock = service->lock;
    if (lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    esp_service_state_t state;
    get_state(service, &state);
    if (service->event_hub == NULL || state == ESP_SERVICE_STATE_STOPPING ||
        (domain != NULL && state == ESP_SERVICE_STATE_UNINITIALIZED)) {
        xSemaphoreGive(lock);
        return ESP_ERR_INVALID_STATE;
    }
    *hub = service->event_hub;
    if (domain != NULL) {
        *domain = service->name;
    }
    atomic_fetch_add_explicit(&service->publish_refs, 1u, memory_order_seq_cst);
    xSemaphoreGive(lock);
    return ESP_OK;
}

static inline void event_end(esp_service_t *service)
{
    atomic_fetch_sub_explicit(&service->publish_refs, 1u, memory_order_seq_cst);
}

static void publish_core_state_changed_event(esp_service_t *service, esp_service_state_t old_state,
                                             esp_service_state_t new_state)
{
    if (old_state == new_state) {
        return;
    }
    const char *domain;
    adf_event_hub_t hub;
    if (event_begin(service, &hub, &domain) != ESP_OK) {
        return;
    }
    esp_service_state_changed_payload_t *payload = malloc(sizeof(*payload));
    if (payload == NULL) {
        ESP_LOGW(TAG, "[%s] Failed to allocate state changed payload", domain);
        event_end(service);
        return;
    }
    payload->old_state = old_state;
    payload->new_state = new_state;

    adf_event_t evt = {
        .domain = domain,
        .event_id = ESP_SERVICE_EVENT_STATE_CHANGED,
        .payload = payload,
        .payload_len = sizeof(*payload),
    };
    /* Ownership transfers to hub: free_event_payload handles free on every
     * path (success or any non-INVALID_ARG error). INVALID_ARG is unreachable
     * because the borrowed hub and &evt are both non-NULL here. */
    esp_err_t ret = adf_event_hub_publish(hub, &evt, free_event_payload, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[%s] Publish state changed failed: %s", domain, esp_err_to_name(ret));
    }
    event_end(service);
}

static void commit_transition(esp_service_t *service, esp_service_state_t old_state,
                              esp_service_state_t new_state, esp_err_t ret)
{
    SemaphoreHandle_t lock = service->lock;
    xSemaphoreTake(lock, portMAX_DELAY);
    service->last_err = ret;
    set_state(service, new_state);
    xSemaphoreGive(lock);
    publish_core_state_changed_event(service, old_state, new_state);
    ESP_LOGD(TAG, "[%s] State: %s -> %s", svc_name(service), get_state_name(old_state), get_state_name(new_state));
}

static esp_err_t handle_start_sync(esp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    get_state(service, &state);
    if (state != ESP_SERVICE_STATE_INITIALIZED) {
        ESP_LOGE(TAG, "[%s] Start: invalid state %s (expect INITIALIZED)", svc_name(service), get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_start) {
        esp_err_t ret = service->ops->on_start(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_start failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            commit_transition(service, state, ESP_SERVICE_STATE_ERROR, ret);
            return ret;
        }
    }
    commit_transition(service, state, ESP_SERVICE_STATE_RUNNING, ESP_OK);
    ESP_LOGI(TAG, "[%s] Started", svc_name(service));
    return ESP_OK;
}

static esp_err_t handle_stop_sync(esp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_UNINITIALIZED || state == ESP_SERVICE_STATE_INITIALIZED ||
        state == ESP_SERVICE_STATE_STOPPING) {
        return ESP_OK;
    }
    esp_service_state_t next = ESP_SERVICE_STATE_INITIALIZED;
    esp_err_t ret = ESP_OK;
    if (service->ops && service->ops->on_stop) {
        ret = service->ops->on_stop(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_stop failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            next = ESP_SERVICE_STATE_ERROR;
        }
    }
    commit_transition(service, state, next, ret);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[%s] Stopped (sync)", svc_name(service));
    }
    return ret;
}

static esp_err_t handle_pause_sync(esp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    get_state(service, &state);
    if (state != ESP_SERVICE_STATE_RUNNING) {
        ESP_LOGE(TAG, "[%s] Pause: invalid state %s (expect RUNNING)", svc_name(service), get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_pause) {
        esp_err_t ret = service->ops->on_pause(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_pause failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            commit_transition(service, state, ESP_SERVICE_STATE_ERROR, ret);
            return ret;
        }
    }
    commit_transition(service, state, ESP_SERVICE_STATE_PAUSED, ESP_OK);
    return ESP_OK;
}

static esp_err_t handle_resume_sync(esp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    get_state(service, &state);
    if (state != ESP_SERVICE_STATE_PAUSED) {
        ESP_LOGE(TAG, "[%s] Resume: invalid state %s (expect PAUSED)", svc_name(service), get_state_name(state));
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_resume) {
        esp_err_t ret = service->ops->on_resume(service);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] On_resume failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            commit_transition(service, state, ESP_SERVICE_STATE_ERROR, ret);
            return ret;
        }
    }
    commit_transition(service, state, ESP_SERVICE_STATE_RUNNING, ESP_OK);
    return ESP_OK;
}

/* The lowpower hooks touch the derived context, so they are refused once
 * deinit has claimed the instance or released it. */
static bool lowpower_allowed(esp_service_t *service, const char *op)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_UNINITIALIZED || state == ESP_SERVICE_STATE_STOPPING) {
        ESP_LOGE(TAG, "[%s] %s: invalid state %s", svc_name(service), op, get_state_name(state));
        return false;
    }
    return true;
}

static esp_err_t handle_lowpower_enter(esp_service_t *service)
{
    if (!lowpower_allowed(service, "Lowpower enter")) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_lowpower_enter) {
        esp_err_t ret = service->ops->on_lowpower_enter(service);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[%s] On_lowpower_enter failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
        }
        return ret;
    }
    return ESP_OK;
}

static esp_err_t handle_lowpower_exit(esp_service_t *service)
{
    if (!lowpower_allowed(service, "Lowpower exit")) {
        return ESP_ERR_INVALID_STATE;
    }
    if (service->ops && service->ops->on_lowpower_exit) {
        esp_err_t ret = service->ops->on_lowpower_exit(service);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "[%s] On_lowpower_exit failed: %s (0x%x)", service->name, esp_err_to_name(ret), ret);
        }
        return ret;
    }
    return ESP_OK;
}

static bool reject_event_lifecycle(const char *op)
{
    bool dispatching;
    adf_event_hub_is_dispatching(&dispatching);
    if (dispatching) {
        ESP_LOGE(TAG, "%s rejected: called from event dispatch", op);
    }
    return dispatching;
}

/* Keep the operation claimed through notifications, but run hooks and
 * callbacks outside the mutex so reentrant callers can check the claim. */
static esp_err_t run_lifecycle(esp_service_t *service, const char *op, esp_err_t (*handler)(esp_service_t *))
{
    if (service == NULL) {
        ESP_LOGE(TAG, "%s: invalid arg (service=NULL)", op);
        return ESP_ERR_INVALID_ARG;
    }
    if (reject_event_lifecycle(op)) {
        return ESP_ERR_INVALID_STATE;
    }
    SemaphoreHandle_t lock = service->lock;
    if (lock == NULL) {
        /* Never initialized: the handler only evaluates the UNINITIALIZED
         * state, nothing to serialize. */
        return handler(service);
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    if (service->lifecycle_active) {
        xSemaphoreGive(lock);
        ESP_LOGE(TAG, "%s rejected: lifecycle operation active", op);
        return ESP_ERR_INVALID_STATE;
    }
    service->lifecycle_active = true;
    xSemaphoreGive(lock);

    esp_err_t ret = handler(service);

    xSemaphoreTake(lock, portMAX_DELAY);
    service->lifecycle_active = false;
    xSemaphoreGive(lock);
    return ret;
}

esp_err_t esp_service_init(esp_service_t *service, const esp_service_config_t *config, const esp_service_ops_t *ops)
{
    if (service == NULL || config == NULL) {
        ESP_LOGE(TAG, "Init: invalid arg (service=%p, config=%p)", (void *)service, (void *)config);
        return ESP_ERR_INVALID_ARG;
    }
    if (reject_event_lifecycle("Init")) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(service, 0, sizeof(*service));
    atomic_init(&service->state, ESP_SERVICE_STATE_UNINITIALIZED);
    atomic_init(&service->publish_refs, 0u);
    SemaphoreHandle_t lock = xSemaphoreCreateMutexStatic(&service->lock_storage);
    if (lock == NULL) {
        ESP_LOGE(TAG, "Init: failed to create lifecycle mutex");
        return ESP_FAIL;
    }
    service->lock = lock;
    xSemaphoreTake(lock, portMAX_DELAY);
    service->lifecycle_active = true;
    xSemaphoreGive(lock);
    service->last_err = ESP_OK;
    service->ops = ops;
    service->user_data = config->user_data;

    esp_err_t ret;
    service->name = strdup(config->name ? config->name : "unnamed");
    if (service->name == NULL) {
        ESP_LOGE(TAG, "Init: no memory for service name");
        ret = ESP_ERR_NO_MEM;
        goto finish;
    }

    adf_event_hub_t hub = NULL;
    ret = adf_event_hub_create(service->name, &hub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Init: event hub create failed %s (0x%x)", service->name, esp_err_to_name(ret), ret);
        goto err_cleanup;
    }

    xSemaphoreTake(lock, portMAX_DELAY);
    service->event_hub = hub;
    xSemaphoreGive(lock);
    if (ops && ops->on_init) {
        ret = ops->on_init(service, config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[%s] Init: on_init failed %s (0x%x)", service->name, esp_err_to_name(ret), ret);
            goto err_cleanup;
        }
    }

    ESP_LOGI(TAG, "[%s] Initialized", service->name);
    goto finish;

err_cleanup:
    xSemaphoreTake(lock, portMAX_DELAY);
    hub = service->event_hub;
    service->event_hub = NULL;
    xSemaphoreGive(lock);
    while (atomic_load_explicit(&service->publish_refs, memory_order_seq_cst) != 0u) {
        vTaskDelay(1);
    }
    if (hub) {
        adf_event_hub_destroy(hub);
    }
    if (service->name) {
        free((void *)service->name);
        service->name = NULL;
    }

finish:
    xSemaphoreTake(lock, portMAX_DELAY);
    if (ret == ESP_OK) {
        set_state(service, ESP_SERVICE_STATE_INITIALIZED);
    }
    service->lifecycle_active = false;
    xSemaphoreGive(lock);
    return ret;
}

esp_err_t esp_service_deinit(esp_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Deinit: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    if (reject_event_lifecycle("Deinit")) {
        return ESP_ERR_INVALID_STATE;
    }
    SemaphoreHandle_t lock = service->lock;
    if (lock == NULL) {
        return ESP_OK;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    if (service->lifecycle_active) {
        xSemaphoreGive(lock);
        ESP_LOGE(TAG, "Deinit rejected: lifecycle operation active");
        return ESP_ERR_INVALID_STATE;
    }
    esp_service_state_t state;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_UNINITIALIZED) {
        xSemaphoreGive(lock);
        return ESP_OK;
    }
    service->lifecycle_active = true;
    if (state != ESP_SERVICE_STATE_INITIALIZED && state != ESP_SERVICE_STATE_STOPPING) {
        xSemaphoreGive(lock);
        esp_err_t stop_ret = handle_stop_sync(service);
        if (stop_ret != ESP_OK) {
            xSemaphoreTake(lock, portMAX_DELAY);
            service->lifecycle_active = false;
            xSemaphoreGive(lock);
            return stop_ret;
        }
        xSemaphoreTake(lock, portMAX_DELAY);
    }
    set_state(service, ESP_SERVICE_STATE_STOPPING);
    adf_event_hub_t hub = service->event_hub;
    service->event_hub = NULL;
    xSemaphoreGive(lock);

    esp_err_t ret = ESP_OK;
    /* Drain borrowed hub/name and detach subscriptions before releasing any
     * callback context. A failed private cleanup needs no new hub reference. */
    while (atomic_load_explicit(&service->publish_refs, memory_order_seq_cst) != 0u) {
        vTaskDelay(1);
    }
    if (hub) {
        ret = adf_event_hub_unsubscribe_all(hub);
        if (ret != ESP_OK) {
            goto finish;
        }
        ret = adf_event_hub_destroy(hub);
        if (ret != ESP_OK) {
            goto finish;
        }
        hub = NULL;
    }
    if (service->ops && service->ops->on_deinit) {
        ret = service->ops->on_deinit(service);
        if (ret != ESP_OK) {
            goto finish;
        }
    }
    ESP_LOGI(TAG, "[%s] Deinitialized", service->name);
    const char *name = service->name;
    service->name = NULL;
    free((void *)name);
    service->user_data = NULL;

finish:
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Deinit failed: %s", svc_name(service), esp_err_to_name(ret));
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    service->event_hub = hub;
    service->last_err = ret;
    service->lifecycle_active = false;
    if (ret == ESP_OK) {
        set_state(service, ESP_SERVICE_STATE_UNINITIALIZED);
    }
    xSemaphoreGive(lock);
    return ret;
}

esp_err_t esp_service_start(esp_service_t *service)
{
    return run_lifecycle(service, "Start", handle_start_sync);
}

esp_err_t esp_service_stop(esp_service_t *service)
{
    return run_lifecycle(service, "Stop", handle_stop_sync);
}

esp_err_t esp_service_pause(esp_service_t *service)
{
    return run_lifecycle(service, "Pause", handle_pause_sync);
}

esp_err_t esp_service_resume(esp_service_t *service)
{
    return run_lifecycle(service, "Resume", handle_resume_sync);
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
    const char *domain;
    adf_event_hub_t hub;
    esp_err_t ret = event_begin(service, &hub, &domain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Publish event rejected: event admission closed");
        if (release_cb) {
            release_cb(payload, release_ctx);
        }
        return ESP_ERR_INVALID_STATE;
    }

    adf_event_t evt = {
        .domain = domain,
        .event_id = event_id,
        .payload = payload,
        .payload_len = payload_len,
    };
    ret = adf_event_hub_publish(hub, &evt, release_cb, release_ctx);
    event_end(service);
    return ret;
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
    return run_lifecycle(service, "Lowpower enter", handle_lowpower_enter);
}

esp_err_t esp_service_lowpower_exit(esp_service_t *service)
{
    return run_lifecycle(service, "Lowpower exit", handle_lowpower_exit);
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
    SemaphoreHandle_t lock = service->lock;
    if (lock == NULL) {
        ESP_LOGE(TAG, "Set event hub rejected: service not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    esp_service_state_t state;
    get_state(service, &state);
    if (state == ESP_SERVICE_STATE_STOPPING ||
        (state == ESP_SERVICE_STATE_UNINITIALIZED && service->event_hub == NULL) ||
        atomic_load_explicit(&service->publish_refs, memory_order_seq_cst) != 0u) {
        xSemaphoreGive(lock);
        ESP_LOGE(TAG, "Set event hub rejected: service not ready or hub in use");
        return ESP_ERR_INVALID_STATE;
    }
    service->event_hub = hub;
    xSemaphoreGive(lock);
    return ESP_OK;
}

esp_err_t esp_service_event_subscribe(esp_service_t *service, const adf_event_subscribe_info_t *info)
{
    if (service == NULL || info == NULL) {
        ESP_LOGE(TAG, "Subscribe: invalid arg (service=%p, info=%p)", (void *)service, (void *)info);
        return ESP_ERR_INVALID_ARG;
    }
    adf_event_hub_t hub;
    esp_err_t ret = event_begin(service, &hub, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe rejected: event admission closed");
        return ret;
    }
    ret = adf_event_hub_subscribe(hub, info);
    event_end(service);
    return ret;
}

esp_err_t esp_service_event_unsubscribe(esp_service_t *service, const char *domain, uint16_t event_id)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Unsubscribe: invalid arg (service=NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    adf_event_hub_t hub;
    esp_err_t ret = event_begin(service, &hub, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unsubscribe rejected: event admission closed");
        return ret;
    }
    ret = adf_event_hub_unsubscribe(hub, domain, event_id);
    event_end(service);
    return ret;
}

esp_err_t esp_service_event_delivery_done(esp_service_t *service, adf_event_delivery_t *delivery)
{
    if (service == NULL || delivery == NULL) {
        ESP_LOGE(TAG, "Delivery done: invalid arg (service=%p, delivery=%p)", (void *)service, (void *)delivery);
        return ESP_ERR_INVALID_ARG;
    }
    /* Owners drain queue deliveries before deinit. Do not pin across the
     * release callback: it may run user code synchronously. */
    adf_event_hub_t hub;
    esp_service_get_event_hub(service, &hub);
    if (hub == NULL) {
        ESP_LOGE(TAG, "Delivery done: no event hub bound");
        return ESP_ERR_INVALID_STATE;
    }
    return adf_event_hub_delivery_done(hub, delivery);
}

esp_err_t esp_service_get_event_hub(const esp_service_t *service, adf_event_hub_t *out_hub)
{
    if (service == NULL || out_hub == NULL) {
        ESP_LOGE(TAG, "Get event hub: invalid arg (service=%p, out=%p)", (void *)service, (void *)out_hub);
        return ESP_ERR_INVALID_ARG;
    }
    SemaphoreHandle_t lock = service->lock;
    if (lock == NULL) {
        *out_hub = NULL;
        return ESP_OK;
    }
    xSemaphoreTake(lock, portMAX_DELAY);
    *out_hub = service->event_hub;
    xSemaphoreGive(lock);
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
