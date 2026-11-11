/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_log.h"
#if CONFIG_BT_CLASSIC_ENABLED
#include "esp_gap_bt_api.h"
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#if CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#include "esp_gattc_api.h"
#endif  /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
#include "tinycrypt/aes.h"
#include "tinycrypt/constants.h"
#endif  /* CONFIG_BT_BLE_ENABLED */

#include "esp_bt_hub_priv.h"

static const char *TAG = "ESP_BT_HUB_BDROID";

#define BDROID_RESOURCE_CONTROLLER_INIT     (1U << 0)
#define BDROID_RESOURCE_CONTROLLER_ENABLED  (1U << 1)
#define BDROID_RESOURCE_STACK_INIT          (1U << 2)
#define BDROID_RESOURCE_STACK_ENABLED       (1U << 3)

#define ESP_BT_HUB_LE_PEER_SLOTS      8
#define ESP_BT_HUB_GAP_OP_TIMEOUT_MS  5000

/**
 * @brief  Peer address recorded from a BLE Audio ACL connect event
 */
typedef struct {
    uint8_t   addr[6];      /*!< Peer address in Bluedroid byte order */
    uint16_t  conn_handle;  /*!< HCI connection handle */
    bool      valid;        /*!< Slot holds a peer */
} esp_bt_hub_le_peer_t;

typedef enum {
    CLASSIC_DISCOVERY_IDLE = 0,
    CLASSIC_DISCOVERY_STARTING,
    CLASSIC_DISCOVERY_ACTIVE,
    CLASSIC_DISCOVERY_STOPPING,
} classic_discovery_state_t;

typedef enum {
    LEGACY_OP_IDLE = 0,
    LEGACY_OP_CONFIGURING,
    LEGACY_OP_STARTING,
    LEGACY_OP_ACTIVE,
} legacy_op_state_t;

/**
 * @brief  Bluedroid backend runtime context
 */
typedef struct {
    uint32_t      lifecycle_resources;  /*!< Acquired lifecycle resources */
    portMUX_TYPE  state_lock;           /*!< Protects backend runtime state */
#if CONFIG_BT_CLASSIC_ENABLED
    QueueHandle_t              discovery_queue;  /*!< Discovery stop completion queue */
    SemaphoreHandle_t          discovery_mutex;  /*!< Serializes discovery operations */
    classic_discovery_state_t  discovery_state;  /*!< Discovery operation state */
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    legacy_op_state_t     scan_state;          /*!< Legacy scanning operation state */
    uint32_t              scan_duration_s;     /*!< Legacy scan duration in seconds */
    legacy_op_state_t     adv_state;           /*!< Legacy advertising operation state */
    esp_ble_adv_params_t  adv_params;          /*!< Legacy advertising parameters */
    bool                  le_privacy_enabled;  /*!< LE privacy is enabled */
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if CONFIG_BT_BLE_ENABLED
    QueueHandle_t           gap_queue;           /*!< GAP operation completion queue */
    SemaphoreHandle_t       gap_mutex;           /*!< Serializes synchronous GAP operations */
    esp_gap_ble_cb_event_t  gap_expected_event;  /*!< Expected GAP completion event */
    bool                    gap_pending;         /*!< GAP completion is pending */
#if CONFIG_BT_BLE_50_FEATURES_SUPPORTED
    uint8_t  connect_gattc_if;  /*!< Pending connection GATTC interface */
    bool     connect_pending;   /*!< Connection attempt is pending */
    uint8_t  connect_peer[6];   /*!< Pending connection peer */
#endif  /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    uint32_t  ext_adv_generation[ESP_BT_HUB_MAX_EXT_ADV_INSTANCES];  /*!< Advertising generations */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    esp_bt_hub_le_peer_t  peers[ESP_BT_HUB_LE_PEER_SLOTS];           /*!< Connected LE peers */
#endif  /* CONFIG_BT_BLE_ENABLED */
} esp_bt_hub_bluedroid_ctx_t;

static esp_bt_hub_bluedroid_ctx_t *s_ctx;

#define s_lifecycle_resources  s_ctx->lifecycle_resources
#define s_discovery_queue      s_ctx->discovery_queue
#define s_discovery_mutex      s_ctx->discovery_mutex
#define s_discovery_state      s_ctx->discovery_state
#define s_state_lock           s_ctx->state_lock
#define s_scan_state           s_ctx->scan_state
#define s_scan_duration_s      s_ctx->scan_duration_s
#define s_adv_state            s_ctx->adv_state
#define s_adv_params           s_ctx->adv_params
#define s_le_privacy_enabled   s_ctx->le_privacy_enabled
#define s_gap_queue            s_ctx->gap_queue
#define s_gap_mutex            s_ctx->gap_mutex
#define s_gap_expected_event   s_ctx->gap_expected_event
#define s_gap_pending          s_ctx->gap_pending
#define s_connect_gattc_if     s_ctx->connect_gattc_if
#define s_connect_pending      s_ctx->connect_pending
#define s_connect_peer         s_ctx->connect_peer
#define s_ext_adv_generation   s_ctx->ext_adv_generation
#define s_peers                s_ctx->peers

static esp_err_t bdroid_deinit(void);

#if CONFIG_BT_BLE_ENABLED
static esp_err_t gap_op_begin(esp_gap_ble_cb_event_t expected_event);
static esp_err_t gap_op_end(esp_err_t err);
static void gap_op_done(esp_gap_ble_cb_event_t event, esp_bt_status_t status);
#endif  /* CONFIG_BT_BLE_ENABLED */

#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
static void legacy_lock(void)
{
    portENTER_CRITICAL(&s_state_lock);
}

static void legacy_unlock(void)
{
    portEXIT_CRITICAL(&s_state_lock);
}

static esp_err_t bdroid_le_scan_start(const esp_bt_hub_le_scan_params_t *params)
{
    esp_ble_scan_params_t scan = {
        .scan_type = params->active ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = params->interval,
        .scan_window = params->window,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
    };
    esp_err_t err;

    legacy_lock();
    scan.own_addr_type = s_le_privacy_enabled ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_PUBLIC;
    s_scan_duration_s = params->duration_ms == 0 ? 0 : (params->duration_ms + 999) / 1000;
    s_scan_state = LEGACY_OP_CONFIGURING;
    legacy_unlock();
    err = esp_ble_gap_set_scan_params(&scan);
    if (err != ESP_OK) {
        legacy_lock();
        s_scan_state = LEGACY_OP_IDLE;
        legacy_unlock();
        ESP_LOGE(TAG, "Start LE scan failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_scan_stop(void)
{
    bool active;
    esp_err_t err;

    legacy_lock();
    active = s_scan_state == LEGACY_OP_ACTIVE;
    legacy_unlock();
    if (!active) {
        return ESP_OK;
    }
    err = gap_op_begin(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT);
    if (err != ESP_OK) {
        return err;
    }
    err = gap_op_end(esp_ble_gap_stop_scanning());
    if (err != ESP_OK) {
        legacy_lock();
        active = s_scan_state == LEGACY_OP_ACTIVE;
        legacy_unlock();
        if (!active) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Stop LE scan failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_adv_start(const esp_bt_hub_le_adv_params_t *params)
{
    esp_err_t err;

    legacy_lock();
    memset(&s_adv_params, 0, sizeof(s_adv_params));
    s_adv_params.adv_int_min = params->interval_min;
    s_adv_params.adv_int_max = params->interval_max;
    s_adv_params.adv_type = (esp_ble_adv_type_t)params->adv_type;
    s_adv_params.own_addr_type = s_le_privacy_enabled ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_PUBLIC;
    s_adv_params.channel_map = params->channel_map ? (esp_ble_adv_channel_t)params->channel_map : ADV_CHNL_ALL;
    s_adv_params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    s_adv_state = LEGACY_OP_CONFIGURING;
    legacy_unlock();
    err = esp_ble_gap_config_adv_data_raw((uint8_t *)params->data, params->data_len);
    if (err != ESP_OK) {
        legacy_lock();
        s_adv_state = LEGACY_OP_IDLE;
        legacy_unlock();
        ESP_LOGE(TAG, "Start legacy advertising failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_adv_stop(void)
{
    bool active;
    esp_err_t err;

    legacy_lock();
    active = s_adv_state == LEGACY_OP_ACTIVE;
    legacy_unlock();
    if (!active) {
        return ESP_OK;
    }
    err = gap_op_begin(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT);
    if (err != ESP_OK) {
        return err;
    }
    err = gap_op_end(esp_ble_gap_stop_advertising());
    if (err != ESP_OK) {
        legacy_lock();
        active = s_adv_state == LEGACY_OP_ACTIVE;
        legacy_unlock();
        if (!active) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Stop legacy advertising failed: %s", esp_err_to_name(err));
    }
    return err;
}

static void bdroid_le_legacy_on_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
        uint32_t duration_s;
        bool expected;

        legacy_lock();
        expected = s_scan_state == LEGACY_OP_CONFIGURING;
        duration_s = s_scan_duration_s;
        if (expected) {
            s_scan_state = param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS ? LEGACY_OP_STARTING : LEGACY_OP_IDLE;
        }
        legacy_unlock();
        if (!expected) {
            return;
        }
        if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            esp_err_t err = esp_ble_gap_start_scanning(duration_s);

            if (err != ESP_OK) {
                legacy_lock();
                s_scan_state = LEGACY_OP_IDLE;
                legacy_unlock();
                ESP_LOGE(TAG, "Start LE scan failed: %s", esp_err_to_name(err));
                esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, false);
            }
        } else {
            ESP_LOGE(TAG, "Start LE scan failed: set scan params status %d", param->scan_param_cmpl.status);
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, false);
        }
    } else if (event == ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
        bool expected;
        bool success = param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS;

        legacy_lock();
        expected = s_scan_state == LEGACY_OP_STARTING;
        if (expected) {
            s_scan_state = success ? LEGACY_OP_ACTIVE : LEGACY_OP_IDLE;
        }
        legacy_unlock();
        if (!expected) {
            return;
        }
        if (!success) {
            ESP_LOGE(TAG, "Start LE scan failed: start status %d", param->scan_start_cmpl.status);
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, false);
        } else {
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_SCAN, true);
        }
    } else if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        esp_ble_adv_params_t adv_params;
        bool expected;

        legacy_lock();
        expected = s_adv_state == LEGACY_OP_CONFIGURING;
        adv_params = s_adv_params;
        if (expected) {
            s_adv_state = param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS ? LEGACY_OP_STARTING : LEGACY_OP_IDLE;
        }
        legacy_unlock();
        if (!expected) {
            return;
        }
        if (param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            esp_err_t err = esp_ble_gap_start_advertising(&adv_params);

            if (err != ESP_OK) {
                legacy_lock();
                s_adv_state = LEGACY_OP_IDLE;
                legacy_unlock();
                ESP_LOGE(TAG, "Start legacy advertising failed: %s", esp_err_to_name(err));
                esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, false);
            }
        } else {
            ESP_LOGE(TAG, "Start legacy advertising failed: set adv data status %d",
                     param->adv_data_raw_cmpl.status);
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, false);
        }
    } else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        bool expected;
        bool success = param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS;

        legacy_lock();
        expected = s_adv_state == LEGACY_OP_STARTING;
        if (expected) {
            s_adv_state = success ? LEGACY_OP_ACTIVE : LEGACY_OP_IDLE;
        }
        legacy_unlock();
        if (!expected) {
            return;
        }
        if (!success) {
            ESP_LOGE(TAG, "Start legacy advertising failed: start status %d", param->adv_start_cmpl.status);
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, false);
        } else {
            esp_bt_hub_async_resource_complete(ESP_BT_HUB_ASYNC_LE_ADV, true);
        }
    } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT &&
               param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        legacy_lock();
        s_scan_state = LEGACY_OP_IDLE;
        legacy_unlock();
        gap_op_done(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT, ESP_BT_STATUS_SUCCESS);
    } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT &&
               param->scan_rst.search_evt == ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT) {
        legacy_lock();
        s_scan_state = LEGACY_OP_IDLE;
        legacy_unlock();
    } else if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
        legacy_lock();
        s_scan_state = param->scan_stop_cmpl.status == ESP_BT_STATUS_SUCCESS ? LEGACY_OP_IDLE : LEGACY_OP_ACTIVE;
        legacy_unlock();
        gap_op_done(ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT, param->scan_stop_cmpl.status);
    } else if (event == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT) {
        legacy_lock();
        s_adv_state = param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS ? LEGACY_OP_IDLE : LEGACY_OP_ACTIVE;
        legacy_unlock();
        gap_op_done(ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT, param->adv_stop_cmpl.status);
    }
}
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

static esp_bt_mode_t controller_mode(void)
{
#if defined(BTDM_CONTROLLER_MODE_EFF)
    return BTDM_CONTROLLER_MODE_EFF;
#elif CONFIG_BT_CLASSIC_ENABLED && CONFIG_BT_BLE_ENABLED
    return ESP_BT_MODE_BTDM;
#elif CONFIG_BT_CLASSIC_ENABLED
    return ESP_BT_MODE_CLASSIC_BT;
#else
    return ESP_BT_MODE_BLE;
#endif  /* defined(BTDM_CONTROLLER_MODE_EFF) */
}

#if CONFIG_BT_CLASSIC_ENABLED
static uint8_t classic_io_cap(esp_bt_hub_io_cap_t cap)
{
    if (cap > ESP_BT_HUB_IO_CAP_NONE) {
        return ESP_BT_IO_CAP_NONE;
    }
    return (uint8_t)cap;
}
#endif  /* CONFIG_BT_CLASSIC_ENABLED */

#if CONFIG_BT_BLE_ENABLED
static esp_ble_auth_req_t le_auth_req(const esp_bt_hub_config_t *config)
{
    esp_ble_auth_req_t auth = ESP_LE_AUTH_NO_BOND;

    if (config->bonding) {
        auth |= ESP_LE_AUTH_BOND;
    }
    if (config->mitm) {
        auth |= ESP_LE_AUTH_REQ_MITM;
    }
    if (config->secure_connections) {
        auth |= ESP_LE_AUTH_REQ_SC_ONLY;
    }
    return auth;
}
#endif  /* CONFIG_BT_BLE_ENABLED */

#if CONFIG_BT_CLASSIC_ENABLED
static void classic_auto_reply(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    const esp_bt_hub_config_t *config = esp_bt_hub_config_get();
    esp_bt_pin_code_t pin = {0};
    uint8_t pin_len;

    if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        if (param->pin_req.min_16_digit) {
            pin_len = 16;
        } else if (config->classic_pin_len > 0) {
            pin_len = config->classic_pin_len;
            memcpy(pin, config->classic_pin, pin_len);
        } else {
            pin_len = 4;
        }
        esp_bt_gap_pin_reply(param->pin_req.bda, true, pin_len, pin);
    } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
    } else if (event == ESP_BT_GAP_KEY_REQ_EVT) {
        esp_err_t err;

        ESP_LOGW(TAG, "Auto-accept pairing: Classic passkey entry is rejected");
        err = esp_bt_gap_ssp_passkey_reply(param->key_req.bda, false, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Reject Classic passkey failed: %s", esp_err_to_name(err));
        }
    }
}

static inline bool classic_pairing_event(esp_bt_gap_cb_event_t event)
{
    return event == ESP_BT_GAP_PIN_REQ_EVT || event == ESP_BT_GAP_CFM_REQ_EVT || event == ESP_BT_GAP_KEY_REQ_EVT;
}

static void classic_discovery_state_set(classic_discovery_state_t state)
{
    taskENTER_CRITICAL(&s_state_lock);
    s_discovery_state = state;
    taskEXIT_CRITICAL(&s_state_lock);
}

static classic_discovery_state_t classic_discovery_state_get(void)
{
    classic_discovery_state_t state;

    taskENTER_CRITICAL(&s_state_lock);
    state = s_discovery_state;
    taskEXIT_CRITICAL(&s_state_lock);
    return state;
}

static void classic_discovery_started(void)
{
    taskENTER_CRITICAL(&s_state_lock);
    if (s_discovery_state == CLASSIC_DISCOVERY_STARTING) {
        s_discovery_state = CLASSIC_DISCOVERY_ACTIVE;
    }
    taskEXIT_CRITICAL(&s_state_lock);
}

static bool classic_discovery_stop_begin(void)
{
    bool active;

    taskENTER_CRITICAL(&s_state_lock);
    active = s_discovery_state != CLASSIC_DISCOVERY_IDLE;
    if (active) {
        s_discovery_state = CLASSIC_DISCOVERY_STOPPING;
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return active;
}

static bool classic_discovery_stopped(void)
{
    bool completed;

    taskENTER_CRITICAL(&s_state_lock);
    completed = s_discovery_state == CLASSIC_DISCOVERY_ACTIVE || s_discovery_state == CLASSIC_DISCOVERY_STOPPING;
    if (completed) {
        s_discovery_state = CLASSIC_DISCOVERY_IDLE;
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return completed;
}

static void classic_discovery_sync_deinit(void)
{
    if (s_discovery_queue) {
        vQueueDelete(s_discovery_queue);
        s_discovery_queue = NULL;
    }
    if (s_discovery_mutex) {
        vSemaphoreDelete(s_discovery_mutex);
        s_discovery_mutex = NULL;
    }
}

static void host_classic_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT && param) {
        if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
            classic_discovery_started();
        } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED &&
                   classic_discovery_stopped()) {
            uint8_t completed = 1;

            if (s_discovery_queue) {
                xQueueOverwrite(s_discovery_queue, &completed);
            }
        }
    }
    if (classic_pairing_event(event) && esp_bt_hub_pairing_auto_reply_enabled()) {
        classic_auto_reply(event, param);
    }
    esp_bt_hub_gap_dispatch(ESP_BT_HUB_GAP_DOMAIN_CLASSIC, (uint32_t)event, param);
}
#endif  /* CONFIG_BT_CLASSIC_ENABLED */

#if CONFIG_BT_BLE_ENABLED
#define ESP_BT_HUB_SCAN_UNIT_MS     10
#define ESP_BT_HUB_EXT_SCAN_MAX_MS  ((uint32_t)UINT16_MAX * ESP_BT_HUB_SCAN_UNIT_MS)

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static void le_connect_complete_locked(const uint8_t peer[6])
{
    if (s_connect_pending && (!peer || memcmp(s_connect_peer, peer, sizeof(s_connect_peer)) == 0)) {
        s_connect_gattc_if = ESP_GATT_IF_NONE;
        s_connect_pending = false;
        memset(s_connect_peer, 0, sizeof(s_connect_peer));
    }
}

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

static esp_err_t le_peer_remember(uint16_t conn_handle, const uint8_t addr[6])
{
    esp_bt_hub_le_peer_t *slot = NULL;

    if (!s_ctx) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < ESP_BT_HUB_LE_PEER_SLOTS; i++) {
        if (s_peers[i].valid && s_peers[i].conn_handle == conn_handle) {
            slot = &s_peers[i];
            break;
        }
        if (!slot && !s_peers[i].valid) {
            slot = &s_peers[i];
        }
    }
    if (slot) {
        memcpy(slot->addr, addr, sizeof(slot->addr));
        slot->conn_handle = conn_handle;
        slot->valid = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (!slot) {
        ESP_LOGE(TAG, "Record LE peer failed: table full, conn_handle %u", conn_handle);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t le_peer_forget_handle(uint16_t conn_handle)
{
    if (!s_ctx) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < ESP_BT_HUB_LE_PEER_SLOTS; i++) {
        if (s_peers[i].valid && s_peers[i].conn_handle == conn_handle) {
            memset(&s_peers[i], 0, sizeof(s_peers[i]));
        }
    }
    portEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

static void le_peer_forget_all(void)
{
    if (!s_ctx) {
        return;
    }
    portENTER_CRITICAL(&s_state_lock);
    memset(s_peers, 0, sizeof(s_peers));
    portEXIT_CRITICAL(&s_state_lock);
}

/**
 * @brief  Resolve the Bluedroid peer address recorded for an HCI ACL handle
 */
static esp_err_t le_peer_addr_from_handle(uint16_t conn_handle, esp_bd_addr_t addr)
{
    bool found = false;

    if (!s_ctx) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < ESP_BT_HUB_LE_PEER_SLOTS; i++) {
        if (s_peers[i].valid && s_peers[i].conn_handle == conn_handle) {
            memcpy(addr, s_peers[i].addr, sizeof(esp_bd_addr_t));
            found = true;
            break;
        }
    }
    portEXIT_CRITICAL(&s_state_lock);
    return found ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t le_peer_set(uint16_t conn_handle, const uint8_t addr[6])
{
    if (!addr) {
        ESP_LOGE(TAG, "Record LE peer failed: addr is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ctx) {
        ESP_LOGE(TAG, "Record LE peer failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    portENTER_CRITICAL(&s_state_lock);
    le_connect_complete_locked(addr);
    portEXIT_CRITICAL(&s_state_lock);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    return le_peer_remember(conn_handle, addr);
}

esp_err_t esp_bt_hub_le_peer_set(uint16_t conn_handle, const uint8_t addr[6])
{
    esp_err_t err;

    if (!addr) {
        ESP_LOGE(TAG, "Record LE peer failed: addr is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    err = esp_bt_hub_context_enter();
    if (err != ESP_OK) {
        return err;
    }
    err = le_peer_set(conn_handle, addr);
    esp_bt_hub_context_leave();
    return err;
}

static esp_err_t le_peer_clear(uint16_t conn_handle)
{
    if (!s_ctx) {
        ESP_LOGE(TAG, "Clear LE peer failed: hub is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return le_peer_forget_handle(conn_handle);
}

esp_err_t esp_bt_hub_le_peer_clear(uint16_t conn_handle)
{
    esp_err_t err = esp_bt_hub_context_enter();

    if (err != ESP_OK) {
        return err;
    }
    err = le_peer_clear(conn_handle);
    esp_bt_hub_context_leave();
    return err;
}

static esp_err_t gap_sync_ensure(void)
{
    if (!s_gap_queue || !s_gap_mutex) {
        ESP_LOGE(TAG, "Begin GAP operation failed: synchronization is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static esp_err_t gap_op_begin(esp_gap_ble_cb_event_t expected_event)
{
    esp_err_t err = gap_sync_ensure();
    esp_bt_status_t stale_status;

    if (err != ESP_OK) {
        return err;
    }
    if (xSemaphoreTake(s_gap_mutex, pdMS_TO_TICKS(ESP_BT_HUB_GAP_OP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Acquire GAP operation lock timed out");
        return ESP_ERR_TIMEOUT;
    }
    while (xQueueReceive(s_gap_queue, &stale_status, 0) == pdTRUE) {
    }
    taskENTER_CRITICAL(&s_state_lock);
    s_gap_expected_event = expected_event;
    s_gap_pending = true;
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

static esp_err_t gap_op_end(esp_err_t err)
{
    esp_err_t ret = err;
    esp_bt_status_t status;

    if (err == ESP_OK) {
        if (xQueueReceive(s_gap_queue, &status, pdMS_TO_TICKS(ESP_BT_HUB_GAP_OP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "GAP operation completion timed out");
            ret = ESP_ERR_TIMEOUT;
        } else {
            ret = status == ESP_BT_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
        }
    }
    taskENTER_CRITICAL(&s_state_lock);
    s_gap_pending = false;
    taskEXIT_CRITICAL(&s_state_lock);
    xSemaphoreGive(s_gap_mutex);
    return ret;
}

static void gap_op_done(esp_gap_ble_cb_event_t event, esp_bt_status_t status)
{
    bool expected;

    if (!s_gap_queue) {
        return;
    }
    taskENTER_CRITICAL(&s_state_lock);
    expected = s_gap_pending && s_gap_expected_event == event;
    taskEXIT_CRITICAL(&s_state_lock);
    if (expected && xQueueSend(s_gap_queue, &status, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Complete GAP operation: completion queue full, event %d dropped", event);
    }
}

static void bdroid_le_ext_deinit(void)
{
    if (s_gap_queue) {
        vQueueDelete(s_gap_queue);
        s_gap_queue = NULL;
    }
    if (s_gap_mutex) {
        vSemaphoreDelete(s_gap_mutex);
        s_gap_mutex = NULL;
    }
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    taskENTER_CRITICAL(&s_state_lock);
    le_connect_complete_locked(NULL);
    taskEXIT_CRITICAL(&s_state_lock);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    taskENTER_CRITICAL(&s_state_lock);
    memset(s_ext_adv_generation, 0, sizeof(s_ext_adv_generation));
    taskEXIT_CRITICAL(&s_state_lock);
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
}

static void bdroid_le_ext_on_gap(esp_gap_ble_cb_event_t event, void *param)
{
    esp_ble_gap_cb_param_t *gap = param;

    if (!gap) {
        return;
    }
    switch (event) {
        case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
            gap_op_done(event, gap->local_privacy_cmpl.status);
            break;
#if CONFIG_BT_BLE_50_FEATURES_SUPPORTED
        case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
            gap_op_done(event, gap->set_ext_scan_params.status);
            break;
        case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
            gap_op_done(event, gap->ext_scan_start.status);
            break;
        case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
            gap_op_done(event, gap->ext_scan_stop.status);
            break;
        case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
            esp_bt_hub_le_ext_scan_complete();
            break;
        case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
            gap_op_done(event, gap->ext_adv_set_params.status);
            break;
        case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
            gap_op_done(event, gap->ext_adv_data_set.status);
            break;
        case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            gap_op_done(event, gap->scan_rsp_set.status);
            break;
        case ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT:
            gap_op_done(event, gap->ext_adv_remove.status);
            break;
        case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
            gap_op_done(event, gap->ext_adv_start.status);
            break;
        case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
            gap_op_done(event, gap->ext_adv_stop.status);
            break;
#if ESP_BT_HUB_LE_EXT_SUPPORTED
        case ESP_GAP_BLE_ADV_TERMINATED_EVT:
            if (gap->adv_terminate.adv_instance < ESP_BT_HUB_MAX_EXT_ADV_INSTANCES) {
                uint8_t instance = gap->adv_terminate.adv_instance;
                uint32_t generation = ext_adv_generation_take(instance);

                esp_bt_hub_le_ext_adv_complete(instance, generation);
            }
            break;
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
        case ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT:
            gap_op_done(event, gap->peroid_adv_set_params.status);
            break;
        case ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT:
            gap_op_done(event, gap->period_adv_data_set.status);
            break;
        case ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT:
            gap_op_done(event, gap->period_adv_start.status);
            break;
        case ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT:
            gap_op_done(event, gap->period_adv_stop.status);
            break;
#if CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER
        case ESP_GAP_BLE_SET_PAST_PARAMS_COMPLETE_EVT:
            gap_op_done(event, gap->set_past_params.status);
            break;
#endif  /* CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER */
#endif  /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
        default:
            break;
    }
}

static inline bool ble_pairing_event(esp_gap_ble_cb_event_t event)
{
    return event == ESP_GAP_BLE_SEC_REQ_EVT || event == ESP_GAP_BLE_NC_REQ_EVT || event == ESP_GAP_BLE_PASSKEY_REQ_EVT;
}

static void ble_auto_reply(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    } else if (event == ESP_GAP_BLE_NC_REQ_EVT) {
        esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
    } else if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT) {
        esp_err_t err;

        ESP_LOGW(TAG, "Auto-accept pairing: LE passkey entry is rejected");
        err = esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, false, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Reject LE passkey failed: %s", esp_err_to_name(err));
        }
    }
}

static void host_ble_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    bdroid_le_legacy_on_gap(event, param);
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

    bdroid_le_ext_on_gap(event, param);

    if (ble_pairing_event(event) && esp_bt_hub_pairing_auto_reply_enabled()) {
        ble_auto_reply(event, param);
    }
    esp_bt_hub_gap_dispatch(ESP_BT_HUB_GAP_DOMAIN_BLE, (uint32_t)event, param);
}
#endif  /* CONFIG_BT_BLE_ENABLED */

static esp_err_t bdroid_init(const esp_bt_hub_config_t *config)
{
    esp_err_t err;
#if CONFIG_BT_CLASSIC_ENABLED
    uint8_t iocap;
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if CONFIG_BT_BLE_ENABLED
    esp_ble_auth_req_t auth;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
#endif  /* CONFIG_BT_BLE_ENABLED */

    if (s_ctx) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: backend context already exists");
        return ESP_ERR_INVALID_STATE;
    }
    s_ctx = heap_caps_calloc_prefer(1, sizeof(*s_ctx), 2,
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
    if (!s_ctx) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: no memory for backend context");
        return ESP_ERR_NO_MEM;
    }
    s_state_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
#if CONFIG_BT_CLASSIC_ENABLED
    s_discovery_queue = xQueueCreate(1, sizeof(uint8_t));
    s_discovery_mutex = xSemaphoreCreateMutex();
    if (!s_discovery_queue || !s_discovery_mutex) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: no memory for discovery synchronization");
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if CONFIG_BT_BLE_ENABLED
    s_gap_queue = xQueueCreate(1, sizeof(esp_bt_status_t));
    s_gap_mutex = xSemaphoreCreateMutex();
    if (!s_gap_queue || !s_gap_mutex) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: no memory for GAP synchronization");
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
#if CONFIG_BT_BLE_50_FEATURES_SUPPORTED
    s_connect_gattc_if = ESP_GATT_IF_NONE;
#endif  /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
#endif  /* CONFIG_BT_BLE_ENABLED */
    s_lifecycle_resources = 0;
    err = esp_bt_controller_init((esp_bt_controller_config_t *)&config->controller_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: controller init %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= BDROID_RESOURCE_CONTROLLER_INIT;
    err = esp_bt_controller_enable(controller_mode());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: controller enable %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= BDROID_RESOURCE_CONTROLLER_ENABLED;

    err = esp_bluedroid_init_with_cfg((esp_bluedroid_config_t *)&config->bluedroid_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: stack init %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= BDROID_RESOURCE_STACK_INIT;
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: stack enable %s", esp_err_to_name(err));
        goto fail;
    }
    s_lifecycle_resources |= BDROID_RESOURCE_STACK_ENABLED;

#if CONFIG_BT_CLASSIC_ENABLED
    err = esp_bt_gap_set_device_name(config->device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set device name %s", esp_err_to_name(err));
        goto fail;
    }
    iocap = classic_io_cap(config->io_capability);
    err = esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set IO capability %s", esp_err_to_name(err));
        goto fail;
    }
    if (config->classic_pin_len > 0) {
        esp_bt_pin_code_t pin = {0};
        memcpy(pin, config->classic_pin, config->classic_pin_len);
        err = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, config->classic_pin_len, pin);
    } else {
        err = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, NULL);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set PIN %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_bt_gap_register_callback(host_classic_gap_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: register Classic GAP %s", esp_err_to_name(err));
        goto fail;
    }
#endif  /* CONFIG_BT_CLASSIC_ENABLED */

#if CONFIG_BT_BLE_ENABLED
    auth = le_auth_req(config);
    err = esp_ble_gap_set_device_name(config->device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE device name %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, sizeof(auth));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE auth %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, (void *)&config->io_capability, sizeof(uint8_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE IO capability %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE key size %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE init key %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: set BLE rsp key %s", esp_err_to_name(err));
        goto fail;
    }
    err = esp_ble_gap_register_callback(host_ble_gap_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' failed: register BLE GAP %s", esp_err_to_name(err));
        goto fail;
    }
#endif  /* CONFIG_BT_BLE_ENABLED */

    ESP_LOGI(TAG, "Init 'bluedroid': success");
    return ESP_OK;

fail:
    esp_err_t rollback_err = bdroid_deinit();

    if (rollback_err != ESP_OK) {
        ESP_LOGE(TAG, "Init 'bluedroid' rollback failed: %s", esp_err_to_name(rollback_err));
        return rollback_err;
    }
    return err;
}

static esp_err_t bdroid_start(void)
{
    return ESP_OK;
}

static esp_err_t bdroid_stop(void)
{
    return ESP_OK;
}

static esp_err_t bdroid_deinit(void)
{
    esp_bt_hub_bluedroid_ctx_t *ctx;
    esp_err_t err;

    if (!s_ctx) {
        return ESP_OK;
    }
#if CONFIG_BT_BLE_ENABLED
    le_peer_forget_all();
#endif  /* CONFIG_BT_BLE_ENABLED */
    if (s_lifecycle_resources & BDROID_RESOURCE_STACK_ENABLED) {
        err = esp_bluedroid_disable();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'bluedroid' failed: stack disable %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~BDROID_RESOURCE_STACK_ENABLED;
    }
    if (s_lifecycle_resources & BDROID_RESOURCE_STACK_INIT) {
        err = esp_bluedroid_deinit();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'bluedroid' failed: stack deinit %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~BDROID_RESOURCE_STACK_INIT;
    }
    if (s_lifecycle_resources & BDROID_RESOURCE_CONTROLLER_ENABLED) {
        err = esp_bt_controller_disable();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'bluedroid' failed: controller disable %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~BDROID_RESOURCE_CONTROLLER_ENABLED;
    }
    if (s_lifecycle_resources & BDROID_RESOURCE_CONTROLLER_INIT) {
        err = esp_bt_controller_deinit();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Deinit 'bluedroid' failed: controller deinit %s", esp_err_to_name(err));
            return err;
        }
        s_lifecycle_resources &= ~BDROID_RESOURCE_CONTROLLER_INIT;
    }
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    s_le_privacy_enabled = false;
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if CONFIG_BT_BLE_ENABLED
    bdroid_le_ext_deinit();
#endif  /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_CLASSIC_ENABLED
    classic_discovery_sync_deinit();
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
    ctx = s_ctx;
    s_ctx = NULL;
    heap_caps_free(ctx);
    ESP_LOGI(TAG, "Deinit 'bluedroid': success");
    return ESP_OK;
}

static bool bdroid_teardown_pending(void)
{
    return s_ctx && s_lifecycle_resources != 0;
}

static esp_err_t bdroid_set_device_name(const char *name)
{
    esp_err_t err = ESP_OK;

#if CONFIG_BT_CLASSIC_ENABLED
    err = esp_bt_gap_set_device_name(name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set device name failed: %s", esp_err_to_name(err));
        return err;
    }
#endif  /* CONFIG_BT_CLASSIC_ENABLED */
#if CONFIG_BT_BLE_ENABLED
    err = esp_ble_gap_set_device_name(name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set BLE device name failed: %s", esp_err_to_name(err));
    }
#endif  /* CONFIG_BT_BLE_ENABLED */
    return err;
}

static esp_err_t bdroid_get_address(uint8_t address[6])
{
    const uint8_t *local = esp_bt_dev_get_address();

    if (!local) {
        ESP_LOGE(TAG, "Get local address failed: address is not ready");
        return ESP_ERR_INVALID_STATE;
    }
    memcpy(address, local, 6);
    return ESP_OK;
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static esp_err_t bdroid_classic_scan_mode(bool connectable, esp_bt_hub_discoverable_t discoverable)
{
    esp_bt_connection_mode_t conn = connectable ? ESP_BT_CONNECTABLE : ESP_BT_NON_CONNECTABLE;
    esp_err_t err = esp_bt_gap_set_scan_mode(conn, (esp_bt_discovery_mode_t)discoverable);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set scan mode failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_classic_discovery_start(uint8_t inquiry_mode, uint8_t inquiry_len, uint8_t num_responses)
{
    uint8_t stale;
    esp_err_t err;

    if (xSemaphoreTake(s_discovery_mutex, pdMS_TO_TICKS(ESP_BT_HUB_GAP_OP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Start discovery failed: acquire operation lock timed out");
        return ESP_ERR_TIMEOUT;
    }
    while (xQueueReceive(s_discovery_queue, &stale, 0) == pdTRUE) {
    }
    classic_discovery_state_set(CLASSIC_DISCOVERY_STARTING);
    err = esp_bt_gap_start_discovery((esp_bt_inq_mode_t)inquiry_mode, inquiry_len, num_responses);
    if (err != ESP_OK) {
        classic_discovery_state_set(CLASSIC_DISCOVERY_IDLE);
    }
    xSemaphoreGive(s_discovery_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start discovery failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_classic_discovery_stop(void)
{
    uint8_t completed;
    esp_err_t err;

    if (xSemaphoreTake(s_discovery_mutex, pdMS_TO_TICKS(ESP_BT_HUB_GAP_OP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Stop discovery failed: acquire operation lock timed out");
        return ESP_ERR_TIMEOUT;
    }
    while (xQueueReceive(s_discovery_queue, &completed, 0) == pdTRUE) {
    }
    if (!classic_discovery_stop_begin()) {
        xSemaphoreGive(s_discovery_mutex);
        return ESP_OK;
    }
    err = esp_bt_gap_cancel_discovery();
    if (err == ESP_OK) {
        if (xQueueReceive(s_discovery_queue, &completed,
                          pdMS_TO_TICKS(ESP_BT_HUB_GAP_OP_TIMEOUT_MS)) != pdTRUE) {
            err = classic_discovery_state_get() == CLASSIC_DISCOVERY_IDLE ? ESP_OK : ESP_ERR_TIMEOUT;
        }
    } else if (classic_discovery_state_get() == CLASSIC_DISCOVERY_IDLE) {
        err = ESP_OK;
    }
    xSemaphoreGive(s_discovery_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop discovery failed: %s", esp_err_to_name(err));
    }
    return err;
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if CONFIG_BT_BLE_ENABLED
static bool bdroid_rpa_matches_irk(const uint8_t rpa[6], const uint8_t irk[16])
{
    uint8_t key[16];
    uint8_t plaintext[16] = {0};
    uint8_t ciphertext[16];
    struct tc_aes_key_sched_struct sched;

    for (size_t i = 0; i < sizeof(key); i++) {
        key[i] = irk[sizeof(key) - 1 - i];
    }
    plaintext[13] = rpa[0];
    plaintext[14] = rpa[1];
    plaintext[15] = rpa[2];

    if (tc_aes128_set_encrypt_key(&sched, key) == TC_CRYPTO_FAIL ||
        tc_aes_encrypt(ciphertext, plaintext, &sched) == TC_CRYPTO_FAIL) {
        memset(&sched, 0, sizeof(sched));
        return false;
    }
    memset(&sched, 0, sizeof(sched));
    return ciphertext[15] == rpa[5] &&
           ciphertext[14] == rpa[4] &&
           ciphertext[13] == rpa[3];
}
static size_t le_bond_count(void)
{
    int count;

    if (!esp_bt_hub_is_running()) {
        return 0;
    }
    count = esp_ble_get_bond_device_num();
    return count > 0 ? (size_t)count : 0;
}

static bool le_bond_exists(uint8_t addr_type, const uint8_t addr[6])
{
    esp_ble_bond_dev_t *peers = NULL;
    uint8_t bd_addr[6];
    bool found = false;
    int count;

    if (!addr || !esp_bt_hub_is_running()) {
        return false;
    }
    count = esp_ble_get_bond_device_num();
    if (count <= 0) {
        return false;
    }
    peers = heap_caps_calloc(count, sizeof(*peers), MALLOC_CAP_DEFAULT);
    if (!peers) {
        return false;
    }
    esp_bt_hub_le_addr_swap_if_need(bd_addr, addr);
    if (esp_ble_get_bond_device_list(&count, peers) == ESP_OK) {
        for (int i = 0; i < count; i++) {
            bool identity_match =
                memcmp(peers[i].bd_addr, bd_addr, 6) == 0 &&
                peers[i].bd_addr_type == addr_type;
            bool rpa_match =
                (addr_type == BLE_ADDR_TYPE_RANDOM || addr_type == BLE_ADDR_TYPE_RPA_PUBLIC || addr_type == BLE_ADDR_TYPE_RPA_RANDOM) && (bd_addr[0] & 0xc0U) == 0x40U && (peers[i].bond_key.key_mask & ESP_BLE_ID_KEY_MASK) && bdroid_rpa_matches_irk(bd_addr, peers[i].bond_key.pid_key.irk);
            if (identity_match || rpa_match) {
                found = true;
                break;
            }
        }
    }
    heap_caps_free(peers);
    return found;
}

static esp_err_t le_disconnect(uint16_t conn_handle, uint8_t reason)
{
    esp_bd_addr_t addr;
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Disconnect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = le_peer_addr_from_handle(conn_handle, addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Disconnect failed: no ACL connection for conn_handle %u", conn_handle);
        return err;
    }
    err = esp_ble_gap_disconnect(addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Disconnect GAP link failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_security_initiate(uint16_t conn_handle)
{
    esp_bd_addr_t addr;
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Security initiate failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = le_peer_addr_from_handle(conn_handle, addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Security initiate failed: no ACL connection for conn_handle %u", conn_handle);
        return err;
    }
    err = esp_ble_set_encryption(addr, ESP_BLE_SEC_ENCRYPT_NO_MITM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set encryption failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t esp_bt_hub_le_id_infer_auto(int privacy, uint8_t *own_addr_type)
{
    if (!own_addr_type) {
        ESP_LOGE(TAG, "Infer own address failed: own_addr_type is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *own_addr_type = privacy ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_PUBLIC;
    return ESP_OK;
}

static esp_err_t le_privacy_enable(void)
{
    int bond_count;
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Enable LE privacy failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    legacy_lock();
    bool already_enabled = s_le_privacy_enabled;
    legacy_unlock();
    if (already_enabled) {
        return ESP_OK;
    }
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
    bond_count = esp_ble_get_bond_device_num();
    if (bond_count > 0) {
        esp_ble_bond_dev_t *bonds = heap_caps_calloc(bond_count, sizeof(*bonds), MALLOC_CAP_DEFAULT);
        if (!bonds) {
            ESP_LOGE(TAG, "Enable LE privacy failed: no memory for bond list");
            return ESP_ERR_NO_MEM;
        }
        if (esp_ble_get_bond_device_list(&bond_count, bonds) == ESP_OK) {
            for (int i = 0; i < bond_count; i++) {
                esp_err_t add_err;

                if (!(bonds[i].bond_key.key_mask & ESP_BLE_ID_KEY_MASK)) {
                    continue;
                }
                add_err = esp_ble_gap_add_device_to_resolving_list(bonds[i].bond_key.pid_key.static_addr,
                                                                   bonds[i].bond_key.pid_key.addr_type,
                                                                   bonds[i].bond_key.pid_key.irk);
                if (add_err != ESP_OK) {
                    ESP_LOGW(TAG, "Add bonded peer to resolving list failed: %s", esp_err_to_name(add_err));
                }
            }
        }
        heap_caps_free(bonds);
    }
    err = gap_op_begin(ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_config_local_privacy(true));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Enable local privacy failed: %s", esp_err_to_name(err));
        return err;
    }
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    legacy_lock();
    s_le_privacy_enabled = true;
    legacy_unlock();
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
    return ESP_OK;
}

static esp_err_t le_security_reply(const uint8_t peer[6], bool accept)
{
    esp_bd_addr_t addr;
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE security reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE security reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_bt_hub_le_addr_swap_if_need(addr, peer);
    err = esp_ble_gap_security_rsp(addr, accept);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE security reply failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_confirm_reply(const uint8_t peer[6], bool accept)
{
    esp_bd_addr_t addr;
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE confirm reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE confirm reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_bt_hub_le_addr_swap_if_need(addr, peer);
    err = esp_ble_confirm_reply(addr, accept);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE confirm reply failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_passkey_reply(const uint8_t peer[6], bool accept, uint32_t passkey)
{
    esp_bd_addr_t addr;
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "LE passkey reply failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "LE passkey reply failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    esp_bt_hub_le_addr_swap_if_need(addr, peer);
    err = esp_ble_passkey_reply(addr, accept, passkey);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LE passkey reply failed: %s", esp_err_to_name(err));
    }
    return err;
}

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static esp_err_t le_connect(const esp_bt_hub_le_connect_params_t *params)
{
    esp_bd_addr_t remote_addr = {0};
    esp_ble_gap_conn_params_t conn_params = {0};
    uint8_t phy_mask;
    esp_err_t ret;

    if (!params || !params->conn_params ||
        (params->phy_mask & ~(ESP_BT_HUB_LE_PHY_1M_MASK |
                              ESP_BT_HUB_LE_PHY_2M_MASK |
                              ESP_BT_HUB_LE_PHY_CODED_MASK))) {
        ESP_LOGE(TAG, "Connect failed: params, connection parameters, or PHY mask is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (params->gattc_if == ESP_GATT_IF_NONE) {
        ESP_LOGE(TAG, "Connect failed: GATTC interface is none");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Connect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }

    conn_params.scan_interval = params->conn_params->scan_itvl;
    conn_params.scan_window = params->conn_params->scan_window;
    conn_params.interval_min = params->conn_params->itvl_min;
    conn_params.interval_max = params->conn_params->itvl_max;
    conn_params.latency = params->conn_params->latency;
    conn_params.supervision_timeout = params->conn_params->supervision_timeout;
    conn_params.min_ce_len = params->conn_params->min_ce_len;
    conn_params.max_ce_len = params->conn_params->max_ce_len;
    esp_bt_hub_le_addr_swap_if_need(remote_addr, params->peer);
    phy_mask = params->phy_mask ? params->phy_mask : ESP_BT_HUB_LE_PHY_1M_MASK;

    ret = esp_ble_gap_prefer_ext_connect_params_set(
        remote_addr, phy_mask,
        (phy_mask & ESP_BT_HUB_LE_PHY_1M_MASK) ? &conn_params : NULL,
        (phy_mask & ESP_BT_HUB_LE_PHY_2M_MASK) ? &conn_params : NULL,
        (phy_mask & ESP_BT_HUB_LE_PHY_CODED_MASK) ? &conn_params : NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set preferred ext connect params failed: %s", esp_err_to_name(ret));
        return ret;
    }
    taskENTER_CRITICAL(&s_state_lock);
    if (s_connect_pending) {
        taskEXIT_CRITICAL(&s_state_lock);
        ESP_LOGE(TAG, "Connect failed: another connection attempt is pending");
        return ESP_ERR_INVALID_STATE;
    }
    s_connect_gattc_if = params->gattc_if;
    memcpy(s_connect_peer, remote_addr, sizeof(s_connect_peer));
    s_connect_pending = true;
    taskEXIT_CRITICAL(&s_state_lock);
    ret = esp_ble_gattc_aux_open((esp_gatt_if_t)params->gattc_if, remote_addr,
                                 (esp_ble_addr_type_t)params->peer_addr_type, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Open GATTC aux connection failed: %s", esp_err_to_name(ret));
        taskENTER_CRITICAL(&s_state_lock);
        le_connect_complete_locked(remote_addr);
        taskEXIT_CRITICAL(&s_state_lock);
        return ret;
    }
    return ESP_OK;
}

static esp_err_t le_connect_cancel(void)
{
    esp_ble_gattc_cancel_open_params_t params = {0};
    bool pending;
    esp_err_t ret;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Cancel connect failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    taskENTER_CRITICAL(&s_state_lock);
    pending = s_connect_pending;
    params.gattc_if = (esp_gatt_if_t)s_connect_gattc_if;
    memcpy(params.remote_bda, s_connect_peer, sizeof(params.remote_bda));
    taskEXIT_CRITICAL(&s_state_lock);
    if (!pending) {
        return ESP_OK;
    }
    ret = esp_ble_gattc_cancel_open(&params);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Cancel pending connect failed: %s", esp_err_to_name(ret));
        return ret;
    }
    taskENTER_CRITICAL(&s_state_lock);
    if (s_connect_gattc_if == (uint8_t)params.gattc_if &&
        memcmp(s_connect_peer, params.remote_bda, sizeof(s_connect_peer)) == 0) {
        le_connect_complete_locked(params.remote_bda);
    }
    taskEXIT_CRITICAL(&s_state_lock);
    return ESP_OK;
}

esp_err_t esp_bt_hub_le_connect_complete(const uint8_t peer[6])
{
    uint8_t remote_addr[6];
    esp_err_t err;

    if (!peer) {
        ESP_LOGE(TAG, "Complete LE connect failed: peer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_bt_hub_le_addr_swap_if_need(remote_addr, peer);
    err = esp_bt_hub_context_enter();
    if (err != ESP_OK) {
        return err;
    }
    taskENTER_CRITICAL(&s_state_lock);
    le_connect_complete_locked(remote_addr);
    taskEXIT_CRITICAL(&s_state_lock);
    esp_bt_hub_context_leave();
    return ESP_OK;
}

static esp_err_t bdroid_le_ext_adv_configure(uint8_t instance, const esp_bt_hub_le_ext_adv_params_t *params)
{
    esp_ble_gap_ext_adv_params_t ep = {0};
    uint8_t props = 0;
    esp_err_t err;

    if (!params) {
        ESP_LOGE(TAG, "Set ext adv params failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext adv params failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    if (params->connectable) {
        props |= ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
    }
    if (params->scannable) {
        props |= ESP_BLE_GAP_SET_EXT_ADV_PROP_SCANNABLE;
    }
    if (params->legacy_pdu) {
        props |= ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY;
    }
    ep.type = props;
    ep.own_addr_type = params->own_addr_type;
    ep.primary_phy = params->primary_phy;
    ep.secondary_phy = params->secondary_phy;
    ep.tx_power = params->tx_power;
    ep.sid = params->sid;
    ep.interval_min = params->itvl_min;
    ep.interval_max = params->itvl_max;
    ep.channel_map = 0x07;
    err = gap_op_begin(ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_ext_adv_set_params(instance, &ep));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set ext adv params failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_ext_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set ext adv data failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext adv data failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_config_ext_adv_data_raw(instance, (uint16_t)len, (uint8_t *)data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set ext adv data failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_ext_adv_set_scan_rsp(uint8_t instance, const uint8_t *data, size_t len)
{
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set ext scan response failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set ext scan response failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT);
    if (err != ESP_OK) {
        return err;
    }
    err = gap_op_end(esp_ble_gap_config_ext_scan_rsp_data_raw(instance, (uint16_t)len, (uint8_t *)data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set ext scan response failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_ext_adv_start(uint8_t instance, uint32_t generation, int duration, int max_events)
{
    esp_ble_gap_ext_adv_t adv = {
        .instance = instance,
        .duration = duration,
        .max_events = max_events,
    };
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Start ext adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    ext_adv_generation_set(instance, generation);
    err = gap_op_begin(ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT);
    if (err != ESP_OK) {
        ext_adv_generation_take(instance);
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_ext_adv_start(1, &adv));
    if (err != ESP_OK) {
        ext_adv_generation_take(instance);
        ESP_LOGE(TAG, "Start ext adv failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_ext_adv_stop(uint8_t instance)
{
    uint8_t handle = instance;
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Stop ext adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_ext_adv_stop(1, &handle));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop ext adv failed: instance %u, %s", instance, esp_err_to_name(err));
    } else {
        ext_adv_generation_take(instance);
    }
    return err;
}

static esp_err_t bdroid_le_ext_adv_remove(uint8_t instance)
{
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Remove ext adv set failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_ext_adv_set_remove(instance));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Remove ext adv set failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static inline uint16_t scan_duration_10ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return 0;
    }
    return (uint16_t)((timeout_ms / ESP_BT_HUB_SCAN_UNIT_MS) +
                      ((timeout_ms % ESP_BT_HUB_SCAN_UNIT_MS) ? 1 : 0));
}

static esp_err_t bdroid_le_ext_scan_start(const esp_bt_hub_le_ext_scan_params_t *params)
{
    esp_ble_ext_scan_params_t scan = {0};
    uint8_t phy_mask;
    uint16_t duration;
    esp_err_t err;

    if (params->duration_ms > ESP_BT_HUB_EXT_SCAN_MAX_MS) {
        ESP_LOGE(TAG, "Start ext scan failed: duration cannot be represented by Bluedroid");
        return ESP_ERR_INVALID_ARG;
    }
    duration = scan_duration_10ms(params->duration_ms);
    scan.own_addr_type = (esp_ble_addr_type_t)params->own_addr_type;
    scan.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
    scan.scan_duplicate = params->filter_duplicates ? BLE_SCAN_DUPLICATE_ENABLE : BLE_SCAN_DUPLICATE_DISABLE;
    phy_mask = params->phy_mask ? params->phy_mask : ESP_BT_HUB_LE_PHY_1M_MASK;
    if (phy_mask & ESP_BT_HUB_LE_PHY_1M_MASK) {
        scan.cfg_mask |= ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
        scan.uncoded_cfg.scan_type = params->passive ? BLE_SCAN_TYPE_PASSIVE : BLE_SCAN_TYPE_ACTIVE;
        scan.uncoded_cfg.scan_interval = params->interval;
        scan.uncoded_cfg.scan_window = params->window;
    }
    if (phy_mask & ESP_BT_HUB_LE_PHY_CODED_MASK) {
        scan.cfg_mask |= ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK;
        scan.coded_cfg.scan_type = params->passive ? BLE_SCAN_TYPE_PASSIVE : BLE_SCAN_TYPE_ACTIVE;
        scan.coded_cfg.scan_interval = params->interval;
        scan.coded_cfg.scan_window = params->window;
    }

    err = gap_op_begin(ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_set_ext_scan_params(&scan));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set ext scan params failed: %s", esp_err_to_name(err));
        return err;
    }
    err = gap_op_begin(ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_start_ext_scan(duration, 0));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start ext scan failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_ext_scan_stop(void)
{
    esp_err_t err;

    /* Stopping an already idle scan still succeeds: BTM reports the HCI
     * command-send status, not whether scanning was running. */
    err = gap_op_begin(ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_stop_ext_scan());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop ext scan failed: %s", esp_err_to_name(err));
    }
    return err;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if CONFIG_BT_BLE_50_FEATURES_SUPPORTED
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
static esp_err_t bdroid_le_periodic_adv_configure(uint8_t instance,
                                                  const esp_bt_hub_le_periodic_adv_params_t *params)
{
    esp_ble_gap_periodic_adv_params_t pp = {0};
    esp_err_t err;

    if (!params) {
        ESP_LOGE(TAG, "Set periodic adv params failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set periodic adv params failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    pp.interval_min = params->itvl_min;
    pp.interval_max = params->itvl_max;
    pp.properties = params->include_tx_power ? 0x40 : 0;
    err = gap_op_begin(ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_periodic_adv_set_params(instance, &pp));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set periodic adv params failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_periodic_adv_set_data(uint8_t instance, const uint8_t *data, size_t len)
{
    esp_err_t err;

    if ((len != 0 && !data) || len > UINT16_MAX) {
        ESP_LOGE(TAG, "Set periodic adv data failed: data or len is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Set periodic adv data failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
#if CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH
    err = gap_op_end(esp_ble_gap_config_periodic_adv_data_raw(instance, (uint16_t)len,
                                                              (uint8_t *)data, false));
#else
    err = gap_op_end(esp_ble_gap_config_periodic_adv_data_raw(instance, (uint16_t)len, (uint8_t *)data));
#endif  /* CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set periodic adv data failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_periodic_adv_start(uint8_t instance)
{
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Start periodic adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
#if CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH
    err = gap_op_end(esp_ble_gap_periodic_adv_start(instance, false));
#else
    err = gap_op_end(esp_ble_gap_periodic_adv_start(instance));
#endif  /* CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start periodic adv failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t bdroid_le_periodic_adv_stop(uint8_t instance)
{
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Stop periodic adv failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = gap_op_begin(ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_periodic_adv_stop(instance));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Stop periodic adv failed: instance %u, %s", instance, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_pa_sync_create(uint8_t addr_type, const uint8_t addr[6], uint8_t sid,
                                   const esp_bt_hub_le_periodic_sync_params_t *params)
{
    esp_ble_gap_periodic_adv_sync_params_t sp = {0};
    esp_err_t err;

    if (!addr || !params) {
        ESP_LOGE(TAG, "Create PA sync failed: addr or params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Create PA sync failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    sp.filter_policy = 0;
    sp.sid = sid;
    sp.addr_type = addr_type;
    esp_bt_hub_le_addr_swap_if_need(sp.addr, addr);
    sp.skip = params->skip;
    sp.sync_timeout = params->sync_timeout;
    err = esp_ble_gap_periodic_adv_create_sync(&sp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Create periodic adv sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_pa_sync_terminate(uint16_t sync_handle)
{
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Terminate PA sync failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = esp_ble_gap_periodic_adv_sync_terminate(sync_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Terminate PA sync failed: sync_handle %u, %s", sync_handle, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t le_pa_sync_create_cancel(void)
{
    esp_err_t err;

    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "Cancel PA sync create failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = esp_ble_gap_periodic_adv_sync_cancel();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Cancel PA sync create failed: %s", esp_err_to_name(err));
    }
    return err;
}

#if CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER
static esp_err_t le_pa_sync_receive(uint16_t conn_handle, const esp_bt_hub_le_periodic_sync_params_t *params)
{
    esp_bd_addr_t addr;
    esp_ble_gap_past_params_t past = {
        .mode = ESP_BLE_GAP_PAST_MODE_DUP_FILTER_DISABLED,
        .skip = params ? params->skip : 0,
        .sync_timeout = params ? params->sync_timeout : 1000,
        .cte_type = 0,
    };
    esp_err_t err;

    if (!params) {
        ESP_LOGE(TAG, "PAST receive failed: params is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!esp_bt_hub_is_running()) {
        ESP_LOGE(TAG, "PAST receive failed: hub is not running");
        return ESP_ERR_INVALID_STATE;
    }
    err = le_peer_addr_from_handle(conn_handle, addr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PAST receive failed: no ACL connection for conn_handle %u", conn_handle);
        return err;
    }
    err = gap_op_begin(ESP_GAP_BLE_SET_PAST_PARAMS_COMPLETE_EVT);
    if (err != ESP_OK) {
        /* helper already logged */
        return err;
    }
    err = gap_op_end(esp_ble_gap_set_periodic_adv_sync_trans_params(addr, &past));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set PAST receive params failed: conn_handle %u, %s", conn_handle, esp_err_to_name(err));
    }
    return err;
}

#endif  /* CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER */

#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* CONFIG_BT_BLE_50_FEATURES_SUPPORTED */
#endif  /* CONFIG_BT_BLE_ENABLED */

const esp_bt_hub_backend_ops_t esp_bt_hub_bluedroid_ops = {
    .init             = bdroid_init,
    .start            = bdroid_start,
    .stop             = bdroid_stop,
    .deinit           = bdroid_deinit,
    .teardown_pending = bdroid_teardown_pending,
    .set_device_name  = bdroid_set_device_name,
    .get_address      = bdroid_get_address,
#if ESP_BT_HUB_LE_SUPPORTED
    .le_bond_count        = le_bond_count,
    .le_bond_exists       = le_bond_exists,
    .le_disconnect        = le_disconnect,
    .le_security_initiate = le_security_initiate,
    .le_privacy_enable    = le_privacy_enable,
    .le_security_reply    = le_security_reply,
    .le_confirm_reply     = le_confirm_reply,
    .le_passkey_reply     = le_passkey_reply,
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    .le_connect        = le_connect,
    .le_connect_cancel = le_connect_cancel,
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    .le_pa_sync_create        = le_pa_sync_create,
    .le_pa_sync_terminate     = le_pa_sync_terminate,
    .le_pa_sync_create_cancel = le_pa_sync_create_cancel,
#if ESP_BT_HUB_LE_PAST_SUPPORTED
    .le_pa_sync_receive = le_pa_sync_receive,
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_SUPPORTED */
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    .classic_scan_mode       = bdroid_classic_scan_mode,
    .classic_discovery_start = bdroid_classic_discovery_start,
    .classic_discovery_stop  = bdroid_classic_discovery_stop,
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
    .le_scan_start = bdroid_le_scan_start,
    .le_scan_stop  = bdroid_le_scan_stop,
    .le_adv_start  = bdroid_le_adv_start,
    .le_adv_stop   = bdroid_le_adv_stop,
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    .le_ext_adv_configure    = bdroid_le_ext_adv_configure,
    .le_ext_adv_set_data     = bdroid_le_ext_adv_set_data,
    .le_ext_adv_set_scan_rsp = bdroid_le_ext_adv_set_scan_rsp,
    .le_ext_adv_remove       = bdroid_le_ext_adv_remove,
    .le_ext_adv_start        = bdroid_le_ext_adv_start,
    .le_ext_adv_stop         = bdroid_le_ext_adv_stop,
    .le_ext_scan_start       = bdroid_le_ext_scan_start,
    .le_ext_scan_stop        = bdroid_le_ext_scan_stop,
#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
    .le_periodic_adv_configure = bdroid_le_periodic_adv_configure,
    .le_periodic_adv_set_data  = bdroid_le_periodic_adv_set_data,
    .le_periodic_adv_start     = bdroid_le_periodic_adv_start,
    .le_periodic_adv_stop      = bdroid_le_periodic_adv_stop,
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
};

const esp_bt_hub_backend_ops_t *esp_bt_hub_backend_default_get(void)
{
    return &esp_bt_hub_bluedroid_ops;
}
