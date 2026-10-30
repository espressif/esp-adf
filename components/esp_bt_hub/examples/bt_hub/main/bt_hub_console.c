/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt_hub.h"
#include "esp_bt_hub_classic.h"
#include "esp_bt_hub_le.h"
#include "esp_console.h"
#include "esp_log.h"

#include "bt_hub_console.h"

#define EXAMPLE_MAX_CLIENTS          4
#define EXAMPLE_CLIENT_NAME_LEN      24
#define EXAMPLE_INVALID_CLIENT_ID    (-1)
#define EXAMPLE_CLASSIC_INQUIRY_LEN  10
#define EXAMPLE_LE_SCAN_INTERVAL     0x50
#define EXAMPLE_LE_SCAN_WINDOW       0x30
#define EXAMPLE_LE_ADV_INTERVAL      0x100
#define EXAMPLE_LE_PUBLIC_ADDR_TYPE  0
#define EXAMPLE_LE_PHY_1M            1
#define EXAMPLE_GAP_PARSER_SUB       0
#define EXAMPLE_GAP_OBSERVER_SUB     1
#define EXAMPLE_GAP_SUB_COUNT        2
#define EXAMPLE_ARRAY_SIZE(array)    (sizeof(array) / sizeof((array)[0]))

#define EXAMPLE_ADDR_FMT       "%02x:%02x:%02x:%02x:%02x:%02x"
#define EXAMPLE_ADDR(addr)     (addr)[0], (addr)[1], (addr)[2], (addr)[3], (addr)[4], (addr)[5]
#define EXAMPLE_LE_ADDR(addr)  (addr)[5], (addr)[4], (addr)[3], (addr)[2], (addr)[1], (addr)[0]

/**
 * @brief  One named Bluetooth hub client managed by the console.
 */
typedef struct {
    esp_bt_hub_client_handle_t  handle;                         /*!< Hub client handle */
    char                        name[EXAMPLE_CLIENT_NAME_LEN];  /*!< Client name */
} example_client_t;

/**
 * @brief  Interactive example state.
 */
typedef struct {
    example_client_t                clients[EXAMPLE_MAX_CLIENTS];        /*!< Named clients */
    esp_bt_hub_gap_sub_handle_t     classic_sub[EXAMPLE_GAP_SUB_COUNT];  /*!< Classic GAP subscriptions */
    esp_bt_hub_gap_sub_handle_t     le_sub[EXAMPLE_GAP_SUB_COUNT];       /*!< LE GAP subscriptions */
    esp_bt_hub_discovery_handle_t   discovery;                           /*!< Classic discovery lease */
    esp_bt_hub_le_scan_handle_t     le_scan;                             /*!< LE scan lease */
    esp_bt_hub_le_ext_adv_handle_t  le_adv;                              /*!< LE advertising instance */
    atomic_uint_least32_t           classic_parser_events;               /*!< Classic parser callback count */
    atomic_uint_least32_t           classic_observer_events;             /*!< Classic observer callback count */
    atomic_uint_least32_t           le_parser_events;                    /*!< LE parser callback count */
    atomic_uint_least32_t           le_observer_events;                  /*!< LE observer callback count */
    int                             discovery_owner;                     /*!< Classic discovery owner index */
    int                             le_scan_owner;                       /*!< LE scan owner index */
    int                             le_adv_owner;                        /*!< LE advertising owner index */
    esp_console_repl_t             *repl;                                /*!< Console REPL handle */
} example_context_t;

static const char *TAG = "BT_HUB_EXAMPLE";
#if ESP_BT_HUB_LE_EXT_SUPPORTED
static const uint8_t s_le_adv_data[] = {
    0x02, 0x01, 0x06, 0x05, 0xFF, 0x47, 0x4D, 0x46, 0x31,
};
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

static const char *hub_state_name(esp_bt_hub_state_t state)
{
    switch (state) {
        case ESP_BT_HUB_STATE_STOPPED:
            return "STOPPED";
        case ESP_BT_HUB_STATE_INITIALIZING:
            return "INITIALIZING";
        case ESP_BT_HUB_STATE_INITIALIZED:
            return "INITIALIZED";
        case ESP_BT_HUB_STATE_STARTING:
            return "STARTING";
        case ESP_BT_HUB_STATE_RUNNING:
            return "RUNNING";
        case ESP_BT_HUB_STATE_STOPPING:
            return "STOPPING";
        case ESP_BT_HUB_STATE_DEINITIALIZING:
            return "DEINITIALIZING";
        case ESP_BT_HUB_STATE_FAULT:
            return "FAULT";
        default:
            return "UNKNOWN";
    }
}

static void print_operation_result(const char *operation, esp_err_t err)
{
    if (err == ESP_OK) {
        printf("%s: success\n", operation);
    } else {
        printf("%s failed: %s\n", operation, esp_err_to_name(err));
    }
}

static bool parse_client_id(const char *text, int *out_id)
{
    char *end = NULL;

    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 0 || value >= EXAMPLE_MAX_CLIENTS) {
        return false;
    }
    *out_id = (int)value;
    return true;
}

static example_client_t *get_open_client(example_context_t *ctx, const char *text, int *out_id)
{
    int id;

    if (!parse_client_id(text, &id) || ctx->clients[id].handle == NULL) {
        return NULL;
    }
    if (out_id != NULL) {
        *out_id = id;
    }
    return &ctx->clients[id];
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED || ESP_BT_HUB_LE_EXT_SUPPORTED
static bool validate_owner_id(const char *text, int expected_id, const char *operation)
{
    int id;

    if (!parse_client_id(text, &id)) {
        printf("%s failed: invalid client id\n", operation);
        return false;
    }
    if (id != expected_id) {
        printf("%s failed: client %d does not own the active resource\n", operation, id);
        return false;
    }
    return true;
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED || ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static void classic_gap_cb(uint32_t event, void *param, void *user_ctx)
{
    example_context_t *ctx = user_ctx;
    const esp_bt_hub_gap_classic_param_t *gap = param;

    atomic_fetch_add_explicit(&ctx->classic_parser_events, 1, memory_order_relaxed);
    switch ((esp_bt_gap_cb_event_t)event) {
        case ESP_BT_GAP_DISC_RES_EVT: {
            esp_bt_hub_classic_inquiry_result_t result;
            esp_err_t err = esp_bt_hub_classic_inquiry_result_parse(param, &result);

            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Parse Classic inquiry result failed: %s", esp_err_to_name(err));
                break;
            }
            if (result.has_rssi) {
                ESP_LOGI(TAG, "Classic result " EXAMPLE_ADDR_FMT " name='%s' rssi=%d", EXAMPLE_ADDR(result.addr),
                         result.name[0] == '\0' ? "<unknown>" : result.name, result.rssi);
            } else {
                ESP_LOGI(TAG, "Classic result " EXAMPLE_ADDR_FMT " name='%s' rssi=unavailable",
                         EXAMPLE_ADDR(result.addr), result.name[0] == '\0' ? "<unknown>" : result.name);
            }
            break;
        }
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic inquiry state failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic inquiry state: %d", gap->disc_st_chg.state);
            break;
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic authentication complete failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic authentication complete: " EXAMPLE_ADDR_FMT " status=%d name='%s'",
                     EXAMPLE_ADDR(gap->auth_cmpl.bda), gap->auth_cmpl.stat, (const char *)gap->auth_cmpl.device_name);
            break;
        case ESP_BT_GAP_PIN_REQ_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic PIN request failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic PIN request: " EXAMPLE_ADDR_FMT " min_16_digit=%d", EXAMPLE_ADDR(gap->pin_req.bda),
                     gap->pin_req.min_16_digit);
            break;
        case ESP_BT_GAP_CFM_REQ_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic numeric comparison failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic numeric comparison: " EXAMPLE_ADDR_FMT " value=%lu", EXAMPLE_ADDR(gap->cfm_req.bda),
                     (unsigned long)gap->cfm_req.num_val);
            break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic passkey notification failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic passkey notification: " EXAMPLE_ADDR_FMT " passkey=%lu",
                     EXAMPLE_ADDR(gap->key_notif.bda), (unsigned long)gap->key_notif.passkey);
            break;
        case ESP_BT_GAP_KEY_REQ_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic passkey request failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic passkey request: " EXAMPLE_ADDR_FMT, EXAMPLE_ADDR(gap->key_req.bda));
            break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic remote name failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic remote name: " EXAMPLE_ADDR_FMT " status=%d name='%s'",
                     EXAMPLE_ADDR(gap->read_rmt_name.bda), gap->read_rmt_name.stat,
                     (const char *)gap->read_rmt_name.rmt_name);
            break;
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic ACL connected failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic ACL connected: " EXAMPLE_ADDR_FMT " status=%d handle=%u",
                     EXAMPLE_ADDR(gap->acl_conn_cmpl_stat.bda), gap->acl_conn_cmpl_stat.stat,
                     gap->acl_conn_cmpl_stat.handle);
            break;
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic ACL disconnected failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic ACL disconnected: " EXAMPLE_ADDR_FMT " reason=%d handle=%u",
                     EXAMPLE_ADDR(gap->acl_disconn_cmpl_stat.bda), gap->acl_disconn_cmpl_stat.reason,
                     gap->acl_disconn_cmpl_stat.handle);
            break;
        case ESP_BT_GAP_ENC_CHG_EVT:
            if (gap == NULL) {
                ESP_LOGW(TAG, "Classic encryption change failed: parameter is NULL");
                break;
            }
            ESP_LOGI(TAG, "Classic encryption changed: " EXAMPLE_ADDR_FMT " mode=%u", EXAMPLE_ADDR(gap->enc_chg.bda),
                     gap->enc_chg.enc_mode);
            break;
        default:
            ESP_LOGD(TAG, "Classic event=%lu", (unsigned long)event);
            break;
    }
}

static void classic_gap_observer_cb(uint32_t event, void *param, void *user_ctx)
{
    example_context_t *ctx = user_ctx;
    const esp_bt_hub_gap_classic_param_t *gap = param;

    atomic_fetch_add_explicit(&ctx->classic_observer_events, 1, memory_order_relaxed);
    switch ((esp_bt_gap_cb_event_t)event) {
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            ESP_LOGI(TAG, "Classic observer inquiry state: %d", gap == NULL ? -1 : gap->disc_st_chg.state);
            break;
        case ESP_BT_GAP_AUTH_CMPL_EVT:
            ESP_LOGI(TAG, "Classic observer authentication complete status=%d", gap == NULL ? -1 : gap->auth_cmpl.stat);
            break;
        case ESP_BT_GAP_PIN_REQ_EVT:
            ESP_LOGI(TAG, "Classic observer PIN request");
            break;
        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(TAG, "Classic observer numeric comparison value=%lu",
                     gap == NULL ? 0UL : (unsigned long)gap->cfm_req.num_val);
            break;
        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "Classic observer passkey=%lu", gap == NULL ? 0UL : (unsigned long)gap->key_notif.passkey);
            break;
        case ESP_BT_GAP_KEY_REQ_EVT:
            ESP_LOGI(TAG, "Classic observer passkey request");
            break;
        case ESP_BT_GAP_READ_REMOTE_NAME_EVT:
            ESP_LOGI(TAG, "Classic observer remote name status=%d", gap == NULL ? -1 : gap->read_rmt_name.stat);
            break;
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            ESP_LOGI(TAG, "Classic observer ACL connected handle=%u", gap == NULL ? 0 : gap->acl_conn_cmpl_stat.handle);
            break;
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            ESP_LOGI(TAG, "Classic observer ACL disconnected handle=%u reason=%d",
                     gap == NULL ? 0 : gap->acl_disconn_cmpl_stat.handle,
                     gap == NULL ? -1 : gap->acl_disconn_cmpl_stat.reason);
            break;
        case ESP_BT_GAP_ENC_CHG_EVT:
            ESP_LOGI(TAG, "Classic observer encryption mode=%u", gap == NULL ? 0 : gap->enc_chg.enc_mode);
            break;
        default:
            ESP_LOGD(TAG, "Classic observer event=%lu", (unsigned long)event);
            break;
    }
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static void le_gap_cb(uint32_t event, void *param, void *user_ctx)
{
    example_context_t *ctx = user_ctx;
    esp_bt_hub_le_adv_report_t report;
    bool is_adv_report = false;

    atomic_fetch_add_explicit(&ctx->le_parser_events, 1, memory_order_relaxed);
#if CONFIG_BT_NIMBLE_ENABLED
    const struct ble_gap_event *native = param;

    if (native == NULL) {
        ESP_LOGW(TAG, "Handle NimBLE event failed: parameter is NULL");
        return;
    }
    switch (event) {
        case BLE_GAP_EVENT_EXT_DISC:
            ESP_LOGI(TAG, "NimBLE native event=%lu EXT_DISC addr_type=%u sid=%u rssi=%d len=%u",
                     (unsigned long)event, (unsigned)native->ext_disc.addr.type, (unsigned)native->ext_disc.sid,
                     native->ext_disc.rssi, (unsigned)native->ext_disc.length_data);
            is_adv_report = true;
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "NimBLE native event=%lu DISC_COMPLETE reason=%d",
                     (unsigned long)event, native->disc_complete.reason);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "NimBLE native event=%lu ADV_COMPLETE reason=%d instance=%u",
                     (unsigned long)event, native->adv_complete.reason, (unsigned)native->adv_complete.instance);
            break;
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "NimBLE connect status=%d handle=%u", native->connect.status, native->connect.conn_handle);
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "NimBLE disconnect reason=%d handle=%u addr=" EXAMPLE_ADDR_FMT, native->disconnect.reason,
                     native->disconnect.conn.conn_handle, EXAMPLE_LE_ADDR(native->disconnect.conn.peer_ota_addr.val));
            break;
        case BLE_GAP_EVENT_LINK_ESTAB:
            ESP_LOGI(TAG, "NimBLE link established status=%d handle=%u", native->link_estab.status,
                     native->link_estab.conn_handle);
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "NimBLE connection update status=%d handle=%u", native->conn_update.status,
                     native->conn_update.conn_handle);
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "NimBLE encryption change status=%d handle=%u", native->enc_change.status,
                     native->enc_change.conn_handle);
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG, "NimBLE passkey action=%u value=%lu handle=%u", native->passkey.params.action,
                     (unsigned long)native->passkey.params.numcmp, native->passkey.conn_handle);
            break;
        case BLE_GAP_EVENT_PAIRING_COMPLETE:
            ESP_LOGI(TAG, "NimBLE pairing complete status=%d handle=%u", native->pairing_complete.status,
                     native->pairing_complete.conn_handle);
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "NimBLE MTU handle=%u channel=%u value=%u", native->mtu.conn_handle, native->mtu.channel_id,
                     native->mtu.value);
            break;
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
            ESP_LOGI(TAG, "NimBLE PHY update status=%d handle=%u tx=%u rx=%u", native->phy_updated.status,
                     native->phy_updated.conn_handle, native->phy_updated.tx_phy, native->phy_updated.rx_phy);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "NimBLE subscribe handle=%u attr=%u reason=%u notify=%u indicate=%u",
                     native->subscribe.conn_handle, native->subscribe.attr_handle, native->subscribe.reason,
                     native->subscribe.cur_notify, native->subscribe.cur_indicate);
            break;
        default:
            ESP_LOGD(TAG, "NimBLE native event=%lu", (unsigned long)event);
            break;
    }
#elif CONFIG_BT_BLUEDROID_ENABLED
    const esp_ble_gap_cb_param_t *native = param;

    if (native == NULL) {
        ESP_LOGW(TAG, "Handle Bluedroid event failed: parameter is NULL");
        return;
    }
    switch ((esp_gap_ble_cb_event_t)event) {
        case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
            ESP_LOGI(TAG, "Bluedroid native event=%lu EXT_ADV_REPORT addr_type=%u sid=%u rssi=%d len=%u",
                     (unsigned long)event, (unsigned)native->ext_adv_report.params.addr_type,
                     (unsigned)native->ext_adv_report.params.sid, native->ext_adv_report.params.rssi,
                     (unsigned)native->ext_adv_report.params.adv_data_len);
            is_adv_report = true;
            break;
        case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid native event=%lu EXT_SCAN_START status=%d",
                     (unsigned long)event, native->ext_scan_start.status);
            break;
        case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid native event=%lu EXT_SCAN_STOP status=%d",
                     (unsigned long)event, native->ext_scan_stop.status);
            break;
        case ESP_GAP_BLE_ADV_TERMINATED_EVT:
            ESP_LOGI(TAG, "Bluedroid native event=%lu ADV_TERMINATED status=%u instance=%u",
                     (unsigned long)event, (unsigned)native->adv_terminate.status,
                     (unsigned)native->adv_terminate.adv_instance);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            ESP_LOGI(TAG, "Bluedroid authentication complete: " EXAMPLE_ADDR_FMT " success=%d reason=%d",
                     EXAMPLE_ADDR(native->ble_security.auth_cmpl.bd_addr), native->ble_security.auth_cmpl.success,
                     native->ble_security.auth_cmpl.fail_reason);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "Bluedroid security request: " EXAMPLE_ADDR_FMT,
                     EXAMPLE_ADDR(native->ble_security.ble_req.bd_addr));
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGI(TAG, "Bluedroid passkey notification: " EXAMPLE_ADDR_FMT " passkey=%lu",
                     EXAMPLE_ADDR(native->ble_security.key_notif.bd_addr),
                     (unsigned long)native->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            ESP_LOGI(TAG, "Bluedroid passkey request: " EXAMPLE_ADDR_FMT,
                     EXAMPLE_ADDR(native->ble_security.ble_req.bd_addr));
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            ESP_LOGI(TAG, "Bluedroid numeric comparison: " EXAMPLE_ADDR_FMT " value=%lu",
                     EXAMPLE_ADDR(native->ble_security.key_notif.bd_addr),
                     (unsigned long)native->ble_security.key_notif.passkey);
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "Bluedroid connection update: " EXAMPLE_ADDR_FMT
                          " status=%d interval=%u latency=%u timeout=%u",
                     EXAMPLE_ADDR(native->update_conn_params.bda), native->update_conn_params.status,
                     native->update_conn_params.conn_int, native->update_conn_params.latency,
                     native->update_conn_params.timeout);
            break;
        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid packet length status=%d rx=%u tx=%u", native->pkt_data_length_cmpl.status,
                     native->pkt_data_length_cmpl.params.rx_len, native->pkt_data_length_cmpl.params.tx_len);
            break;
        case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid PHY update: " EXAMPLE_ADDR_FMT " status=%d tx=%d rx=%d",
                     EXAMPLE_ADDR(native->phy_update.bda), native->phy_update.status, native->phy_update.tx_phy,
                     native->phy_update.rx_phy);
            break;
#if CONFIG_BT_BLE_50_EXTEND_ADV_EN
        case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid extended advertising start status=%d instances=%u", native->ext_adv_start.status,
                     native->ext_adv_start.instance_num);
            break;
        case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Bluedroid extended advertising stop status=%d instances=%u", native->ext_adv_stop.status,
                     native->ext_adv_stop.instance_num);
            break;
#endif  /* CONFIG_BT_BLE_50_EXTEND_ADV_EN */
        case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
            ESP_LOGI(TAG, "Bluedroid scan timeout");
            break;
        default:
            ESP_LOGD(TAG, "Bluedroid native event=%lu", (unsigned long)event);
            break;
    }
#endif  /* CONFIG_BT_NIMBLE_ENABLED */

    if (!is_adv_report) {
        return;
    }
    esp_err_t err = esp_bt_hub_le_adv_report_parse((const esp_bt_hub_gap_ble_param_t *)param, &report);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Parse LE advertising report failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "LE result %02x:%02x:%02x:%02x:%02x:%02x type=%u sid=%u rssi=%ld len=%u", report.addr[5],
             report.addr[4], report.addr[3], report.addr[2], report.addr[1], report.addr[0], report.addr_type,
             report.sid, (long)report.rssi, (unsigned)report.data_len);
}

static void le_gap_observer_cb(uint32_t event, void *param, void *user_ctx)
{
    example_context_t *ctx = user_ctx;

    atomic_fetch_add_explicit(&ctx->le_observer_events, 1, memory_order_relaxed);
#if CONFIG_BT_NIMBLE_ENABLED
    switch (event) {
        case BLE_GAP_EVENT_EXT_DISC:
            ESP_LOGI(TAG, "LE observer: extended discovery");
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ESP_LOGI(TAG, "LE observer: discovery complete");
            break;
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "LE observer: connect");
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "LE observer: disconnect");
            break;
        case BLE_GAP_EVENT_LINK_ESTAB:
            ESP_LOGI(TAG, "LE observer: link established");
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "LE observer: connection update");
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "LE observer: advertising complete");
            break;
        case BLE_GAP_EVENT_ENC_CHANGE:
            ESP_LOGI(TAG, "LE observer: encryption change");
            break;
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            ESP_LOGI(TAG, "LE observer: passkey action");
            break;
        case BLE_GAP_EVENT_PAIRING_COMPLETE:
            ESP_LOGI(TAG, "LE observer: pairing complete");
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "LE observer: MTU");
            break;
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
            ESP_LOGI(TAG, "LE observer: PHY update");
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "LE observer: subscribe");
            break;
        default:
            ESP_LOGD(TAG, "LE observer event=%lu", (unsigned long)event);
            break;
    }
#elif CONFIG_BT_BLUEDROID_ENABLED
    switch ((esp_gap_ble_cb_event_t)event) {
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            ESP_LOGI(TAG, "LE observer: authentication complete");
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            ESP_LOGI(TAG, "LE observer: security request");
            break;
        case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
            ESP_LOGI(TAG, "LE observer: passkey notification");
            break;
        case ESP_GAP_BLE_PASSKEY_REQ_EVT:
            ESP_LOGI(TAG, "LE observer: passkey request");
            break;
        case ESP_GAP_BLE_NC_REQ_EVT:
            ESP_LOGI(TAG, "LE observer: numeric comparison");
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "LE observer: connection update");
            break;
        case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: packet length");
            break;
        case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: PHY update");
            break;
        case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: extended scan start");
            break;
        case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: extended scan stop");
            break;
        case ESP_GAP_BLE_ADV_TERMINATED_EVT:
            ESP_LOGI(TAG, "LE observer: advertising terminated");
            break;
#if CONFIG_BT_BLE_50_EXTEND_ADV_EN
        case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: extended advertising start");
            break;
        case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "LE observer: extended advertising stop");
            break;
#endif  /* CONFIG_BT_BLE_50_EXTEND_ADV_EN */
        case ESP_GAP_BLE_SCAN_TIMEOUT_EVT:
            ESP_LOGI(TAG, "LE observer: scan timeout");
            break;
        default:
            ESP_LOGD(TAG, "LE observer event=%lu", (unsigned long)event);
            break;
    }
#endif  /* CONFIG_BT_NIMBLE_ENABLED */
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

static esp_err_t gap_subscriptions_remove(esp_bt_hub_gap_sub_handle_t subscriptions[EXAMPLE_GAP_SUB_COUNT])
{
    for (int i = EXAMPLE_GAP_SUB_COUNT - 1; i >= 0; i--) {
        if (subscriptions[i] != NULL) {
            esp_err_t err = esp_bt_hub_gap_unsubscribe(subscriptions[i]);
            if (err != ESP_OK) {
                return err;
            }
            subscriptions[i] = NULL;
        }
    }
    return ESP_OK;
}

static void hub_start_rollback_deinit(void)
{
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    esp_bt_hub_state_get(&state);
    if (state == ESP_BT_HUB_STATE_FAULT) {
        esp_bt_hub_stop();
        esp_bt_hub_state_get(&state);
    }
    if (state == ESP_BT_HUB_STATE_INITIALIZED) {
        esp_bt_hub_deinit();
    }
}

static esp_err_t hub_start(example_context_t *ctx)
{
    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;
    esp_err_t err;

    esp_bt_hub_state_get(&state);
    if (state != ESP_BT_HUB_STATE_STOPPED) {
        return ESP_ERR_INVALID_STATE;
    }
    config.pairing_auto_reply = true;
    err = esp_bt_hub_init(&config);
    if (err != ESP_OK) {
        return err;
    }
    atomic_store_explicit(&ctx->classic_parser_events, 0, memory_order_relaxed);
    atomic_store_explicit(&ctx->classic_observer_events, 0, memory_order_relaxed);
    atomic_store_explicit(&ctx->le_parser_events, 0, memory_order_relaxed);
    atomic_store_explicit(&ctx->le_observer_events, 0, memory_order_relaxed);

#if ESP_BT_HUB_CLASSIC_SUPPORTED
    err = esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_CLASSIC, classic_gap_cb, ctx,
                                   &ctx->classic_sub[EXAMPLE_GAP_PARSER_SUB]);
    if (err == ESP_OK) {
        err = esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_CLASSIC, classic_gap_observer_cb, ctx,
                                       &ctx->classic_sub[EXAMPLE_GAP_OBSERVER_SUB]);
    }
    if (err != ESP_OK) {
        gap_subscriptions_remove(ctx->classic_sub);
        hub_start_rollback_deinit();
        return err;
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
    err = esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, le_gap_cb, ctx,
                                   &ctx->le_sub[EXAMPLE_GAP_PARSER_SUB]);
    if (err == ESP_OK) {
        err = esp_bt_hub_gap_subscribe(ESP_BT_HUB_GAP_DOMAIN_BLE, le_gap_observer_cb, ctx,
                                       &ctx->le_sub[EXAMPLE_GAP_OBSERVER_SUB]);
    }
    if (err != ESP_OK) {
        gap_subscriptions_remove(ctx->le_sub);
        gap_subscriptions_remove(ctx->classic_sub);
        hub_start_rollback_deinit();
        return err;
    }
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    err = esp_bt_hub_start();
    if (err != ESP_OK) {
        gap_subscriptions_remove(ctx->le_sub);
        gap_subscriptions_remove(ctx->classic_sub);
        hub_start_rollback_deinit();
    }
    return err;
}

static bool context_has_clients(const example_context_t *ctx)
{
    for (int i = 0; i < EXAMPLE_MAX_CLIENTS; i++) {
        if (ctx->clients[i].handle != NULL) {
            return true;
        }
    }
    return false;
}

static esp_err_t hub_stop(example_context_t *ctx)
{
    esp_err_t err;
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    esp_bt_hub_state_get(&state);
    if (state != ESP_BT_HUB_STATE_RUNNING && state != ESP_BT_HUB_STATE_FAULT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->discovery != NULL || ctx->le_scan != NULL || ctx->le_adv != NULL || context_has_clients(ctx)) {
        return ESP_ERR_INVALID_STATE;
    }

    err = esp_bt_hub_stop();
    if (err != ESP_OK) {
        esp_bt_hub_state_t current = ESP_BT_HUB_STATE_STOPPED;

        esp_bt_hub_state_get(&current);
        if (current != ESP_BT_HUB_STATE_INITIALIZED && current != ESP_BT_HUB_STATE_FAULT) {
            return err;
        }
    }
    err = gap_subscriptions_remove(ctx->le_sub);
    if (err != ESP_OK) {
        return err;
    }
    err = gap_subscriptions_remove(ctx->classic_sub);
    if (err != ESP_OK) {
        return err;
    }
    return esp_bt_hub_deinit();
}

static int cmd_hub_status(void *context, int argc, char **argv)
{
#if ESP_BT_HUB_CLASSIC_SUPPORTED || ESP_BT_HUB_LE_EXT_SUPPORTED
    example_context_t *ctx = context;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED || ESP_BT_HUB_LE_EXT_SUPPORTED */

    if (argc != 1) {
        printf("Usage: hub_status\n");
        return 1;
    }
    esp_bt_hub_state_t state = ESP_BT_HUB_STATE_STOPPED;

    esp_bt_hub_state_get(&state);
    printf("Hub state: %s\n", hub_state_name(state));
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    printf("Classic GAP callbacks: parser=%lu observer=%lu\n",
           (unsigned long)atomic_load_explicit(&ctx->classic_parser_events, memory_order_relaxed),
           (unsigned long)atomic_load_explicit(&ctx->classic_observer_events, memory_order_relaxed));
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    printf("LE GAP callbacks: parser=%lu observer=%lu\n",
           (unsigned long)atomic_load_explicit(&ctx->le_parser_events, memory_order_relaxed),
           (unsigned long)atomic_load_explicit(&ctx->le_observer_events, memory_order_relaxed));
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    printf("Classic inquiry: %s", ctx->discovery == NULL ? "idle" : "active");
    if (ctx->discovery != NULL) {
        printf(" owner=%d", ctx->discovery_owner);
    }
    printf("\n");
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    printf("LE scan: %s", ctx->le_scan == NULL ? "idle" : "active");
    if (ctx->le_scan != NULL) {
        printf(" owner=%d", ctx->le_scan_owner);
    }
    printf("\nLE advertising: %s", ctx->le_adv == NULL ? "idle" : "active");
    if (ctx->le_adv != NULL) {
        printf(" owner=%d", ctx->le_adv_owner);
    }
    printf("\n");
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    return 0;
}

static int cmd_hub_start(void *context, int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: hub_start\n");
        return 1;
    }
    esp_err_t err = hub_start(context);

    print_operation_result("Hub start", err);
    return err == ESP_OK ? 0 : 1;
}

static int cmd_hub_stop(void *context, int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: hub_stop\n");
        return 1;
    }
    esp_err_t err = hub_stop(context);

    print_operation_result("Hub stop", err);
    if (err == ESP_ERR_INVALID_STATE) {
        printf("Release all leases and close all clients before stopping the hub\n");
    }
    return err == ESP_OK ? 0 : 1;
}

static int cmd_clients(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    bool found = false;

    if (argc != 1) {
        printf("Usage: clients\n");
        return 1;
    }
    for (int i = 0; i < EXAMPLE_MAX_CLIENTS; i++) {
        if (ctx->clients[i].handle != NULL) {
            printf("Client: id=%d name=%s\n", i, ctx->clients[i].name);
            found = true;
        }
    }
    if (!found) {
        printf("No open clients\n");
    }
    return 0;
}

static int cmd_client_open(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    int free_id = EXAMPLE_INVALID_CLIENT_ID;

    if (argc != 2) {
        printf("Usage: client_open <name>\n");
        return 1;
    }
    if (strlen(argv[1]) >= EXAMPLE_CLIENT_NAME_LEN) {
        printf("Client open failed: name is too long\n");
        return 1;
    }
    for (int i = 0; i < EXAMPLE_MAX_CLIENTS; i++) {
        if (ctx->clients[i].handle == NULL) {
            free_id = i;
            break;
        }
    }
    if (free_id == EXAMPLE_INVALID_CLIENT_ID) {
        printf("Client open failed: no free console slots\n");
        return 1;
    }

    esp_err_t err = esp_bt_hub_client_open(argv[1], &ctx->clients[free_id].handle);
    if (err != ESP_OK) {
        print_operation_result("Client open", err);
        return 1;
    }
    snprintf(ctx->clients[free_id].name, sizeof(ctx->clients[free_id].name), "%s", argv[1]);
    printf("Client opened: id=%d name=%s\n", free_id, ctx->clients[free_id].name);
    return 0;
}

static int cmd_client_close(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    int id;
    example_client_t *client;

    if (argc != 2) {
        printf("Usage: client_close <id>\n");
        return 1;
    }
    client = get_open_client(ctx, argv[1], &id);
    if (client == NULL) {
        printf("Client close failed: invalid client id\n");
        return 1;
    }

    esp_err_t err = esp_bt_hub_client_close(client->handle);
    if (err != ESP_OK) {
        print_operation_result("Client close", err);
        return 1;
    }
    memset(client, 0, sizeof(*client));
    printf("Client closed: id=%d\n", id);
    return 0;
}

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static bool parse_discoverable(const char *text, esp_bt_hub_discoverable_t *out_mode)
{
    if (strcmp(text, "none") == 0) {
        *out_mode = ESP_BT_HUB_DISCOVERABLE_NONE;
    } else if (strcmp(text, "limited") == 0) {
        *out_mode = ESP_BT_HUB_DISCOVERABLE_LIMITED;
    } else if (strcmp(text, "general") == 0) {
        *out_mode = ESP_BT_HUB_DISCOVERABLE_GENERAL;
    } else {
        return false;
    }
    return true;
}

static int cmd_classic_mode(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    example_client_t *client;
    esp_bt_hub_discoverable_t discoverable;
    bool connectable;

    if (argc != 4) {
        printf("Usage: classic_mode <id> <0|1> <none|limited|general>\n");
        return 1;
    }
    client = get_open_client(ctx, argv[1], NULL);
    if (client == NULL || (strcmp(argv[2], "0") != 0 && strcmp(argv[2], "1") != 0) ||
        !parse_discoverable(argv[3], &discoverable)) {
        printf("Classic mode failed: invalid argument\n");
        return 1;
    }
    connectable = strcmp(argv[2], "1") == 0;

    esp_err_t err = esp_bt_hub_classic_scan_mode_set(client->handle, connectable, discoverable);

    print_operation_result("Classic mode", err);
    return err == ESP_OK ? 0 : 1;
}

static int cmd_classic_inquiry_start(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    example_client_t *client;
    int id;

    if (argc != 2) {
        printf("Usage: classic_inquiry_start <id>\n");
        return 1;
    }
    client = get_open_client(ctx, argv[1], &id);
    if (client == NULL) {
        printf("Classic inquiry start failed: invalid client id\n");
        return 1;
    }
    if (ctx->discovery != NULL) {
        print_operation_result("Classic inquiry start", ESP_ERR_INVALID_STATE);
        return 1;
    }

    esp_err_t err = esp_bt_hub_classic_discovery_start(client->handle, ESP_BT_INQ_MODE_GENERAL_INQUIRY,
                                                       EXAMPLE_CLASSIC_INQUIRY_LEN, 0, &ctx->discovery);
    if (err == ESP_OK) {
        ctx->discovery_owner = id;
    }
    print_operation_result("Classic inquiry start", err);
    return err == ESP_OK ? 0 : 1;
}

static int cmd_classic_inquiry_stop(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;

    if (argc != 2) {
        printf("Usage: classic_inquiry_stop <id>\n");
        return 1;
    }
    if (ctx->discovery == NULL) {
        printf("Classic inquiry stop failed: no active lease\n");
        return 1;
    }
    if (!validate_owner_id(argv[1], ctx->discovery_owner, "Classic inquiry stop")) {
        return 1;
    }

    esp_err_t err = esp_bt_hub_classic_discovery_stop(ctx->discovery);
    if (err == ESP_OK) {
        ctx->discovery = NULL;
        ctx->discovery_owner = EXAMPLE_INVALID_CLIENT_ID;
    }
    print_operation_result("Classic inquiry stop", err);
    return err == ESP_OK ? 0 : 1;
}
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static int cmd_le_scan_start(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    example_client_t *client;
    int id;
    const esp_bt_hub_le_ext_scan_params_t params = {
        .passive = false,
        .filter_duplicates = true,
        .interval = EXAMPLE_LE_SCAN_INTERVAL,
        .window = EXAMPLE_LE_SCAN_WINDOW,
        .duration_ms = 0,
        .own_addr_type = EXAMPLE_LE_PUBLIC_ADDR_TYPE,
    };

    if (argc != 2) {
        printf("Usage: le_scan_start <id>\n");
        return 1;
    }
    client = get_open_client(ctx, argv[1], &id);
    if (client == NULL) {
        printf("LE scan start failed: invalid client id\n");
        return 1;
    }
    if (ctx->le_scan != NULL) {
        print_operation_result("LE scan lease acquire", ESP_ERR_INVALID_STATE);
        return 1;
    }

    esp_err_t err = esp_bt_hub_le_external_scan_acquire(client->handle, &ctx->le_scan);
    if (err != ESP_OK) {
        print_operation_result("LE scan lease acquire", err);
        return 1;
    }
    err = esp_bt_hub_le_ext_scan_start(ctx->le_scan, &params);
    if (err != ESP_OK) {
        esp_bt_hub_le_external_scan_release(ctx->le_scan);
        ctx->le_scan = NULL;
        print_operation_result("LE scan start", err);
        return 1;
    }
    ctx->le_scan_owner = id;
    print_operation_result("LE scan start", ESP_OK);
    return 0;
}

static int cmd_le_scan_stop(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;

    if (argc != 2) {
        printf("Usage: le_scan_stop <id>\n");
        return 1;
    }
    if (ctx->le_scan == NULL) {
        printf("LE scan stop failed: no active lease\n");
        return 1;
    }
    if (!validate_owner_id(argv[1], ctx->le_scan_owner, "LE scan stop")) {
        return 1;
    }

    esp_err_t err = esp_bt_hub_le_ext_scan_stop(ctx->le_scan);
    if (err != ESP_OK) {
        print_operation_result("LE scan stop", err);
        return 1;
    }
    err = esp_bt_hub_le_external_scan_release(ctx->le_scan);
    if (err == ESP_OK) {
        ctx->le_scan = NULL;
        ctx->le_scan_owner = EXAMPLE_INVALID_CLIENT_ID;
    }
    print_operation_result("LE scan stop", err);
    return err == ESP_OK ? 0 : 1;
}

static int cmd_le_adv_start(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;
    example_client_t *client;
    int id;
    const esp_bt_hub_le_ext_adv_params_t params = {
        .connectable = false,
        .scannable = false,
        .legacy_pdu = false,
        .own_addr_type = EXAMPLE_LE_PUBLIC_ADDR_TYPE,
        .primary_phy = EXAMPLE_LE_PHY_1M,
        .secondary_phy = EXAMPLE_LE_PHY_1M,
        .tx_power = 0,
        .sid = 0,
        .itvl_min = EXAMPLE_LE_ADV_INTERVAL,
        .itvl_max = EXAMPLE_LE_ADV_INTERVAL,
    };

    if (argc != 2) {
        printf("Usage: le_adv_start <id>\n");
        return 1;
    }
    client = get_open_client(ctx, argv[1], &id);
    if (client == NULL) {
        printf("LE advertising start failed: invalid client id\n");
        return 1;
    }
    if (ctx->le_adv != NULL) {
        print_operation_result("LE advertising start", ESP_ERR_INVALID_STATE);
        return 1;
    }

    esp_err_t err = esp_bt_hub_le_ext_adv_acquire(client->handle, &ctx->le_adv);
    if (err == ESP_OK) {
        err = esp_bt_hub_le_ext_adv_configure(ctx->le_adv, &params);
    }
    if (err == ESP_OK) {
        err = esp_bt_hub_le_ext_adv_set_data(ctx->le_adv, s_le_adv_data, sizeof(s_le_adv_data));
    }
    if (err == ESP_OK) {
        err = esp_bt_hub_le_ext_adv_start(ctx->le_adv, 0, 0);
    }
    if (err != ESP_OK) {
        if (ctx->le_adv != NULL) {
            if (esp_bt_hub_le_ext_adv_release(ctx->le_adv) == ESP_OK) {
                ctx->le_adv = NULL;
            } else {
                ctx->le_adv_owner = id;
            }
        }
        print_operation_result("LE advertising start", err);
        return 1;
    }
    ctx->le_adv_owner = id;
    print_operation_result("LE advertising start", ESP_OK);
    return 0;
}

static int cmd_le_adv_stop(void *context, int argc, char **argv)
{
    example_context_t *ctx = context;

    if (argc != 2) {
        printf("Usage: le_adv_stop <id>\n");
        return 1;
    }
    if (ctx->le_adv == NULL) {
        printf("LE advertising stop failed: no active instance\n");
        return 1;
    }
    if (!validate_owner_id(argv[1], ctx->le_adv_owner, "LE advertising stop")) {
        return 1;
    }

    esp_err_t err = esp_bt_hub_le_ext_adv_stop(ctx->le_adv);
    if (err != ESP_OK) {
        print_operation_result("LE advertising stop", err);
        return 1;
    }
    err = esp_bt_hub_le_ext_adv_release(ctx->le_adv);
    if (err == ESP_OK) {
        ctx->le_adv = NULL;
        ctx->le_adv_owner = EXAMPLE_INVALID_CLIENT_ID;
    }
    print_operation_result("LE advertising stop", err);
    return err == ESP_OK ? 0 : 1;
}
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

static const esp_console_cmd_t s_common_commands[] = {
    {
        .command        = "hub_status",
        .help           = "Show hub, client, and lease state",
        .func_w_context = cmd_hub_status,
    },
    {
        .command        = "hub_start",
        .help           = "Initialize the Bluetooth hub",
        .func_w_context = cmd_hub_start,
    },
    {
        .command        = "hub_stop",
        .help           = "Deinitialize an idle Bluetooth hub",
        .func_w_context = cmd_hub_stop,
    },
    {
        .command        = "clients",
        .help           = "List open Bluetooth hub clients",
        .func_w_context = cmd_clients,
    },
    {
        .command        = "client_open",
        .help           = "Open a named Bluetooth hub client",
        .hint           = "<name>",
        .func_w_context = cmd_client_open,
    },
    {
        .command        = "client_close",
        .help           = "Close a Bluetooth hub client",
        .hint           = "<id>",
        .func_w_context = cmd_client_close,
    },
};

#if ESP_BT_HUB_CLASSIC_SUPPORTED
static const esp_console_cmd_t s_classic_commands[] = {
    {
        .command        = "classic_mode",
        .help           = "Set one client's Classic scan-mode vote",
        .hint           = "<id> <0|1> <none|limited|general>",
        .func_w_context = cmd_classic_mode,
    },
    {
        .command        = "classic_inquiry_start",
        .help           = "Start exclusive Classic inquiry",
        .hint           = "<id>",
        .func_w_context = cmd_classic_inquiry_start,
    },
    {
        .command        = "classic_inquiry_stop",
        .help           = "Stop and release owned Classic inquiry",
        .hint           = "<id>",
        .func_w_context = cmd_classic_inquiry_stop,
    },
};
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
static const esp_console_cmd_t s_le_commands[] = {
    {
        .command        = "le_scan_start",
        .help           = "Acquire the LE scan lease and start extended scan",
        .hint           = "<id>",
        .func_w_context = cmd_le_scan_start,
    },
    {
        .command        = "le_scan_stop",
        .help           = "Stop owned LE scan and release its lease",
        .hint           = "<id>",
        .func_w_context = cmd_le_scan_stop,
    },
    {
        .command        = "le_adv_start",
        .help           = "Acquire and start an LE extended advertising instance",
        .hint           = "<id>",
        .func_w_context = cmd_le_adv_start,
    },
    {
        .command        = "le_adv_stop",
        .help           = "Stop and release the owned LE advertising instance",
        .hint           = "<id>",
        .func_w_context = cmd_le_adv_stop,
    },
};
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

/**
 * @brief  Number of registered commands in each command table
 */
typedef struct {
    size_t  common;   /*!< Registered common commands */
    size_t  classic;  /*!< Registered Classic commands */
    size_t  le;       /*!< Registered LE commands */
} command_registration_t;

static esp_err_t register_command_table(const esp_console_cmd_t *commands, size_t count,
                                        example_context_t *ctx, size_t *registered)
{
    for (size_t i = 0; i < count; i++) {
        esp_console_cmd_t command = commands[i];
        esp_err_t err;

        command.context = ctx;
        err = esp_console_cmd_register(&command);
        if (err != ESP_OK) {
            return err;
        }
        *registered = i + 1;
    }
    return ESP_OK;
}

static void unregister_command_table(const esp_console_cmd_t *commands, size_t registered)
{
    while (registered > 0) {
        esp_err_t err = esp_console_cmd_deregister(commands[--registered].command);

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Deregister command '%s' failed: %s",
                     commands[registered].command, esp_err_to_name(err));
        }
    }
}

static void unregister_commands(command_registration_t *registration)
{
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    unregister_command_table(s_le_commands, registration->le);
    registration->le = 0;
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    unregister_command_table(s_classic_commands, registration->classic);
    registration->classic = 0;
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
    unregister_command_table(s_common_commands, registration->common);
    registration->common = 0;
}

static esp_err_t register_commands(example_context_t *ctx, command_registration_t *registration)
{
    esp_err_t err = esp_console_register_help_command();

    if (err == ESP_OK) {
        err = register_command_table(s_common_commands, EXAMPLE_ARRAY_SIZE(s_common_commands),
                                     ctx, &registration->common);
    }
#if ESP_BT_HUB_CLASSIC_SUPPORTED
    if (err == ESP_OK) {
        err = register_command_table(s_classic_commands, EXAMPLE_ARRAY_SIZE(s_classic_commands),
                                     ctx, &registration->classic);
    }
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */
#if ESP_BT_HUB_LE_EXT_SUPPORTED
    if (err == ESP_OK) {
        err = register_command_table(s_le_commands, EXAMPLE_ARRAY_SIZE(s_le_commands),
                                     ctx, &registration->le);
    }
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */
    return err;
}

static esp_err_t console_repl_destroy(example_context_t *ctx)
{
    esp_err_t err;

    if (ctx->repl == NULL) {
        return ESP_OK;
    }
    err = esp_console_start_repl(ctx->repl);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_console_stop_repl(ctx->repl);
    if (err == ESP_OK) {
        ctx->repl = NULL;
    }
    return err;
}

static esp_err_t console_start(example_context_t *ctx)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    command_registration_t registration = {0};
    esp_err_t cleanup_err;
    esp_err_t err;

    repl_config.prompt = "bt_hub>";
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t device_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    err = esp_console_new_repl_uart(&device_config, &repl_config, &ctx->repl);
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t device_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_cdc(&device_config, &repl_config, &ctx->repl);
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t device_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    err = esp_console_new_repl_usb_serial_jtag(&device_config, &repl_config, &ctx->repl);
#else
    ESP_LOGE(TAG, "Start console failed: no supported console transport is enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_CONSOLE_UART */
    if (err != ESP_OK) {
        return err;
    }
    err = register_commands(ctx, &registration);
    if (err != ESP_OK) {
        unregister_commands(&registration);
        cleanup_err = console_repl_destroy(ctx);
        if (cleanup_err != ESP_OK) {
            ESP_LOGE(TAG, "Destroy console after command registration failure failed: %s",
                     esp_err_to_name(cleanup_err));
        }
        return err;
    }
    err = esp_console_start_repl(ctx->repl);
    if (err != ESP_OK) {
        unregister_commands(&registration);
        cleanup_err = console_repl_destroy(ctx);
        if (cleanup_err != ESP_OK) {
            ESP_LOGE(TAG, "Destroy console after REPL start failure failed: %s",
                     esp_err_to_name(cleanup_err));
        }
    }
    return err;
}

esp_err_t bt_hub_console_run(void)
{
    example_context_t *ctx = calloc(1, sizeof(*ctx));

    if (ctx == NULL) {
        ESP_LOGE(TAG, "Start example failed: no memory for context");
        return ESP_ERR_NO_MEM;
    }
    ctx->discovery_owner = EXAMPLE_INVALID_CLIENT_ID;
    ctx->le_scan_owner = EXAMPLE_INVALID_CLIENT_ID;
    ctx->le_adv_owner = EXAMPLE_INVALID_CLIENT_ID;

    esp_err_t err = hub_start(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start hub failed: %s", esp_err_to_name(err));
        free(ctx);
        return err;
    }
    err = console_start(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start console failed: %s", esp_err_to_name(err));
        if (ctx->repl != NULL) {
            ESP_LOGE(TAG, "Console rollback incomplete; retain example context");
            return err;
        }
        esp_err_t stop_err = hub_stop(ctx);

        if (stop_err != ESP_OK) {
            ESP_LOGE(TAG, "Stop hub after console failure failed: %s; retain example context",
                     esp_err_to_name(stop_err));
            return err;
        }
        free(ctx);
        return err;
    }
    ESP_LOGI(TAG, "Start 'bt_hub' console: success");
    return ESP_OK;
}
