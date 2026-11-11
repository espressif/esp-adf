/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <assert.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#if CONFIG_BT_CLASSIC_ENABLED
#include "esp_gap_bt_api.h"
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if CONFIG_BT_BLUEDROID_ENABLED && CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#include "esp_gattc_api.h"
#endif  /* CONFIG_BT_BLUEDROID_ENABLED && CONFIG_BT_BLE_50_FEATURES_SUPPORTED */

#include "esp_bt_hub_priv.h"

#define ESP_BT_HUB_MAX_CLIENTS              8
#define ESP_BT_HUB_MAX_SUBS                 8
#define ESP_BT_HUB_CLIENT_NAME_LEN          24
#define ESP_BT_HUB_TOKEN_INDEX_MASK         ((1U << ESP_BT_HUB_TOKEN_INDEX_BITS) - 1U)
#define ESP_BT_HUB_LE_ADV_PROP_CONNECTABLE  (1U << 0)
#define ESP_BT_HUB_DISPATCH_IDLE_BIT        (1U << 0)

_Static_assert(ESP_BT_HUB_TOKEN_INDEX_BITS < sizeof(uintptr_t) * 8U, "Token generation needs at least one bit");
_Static_assert(ESP_BT_HUB_MAX_CLIENTS <= ESP_BT_HUB_TOKEN_INDEX_MASK, "Client index does not fit token");
_Static_assert(ESP_BT_HUB_MAX_SUBS <= ESP_BT_HUB_TOKEN_INDEX_MASK, "Subscription index does not fit token");

typedef enum {
    RESOURCE_FREE = 0,
    RESOURCE_ACQUIRING,
    RESOURCE_ACTIVE,
    RESOURCE_STOPPING,
    RESOURCE_FAULT,
} resource_state_t;

typedef enum {
    HOST_FAULT_NONE = 0,
    HOST_FAULT_STOP,
    HOST_FAULT_DEINIT,
} host_fault_phase_t;

/**
 * @brief  Bluetooth hub client slot
 */
struct esp_bt_hub_client {
    bool                       in_use;                            /*!< Slot is allocated */
    bool                       closing;                           /*!< Client close is in progress */
    uint32_t                   generation;                        /*!< Handle generation */
    char                       name[ESP_BT_HUB_CLIENT_NAME_LEN];  /*!< Client name */
    bool                       connectable;                       /*!< Classic connectability request */
    esp_bt_hub_discoverable_t  discoverable;                      /*!< Classic discoverability request */
};

/**
 * @brief  GAP event subscription slot
 */
struct esp_bt_hub_gap_sub {
    bool                     in_use;          /*!< Slot is allocated */
    bool                     pending_remove;  /*!< Removal is deferred until dispatch completes */
    uint32_t                 generation;      /*!< Handle generation */
    esp_bt_hub_gap_domain_t  domain;          /*!< Subscribed GAP domain */
    esp_bt_hub_gap_cb_t      callback;        /*!< GAP event callback */
    void                    *user_ctx;        /*!< Callback context */
};

/**
 * @brief  Bluetooth controller resource lease slot
 */
typedef struct {
    resource_state_t            state;                 /*!< Resource lifecycle state */
    uint32_t                    generation;            /*!< Handle generation */
    esp_bt_hub_client_handle_t  owner;                 /*!< Client that owns the resource */
    bool                        external;              /*!< Resource is managed by an external caller */
    bool                        native_configured;     /*!< Native advertising set has been configured */
    bool                        ext_adv_started;       /*!< Extended advertising is active */
    bool                        periodic_adv_started;  /*!< Periodic advertising is active */
    bool                        ext_scan_started;      /*!< Extended scanning is active */
    bool                        ext_scan_busy;         /*!< Extended scan operation is in flight */
} resource_slot_t;

/**
 * @brief  ESP Bluetooth hub core runtime context
 */
typedef struct {
    StaticSemaphore_t   control_storage;         /*!< Control mutex storage */
    SemaphoreHandle_t   control_mutex;           /*!< Serializes hub state */
    StaticEventGroup_t  dispatch_event_storage;  /*!< Dispatch event storage */
    EventGroupHandle_t  dispatch_event;          /*!< Signals idle GAP dispatch */
    TaskHandle_t        dispatch_task;           /*!< Task running GAP dispatch */
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    StaticSemaphore_t  scan_mode_storage;  /*!< Classic scan mode mutex storage */
    SemaphoreHandle_t  scan_mode_mutex;    /*!< Serializes Classic scan mode changes */
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    StaticSemaphore_t  ext_adv_operation_storage[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES];  /*!< Mutex storage */
    SemaphoreHandle_t  ext_adv_operation_mutex[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES];    /*!< Operation mutexes */
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
    esp_bt_hub_state_t         host_state;                       /*!< Hub lifecycle state */
    host_fault_phase_t         fault_phase;                      /*!< Lifecycle operation to retry */
    uint32_t                   active_calls;                     /*!< Runtime calls using the backend */
    int                        dispatching;                      /*!< Active GAP dispatch depth */
    esp_bt_hub_config_t        config;                           /*!< Active hub configuration */
    struct esp_bt_hub_client   clients[ESP_BT_HUB_MAX_CLIENTS];  /*!< Client slots */
    struct esp_bt_hub_gap_sub  subs[ESP_BT_HUB_MAX_SUBS];        /*!< GAP subscriptions */
    resource_slot_t            discovery;                        /*!< Classic discovery lease */
    resource_slot_t            le_scan;                          /*!< LE scan lease */
    resource_slot_t            le_adv;                           /*!< Legacy LE advertising lease */
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    resource_slot_t  ext_adv[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES];  /*!< Extended advertising leases */
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    bool  scan_mode_fault;  /*!< Classic scan mode reconciliation failed */
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
} esp_bt_hub_ctx_t;

static const char *TAG = "ESP_BT_HUB";
static portMUX_TYPE s_bootstrap_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_bt_hub_ctx_t *s_ctx;

#define s_control_storage            s_ctx->control_storage
#define s_control_mutex              s_ctx->control_mutex
#define s_dispatch_event_storage     s_ctx->dispatch_event_storage
#define s_dispatch_event             s_ctx->dispatch_event
#define s_dispatch_task              s_ctx->dispatch_task
#define s_scan_mode_storage          s_ctx->scan_mode_storage
#define s_scan_mode_mutex            s_ctx->scan_mode_mutex
#define s_ext_adv_operation_storage  s_ctx->ext_adv_operation_storage
#define s_ext_adv_operation_mutex    s_ctx->ext_adv_operation_mutex
#define s_host_state                 s_ctx->host_state
#define s_fault_phase                s_ctx->fault_phase
#define s_active_calls               s_ctx->active_calls
#define s_dispatching                s_ctx->dispatching
#define s_config                     s_ctx->config
#define s_clients                    s_ctx->clients
#define s_subs                       s_ctx->subs
#define s_discovery                  s_ctx->discovery
#define s_le_scan                    s_ctx->le_scan
#define s_le_adv                     s_ctx->le_adv
#define s_ext_adv                    s_ctx->ext_adv
#define s_scan_mode_fault            s_ctx->scan_mode_fault

static bool control_lock(void);
static void control_unlock(void);
#if ESP_BT_HUB_CLASSIC_SUPPORTED
static void scan_mode_merge_locked(bool *connectable, esp_bt_hub_discoverable_t *discoverable);
static esp_err_t scan_mode_apply(bool connectable, esp_bt_hub_discoverable_t discoverable);
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

__attribute__((weak)) const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_get(void)
{
    return esp_bt_hub_backend_default_get();
}

static esp_err_t control_init(void)
{
    esp_bt_hub_ctx_t *ctx;

    if (s_ctx) {
        return ESP_OK;
    }
    ctx = heap_caps_calloc_prefer(1, sizeof(*ctx), 2,
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!ctx) {
        ESP_LOGE(TAG, "Initialize hub context failed: no memory");
        return ESP_ERR_NO_MEM;
    }
    ctx->control_mutex = xSemaphoreCreateMutexStatic(&ctx->control_storage);
    ctx->dispatch_event = xEventGroupCreateStatic(&ctx->dispatch_event_storage);
    xEventGroupSetBits(ctx->dispatch_event, ESP_BT_HUB_DISPATCH_IDLE_BIT);
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    ctx->scan_mode_mutex = xSemaphoreCreateMutexStatic(&ctx->scan_mode_storage);
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    for (size_t i = 0; i < (size_t)ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
        ctx->ext_adv_operation_mutex[i] = xSemaphoreCreateMutexStatic(&ctx->ext_adv_operation_storage[i]);
    }
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
    portENTER_CRITICAL(&s_bootstrap_lock);
    if (!s_ctx) {
        s_ctx = ctx;
        ctx = NULL;
    }
    portEXIT_CRITICAL(&s_bootstrap_lock);
    if (ctx) {
        heap_caps_free(ctx);
    }
    return ESP_OK;
}

static bool control_lock(void)
{
    if (!s_ctx) {
        return false;
    }
    xSemaphoreTake(s_control_mutex, portMAX_DELAY);
    return true;
}

static void control_unlock(void)
{
    if (s_ctx) {
        xSemaphoreGive(s_control_mutex);
    }
}

static inline void *token_make(size_t index, uint32_t generation)
{
    uintptr_t token = ((uintptr_t)generation << ESP_BT_HUB_TOKEN_INDEX_BITS) | (index + 1U);

    return (void *)token;
}

static inline bool token_decode(const void *token, size_t count, size_t *index, uint32_t *generation)
{
    uintptr_t value = (uintptr_t)token;
    uint32_t decoded_generation;
    size_t slot;

    if (!value) {
        return false;
    }
    slot = (value & ESP_BT_HUB_TOKEN_INDEX_MASK);
    if (slot == 0 || slot > count) {
        return false;
    }
    *index = slot - 1U;
    decoded_generation = (uint32_t)(value >> ESP_BT_HUB_TOKEN_INDEX_BITS);
    if (generation) {
        *generation = decoded_generation;
    }
    return decoded_generation != 0;
}

static bool client_lookup_locked(esp_bt_hub_client_handle_t client, struct esp_bt_hub_client **out)
{
    uint32_t generation;
    size_t index;

    if (!token_decode(client, ESP_BT_HUB_MAX_CLIENTS, &index, &generation) ||
        !s_clients[index].in_use || s_clients[index].closing || s_clients[index].generation != generation) {
        return false;
    }
    if (out) {
        *out = &s_clients[index];
    }
    return true;
}

static bool sub_lookup_locked(esp_bt_hub_gap_sub_handle_t sub, struct esp_bt_hub_gap_sub **out)
{
    uint32_t generation;
    size_t index;

    if (!token_decode(sub, ESP_BT_HUB_MAX_SUBS, &index, &generation) ||
        !s_subs[index].in_use || s_subs[index].generation != generation) {
        return false;
    }
    if (out) {
        *out = &s_subs[index];
    }
    return true;
}

static bool resource_lookup_locked(resource_slot_t *slots, size_t count, const void *handle,
                                   resource_slot_t **out, size_t *out_index)
{
    uint32_t generation;
    size_t index;

    if (!token_decode(handle, count, &index, &generation) ||
        slots[index].state == RESOURCE_FREE || slots[index].generation != generation) {
        return false;
    }
    if (out) {
        *out = &slots[index];
    }
    if (out_index) {
        *out_index = index;
    }
    return true;
}

static void resource_free_locked(resource_slot_t *slot)
{
    slot->state = RESOURCE_FREE;
    slot->owner = NULL;
    slot->external = false;
    slot->native_configured = false;
    slot->ext_adv_started = false;
    slot->periodic_adv_started = false;
    slot->ext_scan_started = false;
    slot->ext_scan_busy = false;
}

static inline bool resource_owned_by_locked(const resource_slot_t *slot, esp_bt_hub_client_handle_t client)
{
    return slot->state != RESOURCE_FREE && slot->owner == client;
}

static bool client_owns_resource_locked(esp_bt_hub_client_handle_t client)
{
    if (resource_owned_by_locked(&s_discovery, client) || resource_owned_by_locked(&s_le_scan, client) ||
        resource_owned_by_locked(&s_le_adv, client)) {
        return true;
    }
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    for (size_t i = 0; i < (size_t)ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
        if (resource_owned_by_locked(&s_ext_adv[i], client)) {
            return true;
        }
    }
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
    return false;
}

static esp_err_t resource_acquire_locked(resource_slot_t *slot, esp_bt_hub_client_handle_t client,
                                         bool external, void **out_handle)
{
    if (!client_lookup_locked(client, NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_host_state != ESP_BT_HUB_STATE_RUNNING || slot->state != RESOURCE_FREE) {
        return ESP_ERR_INVALID_STATE;
    }
    slot->generation = esp_bt_hub_generation_next(slot->generation);
    slot->state = external ? RESOURCE_ACTIVE : RESOURCE_ACQUIRING;
    slot->owner = client;
    slot->external = external;
    *out_handle = token_make(0, slot->generation);
    return ESP_OK;
}

#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
static esp_err_t le_adv_resource_acquire_locked(esp_bt_hub_client_handle_t client, bool external,
                                                void **out_handle)
{
    void *handle = NULL;
    esp_err_t err = resource_acquire_locked(&s_le_adv, client, external, &handle);

#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV
    if (err == ESP_OK) {
        size_t instance = (size_t)ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE;

        if (instance < (size_t)ESP_BT_HUB_MAX_EXT_ADV_INSTANCES &&
            s_ext_adv[instance].state != RESOURCE_FREE) {
            resource_free_locked(&s_le_adv);
            err = ESP_ERR_INVALID_STATE;
        }
    }
#endif  /* CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV */
    if (err == ESP_OK) {
        *out_handle = handle;
    }
    return err;
}
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

static bool any_client_locked(void)
{
    for (size_t i = 0; i < ESP_BT_HUB_MAX_CLIENTS; i++) {
        if (s_clients[i].in_use) {
            return true;
        }
    }
    return false;
}

static bool any_sub_locked(void)
{
    for (size_t i = 0; i < ESP_BT_HUB_MAX_SUBS; i++) {
        if (s_subs[i].in_use) {
            return true;
        }
    }
    return false;
}

static bool any_resource_locked(void)
{
    if (s_discovery.state != RESOURCE_FREE || s_le_scan.state != RESOURCE_FREE ||
        s_le_adv.state != RESOURCE_FREE) {
        return true;
    }
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    for (size_t i = 0; i < (size_t)ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
        if (s_ext_adv[i].state != RESOURCE_FREE) {
            return true;
        }
    }
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
    return false;
}

static inline bool host_accepts_profiles_locked(void)
{
    return s_host_state == ESP_BT_HUB_STATE_INITIALIZED || s_host_state == ESP_BT_HUB_STATE_RUNNING;
}

static inline bool domain_supported(esp_bt_hub_gap_domain_t domain)
{
#if CONFIG_BT_NIMBLE_ENABLED
    return domain == ESP_BT_HUB_GAP_DOMAIN_BLE;
#elif CONFIG_BT_BLUEDROID_ENABLED
    return (domain == ESP_BT_HUB_GAP_DOMAIN_CLASSIC && ESP_BT_HUB_CLASSIC_SUPPORTED) ||
           (domain == ESP_BT_HUB_GAP_DOMAIN_BLE && ESP_BT_HUB_LE_SUPPORTED);
#else
    return false;
#endif  /* CONFIG_BT_NIMBLE_ENABLED */
}

const esp_bt_hub_config_t *esp_bt_hub_config_get(void)
{
    return s_ctx ? &s_config : NULL;
}

bool esp_bt_hub_pairing_auto_reply_enabled(void)
{
    return s_ctx ? s_config.pairing_auto_reply : false;
}

void esp_bt_hub_gap_dispatch(esp_bt_hub_gap_domain_t domain, uint32_t event, void *param)
{
    esp_bt_hub_gap_cb_t callbacks[ESP_BT_HUB_MAX_SUBS];
    void *contexts[ESP_BT_HUB_MAX_SUBS];
    size_t count = 0;

    if (!control_lock()) {
        return;
    }
    if (s_dispatching == 0) {
        s_dispatch_task = xTaskGetCurrentTaskHandle();
        xEventGroupClearBits(s_dispatch_event, ESP_BT_HUB_DISPATCH_IDLE_BIT);
    }
    s_dispatching++;
    for (size_t i = 0; i < ESP_BT_HUB_MAX_SUBS; i++) {
        if (s_subs[i].in_use && !s_subs[i].pending_remove && s_subs[i].domain == domain) {
            callbacks[count] = s_subs[i].callback;
            contexts[count] = s_subs[i].user_ctx;
            count++;
        }
    }
    control_unlock();
    for (size_t i = 0; i < count; i++) {
        callbacks[i](event, param, contexts[i]);
    }
    control_lock();
    s_dispatching--;
    if (s_dispatching == 0) {
        for (size_t i = 0; i < ESP_BT_HUB_MAX_SUBS; i++) {
            if (s_subs[i].pending_remove) {
                s_subs[i].in_use = false;
                s_subs[i].pending_remove = false;
                s_subs[i].callback = NULL;
                s_subs[i].user_ctx = NULL;
            }
        }
        s_dispatch_task = NULL;
        xEventGroupSetBits(s_dispatch_event, ESP_BT_HUB_DISPATCH_IDLE_BIT);
    }
    control_unlock();
}

void esp_bt_hub_async_resource_complete(esp_bt_hub_async_resource_t resource, bool success)
{
    resource_slot_t *slot;

    if (!control_lock()) {
        return;
    }
    slot = resource == ESP_BT_HUB_ASYNC_LE_SCAN ? &s_le_scan : &s_le_adv;
    if (slot->state == RESOURCE_ACQUIRING) {
        if (success) {
            slot->state = RESOURCE_ACTIVE;
        } else {
            resource_free_locked(slot);
        }
    }
    control_unlock();
}

void esp_bt_hub_le_ext_adv_complete(uint8_t instance, uint32_t generation)
{
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    if (!control_lock()) {
        return;
    }
    if (instance < ESP_BT_HUB_MAX_EXT_ADV_INSTANCES &&
        s_ext_adv[instance].state == RESOURCE_ACTIVE &&
        s_ext_adv[instance].generation == generation) {
        s_ext_adv[instance].ext_adv_started = false;
    }
    control_unlock();
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
}

void esp_bt_hub_le_ext_scan_complete(void)
{
    if (!control_lock()) {
        return;
    }
    if (s_le_scan.state == RESOURCE_ACTIVE) {
        s_le_scan.ext_scan_started = false;
    }
    control_unlock();
}

esp_err_t esp_bt_hub_state_get(esp_bt_hub_state_t *out_state)
{
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    if (!out_state) {
        ESP_LOGE(TAG, "Get hub state failed: output is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (control_lock()) {
        state = s_host_state;
        control_unlock();
    }
    *out_state = state;
    return ESP_OK;
}

esp_err_t esp_bt_hub_is_initialized(bool *out_initialized)
{
    esp_bt_hub_state_t state;
    esp_err_t err;

    if (!out_initialized) {
        ESP_LOGE(TAG, "Is initialized failed: output is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_state_get(&state);

    if (err != ESP_OK) {
        return err;
    }
    *out_initialized = state == ESP_BT_HUB_STATE_INITIALIZED || state == ESP_BT_HUB_STATE_RUNNING;
    return ESP_OK;
}

bool esp_bt_hub_is_running(void)
{
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    esp_bt_hub_state_get(&state);
    return state == ESP_BT_HUB_STATE_RUNNING;
}

esp_err_t esp_bt_hub_runtime_enter(void)
{
    if (!control_lock()) {
        ESP_LOGE(TAG, "Enter runtime call failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_RUNNING) {
        control_unlock();
        ESP_LOGE(TAG, "Enter runtime call failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    s_active_calls++;
    control_unlock();
    return ESP_OK;
}

esp_err_t esp_bt_hub_context_enter(void)
{
    if (!control_lock()) {
        ESP_LOGE(TAG, "Enter backend context call failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_INITIALIZED &&
        s_host_state != ESP_BT_HUB_STATE_RUNNING) {
        control_unlock();
        ESP_LOGE(TAG, "Enter backend context call failed: hub is transitioning");
        return ESP_ERR_INVALID_STATE;
    }
    s_active_calls++;
    control_unlock();
    return ESP_OK;
}

void esp_bt_hub_runtime_leave(void)
{
    if (!control_lock()) {
        return;
    }
    assert(s_active_calls > 0);
    s_active_calls--;
    control_unlock();
}

void esp_bt_hub_context_leave(void)
{
    esp_bt_hub_runtime_leave();
}

esp_err_t esp_bt_hub_init(const esp_bt_hub_config_t *config)
{
    const esp_bt_hub_backend_ops_t *backend = esp_bt_hub_backend_get();
    esp_err_t err;

    if (!config) {
        ESP_LOGE(TAG, "Init 'hub' failed: config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (config->classic_pin_len > sizeof(config->classic_pin)) {
        ESP_LOGE(TAG, "Init 'hub' failed: Classic PIN length is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!backend || !backend->init) {
        ESP_LOGE(TAG, "Init 'hub' failed: no Bluetooth backend");
        return ESP_ERR_NOT_SUPPORTED;
    }
    err = control_init();
    if (err != ESP_OK) {
        return err;
    }

    control_lock();
    if (s_host_state != ESP_BT_HUB_STATE_STOPPED) {
        control_unlock();
        ESP_LOGE(TAG, "Init 'hub' failed: lifecycle is not stopped");
        return ESP_ERR_INVALID_STATE;
    }
    s_host_state = ESP_BT_HUB_STATE_INITIALIZING;
    s_fault_phase = HOST_FAULT_NONE;
    s_config = *config;
    s_config.device_name[ESP_BT_HUB_MAX_DEV_NAME_LEN - 1] = '\0';
    if (s_config.device_name[0] == '\0') {
        strlcpy(s_config.device_name, "ESP-BT", sizeof(s_config.device_name));
    }
    control_unlock();

    err = backend->init(&s_config);
    control_lock();
    if (err == ESP_OK) {
        s_host_state = ESP_BT_HUB_STATE_INITIALIZED;
    } else {
        if (backend->teardown_pending && backend->teardown_pending()) {
            s_host_state = ESP_BT_HUB_STATE_FAULT;
            s_fault_phase = HOST_FAULT_DEINIT;
        } else {
            memset(&s_config, 0, sizeof(s_config));
            s_host_state = ESP_BT_HUB_STATE_STOPPED;
        }
    }
    control_unlock();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Init 'hub': success");
    }
    return err;
}

esp_err_t esp_bt_hub_start(void)
{
    const esp_bt_hub_backend_ops_t *backend = esp_bt_hub_backend_get();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    bool connectable = false;
    esp_bt_hub_discoverable_t discoverable = ESP_BT_HUB_DISCOVERABLE_NONE;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    esp_err_t rollback_err;
    esp_err_t err;

    if (!backend || !backend->start || !backend->stop) {
        ESP_LOGE(TAG, "Start 'hub' failed: backend lifecycle operations are unavailable");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Start 'hub' failed: lifecycle is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_INITIALIZED) {
        control_unlock();
        ESP_LOGE(TAG, "Start 'hub' failed: lifecycle is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    s_host_state = ESP_BT_HUB_STATE_STARTING;
    control_unlock();

    err = backend->start();
    if (err != ESP_OK) {
        rollback_err = backend->stop();
        control_lock();
        if (rollback_err == ESP_OK) {
            s_host_state = ESP_BT_HUB_STATE_INITIALIZED;
        } else {
            s_host_state = ESP_BT_HUB_STATE_FAULT;
            s_fault_phase = HOST_FAULT_STOP;
        }
        control_unlock();
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "Start 'hub' rollback failed: %s", esp_err_to_name(rollback_err));
            return rollback_err;
        }
        return err;
    }
    control_lock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    scan_mode_merge_locked(&connectable, &discoverable);
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    control_unlock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    err = scan_mode_apply(connectable, discoverable);
    if (err != ESP_OK) {
        rollback_err = backend->stop();
        control_lock();
        if (rollback_err == ESP_OK) {
            s_host_state = ESP_BT_HUB_STATE_INITIALIZED;
            s_scan_mode_fault = false;
        } else {
            s_host_state = ESP_BT_HUB_STATE_FAULT;
            s_fault_phase = HOST_FAULT_STOP;
            s_scan_mode_fault = true;
        }
        control_unlock();
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "Start 'hub' scan mode rollback failed: %s", esp_err_to_name(rollback_err));
            return rollback_err;
        }
        ESP_LOGE(TAG, "Start 'hub' failed: apply Classic scan mode %s", esp_err_to_name(err));
        return err;
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    control_lock();
    s_host_state = ESP_BT_HUB_STATE_RUNNING;
    control_unlock();
    ESP_LOGI(TAG, "Start 'hub': success");
    return ESP_OK;
}

esp_err_t esp_bt_hub_stop(void)
{
    const esp_bt_hub_backend_ops_t *backend = esp_bt_hub_backend_get();
    esp_err_t err;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Stop 'hub' failed: lifecycle is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_RUNNING &&
        !(s_host_state == ESP_BT_HUB_STATE_FAULT && s_fault_phase == HOST_FAULT_STOP)) {
        control_unlock();
        ESP_LOGE(TAG, "Stop 'hub' failed: lifecycle is not running or stop-faulted");
        return ESP_ERR_INVALID_STATE;
    }
    if (any_resource_locked()) {
        control_unlock();
        ESP_LOGE(TAG, "Stop 'hub' failed: a resource lease is still held");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_active_calls != 0) {
        control_unlock();
        ESP_LOGE(TAG, "Stop 'hub' failed: a runtime call is still active");
        return ESP_ERR_INVALID_STATE;
    }
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    if (s_scan_mode_fault) {
        control_unlock();
        ESP_LOGE(TAG, "Stop 'hub' failed: Classic scan mode is not reconciled");
        return ESP_ERR_INVALID_STATE;
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    s_host_state = ESP_BT_HUB_STATE_STOPPING;
    control_unlock();

    err = backend->stop();
    control_lock();
    if (err == ESP_OK) {
        s_host_state = ESP_BT_HUB_STATE_INITIALIZED;
        s_fault_phase = HOST_FAULT_NONE;
    } else {
        s_host_state = ESP_BT_HUB_STATE_FAULT;
        s_fault_phase = HOST_FAULT_STOP;
    }
    control_unlock();
    return err;
}

esp_err_t esp_bt_hub_deinit(void)
{
    const esp_bt_hub_backend_ops_t *backend = esp_bt_hub_backend_get();
    esp_err_t err;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Deinit 'hub' failed: lifecycle is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_INITIALIZED &&
        !(s_host_state == ESP_BT_HUB_STATE_FAULT && s_fault_phase == HOST_FAULT_DEINIT)) {
        control_unlock();
        ESP_LOGE(TAG, "Deinit 'hub' failed: lifecycle is not initialized or deinit-faulted");
        return ESP_ERR_INVALID_STATE;
    }
    if (any_client_locked() || any_sub_locked() || any_resource_locked()) {
        control_unlock();
        ESP_LOGE(TAG, "Deinit 'hub' failed: a client, subscription, or lease is still held");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_active_calls != 0) {
        control_unlock();
        ESP_LOGE(TAG, "Deinit 'hub' failed: a backend context call is still active");
        return ESP_ERR_INVALID_STATE;
    }
    s_host_state = ESP_BT_HUB_STATE_DEINITIALIZING;
    control_unlock();

    err = backend->deinit();
    control_lock();
    if (err == ESP_OK) {
        s_host_state = ESP_BT_HUB_STATE_STOPPED;
        s_fault_phase = HOST_FAULT_NONE;
        memset(&s_config, 0, sizeof(s_config));
    } else {
        s_host_state = ESP_BT_HUB_STATE_FAULT;
        s_fault_phase = HOST_FAULT_DEINIT;
    }
    control_unlock();
    return err;
}

esp_err_t esp_bt_hub_client_open(const char *name, esp_bt_hub_client_handle_t *out_client)
{
    size_t len;

    if (!name || !out_client || name[0] == '\0') {
        ESP_LOGE(TAG, "Open client failed: name or handle is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    len = strnlen(name, ESP_BT_HUB_CLIENT_NAME_LEN);
    if (len >= ESP_BT_HUB_CLIENT_NAME_LEN) {
        ESP_LOGE(TAG, "Open client failed: name is too long");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Open client failed: hub is not initialized or running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!host_accepts_profiles_locked()) {
        control_unlock();
        ESP_LOGE(TAG, "Open client failed: hub is not initialized or running");
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < ESP_BT_HUB_MAX_CLIENTS; i++) {
        if (!s_clients[i].in_use) {
            uint32_t generation = esp_bt_hub_generation_next(s_clients[i].generation);

            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            s_clients[i].generation = generation;
            s_clients[i].in_use = true;
            memcpy(s_clients[i].name, name, len + 1U);
            *out_client = token_make(i, s_clients[i].generation);
            control_unlock();
            return ESP_OK;
        }
    }
    control_unlock();
    ESP_LOGE(TAG, "Open client failed: client table is full");
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_bt_hub_client_close(esp_bt_hub_client_handle_t client)
{
    struct esp_bt_hub_client *slot;
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    bool connectable = false;
    esp_bt_hub_discoverable_t discoverable = ESP_BT_HUB_DISCOVERABLE_NONE;
    bool previous_connectable;
    esp_bt_hub_discoverable_t previous_discoverable;
    bool runtime_active = false;
    esp_bt_hub_state_t state;
    esp_err_t rollback_err;
    esp_err_t err;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

    if (!s_ctx) {
        ESP_LOGE(TAG, "Close client failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    xSemaphoreTake(s_scan_mode_mutex, portMAX_DELAY);
    control_lock();
    state = s_host_state;
    if (state == ESP_BT_HUB_STATE_RUNNING) {
        s_active_calls++;
        runtime_active = true;
    } else if (state != ESP_BT_HUB_STATE_INITIALIZED) {
        control_unlock();
        xSemaphoreGive(s_scan_mode_mutex);
        ESP_LOGE(TAG, "Close client failed: hub is not initialized or running");
        return ESP_ERR_INVALID_STATE;
    }
#else
    control_lock();
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    if (!client_lookup_locked(client, &slot)) {
        control_unlock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
        xSemaphoreGive(s_scan_mode_mutex);
        if (runtime_active) {
            esp_bt_hub_runtime_leave();
        }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
        ESP_LOGE(TAG, "Close client failed: stale or unknown client");
        return ESP_ERR_INVALID_ARG;
    }
    if (client_owns_resource_locked(client)) {
        control_unlock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
        xSemaphoreGive(s_scan_mode_mutex);
        if (runtime_active) {
            esp_bt_hub_runtime_leave();
        }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
        ESP_LOGE(TAG, "Close client failed: client still owns a lease");
        return ESP_ERR_INVALID_STATE;
    }
    slot->closing = true;
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    for (size_t i = 0; i < ESP_BT_HUB_MAX_CLIENTS; i++) {
        if (s_clients[i].in_use && &s_clients[i] != slot) {
            connectable |= s_clients[i].connectable;
            if (s_clients[i].discoverable > discoverable) {
                discoverable = s_clients[i].discoverable;
            }
        }
    }
    previous_connectable = connectable || slot->connectable;
    previous_discoverable = slot->discoverable > discoverable ? slot->discoverable : discoverable;
    control_unlock();
    if (runtime_active) {
        err = esp_bt_hub_backend_get()->classic_scan_mode(connectable, discoverable);
        if (err != ESP_OK) {
            rollback_err = esp_bt_hub_backend_get()->classic_scan_mode(previous_connectable, previous_discoverable);
            control_lock();
            slot->closing = false;
            s_scan_mode_fault = rollback_err != ESP_OK;
            control_unlock();
            xSemaphoreGive(s_scan_mode_mutex);
            esp_bt_hub_runtime_leave();
            return err;
        }
    }
    control_lock();
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    if (client_owns_resource_locked(client)) {
        slot->closing = false;
        control_unlock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
        rollback_err = runtime_active ? esp_bt_hub_backend_get()->classic_scan_mode(previous_connectable, previous_discoverable) : ESP_OK;
        control_lock();
        s_scan_mode_fault = rollback_err != ESP_OK;
        control_unlock();
        xSemaphoreGive(s_scan_mode_mutex);
        if (runtime_active) {
            esp_bt_hub_runtime_leave();
        }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
        ESP_LOGE(TAG, "Close client failed: client acquired a lease while closing");
        return ESP_ERR_INVALID_STATE;
    }
    slot->in_use = false;
    slot->closing = false;
    slot->connectable = false;
    slot->discoverable = ESP_BT_HUB_DISCOVERABLE_NONE;
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    s_scan_mode_fault = false;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    control_unlock();
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    xSemaphoreGive(s_scan_mode_mutex);
    if (runtime_active) {
        esp_bt_hub_runtime_leave();
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    return ESP_OK;
}

esp_err_t esp_bt_hub_gap_subscribe(esp_bt_hub_gap_domain_t domain, esp_bt_hub_gap_cb_t callback,
                                   void *user_ctx, esp_bt_hub_gap_sub_handle_t *out_sub)
{
    if (!callback || !out_sub) {
        ESP_LOGE(TAG, "Subscribe GAP failed: callback or handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!domain_supported(domain)) {
        ESP_LOGE(TAG, "Subscribe GAP failed: domain is unsupported");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Subscribe GAP failed: hub is not initialized or running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!host_accepts_profiles_locked()) {
        control_unlock();
        ESP_LOGE(TAG, "Subscribe GAP failed: hub is not initialized or running");
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < ESP_BT_HUB_MAX_SUBS; i++) {
        if (!s_subs[i].in_use) {
            uint32_t generation = esp_bt_hub_generation_next(s_subs[i].generation);

            memset(&s_subs[i], 0, sizeof(s_subs[i]));
            s_subs[i].generation = generation;
            s_subs[i].in_use = true;
            s_subs[i].domain = domain;
            s_subs[i].callback = callback;
            s_subs[i].user_ctx = user_ctx;
            *out_sub = token_make(i, s_subs[i].generation);
            control_unlock();
            return ESP_OK;
        }
    }
    control_unlock();
    ESP_LOGE(TAG, "Subscribe GAP failed: subscription table is full");
    return ESP_ERR_NO_MEM;
}

esp_err_t esp_bt_hub_gap_unsubscribe(esp_bt_hub_gap_sub_handle_t sub)
{
    struct esp_bt_hub_gap_sub *slot;
    bool wait_for_dispatch = false;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Unsubscribe GAP failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!sub_lookup_locked(sub, &slot)) {
        control_unlock();
        ESP_LOGE(TAG, "Unsubscribe GAP failed: stale or unknown subscription");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_dispatching) {
        slot->pending_remove = true;
        wait_for_dispatch = xTaskGetCurrentTaskHandle() != s_dispatch_task;
    } else {
        slot->in_use = false;
        slot->callback = NULL;
        slot->user_ctx = NULL;
    }
    control_unlock();
    if (wait_for_dispatch) {
        xEventGroupWaitBits(s_dispatch_event, ESP_BT_HUB_DISPATCH_IDLE_BIT,
                            pdFALSE, pdTRUE, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t esp_bt_hub_device_name_get(char *name, size_t length)
{
    if (!name || length == 0) {
        ESP_LOGE(TAG, "Get device name failed: output is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Get device name failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_host_state != ESP_BT_HUB_STATE_RUNNING) {
        control_unlock();
        ESP_LOGE(TAG, "Get device name failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    strlcpy(name, s_config.device_name, length);
    control_unlock();
    return ESP_OK;
}

esp_err_t esp_bt_hub_device_name_set(const char *name)
{
    size_t len;
    esp_err_t err;

    if (!name || name[0] == '\0') {
        ESP_LOGE(TAG, "Set device name failed: name is empty");
        return ESP_ERR_INVALID_ARG;
    }
    len = strnlen(name, ESP_BT_HUB_MAX_DEV_NAME_LEN);
    if (len >= ESP_BT_HUB_MAX_DEV_NAME_LEN) {
        ESP_LOGE(TAG, "Set device name failed: name is too long");
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_bt_hub_runtime_enter() != ESP_OK) {
        ESP_LOGE(TAG, "Set device name failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = esp_bt_hub_backend_get()->set_device_name(name);
    if (err != ESP_OK) {
        esp_bt_hub_runtime_leave();
        return err;
    }
    control_lock();
    strlcpy(s_config.device_name, name, sizeof(s_config.device_name));
    control_unlock();
    esp_bt_hub_runtime_leave();
    return ESP_OK;
}

esp_err_t esp_bt_hub_local_address_get(uint8_t address[6])
{
    if (!address) {
        ESP_LOGE(TAG, "Get local address failed: output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_bt_hub_runtime_enter() != ESP_OK) {
        ESP_LOGE(TAG, "Get local address failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_bt_hub_backend_get()->get_address(address);

    esp_bt_hub_runtime_leave();
    return err;
}

#if ESP_BT_HUB_LE_SUPPORTED
esp_err_t esp_bt_hub_le_bond_count(size_t *out_count)
{
    esp_err_t err;

    if (!out_count) {
        ESP_LOGE(TAG, "Get LE bond count failed: output is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    *out_count = esp_bt_hub_backend_get()->le_bond_count();
    esp_bt_hub_runtime_leave();
    return ESP_OK;
}

esp_err_t esp_bt_hub_le_bond_exists(uint8_t addr_type, const uint8_t addr[6], bool *out_exists)
{
    esp_err_t err;

    if (!addr || !out_exists) {
        ESP_LOGE(TAG, "Check LE bond failed: input is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    *out_exists = esp_bt_hub_backend_get()->le_bond_exists(addr_type, addr);
    esp_bt_hub_runtime_leave();
    return ESP_OK;
}

esp_err_t esp_bt_hub_le_disconnect(uint16_t conn_handle, uint8_t reason)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_disconnect(conn_handle, reason);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_security_initiate(uint16_t conn_handle)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_security_initiate(conn_handle);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_privacy_enable(void)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_privacy_enable();
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_security_reply(const uint8_t peer[6], bool accept)
{
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE security reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_security_reply(peer, accept);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_confirm_reply(const uint8_t peer[6], bool accept)
{
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE confirm reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_confirm_reply(peer, accept);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_passkey_reply(const uint8_t peer[6], bool accept, uint32_t passkey)
{
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE passkey reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_passkey_reply(peer, accept, passkey);
    esp_bt_hub_runtime_leave();
    return err;
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
esp_err_t esp_bt_hub_le_connect(const esp_bt_hub_le_connect_params_t *params)
{
    esp_err_t err;

    if (!params || !params->conn_params) {
        ESP_LOGE(TAG, "Connect failed: params or connection parameters are NULL");
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_BT_BLUEDROID_ENABLED
    if (params->gattc_if == ESP_GATT_IF_NONE) {
        ESP_LOGE(TAG, "Connect failed: GATTC interface is none");
        return ESP_ERR_INVALID_ARG;
    }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_connect(params);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_connect_cancel(void)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_connect_cancel();
    esp_bt_hub_runtime_leave();
    return err;
}

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
esp_err_t esp_bt_hub_le_pa_sync_create(uint8_t addr_type, const uint8_t addr[6], uint8_t sid,
                                       const esp_bt_hub_le_periodic_sync_params_t *params)
{
    esp_err_t err;

    if (!addr || !params) {
        ESP_LOGE(TAG, "Create PA sync failed: addr or params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_pa_sync_create(addr_type, addr, sid, params);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_pa_sync_terminate(uint16_t sync_handle)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_pa_sync_terminate(sync_handle);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_pa_sync_create_cancel(void)
{
    esp_err_t err = esp_bt_hub_runtime_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_pa_sync_create_cancel();
    esp_bt_hub_runtime_leave();
    return err;
}

#if ESP_BT_HUB_LE_PAST_SUPPORTED
esp_err_t esp_bt_hub_le_pa_sync_receive(uint16_t conn_handle,
                                        const esp_bt_hub_le_periodic_sync_params_t *params)
{
    esp_err_t err;

    if (!params) {
        ESP_LOGE(TAG, "Receive PA sync failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_pa_sync_receive(conn_handle, params);
    esp_bt_hub_runtime_leave();
    return err;
}
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static void scan_mode_merge_locked(bool *connectable, esp_bt_hub_discoverable_t *discoverable)
{
    *connectable = false;
    *discoverable = ESP_BT_HUB_DISCOVERABLE_NONE;
    for (size_t i = 0; i < ESP_BT_HUB_MAX_CLIENTS; i++) {
        if (s_clients[i].in_use) {
            *connectable |= s_clients[i].connectable;
            if (s_clients[i].discoverable > *discoverable) {
                *discoverable = s_clients[i].discoverable;
            }
        }
    }
}

static esp_err_t scan_mode_apply(bool connectable, esp_bt_hub_discoverable_t discoverable)
{
    return esp_bt_hub_backend_get()->classic_scan_mode(connectable, discoverable);
}

esp_err_t esp_bt_hub_classic_scan_mode_set(esp_bt_hub_client_handle_t client, bool connectable,
                                           esp_bt_hub_discoverable_t discoverable)
{
    struct esp_bt_hub_client *slot;
    bool old_connectable;
    esp_bt_hub_discoverable_t old_discoverable;
    bool previous_connectable;
    esp_bt_hub_discoverable_t previous_discoverable;
    bool merged_connectable;
    esp_bt_hub_discoverable_t merged_discoverable;
    esp_err_t rollback_err = ESP_OK;
    esp_err_t err;

    if (discoverable > ESP_BT_HUB_DISCOVERABLE_GENERAL) {
        ESP_LOGE(TAG, "Set scan mode failed: discoverable is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (esp_bt_hub_runtime_enter() != ESP_OK) {
        ESP_LOGE(TAG, "Set scan mode failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_scan_mode_mutex, portMAX_DELAY);
    control_lock();
    if (!client_lookup_locked(client, &slot)) {
        control_unlock();
        xSemaphoreGive(s_scan_mode_mutex);
        esp_bt_hub_runtime_leave();
        ESP_LOGE(TAG, "Set scan mode failed: stale or unknown client");
        return ESP_ERR_INVALID_ARG;
    }
    old_connectable = slot->connectable;
    old_discoverable = slot->discoverable;
    scan_mode_merge_locked(&previous_connectable, &previous_discoverable);
    slot->connectable = connectable;
    slot->discoverable = discoverable;
    scan_mode_merge_locked(&merged_connectable, &merged_discoverable);
    control_unlock();
    err = scan_mode_apply(merged_connectable, merged_discoverable);
    if (err != ESP_OK) {
        control_lock();
        slot->connectable = old_connectable;
        slot->discoverable = old_discoverable;
        control_unlock();
        rollback_err = scan_mode_apply(previous_connectable, previous_discoverable);
        if (rollback_err != ESP_OK) {
            ESP_LOGE(TAG, "Set scan mode rollback failed: backend is inconsistent");
        }
    }
    control_lock();
    s_scan_mode_fault = rollback_err != ESP_OK;
    control_unlock();
    xSemaphoreGive(s_scan_mode_mutex);
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_classic_discovery_start(esp_bt_hub_client_handle_t client, uint8_t inquiry_mode,
                                             uint8_t inquiry_len, uint8_t num_responses,
                                             esp_bt_hub_discovery_handle_t *out_discovery)
{
    esp_bt_hub_discovery_handle_t discovery;
    esp_err_t err;

    if (!out_discovery) {
        ESP_LOGE(TAG, "Start discovery failed: output handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Start discovery failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    err = resource_acquire_locked(&s_discovery, client, false, (void **)&discovery);
    control_unlock();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start discovery failed: client or resource state is invalid");
        return err;
    }
    err = esp_bt_hub_backend_get()->classic_discovery_start(inquiry_mode, inquiry_len, num_responses);
    control_lock();
    if (err == ESP_OK) {
        s_discovery.state = RESOURCE_ACTIVE;
        *out_discovery = discovery;
    } else {
        resource_free_locked(&s_discovery);
    }
    control_unlock();
    return err;
}

esp_err_t esp_bt_hub_classic_discovery_stop(esp_bt_hub_discovery_handle_t discovery)
{
    resource_slot_t *slot;
    esp_err_t err;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Stop discovery failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_discovery, 1, discovery, &slot, NULL) ||
        (slot->state != RESOURCE_ACTIVE && slot->state != RESOURCE_FAULT)) {
        control_unlock();
        ESP_LOGE(TAG, "Stop discovery failed: stale or inactive lease");
        return ESP_ERR_INVALID_ARG;
    }
    slot->state = RESOURCE_STOPPING;
    control_unlock();
    err = esp_bt_hub_backend_get()->classic_discovery_stop();
    control_lock();
    if (err == ESP_OK) {
        resource_free_locked(slot);
    } else {
        slot->state = RESOURCE_FAULT;
    }
    control_unlock();
    return err;
}

static void inquiry_copy_name(char *name, size_t name_len, const uint8_t *src, size_t src_len)
{
    size_t copy_len = src_len < name_len ? src_len : name_len - 1U;

    memcpy(name, src, copy_len);
    name[copy_len] = '\0';
}

static bool inquiry_eir_name(const uint8_t *eir, size_t eir_len, char *name, size_t name_len, bool *complete)
{
    size_t offset = 0;
    bool found = false;

    while (offset + 1U < eir_len) {
        uint8_t field_len = eir[offset];
        uint8_t type;

        if (field_len == 0 || offset + field_len >= eir_len) {
            break;
        }
        type = eir[offset + 1U];
        if (type == ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME ||
            (type == ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME && !found)) {
            inquiry_copy_name(name, name_len, eir + offset + 2U, field_len - 1U);
            found = true;
            *complete = type == ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME;
        }
        offset += field_len + 1U;
    }
    return found;
}

esp_err_t esp_bt_hub_classic_inquiry_result_parse(const void *param, esp_bt_hub_classic_inquiry_result_t *out)
{
    const esp_bt_gap_cb_param_t *gap = param;
    bool complete = false;

    if (!param || !out) {
        ESP_LOGE(TAG, "Parse inquiry result failed: argument is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->addr, gap->disc_res.bda, sizeof(out->addr));
    for (int i = 0; i < gap->disc_res.num_prop; i++) {
        esp_bt_gap_dev_prop_t *prop = &gap->disc_res.prop[i];

        if (!prop->val || prop->len == 0) {
            continue;
        }
        if (prop->type == ESP_BT_GAP_DEV_PROP_EIR) {
            inquiry_eir_name(prop->val, prop->len, out->name, sizeof(out->name), &complete);
        } else if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME && !complete && out->name[0] == '\0') {
            inquiry_copy_name(out->name, sizeof(out->name), prop->val, prop->len);
        } else if (prop->type == ESP_BT_GAP_DEV_PROP_COD && prop->len >= sizeof(out->cod)) {
            memcpy(&out->cod, prop->val, sizeof(out->cod));
            out->has_cod = true;
        } else if (prop->type == ESP_BT_GAP_DEV_PROP_RSSI) {
            out->rssi = *(const int8_t *)prop->val;
            out->has_rssi = true;
        }
    }
    return ESP_OK;
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_SUPPORTED
esp_err_t esp_bt_hub_le_adv_report_parse(const esp_bt_hub_gap_ble_param_t *param,
                                         esp_bt_hub_le_adv_report_t *out)
{
    esp_bt_hub_le_adv_report_t report = {0};

    if (!param || !out) {
        ESP_LOGE(TAG, "Parse LE advertising report failed: argument is NULL");
        return ESP_ERR_INVALID_ARG;
    }
#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV
    const struct ble_gap_event *gap = param;
    const struct ble_gap_ext_disc_desc *disc = &gap->ext_disc;
    memcpy(report.addr, disc->addr.val, sizeof(report.addr));
    report.data = disc->data;
    report.data_len = disc->length_data;
    report.rssi = disc->rssi;
    report.addr_type = disc->addr.type;
    report.sid = disc->sid;
    report.connectable = (disc->props & ESP_BT_HUB_LE_ADV_PROP_CONNECTABLE) != 0;
#elif CONFIG_BT_BLE_ENABLED && CONFIG_BT_BLE_50_FEATURES_SUPPORTED
    const esp_ble_gap_cb_param_t *gap = param;
    const esp_ble_gap_ext_adv_report_t *native_report = &gap->ext_adv_report.params;
    esp_bt_hub_le_addr_swap_if_need(report.addr, native_report->addr);
    report.data        = native_report->adv_data;
    report.data_len    = native_report->adv_data_len;
    report.rssi        = native_report->rssi;
    report.addr_type   = native_report->addr_type;
    report.sid         = native_report->sid;
    report.connectable = (native_report->event_type & ESP_BT_HUB_LE_ADV_PROP_CONNECTABLE) != 0;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV */
    *out = report;
    return ESP_OK;
}

esp_err_t esp_bt_hub_le_external_scan_acquire(esp_bt_hub_client_handle_t client,
                                              esp_bt_hub_le_scan_handle_t *out_scan)
{
    esp_err_t err;

    if (!out_scan) {
        ESP_LOGE(TAG, "Acquire external scan failed: output handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Acquire external scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    err = resource_acquire_locked(&s_le_scan, client, true, (void **)out_scan);
    control_unlock();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Acquire external scan failed: client or resource state is invalid");
    }
    return err;
}

esp_err_t esp_bt_hub_le_external_scan_release(esp_bt_hub_le_scan_handle_t scan)
{
    resource_slot_t *slot;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Release external scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_scan, 1, scan, &slot, NULL) ||
        slot->state != RESOURCE_ACTIVE || !slot->external) {
        control_unlock();
        ESP_LOGE(TAG, "Release external scan failed: stale or non-external lease");
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->ext_scan_started || slot->ext_scan_busy) {
        control_unlock();
        ESP_LOGE(TAG, "Release external scan failed: extended scanning is active");
        return ESP_ERR_INVALID_STATE;
    }
    resource_free_locked(slot);
    control_unlock();
    return ESP_OK;
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
esp_err_t esp_bt_hub_le_ext_scan_start(esp_bt_hub_le_scan_handle_t scan,
                                       const esp_bt_hub_le_ext_scan_params_t *params)
{
    resource_slot_t *slot;
    esp_err_t err;

    if (!scan || !params ||
        (params->phy_mask & ~(ESP_BT_HUB_LE_PHY_1M_MASK | ESP_BT_HUB_LE_PHY_CODED_MASK))) {
        ESP_LOGE(TAG, "Start ext scan failed: scan handle, params, or PHY mask is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start ext scan failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!control_lock()) {
        esp_bt_hub_runtime_leave();
        ESP_LOGE(TAG, "Start ext scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_scan, 1, scan, &slot, NULL) ||
        slot->state != RESOURCE_ACTIVE) {
        control_unlock();
        esp_bt_hub_runtime_leave();
        ESP_LOGE(TAG, "Start ext scan failed: caller does not hold an active LE scan lease");
        return ESP_ERR_INVALID_STATE;
    }
    slot->ext_scan_busy = true;
    control_unlock();
    err = esp_bt_hub_backend_get()->le_ext_scan_start(params);
    control_lock();
    /* The lease cannot be freed while ext_scan_busy is set; still write the
     * flags only into the lease that is still active. */
    if (slot->state == RESOURCE_ACTIVE) {
        slot->ext_scan_busy = false;
        if (err == ESP_OK) {
            slot->ext_scan_started = true;
        }
    }
    control_unlock();
    esp_bt_hub_runtime_leave();
    return err;
}

esp_err_t esp_bt_hub_le_ext_scan_stop(esp_bt_hub_le_scan_handle_t scan)
{
    resource_slot_t *slot;
    esp_err_t err;

    if (!scan) {
        ESP_LOGE(TAG, "Stop ext scan failed: scan handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop ext scan failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (!control_lock()) {
        esp_bt_hub_runtime_leave();
        ESP_LOGE(TAG, "Stop ext scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_scan, 1, scan, &slot, NULL) ||
        slot->state != RESOURCE_ACTIVE) {
        control_unlock();
        esp_bt_hub_runtime_leave();
        ESP_LOGE(TAG, "Stop ext scan failed: caller does not hold an active LE scan lease");
        return ESP_ERR_INVALID_STATE;
    }
    slot->ext_scan_busy = true;
    control_unlock();
    /* Both backends treat a stop of an already idle scan as success, so this
     * stays valid after autonomous termination (esp_bt_hub_le_ext_scan_complete()). */
    err = esp_bt_hub_backend_get()->le_ext_scan_stop();
    control_lock();
    /* The lease cannot be freed while ext_scan_busy is set; still write the
     * flags only into the lease that is still active. */
    if (slot->state == RESOURCE_ACTIVE) {
        slot->ext_scan_busy = false;
        if (err == ESP_OK) {
            slot->ext_scan_started = false;
        }
    }
    control_unlock();
    esp_bt_hub_runtime_leave();
    return err;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
static esp_err_t ext_adv_operation_lock_ex(esp_bt_hub_le_ext_adv_handle_t adv, bool allow_fault,
                                           resource_slot_t **out_slot, size_t *out_index)
{
    resource_slot_t *slot;
    size_t index;

    if (!token_decode(adv, ESP_BT_HUB_MAX_EXT_ADV_INSTANCES, &index, NULL)) {
        ESP_LOGE(TAG, "Extended advertising operation failed: invalid handle");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ctx) {
        ESP_LOGE(TAG, "Extended advertising operation failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_ext_adv_operation_mutex[index], portMAX_DELAY);
    control_lock();
    if (!resource_lookup_locked(s_ext_adv, ESP_BT_HUB_MAX_EXT_ADV_INSTANCES, adv, &slot, out_index) ||
        (slot->state != RESOURCE_ACTIVE && !(allow_fault && slot->state == RESOURCE_FAULT))) {
        control_unlock();
        xSemaphoreGive(s_ext_adv_operation_mutex[index]);
        ESP_LOGE(TAG, "Extended advertising operation failed: stale or inactive handle");
        return ESP_ERR_INVALID_ARG;
    }
    if (out_slot) {
        *out_slot = slot;
    }
    control_unlock();
    return ESP_OK;
}

static esp_err_t ext_adv_operation_lock(esp_bt_hub_le_ext_adv_handle_t adv,
                                        resource_slot_t **out_slot, size_t *out_index)
{
    return ext_adv_operation_lock_ex(adv, false, out_slot, out_index);
}

static void ext_adv_operation_unlock(size_t index)
{
    xSemaphoreGive(s_ext_adv_operation_mutex[index]);
}
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */

esp_err_t esp_bt_hub_le_ext_adv_acquire(esp_bt_hub_client_handle_t client,
                                        esp_bt_hub_le_ext_adv_handle_t *out_adv)
{
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES == 0
    ESP_LOGE(TAG, "Acquire extended advertising failed: extended advertising is unsupported");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!out_adv) {
        ESP_LOGE(TAG, "Acquire extended advertising failed: output handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Acquire extended advertising failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!client_lookup_locked(client, NULL)) {
        control_unlock();
        ESP_LOGE(TAG, "Acquire extended advertising failed: stale or unknown client");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_host_state != ESP_BT_HUB_STATE_RUNNING) {
        control_unlock();
        ESP_LOGE(TAG, "Acquire extended advertising failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < (size_t)ESP_BT_HUB_MAX_EXT_ADV_INSTANCES; i++) {
#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV
        if (i == (size_t)ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE &&
            s_le_adv.state != RESOURCE_FREE) {
            continue;
        }
#endif  /* CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV */
#if CONFIG_BT_BLUEDROID_ENABLED
        if (i == 0) {
            /* The Bluedroid legacy-advertising path (esp_ble_gap_start_advertising())
             * drives controller advertising set 0, and unlike the NimBLE guard
             * above this usage is invisible to the hub: applications may call
             * the raw legacy API while holding the external legacy lease (the
             * BLUFI profile does exactly that). Keep set 0 reserved so a handed
             * out extended-advertising instance can never collide with it:
             * LE Set Ext Adv Params is rejected with Command Disallowed while
             * the set is enabled. */
            continue;
        }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
        if (s_ext_adv[i].state == RESOURCE_FREE) {
            s_ext_adv[i].generation = esp_bt_hub_generation_next(s_ext_adv[i].generation);
            s_ext_adv[i].state      = RESOURCE_ACTIVE;
            s_ext_adv[i].owner      = client;
            *out_adv                = token_make(i, s_ext_adv[i].generation);
            control_unlock();
            return ESP_OK;
        }
    }
    control_unlock();
    ESP_LOGE(TAG, "Acquire extended advertising failed: all instances are owned");
    return ESP_ERR_NOT_FOUND;
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES == 0 */
}

esp_err_t esp_bt_hub_le_ext_adv_get_instance(esp_bt_hub_le_ext_adv_handle_t adv,
                                             uint8_t *out_instance)
{
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    size_t index;

    if (!out_instance) {
        ESP_LOGE(TAG, "Get extended advertising instance failed: output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Get extended advertising instance failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(s_ext_adv, ESP_BT_HUB_MAX_EXT_ADV_INSTANCES,
                                adv, NULL, &index)) {
        control_unlock();
        ESP_LOGE(TAG, "Get extended advertising instance failed: stale or inactive lease");
        return ESP_ERR_INVALID_ARG;
    }
    *out_instance = (uint8_t)index;
    control_unlock();
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Get extended advertising instance failed: extended advertising is unsupported");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
}

esp_err_t esp_bt_hub_le_ext_adv_release(esp_bt_hub_le_ext_adv_handle_t adv)
{
#if ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0
    resource_slot_t *slot;
    size_t index;
    esp_err_t err;

    err = ext_adv_operation_lock_ex(adv, true, &slot, &index);
    if (err != ESP_OK) {
        return err;
    }
    control_lock();
    if (slot->ext_adv_started || slot->periodic_adv_started) {
        control_unlock();
        ext_adv_operation_unlock(index);
        ESP_LOGE(TAG, "Release extended advertising failed: advertising is active");
        return ESP_ERR_INVALID_STATE;
    }
    if (slot->native_configured) {
        control_unlock();
        err = esp_bt_hub_backend_get()->le_ext_adv_remove((uint8_t)index);
        control_lock();
        if (err != ESP_OK) {
            slot->state = RESOURCE_FAULT;
            control_unlock();
            ext_adv_operation_unlock(index);
            ESP_LOGE(TAG, "Release extended advertising failed: remove native set failed");
            return err;
        }
        slot->native_configured = false;
    }
    resource_free_locked(slot);
    control_unlock();
    ext_adv_operation_unlock(index);
    return ESP_OK;
#else
    ESP_LOGE(TAG, "Release extended advertising failed: extended advertising is unsupported");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* ESP_BT_HUB_MAX_EXT_ADV_INSTANCES > 0 */
}

bool esp_bt_hub_le_scan_handle_valid(esp_bt_hub_le_scan_handle_t scan)
{
    resource_slot_t *slot;
    bool valid;

    if (!control_lock()) {
        return false;
    }
    valid = resource_lookup_locked(&s_le_scan, 1, scan, &slot, NULL) && (slot->state == RESOURCE_ACTIVE || slot->state == RESOURCE_ACQUIRING);
    control_unlock();
    return valid;
}
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
esp_err_t esp_bt_hub_le_scan_start(esp_bt_hub_client_handle_t client, const esp_bt_hub_le_scan_params_t *params,
                                   esp_bt_hub_le_scan_handle_t *out_scan)
{
    esp_bt_hub_le_scan_handle_t scan;
    esp_err_t err;

    if (!params || !out_scan) {
        ESP_LOGE(TAG, "Start LE scan failed: params or output handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Start LE scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    err = resource_acquire_locked(&s_le_scan, client, false, (void **)&scan);
    control_unlock();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start LE scan failed: client or resource state is invalid");
        return err;
    }
    err = esp_bt_hub_backend_get()->le_scan_start(params);
    if (err != ESP_OK) {
        control_lock();
        resource_free_locked(&s_le_scan);
        control_unlock();
    }
#if CONFIG_BT_NIMBLE_ENABLED
    else {
        esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, true);
    }
#endif  /* CONFIG_BT_NIMBLE_ENABLED */
    if (err == ESP_OK) {
        *out_scan = scan;
    }
    return err;
}

esp_err_t esp_bt_hub_le_scan_stop(esp_bt_hub_le_scan_handle_t scan)
{
    resource_slot_t *slot;
    bool ext_scan_started;
    esp_err_t err;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Stop LE scan failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_scan, 1, scan, &slot, NULL) ||
        (slot->state != RESOURCE_ACTIVE && slot->state != RESOURCE_FAULT) || slot->external) {
        control_unlock();
        ESP_LOGE(TAG, "Stop LE scan failed: stale or inactive lease");
        return ESP_ERR_INVALID_ARG;
    }
    if (slot->ext_scan_busy) {
        control_unlock();
        ESP_LOGE(TAG, "Stop LE scan failed: an extended scan operation is in flight");
        return ESP_ERR_INVALID_STATE;
    }
    ext_scan_started = slot->ext_scan_started;
    slot->state = RESOURCE_STOPPING;
    control_unlock();
    if (ext_scan_started) {
        /* Bluedroid legacy stop does not stop an extended scan, so stop it
         * first or the released lease would leave the controller scanning. */
        err = esp_bt_hub_backend_get()->le_ext_scan_stop();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Stop LE scan failed: stop extended scanning %s", esp_err_to_name(err));
            control_lock();
            slot->state = RESOURCE_FAULT;
            control_unlock();
            return err;
        }
    }
    err = esp_bt_hub_backend_get()->le_scan_stop();
    control_lock();
    if (err == ESP_OK) {
        resource_free_locked(slot);
    } else {
        slot->state = RESOURCE_FAULT;
    }
    control_unlock();
    return err;
}

esp_err_t esp_bt_hub_le_adv_start(esp_bt_hub_client_handle_t client, const esp_bt_hub_le_adv_params_t *params,
                                  esp_bt_hub_le_adv_handle_t *out_adv)
{
    esp_bt_hub_le_adv_handle_t adv;
    esp_err_t err;

    if (!params || !out_adv || (params->data_len && !params->data)) {
        ESP_LOGE(TAG, "Start legacy advertising failed: argument is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Start legacy advertising failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    err = le_adv_resource_acquire_locked(client, false, (void **)&adv);
    control_unlock();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start legacy advertising failed: client or resource state is invalid");
        return err;
    }
    err = esp_bt_hub_backend_get()->le_adv_start(params);
    if (err != ESP_OK) {
        control_lock();
        resource_free_locked(&s_le_adv);
        control_unlock();
    }
#if CONFIG_BT_NIMBLE_ENABLED
    else {
        esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, true);
    }
#endif  /* CONFIG_BT_NIMBLE_ENABLED */
    if (err == ESP_OK) {
        *out_adv = adv;
    }
    return err;
}

esp_err_t esp_bt_hub_le_adv_stop(esp_bt_hub_le_adv_handle_t adv)
{
    resource_slot_t *slot;
    esp_err_t err;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Stop legacy advertising failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_adv, 1, adv, &slot, NULL) ||
        (slot->state != RESOURCE_ACTIVE && slot->state != RESOURCE_FAULT) || slot->external) {
        control_unlock();
        ESP_LOGE(TAG, "Stop legacy advertising failed: stale or inactive lease");
        return ESP_ERR_INVALID_ARG;
    }
    slot->state = RESOURCE_STOPPING;
    control_unlock();
    err = esp_bt_hub_backend_get()->le_adv_stop();
    control_lock();
    if (err == ESP_OK) {
        resource_free_locked(slot);
    } else {
        slot->state = RESOURCE_FAULT;
    }
    control_unlock();
    return err;
}

esp_err_t esp_bt_hub_le_external_adv_acquire(esp_bt_hub_client_handle_t client,
                                             esp_bt_hub_le_adv_handle_t *out_adv)
{
    esp_err_t err;

    if (!out_adv) {
        ESP_LOGE(TAG, "Acquire external advertising failed: output handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!control_lock()) {
        ESP_LOGE(TAG, "Acquire external advertising failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    err = le_adv_resource_acquire_locked(client, true, (void **)out_adv);
    control_unlock();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Acquire external advertising failed: client or resource state is invalid");
    }
    return err;
}

esp_err_t esp_bt_hub_le_external_adv_release(esp_bt_hub_le_adv_handle_t adv)
{
    resource_slot_t *slot;

    if (!control_lock()) {
        ESP_LOGE(TAG, "Release external advertising failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!resource_lookup_locked(&s_le_adv, 1, adv, &slot, NULL) ||
        slot->state != RESOURCE_ACTIVE || !slot->external) {
        control_unlock();
        ESP_LOGE(TAG, "Release external advertising failed: stale or non-external lease");
        return ESP_ERR_INVALID_ARG;
    }
    resource_free_locked(slot);
    control_unlock();
    return ESP_OK;
}
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
esp_err_t esp_bt_hub_le_ext_adv_configure(esp_bt_hub_le_ext_adv_handle_t adv,
                                          const esp_bt_hub_le_ext_adv_params_t *params)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err;

    if (!params || (params->tx_power != ESP_BT_HUB_LE_EXT_ADV_TX_POWER_NO_PREFERENCE &&
                    (params->tx_power < ESP_BT_HUB_LE_EXT_ADV_TX_POWER_MIN_DBM ||
                     params->tx_power > ESP_BT_HUB_LE_EXT_ADV_TX_POWER_MAX_DBM))) {
        ESP_LOGE(TAG, "Configure extended advertising failed: params or tx power is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = ext_adv_operation_lock(adv, &slot, &index);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_ext_adv_configure((uint8_t)index, params);
    if (err == ESP_OK) {
        control_lock();
        slot->native_configured = true;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_ext_adv_set_data(esp_bt_hub_le_ext_adv_handle_t adv, const uint8_t *data, size_t len)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set extended advertising data failed: data or length is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = ext_adv_operation_lock(adv, &slot, &index);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_ext_adv_set_data((uint8_t)index, data, len);
    if (err == ESP_OK) {
        control_lock();
        slot->native_configured = true;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_ext_adv_set_scan_rsp(esp_bt_hub_le_ext_adv_handle_t adv, const uint8_t *data, size_t len)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set extended advertising scan response failed: data or length is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = ext_adv_operation_lock(adv, &slot, &index);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_ext_adv_set_scan_rsp((uint8_t)index, data, len);
    if (err == ESP_OK) {
        control_lock();
        slot->native_configured = true;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_ext_adv_start(esp_bt_hub_le_ext_adv_handle_t adv, int duration, int max_events)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err = ext_adv_operation_lock(adv, &slot, &index);

    if (err != ESP_OK) {
        return err;
    }
    control_lock();
    if (slot->ext_adv_started) {
        control_unlock();
        ext_adv_operation_unlock(index);
        ESP_LOGE(TAG, "Start extended advertising failed: instance is already active");
        return ESP_ERR_INVALID_STATE;
    }
    slot->ext_adv_started = true;
    control_unlock();
    err = esp_bt_hub_backend_get()->le_ext_adv_start((uint8_t)index, slot->generation, duration, max_events);
    if (err != ESP_OK) {
        control_lock();
        slot->ext_adv_started = false;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_ext_adv_stop(esp_bt_hub_le_ext_adv_handle_t adv)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err = ext_adv_operation_lock(adv, &slot, &index);

    if (err != ESP_OK) {
        return err;
    }
    control_lock();
    if (!slot->ext_adv_started) {
        control_unlock();
        ext_adv_operation_unlock(index);
        ESP_LOGE(TAG, "Stop extended advertising failed: instance is not active");
        return ESP_ERR_INVALID_STATE;
    }
    control_unlock();
    err = esp_bt_hub_backend_get()->le_ext_adv_stop((uint8_t)index);
    if (err == ESP_OK) {
        control_lock();
        slot->ext_adv_started = false;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
esp_err_t esp_bt_hub_le_periodic_adv_configure(esp_bt_hub_le_ext_adv_handle_t adv,
                                               const esp_bt_hub_le_periodic_adv_params_t *params)
{
    size_t index;
    esp_err_t err;

    if (!params) {
        ESP_LOGE(TAG, "Configure periodic advertising failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = ext_adv_operation_lock(adv, NULL, &index);

    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_periodic_adv_configure((uint8_t)index, params);
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_periodic_adv_set_data(esp_bt_hub_le_ext_adv_handle_t adv,
                                              const uint8_t *data, size_t len)
{
    size_t index;
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set periodic advertising data failed: data or length is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    err = ext_adv_operation_lock(adv, NULL, &index);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_bt_hub_backend_get()->le_periodic_adv_set_data((uint8_t)index, data, len);
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_periodic_adv_start(esp_bt_hub_le_ext_adv_handle_t adv)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err = ext_adv_operation_lock(adv, &slot, &index);

    if (err != ESP_OK) {
        return err;
    }
    control_lock();
    if (slot->periodic_adv_started) {
        control_unlock();
        ext_adv_operation_unlock(index);
        ESP_LOGE(TAG, "Start periodic advertising failed: instance is already active");
        return ESP_ERR_INVALID_STATE;
    }
    slot->periodic_adv_started = true;
    control_unlock();
    err = esp_bt_hub_backend_get()->le_periodic_adv_start((uint8_t)index);
    if (err != ESP_OK) {
        control_lock();
        slot->periodic_adv_started = false;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}

esp_err_t esp_bt_hub_le_periodic_adv_stop(esp_bt_hub_le_ext_adv_handle_t adv)
{
    resource_slot_t *slot;
    size_t index;
    esp_err_t err = ext_adv_operation_lock(adv, &slot, &index);

    if (err != ESP_OK) {
        return err;
    }
    control_lock();
    if (!slot->periodic_adv_started) {
        control_unlock();
        ext_adv_operation_unlock(index);
        ESP_LOGE(TAG, "Stop periodic advertising failed: instance is not active");
        return ESP_ERR_INVALID_STATE;
    }
    control_unlock();
    err = esp_bt_hub_backend_get()->le_periodic_adv_stop((uint8_t)index);
    if (err == ESP_OK) {
        control_lock();
        slot->periodic_adv_started = false;
        control_unlock();
    }
    ext_adv_operation_unlock(index);
    return err;
}
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
