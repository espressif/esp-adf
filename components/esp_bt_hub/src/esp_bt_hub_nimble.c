/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "host/ble_gap.h"
#include "os/os_mbuf.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#include "esp_bt_hub_priv.h"

#define ESP_BT_HUB_NIMBLE_STOP_MS     5000
#define ESP_BT_HUB_NIMBLE_SYNC_MS     5000
#define ESP_BT_HUB_NIMBLE_PAIR_SLOTS  4
#define ESP_BT_HUB_EXT_SCAN_UNIT_MS   10
#define ESP_BT_HUB_EXT_SCAN_MAX_MS    ((uint32_t)UINT16_MAX * ESP_BT_HUB_EXT_SCAN_UNIT_MS)

#if defined(CONFIG_BT_NIMBLE_EXT_ADV) && CONFIG_BT_NIMBLE_EXT_ADV && \
    !defined(MYNEWT_VAL_BLE_HS_EXT_ADV_LEGACY_INSTANCE)
#define ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT  1
#else
#define ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT  0
#endif

#if ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT
#define ESP_BT_HUB_NIMBLE_ADV_CHANNEL_MAP_DEFAULT         0x07
#define ESP_BT_HUB_NIMBLE_ADV_TX_POWER_NO_PREFERENCE      127
#endif

/* Numeric values of esp_ble_adv_type_t. This backend does not include the Bluedroid GAP header. */
#define HOST_ADV_TYPE_IND              0x00
#define HOST_ADV_TYPE_DIRECT_IND_HIGH  0x01
#define HOST_ADV_TYPE_SCAN_IND         0x02
#define HOST_ADV_TYPE_NONCONN_IND      0x03
#define HOST_ADV_TYPE_DIRECT_IND_LOW   0x04

static const char *TAG = "ESP_BT_HUB_NIMBLE";

#define NIMBLE_RESOURCE_CONTROLLER_INIT     (1U << 0)
#define NIMBLE_RESOURCE_CONTROLLER_ENABLED  (1U << 1)
#define NIMBLE_RESOURCE_STACK_INIT          (1U << 2)
#define NIMBLE_RESOURCE_LISTENER            (1U << 3)
#define NIMBLE_RESOURCE_HOST_TASK           (1U << 4)

/**
 * @brief  Pending NimBLE pairing prompt for one connection
 */
struct nimble_pair_slot {
    bool      in_use;       /*!< Slot holds a prompt */
    uint8_t   peer[6];      /*!< Peer identity address */
    uint8_t   peer_ota[6];  /*!< Peer over-the-air address */
    uint16_t  conn_handle;  /*!< ACL connection handle */
    uint8_t   action;       /*!< Pending `BLE_SM_IOACT_*` action */
    uint32_t  generation;   /*!< Prompt generation */
};

typedef enum {
    NIMBLE_SCHEDULER_RESTART_NOT_NEEDED = 0,
    NIMBLE_SCHEDULER_STOP_PENDING,
    NIMBLE_SCHEDULER_RESTART_REQUIRED,
} nimble_scheduler_state_t;

/**
 * @brief  NimBLE backend runtime context
 */
typedef struct {
    SemaphoreHandle_t              stop_sem;                                  /*!< Host task completion semaphore */
    SemaphoreHandle_t              sync_sem;                                  /*!< Host-controller synchronization semaphore */
    nimble_scheduler_state_t       scheduler_state;                           /*!< NimBLE scheduler restart state */
    uint32_t                       lifecycle_resources;                       /*!< Acquired lifecycle resources */
    struct ble_gap_event_listener  listener;                                  /*!< GAP event listener */
    uint8_t                        address[6];                                /*!< Local public address */
    bool                           address_ready;                             /*!< Local address is available */
    portMUX_TYPE                   state_lock;                                /*!< Protects backend runtime state */
    uint32_t                       pair_generation;                           /*!< Latest pairing prompt generation */
    struct nimble_pair_slot        pair_slots[ESP_BT_HUB_NIMBLE_PAIR_SLOTS];  /*!< Pending pairing prompts */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    uint32_t  ext_adv_generation[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES];  /*!< Advertising generations */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
} esp_bt_hub_nimble_ctx_t;

static esp_bt_hub_nimble_ctx_t *s_ctx;

#define s_stop_sem             s_ctx->stop_sem
#define s_sync_sem             s_ctx->sync_sem
#define s_scheduler_state      s_ctx->scheduler_state
#define s_lifecycle_resources  s_ctx->lifecycle_resources
#define s_listener             s_ctx->listener
#define s_address              s_ctx->address
#define s_address_ready        s_ctx->address_ready
#define s_state_lock           s_ctx->state_lock
#define s_pair_generation      s_ctx->pair_generation
#define s_pair_slots           s_ctx->pair_slots
#define s_ext_adv_generation   s_ctx->ext_adv_generation

#if ESP_BT_HUB_LE_EXT_SUPPORTED

static void ext_adv_generation_set(uint8_t instance, uint32_t generation)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_ext_adv_generation[instance] = generation;
    taskEXIT_CRITICAL(&s_state_lock);
}

static uint32_t ext_adv_generation_take(uint8_t instance)
{
    uint32_t generation;

    taskENTER_CRITICAL(&s_state_lock);
    generation = s_ext_adv_generation[instance];
    s_ext_adv_generation[instance] = 0;
    taskEXIT_CRITICAL(&s_state_lock);
    return generation;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

/* Provided by the NimBLE port. */
void ble_store_config_init(void);

static esp_err_t nimble_backend_deinit(void);
static esp_err_t nimble_backend_stop(void);

static void local_address_clear(void)
{
    if (!s_ctx) {
        return;
    }
    taskENTER_CRITICAL(&s_state_lock);
    memset(s_address, 0, sizeof(s_address));
    s_address_ready = false;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void local_address_set(const uint8_t address[6])
{
    taskENTER_CRITICAL(&s_state_lock);
    memcpy(s_address, address, sizeof(s_address));
    s_address_ready = true;
    taskEXIT_CRITICAL(&s_state_lock);
}

static void host_task(void *param)
{
    nimble_port_run();
    if (s_stop_sem) {
        xSemaphoreGive(s_stop_sem);
    }
    /* nimble_port_freertos_deinit() deletes this task; deinit relies on that. */
    nimble_port_freertos_deinit();
}

static void on_reset(int reason)
{
    local_address_clear();
    ESP_LOGE(TAG, "NimBLE reset: reason %d", reason);
}

static void on_sync(void)
{
    uint8_t address[6];

    if (ble_hs_util_ensure_addr(0) != 0) {
        ESP_LOGE(TAG, "NimBLE sync failed: ensure address failed");
        return;
    }
    if (ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, address, NULL) == 0) {
        local_address_set(address);
        if (s_sync_sem) {
            xSemaphoreGive(s_sync_sem);
        }
    }
}

static void pair_slot_clear_handle(uint16_t conn_handle)
{
    int i;

    taskENTER_CRITICAL(&s_state_lock);
    for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
        if (s_pair_slots[i].in_use && s_pair_slots[i].conn_handle == conn_handle) {
            memset(&s_pair_slots[i], 0, sizeof(s_pair_slots[i]));
        }
    }
    taskEXIT_CRITICAL(&s_state_lock);
}

static void pair_slot_clear_all(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    memset(s_pair_slots, 0, sizeof(s_pair_slots));
    taskEXIT_CRITICAL(&s_state_lock);
}

static void pair_slot_remember(uint16_t conn_handle, uint8_t action)
{
    struct ble_gap_conn_desc desc;
    struct nimble_pair_slot *slot = NULL;
    int i;

    if (ble_gap_conn_find(conn_handle, &desc) != 0) {
        ESP_LOGW(TAG, "Remember LE pairing failed: unknown connection %u", conn_handle);
        return;
    }
    taskENTER_CRITICAL(&s_state_lock);
    for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
        if (s_pair_slots[i].in_use && s_pair_slots[i].conn_handle == conn_handle) {
            slot = &s_pair_slots[i];
            break;
        }
    }
    if (!slot) {
        for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
            if (!s_pair_slots[i].in_use) {
                slot = &s_pair_slots[i];
                break;
            }
        }
    }
    if (!slot) {
        taskEXIT_CRITICAL(&s_state_lock);
        ESP_LOGW(TAG, "Remember LE pairing failed: pairing table is full");
        return;
    }
    s_pair_generation = esp_bt_hub_generation_next(s_pair_generation);
    slot->in_use = true;
    slot->conn_handle = conn_handle;
    slot->action = action;
    slot->generation = s_pair_generation;
    memcpy(slot->peer, desc.peer_id_addr.val, sizeof(slot->peer));
    memcpy(slot->peer_ota, desc.peer_ota_addr.val, sizeof(slot->peer_ota));
    taskEXIT_CRITICAL(&s_state_lock);
}

static esp_err_t pair_slot_lookup(const uint8_t peer[6], struct nimble_pair_slot *out_slot)
{
    ble_addr_t addr;
    struct ble_gap_conn_desc desc;
    int i;

    if (!peer || !out_slot) {
        return ESP_ERR_INVALID_ARG;
    }
    taskENTER_CRITICAL(&s_state_lock);
    for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
        if (!s_pair_slots[i].in_use) {
            continue;
        }
        if (memcmp(s_pair_slots[i].peer, peer, 6) == 0 || memcmp(s_pair_slots[i].peer_ota, peer, 6) == 0) {
            *out_slot = s_pair_slots[i];
            taskEXIT_CRITICAL(&s_state_lock);
            return ESP_OK;
        }
    }
    taskEXIT_CRITICAL(&s_state_lock);

    memset(&addr, 0, sizeof(addr));
    memcpy(addr.val, peer, 6);
    addr.type = BLE_ADDR_PUBLIC;
    if (ble_gap_conn_find_by_addr(&addr, &desc) != 0) {
        addr.type = BLE_ADDR_RANDOM;
        if (ble_gap_conn_find_by_addr(&addr, &desc) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    taskENTER_CRITICAL(&s_state_lock);
    for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
        if (s_pair_slots[i].in_use && s_pair_slots[i].conn_handle == desc.conn_handle) {
            *out_slot = s_pair_slots[i];
            taskEXIT_CRITICAL(&s_state_lock);
            return ESP_OK;
        }
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_ERR_INVALID_STATE;
}

static void pair_slot_clear_snapshot(const struct nimble_pair_slot *snapshot)
{
    int i;

    taskENTER_CRITICAL(&s_state_lock);
    for (i = 0; i < ESP_BT_HUB_NIMBLE_PAIR_SLOTS; i++) {
        if (s_pair_slots[i].in_use &&
            s_pair_slots[i].generation == snapshot->generation) {
            memset(&s_pair_slots[i], 0, sizeof(s_pair_slots[i]));
            break;
        }
    }
    taskEXIT_CRITICAL(&s_state_lock);
}

static int gap_listener(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        pair_slot_clear_handle(event->disconnect.conn.conn_handle);
    } else if (event->type == BLE_GAP_EVENT_PASSKEY_ACTION) {
        pair_slot_remember(event->passkey.conn_handle, event->passkey.params.action);
        if (esp_bt_hub_pairing_auto_reply_enabled()) {
            uint8_t action = event->passkey.params.action;

            if (action == BLE_SM_IOACT_INPUT || action == BLE_SM_IOACT_DISP) {
                int rc;

                ESP_LOGW(TAG, "Auto-accept pairing: LE passkey entry is rejected");
                rc = ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_AUTH_FAIL);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Reject LE passkey failed: terminate %d", rc);
                }
            } else {
                struct ble_sm_io io = {0};

                io.action = action;
                if (io.action == BLE_SM_IOACT_NUMCMP) {
                    io.numcmp_accept = 1;
                }
                ble_sm_inject_io(event->passkey.conn_handle, &io);
            }
            pair_slot_clear_handle(event->passkey.conn_handle);
        }
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        esp_bt_hub_le_ext_scan_complete();
    }
    esp_bt_hub_gap_dispatch(ESP_BT_HUB_GAP_DOMAIN_BLE, (uint32_t)event->type, event);
    return 0;
}

static int gap_conn_cb(struct ble_gap_event *event, void *arg)
{
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE && arg) {
        uint8_t  configured_instance = (uint8_t)((uintptr_t)arg - 1U);
        uint8_t  instance = event->adv_complete.instance;

        if (instance == configured_instance && instance < ESP_BT_HUB_MAX_EXT_ADV_INSTANCES) {
            uint32_t  generation = ext_adv_generation_take(instance);

            esp_bt_hub_le_ext_adv_complete(instance, generation);
        }
        return 0;
    }
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    /* NimBLE delivers passkey actions only to the connection callback. */
    if (event->type != BLE_GAP_EVENT_PASSKEY_ACTION) {
        return 0;
    }
    return gap_listener(event, arg);
}

static esp_err_t nimble_backend_init(const esp_bt_hub_config_t *config)
{
    esp_err_t err;
    int rc;

    if (s_ctx) {
        ESP_LOGE(TAG, "Init 'nimble' failed: backend context already exists");
        return ESP_ERR_INVALID_STATE;
    }
    s_ctx = heap_caps_calloc_prefer(1, sizeof(*s_ctx), 2,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!s_ctx) {
        ESP_LOGE(TAG, "Init 'nimble' failed: no memory for backend context");
        return ESP_ERR_NO_MEM;
    }
    s_state_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    local_address_clear();
    s_scheduler_state = NIMBLE_SCHEDULER_RESTART_NOT_NEEDED;
    s_lifecycle_resources = 0;
    err = esp_bt_controller_init((esp_bt_controller_config_t *)&config->controller_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'nimble' failed: controller init %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= NIMBLE_RESOURCE_CONTROLLER_INIT;
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'nimble' failed: controller enable %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= NIMBLE_RESOURCE_CONTROLLER_ENABLED;

    err = esp_nimble_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'nimble' failed: esp_nimble_init %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= NIMBLE_RESOURCE_STACK_INIT;

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = (uint8_t)config->io_capability;
    ble_hs_cfg.sm_bonding = config->bonding ? 1 : 0;
    ble_hs_cfg.sm_mitm = config->mitm ? 1 : 0;
    ble_hs_cfg.sm_sc = config->secure_connections ? 1 : 0;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    rc = ble_svc_gap_device_name_set(config->device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "Init 'nimble' failed: set device name %d", rc);
        err = ESP_FAIL;
        goto fail;
    }

    ble_store_config_init();
    ESP_LOGI(TAG, "Init 'nimble': success");
    return ESP_OK;

fail:
    esp_err_t rollback_err = nimble_backend_deinit();

    if (rollback_err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'nimble' rollback failed: %s", esp_err_to_name(rollback_err));
        return rollback_err;
    }
    return err;
}

static esp_err_t nimble_backend_start(void)
{
    esp_err_t err;
    int rc;

    if (s_lifecycle_resources & (NIMBLE_RESOURCE_LISTENER | NIMBLE_RESOURCE_HOST_TASK)) {
        ESP_LOGE(TAG, "Start 'nimble' failed: runtime resources already exist");
        return ESP_ERR_INVALID_STATE;
    }
    local_address_clear();
    s_stop_sem = xSemaphoreCreateBinary();
    if (!s_stop_sem) {
        ESP_LOGE(TAG, "Start 'nimble' failed: no memory for stop semaphore");
        return ESP_ERR_NO_MEM;
    }
    s_sync_sem = xSemaphoreCreateBinary();
    if (!s_sync_sem) {
        ESP_LOGE(TAG, "Start 'nimble' failed: no memory for sync semaphore");
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    rc = ble_gap_event_listener_register(&s_listener, gap_listener, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start 'nimble' failed: register GAP listener %d", rc);
        err = ESP_FAIL;
        goto fail;
    }
    s_lifecycle_resources |= NIMBLE_RESOURCE_LISTENER;

    pair_slot_clear_all();
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 2, 0)
    err = nimble_port_freertos_init(host_task);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start 'nimble' failed: host task create %s", esp_err_to_name(err));
        goto fail;
    }
#else
    nimble_port_freertos_init(host_task);
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 2, 0) */
    if (s_scheduler_state == NIMBLE_SCHEDULER_RESTART_REQUIRED) {
        ble_hs_sched_start();
        s_scheduler_state = NIMBLE_SCHEDULER_RESTART_NOT_NEEDED;
    }
    s_lifecycle_resources |= NIMBLE_RESOURCE_HOST_TASK;
    if (xSemaphoreTake(s_sync_sem, pdMS_TO_TICKS(ESP_BT_HUB_NIMBLE_SYNC_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Start 'nimble' failed: host-controller sync timed out");
        err = nimble_backend_stop();
        return err == ESP_OK ? ESP_ERR_TIMEOUT : err;
    }
    vSemaphoreDelete(s_sync_sem);
    s_sync_sem = NULL;
    ESP_LOGI(TAG, "Start 'nimble': success");
    return ESP_OK;

fail:
    if (s_lifecycle_resources & NIMBLE_RESOURCE_LISTENER) {
        ble_gap_event_listener_unregister(&s_listener);
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_LISTENER;
    }
    if (s_stop_sem) {
        vSemaphoreDelete(s_stop_sem);
        s_stop_sem = NULL;
    }
    if (s_sync_sem) {
        vSemaphoreDelete(s_sync_sem);
        s_sync_sem = NULL;
    }
    return err;
}

static esp_err_t nimble_backend_stop(void)
{
    int rc;

    local_address_clear();
    if (s_lifecycle_resources & NIMBLE_RESOURCE_HOST_TASK) {
        if (s_scheduler_state != NIMBLE_SCHEDULER_STOP_PENDING) {
            rc = nimble_port_stop();
            if (rc == BLE_HS_EALREADY) {
                ESP_LOGE(TAG, "Stop 'nimble' failed: host is not enabled yet");
                return ESP_ERR_INVALID_STATE;
            }
            if (rc != 0) {
                ESP_LOGE(TAG, "Stop 'nimble' failed: port stop %d", rc);
                return ESP_FAIL;
            }
            s_scheduler_state = NIMBLE_SCHEDULER_STOP_PENDING;
        }
        if (!s_stop_sem || xSemaphoreTake(s_stop_sem, pdMS_TO_TICKS(ESP_BT_HUB_NIMBLE_STOP_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "Stop 'nimble' failed: host task stop timed out");
            return ESP_ERR_TIMEOUT;
        }
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_HOST_TASK;
        s_scheduler_state = NIMBLE_SCHEDULER_RESTART_REQUIRED;
    }
    if (s_stop_sem) {
        vSemaphoreDelete(s_stop_sem);
        s_stop_sem = NULL;
    }
    if (s_sync_sem) {
        vSemaphoreDelete(s_sync_sem);
        s_sync_sem = NULL;
    }
    if (s_lifecycle_resources & NIMBLE_RESOURCE_LISTENER) {
        ble_gap_event_listener_unregister(&s_listener);
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_LISTENER;
    }
    pair_slot_clear_all();
    ESP_LOGI(TAG, "Stop 'nimble': success");
    return ESP_OK;
}

static esp_err_t nimble_backend_deinit(void)
{
    esp_bt_hub_nimble_ctx_t *ctx;
    esp_err_t err;

    if (!s_ctx) {
        return ESP_OK;
    }
    if (s_lifecycle_resources & NIMBLE_RESOURCE_STACK_INIT) {
        err = esp_nimble_deinit();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'nimble' failed: stack deinit %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_STACK_INIT;
    }
    if (s_lifecycle_resources & NIMBLE_RESOURCE_CONTROLLER_ENABLED) {
        err = esp_bt_controller_disable();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'nimble' failed: controller disable %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_CONTROLLER_ENABLED;
    }
    if (s_lifecycle_resources & NIMBLE_RESOURCE_CONTROLLER_INIT) {
        err = esp_bt_controller_deinit();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'nimble' failed: controller deinit %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~NIMBLE_RESOURCE_CONTROLLER_INIT;
    }
    local_address_clear();
    pair_slot_clear_all();
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    taskENTER_CRITICAL(&s_state_lock);
    memset(s_ext_adv_generation, 0, sizeof(s_ext_adv_generation));
    taskEXIT_CRITICAL(&s_state_lock);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    ctx = s_ctx;
    s_ctx = NULL;
    heap_caps_free(ctx);
    ESP_LOGI(TAG, "Deinit 'nimble': success");
    return ESP_OK;
}

static bool nimble_teardown_pending(void)
{
    return s_ctx && s_lifecycle_resources != 0;
}

static esp_err_t nimble_set_device_name(const char *name)
{
    int rc = ble_svc_gap_device_name_set(name);

    if (rc != 0) {
        ESP_LOGE(TAG, "Set device name failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t nimble_get_address(uint8_t address[6])
{
    bool address_ready;

    taskENTER_CRITICAL(&s_state_lock);
    address_ready = s_address_ready;
    if (address_ready) {
        memcpy(address, s_address, sizeof(s_address));
    }
    taskEXIT_CRITICAL(&s_state_lock);
    if (!address_ready) {
        ESP_LOGE(TAG, "Get local address failed: address is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t nimble_le_scan_start(const esp_bt_hub_le_scan_params_t *params)
{
    struct ble_gap_disc_params disc = {0};
    int32_t duration = params->duration_ms == 0 ? BLE_HS_FOREVER : (int32_t)params->duration_ms;
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);

    if (rc != 0) {
        ESP_LOGE(TAG, "Start LE scan failed: infer address %d", rc);
        return ESP_FAIL;
    }
    disc.passive = params->active ? 0 : 1;
    disc.itvl = params->interval;
    disc.window = params->window;
    disc.filter_duplicates = 1;
    rc = ble_gap_disc(own_addr_type, duration, &disc, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start LE scan failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t nimble_le_scan_stop(void)
{
    int rc = ble_gap_disc_cancel();

    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Stop LE scan failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t nimble_le_adv_start(const esp_bt_hub_le_adv_params_t *params)
{
#if ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT
    struct ble_gap_ext_adv_params adv = {0};
    struct os_mbuf *mbuf;
    uint8_t own_addr_type;
    int rc;

    switch (params->adv_type) {
        case HOST_ADV_TYPE_IND:
            adv.connectable = 1;
            adv.scannable = 1;
            break;
        case HOST_ADV_TYPE_DIRECT_IND_HIGH:
        case HOST_ADV_TYPE_DIRECT_IND_LOW:
            ESP_LOGE(TAG, "Start legacy advertising failed: directed advertising needs a peer address");
            return ESP_ERR_INVALID_ARG;
        case HOST_ADV_TYPE_SCAN_IND:
            adv.scannable = 1;
            break;
        case HOST_ADV_TYPE_NONCONN_IND:
            break;
        default:
            ESP_LOGE(TAG, "Start legacy advertising failed: unknown advertising type %u", params->adv_type);
            return ESP_ERR_INVALID_ARG;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: infer address %d", rc);
        return ESP_FAIL;
    }
    adv.legacy_pdu = 1;
    adv.own_addr_type = own_addr_type;
    adv.primary_phy = BLE_HCI_LE_PHY_1M;
    adv.secondary_phy = BLE_HCI_LE_PHY_1M;
    adv.tx_power = ESP_BT_HUB_NIMBLE_ADV_TX_POWER_NO_PREFERENCE;
    adv.sid = 0;
    adv.itvl_min = params->interval_min;
    adv.itvl_max = params->interval_max;
    adv.channel_map = params->channel_map ? params->channel_map : ESP_BT_HUB_NIMBLE_ADV_CHANNEL_MAP_DEFAULT;
    rc = ble_gap_ext_adv_configure(ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE, &adv, NULL, gap_conn_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: configure %d", rc);
        return ESP_FAIL;
    }

    mbuf = os_msys_get_pkthdr(params->data_len, 0);
    if (!mbuf) {
        ESP_LOGE(TAG, "Start legacy advertising failed: set data %d", BLE_HS_ENOMEM);
        return ESP_FAIL;
    }
    if (params->data_len != 0) {
        rc = os_mbuf_append(mbuf, params->data, params->data_len);
        if (rc != 0) {
            os_mbuf_free_chain(mbuf);
            ESP_LOGE(TAG, "Start legacy advertising failed: set data %d", BLE_HS_ENOMEM);
            return ESP_FAIL;
        }
    }
    rc = ble_gap_ext_adv_set_data(ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE, mbuf);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: set data %d", rc);
        return ESP_FAIL;
    }
    rc = ble_gap_ext_adv_start(ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE, 0, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    struct ble_gap_adv_params adv = {0};
    uint8_t own_addr_type;
    int rc;

    switch (params->adv_type) {
        case HOST_ADV_TYPE_IND:
            adv.conn_mode = BLE_GAP_CONN_MODE_UND;
            adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
            break;
        case HOST_ADV_TYPE_DIRECT_IND_HIGH:
        case HOST_ADV_TYPE_DIRECT_IND_LOW:
            ESP_LOGE(TAG, "Start legacy advertising failed: directed advertising needs a peer address");
            return ESP_ERR_INVALID_ARG;
        case HOST_ADV_TYPE_SCAN_IND:
            adv.conn_mode = BLE_GAP_CONN_MODE_NON;
            adv.disc_mode = BLE_GAP_DISC_MODE_GEN;
            break;
        case HOST_ADV_TYPE_NONCONN_IND:
            adv.conn_mode = BLE_GAP_CONN_MODE_NON;
            adv.disc_mode = BLE_GAP_DISC_MODE_NON;
            break;
        default:
            ESP_LOGE(TAG, "Start legacy advertising failed: unknown advertising type %u", params->adv_type);
            return ESP_ERR_INVALID_ARG;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: infer address %d", rc);
        return ESP_FAIL;
    }
    adv.itvl_min = params->interval_min;
    adv.itvl_max = params->interval_max;
    adv.channel_map = params->channel_map ? params->channel_map : 0x07;
    rc = ble_gap_adv_set_data(params->data, params->data_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: set data %d", rc);
        return ESP_FAIL;
    }
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv, gap_conn_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Start legacy advertising failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
#endif  /* ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT */
}

static esp_err_t nimble_le_adv_stop(void)
{
#if ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT
    int rc = ble_gap_ext_adv_stop(ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_INSTANCE);
#else
    int rc = ble_gap_adv_stop();
#endif  /* ESP_BT_HUB_NIMBLE_LEGACY_EXT_ADV_COMPAT */

    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Stop legacy advertising failed: %d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static size_t le_bond_count(void)
{
    const int max_bonds = MYNEWT_VAL(BLE_STORE_MAX_BONDS);
    ble_addr_t *peers = NULL;
    int count = 0;

    if (!esp_bt_hub_is_running() || max_bonds <= 0) {
        return 0;
    }
    peers = heap_caps_calloc(max_bonds, sizeof(*peers), MALLOC_CAP_DEFAULT);
    if (!peers) {
        return 0;
    }
    if (ble_store_util_bonded_peers(peers, &count, max_bonds) != 0) {
        count = 0;
    }
    heap_caps_free(peers);
    return count > 0 ? (size_t)count : 0;
}

static bool le_bond_exists(uint8_t addr_type, const uint8_t addr[6])
{
    const int max_bonds = MYNEWT_VAL(BLE_STORE_MAX_BONDS);
    ble_addr_t *peers = NULL;
    bool found = false;
    int count = 0;
    uint8_t rpa[6];
    uint8_t identity[6];
    uint8_t identity_type = 0;
    uint8_t match_type;
    const uint8_t *match_addr;

    if (!addr || !esp_bt_hub_is_running() || max_bonds <= 0) {
        return false;
    }
    match_addr = addr;
    match_type = addr_type;
    memcpy(rpa, addr, sizeof(rpa));
    if ((addr_type == BLE_ADDR_RANDOM ||
         addr_type == BLE_ADDR_PUBLIC_ID ||
         addr_type == BLE_ADDR_RANDOM_ID) &&
        ble_gap_rpa_resolve(rpa, identity, &identity_type)) {
        match_addr = identity;
        match_type = identity_type;
    }
    peers = heap_caps_calloc(max_bonds, sizeof(*peers), MALLOC_CAP_DEFAULT);
    if (!peers) {
        return false;
    }
    if (ble_store_util_bonded_peers(peers, &count, max_bonds) == 0) {
        for (int i = 0; i < count; i++) {
            if (memcmp(peers[i].val, match_addr, 6) == 0 &&
                peers[i].type == match_type) {
                found = true;
                break;
            }
        }
    }
    heap_caps_free(peers);
    return found;
}

static esp_err_t nimble_rc(int rc, const char *what)
{
    if (rc != 0) {
        ESP_LOGE(TAG, "%s failed: %d", what, rc);
        return ESP_FAIL;
    }
    return ESP_OK;
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static esp_err_t nimble_le_ext_adv_configure(uint8_t instance, const esp_bt_hub_le_ext_adv_params_t *params)
{
    struct ble_gap_ext_adv_params np = {0};

    if (!params) {
        ESP_LOGE(TAG, "Set ext adv params failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext adv params failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }

    np.connectable = params->connectable ? 1 : 0;
    np.scannable = params->scannable ? 1 : 0;
    np.legacy_pdu = params->legacy_pdu ? 1 : 0;
    np.own_addr_type = params->own_addr_type;
    np.primary_phy = params->primary_phy;
    np.secondary_phy = params->secondary_phy;
    np.tx_power = params->tx_power;
    np.sid = params->sid;
    np.itvl_min = params->itvl_min;
    np.itvl_max = params->itvl_max;
    return nimble_rc(ble_gap_ext_adv_configure(instance, &np, NULL, gap_conn_cb,
                                               (void *)(uintptr_t)(instance + 1U)),
                     "Configure ext adv");
}

static esp_err_t nimble_le_ext_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    struct os_mbuf *mbuf;
    int rc;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set ext adv data failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext adv data failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    mbuf = os_msys_get_pkthdr(len, 0);
    if (!mbuf) {
        ESP_LOGE(TAG, "Set ext adv data failed: no memory for advertising buffer");
        return ESP_ERR_NO_MEM;
    }
    if (len != 0) {
        rc = os_mbuf_append(mbuf, data, len);
        if (rc != 0) {
            os_mbuf_free_chain(mbuf);
            return nimble_rc(rc, "Append ext adv data");
        }
    }
    return nimble_rc(ble_gap_ext_adv_set_data(instance, mbuf), "Set ext adv data");
}

static esp_err_t nimble_le_ext_adv_set_scan_rsp(uint8_t instance, const uint8_t *data, size_t len)
{
    struct os_mbuf *mbuf;
    int rc;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set ext scan response failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext scan response failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    mbuf = os_msys_get_pkthdr(len, 0);
    if (!mbuf) {
        ESP_LOGE(TAG, "Set ext scan response failed: no memory for advertising buffer");
        return ESP_ERR_NO_MEM;
    }
    if (len != 0) {
        rc = os_mbuf_append(mbuf, data, len);
        if (rc != 0) {
            os_mbuf_free_chain(mbuf);
            return nimble_rc(rc, "Append ext scan response");
        }
    }
    return nimble_rc(ble_gap_ext_adv_rsp_set_data(instance, mbuf), "Set ext scan response");
}

static esp_err_t nimble_le_ext_adv_remove(uint8_t instance)
{
    int rc;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Remove ext adv set failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    rc = ble_gap_ext_adv_remove(instance);
    if (rc == BLE_HS_EALREADY) {
        return ESP_OK;
    }
    return nimble_rc(rc, "Remove ext adv set");
}

static esp_err_t nimble_le_ext_adv_start(uint8_t instance, uint32_t generation, int duration, int max_events)
{
    int rc;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Start ext adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    ext_adv_generation_set(instance, generation);
    rc = ble_gap_ext_adv_start(instance, duration, max_events);
    if (rc != 0) {
        ext_adv_generation_take(instance);
    }
    return nimble_rc(rc, "Start ext adv");
}

static esp_err_t nimble_le_ext_adv_stop(uint8_t instance)
{
    int rc;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Stop ext adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    rc = ble_gap_ext_adv_stop(instance);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        return nimble_rc(rc, "Stop ext adv");
    }
    ext_adv_generation_take(instance);
    return ESP_OK;
}

static esp_err_t nimble_le_ext_scan_start(const esp_bt_hub_le_ext_scan_params_t *params)
{
    struct ble_gap_disc_params dp = {0};
    uint8_t phy_mask;
    int32_t duration;

    phy_mask = params->phy_mask ? params->phy_mask : ESP_BT_HUB_LE_PHY_1M_MASK;
    if (params->duration_ms >= INT32_MAX ||
        (phy_mask != ESP_BT_HUB_LE_PHY_1M_MASK && params->duration_ms > ESP_BT_HUB_EXT_SCAN_MAX_MS)) {
        ESP_LOGE(TAG, "Start ext scan failed: duration is too large");
        return ESP_ERR_INVALID_ARG;
    }
    duration = params->duration_ms == 0 ? BLE_HS_FOREVER : (int32_t)params->duration_ms;
    dp.passive = params->passive ? 1 : 0;
    dp.filter_duplicates = params->filter_duplicates ? 1 : 0;
    dp.itvl = params->interval;
    dp.window = params->window;
    if (phy_mask != ESP_BT_HUB_LE_PHY_1M_MASK) {
        struct ble_gap_ext_disc_params phy_params = {
            .itvl = params->interval,
            .window = params->window,
            .passive = params->passive ? 1 : 0,
        };
        uint16_t ext_duration = params->duration_ms == 0 ? 0 : (uint16_t)((params->duration_ms + ESP_BT_HUB_EXT_SCAN_UNIT_MS - 1U) / ESP_BT_HUB_EXT_SCAN_UNIT_MS);

        return nimble_rc(ble_gap_ext_disc(
                             params->own_addr_type, ext_duration, 0,
                             params->filter_duplicates ? 1 : 0, BLE_HCI_SCAN_FILT_NO_WL, 0,
                             (phy_mask & ESP_BT_HUB_LE_PHY_1M_MASK) ? &phy_params : NULL,
                             (phy_mask & ESP_BT_HUB_LE_PHY_CODED_MASK) ? &phy_params : NULL,
                             NULL, NULL),
                         "Start ext scan");
    }
    return nimble_rc(ble_gap_disc(params->own_addr_type, duration, &dp, NULL, NULL),
                     "Start ext scan");
}

static esp_err_t nimble_le_ext_scan_stop(void)
{
    int rc = ble_gap_disc_cancel();

    if (rc != 0 && rc != BLE_HS_EALREADY) {
        return nimble_rc(rc, "Stop ext scan");
    }
    return ESP_OK;
}

static esp_err_t le_connect(const esp_bt_hub_le_connect_params_t *params)
{
    ble_addr_t addr = {0};
    struct ble_gap_conn_params cp = {0};
    uint8_t phy_mask;

    if (!params || !params->conn_params ||
        (params->phy_mask & ~(ESP_BT_HUB_LE_PHY_1M_MASK |
                              ESP_BT_HUB_LE_PHY_2M_MASK |
                              ESP_BT_HUB_LE_PHY_CODED_MASK))) {
        ESP_LOGE(TAG, "Connect failed: params, connection parameters, or PHY mask is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Connect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    addr.type = params->peer_addr_type;
    memcpy(addr.val, params->peer, sizeof(addr.val));
    cp.scan_itvl = params->conn_params->scan_itvl;
    cp.scan_window = params->conn_params->scan_window;
    cp.itvl_min = params->conn_params->itvl_min;
    cp.itvl_max = params->conn_params->itvl_max;
    cp.latency = params->conn_params->latency;
    cp.supervision_timeout = params->conn_params->supervision_timeout;
    cp.min_ce_len = params->conn_params->min_ce_len;
    cp.max_ce_len = params->conn_params->max_ce_len;
    phy_mask = params->phy_mask ? params->phy_mask : ESP_BT_HUB_LE_PHY_1M_MASK;
    if (phy_mask != ESP_BT_HUB_LE_PHY_1M_MASK) {
        return nimble_rc(ble_gap_ext_connect(
                             params->own_addr_type, &addr, (int32_t)params->timeout_ms, phy_mask,
                             (phy_mask & ESP_BT_HUB_LE_PHY_1M_MASK) ? &cp : NULL,
                             (phy_mask & ESP_BT_HUB_LE_PHY_2M_MASK) ? &cp : NULL,
                             (phy_mask & ESP_BT_HUB_LE_PHY_CODED_MASK) ? &cp : NULL,
                             gap_conn_cb, NULL),
                         "Connect");
    }
    return nimble_rc(ble_gap_connect(params->own_addr_type, &addr, (int32_t)params->timeout_ms,
                                     &cp, gap_conn_cb, NULL),
                     "Connect");
}

static esp_err_t le_connect_cancel(void)
{
    int rc;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Cancel connect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    rc = ble_gap_conn_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        return nimble_rc(rc, "Cancel connect");
    }
    return ESP_OK;
}

#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

static esp_err_t le_disconnect(uint16_t conn_handle, uint8_t reason)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Disconnect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_gap_terminate(conn_handle, reason), "Disconnect");
}

static esp_err_t le_security_initiate(uint16_t conn_handle)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Security initiate failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_gap_security_initiate(conn_handle), "Security initiate");
}

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
static esp_err_t nimble_le_periodic_adv_configure(uint8_t instance,
                                                  const esp_bt_hub_le_periodic_adv_params_t *params)
{
    struct ble_gap_periodic_adv_params pp = {0};

    if (!params) {
        ESP_LOGE(TAG, "Set periodic adv params failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set periodic adv params failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    pp.include_tx_power = params->include_tx_power ? 1 : 0;
    pp.itvl_min = params->itvl_min;
    pp.itvl_max = params->itvl_max;
    return nimble_rc(ble_gap_periodic_adv_configure(instance, &pp), "Configure periodic adv");
}

static esp_err_t nimble_le_periodic_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    struct os_mbuf *mbuf;
    int rc;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set periodic adv data failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set periodic adv data failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    mbuf = os_msys_get_pkthdr(len, 0);
    if (!mbuf) {
        ESP_LOGE(TAG, "Set periodic adv data failed: no memory for advertising buffer");
        return ESP_ERR_NO_MEM;
    }
    if (len != 0) {
        rc = os_mbuf_append(mbuf, data, len);
        if (rc != 0) {
            os_mbuf_free_chain(mbuf);
            return nimble_rc(rc, "Append periodic adv data");
        }
    }
#if MYNEWT_VAL(BLE_PERIODIC_ADV_ENH)
    return nimble_rc(ble_gap_periodic_adv_set_data(instance, mbuf, NULL), "Set periodic adv data");
#else
    return nimble_rc(ble_gap_periodic_adv_set_data(instance, mbuf), "Set periodic adv data");
#endif  /* MYNEWT_VAL(BLE_PERIODIC_ADV_ENH) */
}

static esp_err_t nimble_le_periodic_adv_start(uint8_t instance)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Start periodic adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
#if MYNEWT_VAL(BLE_PERIODIC_ADV_ENH)
    return nimble_rc(ble_gap_periodic_adv_start(instance, NULL), "Start periodic adv");
#else
    return nimble_rc(ble_gap_periodic_adv_start(instance), "Start periodic adv");
#endif  /* MYNEWT_VAL(BLE_PERIODIC_ADV_ENH) */
}

static esp_err_t nimble_le_periodic_adv_stop(uint8_t instance)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Stop periodic adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_gap_periodic_adv_stop(instance), "Stop periodic adv");
}

static int gap_periodic_sync_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_PERIODIC_REPORT:
#if MYNEWT_VAL(BLE_PERIODIC_ADV_SYNC_TRANSFER)
        case BLE_GAP_EVENT_PERIODIC_TRANSFER:
#if MYNEWT_VAL(BLE_PERIODIC_ADV_WITH_RESPONSES)
        case BLE_GAP_EVENT_PERIODIC_TRANSFER_V2:
#endif  /* MYNEWT_VAL(BLE_PERIODIC_ADV_WITH_RESPONSES) */
#endif  /* MYNEWT_VAL(BLE_PERIODIC_ADV_SYNC_TRANSFER) */
            esp_bt_hub_gap_dispatch(ESP_BT_HUB_GAP_DOMAIN_BLE, (uint32_t)event->type, event);
            break;
        default:
            break;
    }
    return 0;
}

static esp_err_t le_pa_sync_create(uint8_t addr_type, const uint8_t addr[6], uint8_t sid,
                                   const esp_bt_hub_le_periodic_sync_params_t *params)
{
    ble_addr_t peer = {0};
    struct ble_gap_periodic_sync_params sp = {0};

    if (!addr || !params) {
        ESP_LOGE(TAG, "Create PA sync failed: addr or params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Create PA sync failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    peer.type = addr_type;
    memcpy(peer.val, addr, sizeof(peer.val));
    sp.skip = params->skip;
    sp.sync_timeout = params->sync_timeout;
    return nimble_rc(ble_gap_periodic_adv_sync_create(&peer, sid, &sp, gap_periodic_sync_cb, NULL),
                     "Create PA sync");
}

static esp_err_t le_pa_sync_terminate(uint16_t sync_handle)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Terminate PA sync failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_gap_periodic_adv_sync_terminate(sync_handle), "Terminate PA sync");
}

static esp_err_t le_pa_sync_create_cancel(void)
{
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Cancel PA sync create failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_gap_periodic_adv_sync_create_cancel(), "Cancel PA sync");
}

#if ESP_BT_HUB_LE_PAST_SUPPORTED
static esp_err_t le_pa_sync_receive(uint16_t conn_handle, const esp_bt_hub_le_periodic_sync_params_t *params)
{
    struct ble_gap_periodic_sync_params sp = {0};

    if (!params) {
        ESP_LOGE(TAG, "Receive PA sync failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Receive PA sync failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    sp.skip = params->skip;
    sp.sync_timeout = params->sync_timeout;
    return nimble_rc(ble_gap_periodic_adv_sync_receive(conn_handle, &sp, gap_periodic_sync_cb, NULL),
                     "Receive PA sync");
}

#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */

static esp_err_t le_id_infer_auto(int privacy, uint8_t *own_addr_type)
{
    if (!own_addr_type) {
        ESP_LOGE(TAG, "Infer own address failed: own_addr_type is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Infer own address failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    return nimble_rc(ble_hs_id_infer_auto(privacy, own_addr_type), "Infer own address type");
}

esp_err_t esp_bt_hub_le_id_infer_auto(int privacy, uint8_t *own_addr_type)
{
    esp_err_t err;

    if (!own_addr_type) {
        ESP_LOGE(TAG, "Infer own address failed: own_addr_type is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_runtime_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = le_id_infer_auto(privacy, own_addr_type);
    esp_bt_hub_runtime_leave();
    return err;
}

static esp_err_t le_privacy_enable(void)
{
    return ESP_OK;
}

static esp_err_t inject_io(const struct nimble_pair_slot *slot, struct ble_sm_io *io)
{
    int rc;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE pairing reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    rc = ble_sm_inject_io(slot->conn_handle, io);
    if (rc != 0 &&
        !(io->action == BLE_SM_IOACT_NUMCMP && !io->numcmp_accept &&
          rc == BLE_HS_SM_US_ERR(BLE_SM_ERR_NUMCMP))) {
        ESP_LOGE(TAG, "LE pairing reply failed: %d", rc);
        return ESP_FAIL;
    }
    pair_slot_clear_snapshot(slot);
    return ESP_OK;
}

static esp_err_t le_security_reply(const uint8_t peer[6], bool accept)
{
    struct nimble_pair_slot slot = {0};
    struct ble_sm_io io = {0};
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE security reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE security reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = pair_slot_lookup(peer, &slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE security reply failed: no pending request for peer");
        return err;
    }
    if (slot.action != BLE_SM_IOACT_NONE && slot.action != BLE_SM_IOACT_NUMCMP) {
        ESP_LOGE(TAG, "LE security reply failed: pending action is %u", slot.action);
        return ESP_ERR_INVALID_STATE;
    }
    io.action = slot.action;
    if (io.action == BLE_SM_IOACT_NUMCMP) {
        io.numcmp_accept = accept ? 1 : 0;
    } else if (!accept) {
        ESP_LOGE(TAG, "LE security reply failed: reject is not supported for this action");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return inject_io(&slot, &io);
}

static esp_err_t le_confirm_reply(const uint8_t peer[6], bool accept)
{
    struct nimble_pair_slot slot = {0};
    struct ble_sm_io io = {
        .action = BLE_SM_IOACT_NUMCMP,
        .numcmp_accept = accept ? 1 : 0,
    };
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE confirm reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE confirm reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = pair_slot_lookup(peer, &slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE confirm reply failed: no pending request for peer");
        return err;
    }
    if (slot.action != BLE_SM_IOACT_NUMCMP) {
        ESP_LOGE(TAG, "LE confirm reply failed: pending action is %u", slot.action);
        return ESP_ERR_INVALID_STATE;
    }
    return inject_io(&slot, &io);
}

static esp_err_t le_passkey_reply(const uint8_t peer[6], bool accept, uint32_t passkey)
{
    struct nimble_pair_slot slot = {0};
    struct ble_sm_io io = {0};
    esp_err_t err;
    int rc;

    if (!peer) {
        ESP_LOGE(TAG, "LE passkey reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE passkey reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = pair_slot_lookup(peer, &slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE passkey reply failed: no pending request for peer");
        return err;
    }
    if (slot.action != BLE_SM_IOACT_INPUT && slot.action != BLE_SM_IOACT_DISP) {
        ESP_LOGE(TAG, "LE passkey reply failed: pending action is %u", slot.action);
        return ESP_ERR_INVALID_STATE;
    }
    if (!accept) {
        rc = ble_gap_terminate(slot.conn_handle, BLE_ERR_AUTH_FAIL);
        if (rc != 0) {
            ESP_LOGE(TAG, "LE passkey reject failed: terminate %d", rc);
            return ESP_FAIL;
        }
        pair_slot_clear_snapshot(&slot);
        return ESP_OK;
    }
    io.action = slot.action;
    io.passkey = passkey;
    return inject_io(&slot, &io);
}

const esp_bt_hub_backend_ops_t esp_bt_hub_nimble_ops = {
    .init                 = nimble_backend_init,
    .start                = nimble_backend_start,
    .stop                 = nimble_backend_stop,
    .deinit               = nimble_backend_deinit,
    .teardown_pending     = nimble_teardown_pending,
    .set_device_name      = nimble_set_device_name,
    .get_address          = nimble_get_address,
    .le_bond_count        = le_bond_count,
    .le_bond_exists       = le_bond_exists,
    .le_disconnect        = le_disconnect,
    .le_security_initiate = le_security_initiate,
    .le_privacy_enable    = le_privacy_enable,
    .le_security_reply    = le_security_reply,
    .le_confirm_reply     = le_confirm_reply,
    .le_passkey_reply     = le_passkey_reply,
    .le_scan_start        = nimble_le_scan_start,
    .le_scan_stop         = nimble_le_scan_stop,
    .le_adv_start         = nimble_le_adv_start,
    .le_adv_stop          = nimble_le_adv_stop,
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    .le_connect              = le_connect,
    .le_connect_cancel       = le_connect_cancel,
    .le_ext_adv_configure    = nimble_le_ext_adv_configure,
    .le_ext_adv_set_data     = nimble_le_ext_adv_set_data,
    .le_ext_adv_set_scan_rsp = nimble_le_ext_adv_set_scan_rsp,
    .le_ext_adv_remove       = nimble_le_ext_adv_remove,
    .le_ext_adv_start        = nimble_le_ext_adv_start,
    .le_ext_adv_stop         = nimble_le_ext_adv_stop,
    .le_ext_scan_start       = nimble_le_ext_scan_start,
    .le_ext_scan_stop        = nimble_le_ext_scan_stop,
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    .le_pa_sync_create        = le_pa_sync_create,
    .le_pa_sync_terminate     = le_pa_sync_terminate,
    .le_pa_sync_create_cancel = le_pa_sync_create_cancel,
#if ESP_BT_HUB_LE_PAST_SUPPORTED
    .le_pa_sync_receive = le_pa_sync_receive,
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
    .le_periodic_adv_configure = nimble_le_periodic_adv_configure,
    .le_periodic_adv_set_data  = nimble_le_periodic_adv_set_data,
    .le_periodic_adv_start     = nimble_le_periodic_adv_start,
    .le_periodic_adv_stop      = nimble_le_periodic_adv_stop,
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
};

const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_default_get(void)
{
    return &esp_bt_hub_nimble_ops;
}
