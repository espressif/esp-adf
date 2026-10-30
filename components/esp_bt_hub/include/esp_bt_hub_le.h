/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_bt_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_BT_HUB_LE_PHY_1M_MASK     (1U << 0)
#define ESP_BT_HUB_LE_PHY_2M_MASK     (1U << 1)
#define ESP_BT_HUB_LE_PHY_CODED_MASK  (1U << 2)

#define ESP_BT_HUB_LE_EXT_ADV_TX_POWER_NO_PREFERENCE  ((int8_t)127)
#define ESP_BT_HUB_LE_EXT_ADV_TX_POWER_MIN_DBM        ((int8_t)-127)
#define ESP_BT_HUB_LE_EXT_ADV_TX_POWER_MAX_DBM        ((int8_t)126)

/**
 * @brief  Extended advertising instances the active host can own
 *
 *         `CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES` and `CONFIG_BT_LE_MAX_EXT_ADV_INSTANCES` count
 *         instances beyond instance 0, so the usable count is one greater. NimBLE without that
 *         symbol uses 2.
 */
#if CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV
#if defined(CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES)
#define ESP_BT_HUB_MAX_EXT_ADV_INSTANCES  ((CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES) + 1)
#else
#define ESP_BT_HUB_MAX_EXT_ADV_INSTANCES  (2)
#endif  /* defined(CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES) */
#elif CONFIG_BT_BLUEDROID_ENABLED && defined(CONFIG_BT_LE_MAX_EXT_ADV_INSTANCES)
#define ESP_BT_HUB_MAX_EXT_ADV_INSTANCES  ((CONFIG_BT_LE_MAX_EXT_ADV_INSTANCES) + 1)
#else
#define ESP_BT_HUB_MAX_EXT_ADV_INSTANCES  (0)
#endif  /* CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_EXT_ADV */

/** @brief Opaque exclusive LE scan lease */
typedef struct esp_bt_hub_le_scan_lease *esp_bt_hub_le_scan_handle_t;

/** @brief Opaque exclusive legacy advertising lease */
typedef struct esp_bt_hub_le_adv_lease *esp_bt_hub_le_adv_handle_t;

/** @brief Opaque owned extended-advertising instance */
typedef struct esp_bt_hub_le_ext_adv_lease *esp_bt_hub_le_ext_adv_handle_t;

/**
 * @brief  Parsed LE extended-advertising report
 *
 *         The address uses the on-air, least-significant-byte-first order on both supported hosts.
 *         `data` points into the native GAP callback parameter and is valid only until that callback returns.
 */
typedef struct {
    uint8_t        addr[6];      /*!< Advertiser address, on-air byte order */
    const uint8_t *data;         /*!< Advertising payload borrowed from the GAP event */
    size_t         data_len;     /*!< Advertising payload length in bytes */
    int32_t        rssi;         /*!< RSSI in dBm */
    uint8_t        addr_type;    /*!< Peer address type: 0 public, 1 random, 2 RPA-public, 3 RPA-random */
    uint8_t        sid;          /*!< Advertising Set ID */
    bool           connectable;  /*!< True when the advertising event is connectable */
} esp_bt_hub_le_adv_report_t;

/**
 * @brief  LE scan parameters for the exclusive scan lease
 */
typedef struct {
    uint16_t  interval;     /*!< Scan interval in 0.625 ms units */
    uint16_t  window;       /*!< Scan window in 0.625 ms units */
    bool      active;       /*!< True for an active scan */
    uint32_t  duration_ms;  /*!< Duration in ms. 0 scans until stop. Bluedroid rounds up to seconds */
} esp_bt_hub_le_scan_params_t;

/**
 * @brief  Legacy advertising parameters for the exclusive advertising lease
 */
typedef struct {
    uint16_t       interval_min;  /*!< Minimum interval in 0.625 ms units */
    uint16_t       interval_max;  /*!< Maximum interval in 0.625 ms units */
    uint8_t        adv_type;      /*!< ADV_IND (0x00), ADV_SCAN_IND (0x02), or ADV_NONCONN_IND (0x03) */
    uint8_t        channel_map;   /*!< Primary channel map. 0 selects channels 37, 38, and 39 */
    const uint8_t *data;          /*!< Advertising payload. NULL with `data_len` 0 is empty */
    uint16_t       data_len;      /*!< Payload length in bytes. Non-zero requires `data` */
} esp_bt_hub_le_adv_params_t;

/**
 * @brief  Extended advertising parameters
 */
typedef struct {
    bool      connectable;    /*!< Advertising set accepts connections */
    bool      scannable;      /*!< Advertising set accepts scan requests */
    bool      legacy_pdu;     /*!< Send legacy advertising PDUs */
    uint8_t   own_addr_type;  /*!< Own address type: 0 public, 1 random, 2 RPA-public, 3 RPA-random */
    uint8_t   primary_phy;    /*!< Primary PHY: 1 LE 1M, 3 LE Coded */
    uint8_t   secondary_phy;  /*!< Secondary PHY: 1 LE 1M, 2 LE 2M, 3 LE Coded */
    int8_t    tx_power;       /*!< Requested dBm; use `ESP_BT_HUB_LE_EXT_ADV_TX_POWER_NO_PREFERENCE` for no preference */
    uint8_t   sid;            /*!< Advertising Set ID */
    uint32_t  itvl_min;       /*!< Minimum interval in 0.625 ms units */
    uint32_t  itvl_max;       /*!< Maximum interval in 0.625 ms units */
} esp_bt_hub_le_ext_adv_params_t;

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
/**
 * @brief  Periodic advertising parameters
 */
typedef struct {
    bool      include_tx_power;  /*!< Include Tx Power in the periodic advertising PDU */
    uint16_t  itvl_min;          /*!< Minimum interval in 1.25 ms units */
    uint16_t  itvl_max;          /*!< Maximum interval in 1.25 ms units */
} esp_bt_hub_le_periodic_adv_params_t;

/**
 * @brief  Periodic advertising sync parameters
 */
typedef struct {
    uint16_t  skip;          /*!< Maximum periodic events the scanner may skip */
    uint16_t  sync_timeout;  /*!< Sync timeout in 10 ms units */
} esp_bt_hub_le_periodic_sync_params_t;
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */

/**
 * @brief  Extended scan parameters
 *
 *         The caller must already hold the LE scan lease. `phy_mask` selects LE 1M and/or LE Coded.
 *         Both PHYs use the same scan type, interval, and window.
 */
typedef struct {
    bool      passive;            /*!< True for a passive scan */
    bool      filter_duplicates;  /*!< Drop duplicate advertising reports */
    uint16_t  interval;           /*!< Scan interval in 0.625 ms units */
    uint16_t  window;             /*!< Scan window in 0.625 ms units */
    uint32_t  duration_ms;        /*!< Duration in ms. 0 scans until stop. Bluedroid rounds up to 10 ms */
    uint8_t   own_addr_type;      /*!< Own address type: 0 public, 1 random, 2 RPA-public, 3 RPA-random */
    uint8_t   phy_mask;           /*!< PHY mask: 1M and/or Coded; 0 selects 1M */
} esp_bt_hub_le_ext_scan_params_t;

/**
 * @brief  LE connection parameters used while opening an ACL link
 */
typedef struct {
    uint16_t  scan_itvl;            /*!< Scan interval in 0.625 ms units */
    uint16_t  scan_window;          /*!< Scan window in 0.625 ms units */
    uint16_t  itvl_min;             /*!< Minimum connection interval in 1.25 ms units */
    uint16_t  itvl_max;             /*!< Maximum connection interval in 1.25 ms units */
    uint16_t  latency;              /*!< Peripheral latency in connection events */
    uint16_t  supervision_timeout;  /*!< Supervision timeout in 10 ms units */
    uint16_t  min_ce_len;           /*!< Minimum connection event length in 0.625 ms units */
    uint16_t  max_ce_len;           /*!< Maximum connection event length in 0.625 ms units */
} esp_bt_hub_le_conn_params_t;

/**
 * @brief  Parameters for `esp_bt_hub_le_connect()`
 */
typedef struct {
    uint8_t   own_addr_type;   /*!< Own address type. Bluedroid ignores it */
    uint8_t   peer_addr_type;  /*!< Types 0-3: public, random, RPA-public, RPA-random */
    uint8_t   peer[6];         /*!< Peer address, on-air byte order */
    uint32_t  timeout_ms;      /*!< Connect timeout in ms. Bluedroid ignores it */
#if CONFIG_BT_BLUEDROID_ENABLED
    uint8_t  gattc_if;                               /*!< GATTC interface used by Bluedroid aux_open */
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
    const esp_bt_hub_le_conn_params_t *conn_params;  /*!< LE connection parameters */
    uint8_t                            phy_mask;     /*!< PHY mask: 1M, 2M, and/or Coded; 0 selects 1M */
} esp_bt_hub_le_connect_params_t;

/**
 * @brief  Convert a 6-byte address to the byte order of the active host
 *
 *         On-air and BLE Audio GAP addresses are least-significant-byte first. Bluedroid
 *         `esp_ble_gap_*`, `esp_ble_gattc_*`, and `esp_ble_set_encryption` use the opposite
 *         order, so this reverses `src`. Applying it twice restores the original order. On
 *         NimBLE the bytes are copied when `dst` and `src` differ.
 *
 * @param[out]  dst  Destination buffer
 * @param[in]   src  Source address. May alias `dst`
 */
static inline void esp_bt_hub_le_addr_swap_if_need(uint8_t dst[6], const uint8_t src[6])
{
#if CONFIG_BT_BLUEDROID_ENABLED
    uint8_t tmp[6];

    for (int i = 0; i < 6; i++) {
        tmp[i] = src[5 - i];
    }
    memcpy(dst, tmp, sizeof(tmp));
#else
    if (dst != src) {
        memcpy(dst, src, 6);
    }
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */
}

#if ESP_BT_HUB_LE_SUPPORTED
/**
 * @brief  Parse one extended-advertising report from a native GAP parameter
 *
 *         Call it for Bluedroid `ESP_GAP_BLE_EXT_ADV_REPORT_EVT` or NimBLE `BLE_GAP_EVENT_EXT_DISC`.
 *         `out->data` remains valid only until the GAP callback returns. The address in `out` is
 *         on-air byte order. Not ISR-safe.
 *
 * @param[in]   param  Native parameter for the advertising-report event
 * @param[out]  out    Parsed advertising report
 *
 * @return
 *       - ESP_OK                 Report was parsed
 *       - ESP_ERR_INVALID_ARG    `param` or `out` is NULL
 *       - ESP_ERR_NOT_SUPPORTED  This build has no extended-scan reports
 */
esp_err_t esp_bt_hub_le_adv_report_parse(const esp_bt_hub_gap_ble_param_t *param,
                                         esp_bt_hub_le_adv_report_t *out);

/**
 * @brief  Reserve the LE scan lease without starting a scan
 *
 *         `esp_bt_hub_le_scan_start()` fails until `esp_bt_hub_le_external_scan_release()` drops
 *         this lease. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]   client    Client that will own the lease
 * @param[out]  out_scan  External scan lease. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Lease acquired
 *       - ESP_ERR_INVALID_ARG    `client` or `out_scan` is NULL, or `client` is unknown
 *       - ESP_ERR_INVALID_STATE  Host is not running, or the LE scan lease is already held
 */
esp_err_t esp_bt_hub_le_external_scan_acquire(esp_bt_hub_client_handle_t client,
                                              esp_bt_hub_le_scan_handle_t *out_scan);

/**
 * @brief  Release an external LE scan lease
 *
 *         Scanning started with a raw stack API is left running; stop it before releasing the
 *         lease. A scan started through `esp_bt_hub_le_ext_scan_start()` is tracked by the hub
 *         and must be stopped with `esp_bt_hub_le_ext_scan_stop()` first. Not ISR-safe.
 *         Blocks only on the host lock.
 *
 * @param[in]  scan  External scan lease to release
 *
 * @return
 *       - ESP_OK                 Lease released
 *       - ESP_ERR_INVALID_ARG    `scan` is NULL, stale, inactive, or not an external lease
 *       - ESP_ERR_INVALID_STATE  Host context does not exist, or a hub-started extended scan is active
 */
esp_err_t esp_bt_hub_le_external_scan_release(esp_bt_hub_le_scan_handle_t scan);

/**
 * @brief  Reserve one extended-advertising instance
 *
 *         The handle is bound to `client` and carries a generation. The host selects the controller
 *         advertising set. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]   client   Client that owns the instance
 * @param[out]  out_adv  Owned instance handle. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Instance reserved
 *       - ESP_ERR_INVALID_ARG    `client` or `out_adv` is NULL, or `client` is unknown
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NOT_FOUND      Every instance is in use
 *       - ESP_ERR_NOT_SUPPORTED  Extended advertising is unavailable in this build
 */
esp_err_t esp_bt_hub_le_ext_adv_acquire(esp_bt_hub_client_handle_t client,
                                        esp_bt_hub_le_ext_adv_handle_t *out_adv);

/**
 * @brief  Get the controller advertising-set index of an owned instance
 *
 *         ESP-IDF ISO and BAP calls that take an advertising-set index use this value. It stays
 *         valid while `adv` is owned. Host advertising calls must still go through this component.
 *         Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]   adv           Owned extended-advertising handle
 * @param[out]  out_instance  Controller advertising-set index
 *
 * @return
 *       - ESP_OK                 Index was written
 *       - ESP_ERR_INVALID_ARG    `adv` is NULL or stale, or `out_instance` is NULL
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - ESP_ERR_NOT_SUPPORTED  Extended advertising is unavailable in this build
 */
esp_err_t esp_bt_hub_le_ext_adv_get_instance(esp_bt_hub_le_ext_adv_handle_t adv,
                                             uint8_t *out_instance);

/**
 * @brief  Release an owned extended-advertising instance
 *
 *         Extended and periodic advertising on the instance must already be stopped. A configured native
 *         advertising set is removed before the lease is freed. If removal fails, the handle remains valid
 *         and faulted so release can be retried. Not ISR-safe. May block on the instance lock and native stack.
 *
 * @param[in]  adv  Handle returned by `esp_bt_hub_le_ext_adv_acquire()`
 *
 * @return
 *       - ESP_OK                 Instance released
 *       - ESP_ERR_INVALID_ARG    `adv` is NULL, stale, or inactive
 *       - ESP_ERR_INVALID_STATE  Host context does not exist, or extended or periodic advertising is active
 *       - ESP_ERR_NOT_SUPPORTED  Extended advertising is unavailable in this build
 *       - Others                 Native advertising-set removal failed; the lease remains valid
 */
esp_err_t esp_bt_hub_le_ext_adv_release(esp_bt_hub_le_ext_adv_handle_t adv);

/**
 * @brief  Reply to an LE security request
 *
 *         `peer` is the on-air address. NimBLE maps it to the pending connection and can reject only
 *         numeric comparison. Bluedroid passes the address to `esp_ble_gap_security_rsp()`. Not ISR-safe.
 *         May block.
 *
 * @param[in]  peer    Peer address, on-air byte order
 * @param[in]  accept  True to accept the request
 *
 * @return
 *       - ESP_OK                 Reply was sent
 *       - ESP_ERR_INVALID_ARG    `peer` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running, or NimBLE has no matching pending request
 *       - ESP_ERR_NOT_SUPPORTED  NimBLE cannot reject this pending action
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_security_reply(const uint8_t peer[6], bool accept);

/**
 * @brief  Reply to an LE numeric-comparison request
 *
 *         `peer` is the on-air address. NimBLE requires a pending numeric-comparison action.
 *         Not ISR-safe. May block.
 *
 * @param[in]  peer    Peer address, on-air byte order
 * @param[in]  accept  True to accept the comparison
 *
 * @return
 *       - ESP_OK                 Reply was sent
 *       - ESP_ERR_INVALID_ARG    `peer` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running, or NimBLE has no pending numeric comparison
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_confirm_reply(const uint8_t peer[6], bool accept);

/**
 * @brief  Reply to an LE passkey request
 *
 *         `peer` is the on-air address. NimBLE requires a pending input or display action. Not ISR-safe.
 *         May block.
 *
 * @param[in]  peer     Peer address, on-air byte order
 * @param[in]  accept   True to submit `passkey`
 * @param[in]  passkey  Passkey submitted when `accept` is true
 *
 * @return
 *       - ESP_OK                 Reply was sent
 *       - ESP_ERR_INVALID_ARG    `peer` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running, or NimBLE has no pending passkey action
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_passkey_reply(const uint8_t peer[6], bool accept, uint32_t passkey);

/**
 * @brief  Return the number of bonded LE peers
 *
 *         Not ISR-safe. Blocks on the host lock and may allocate.
 *
 * @param[out]  out_count  Number of bonded LE peers. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 `out_count` is set
 *       - ESP_ERR_INVALID_ARG    `out_count` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 */
esp_err_t esp_bt_hub_le_bond_count(size_t *out_count);

/**
 * @brief  Return whether an LE address belongs to a bonded peer
 *
 *         A resolvable private address matches the identity stored with the bond. Not ISR-safe.
 *         Blocks on the host lock and may allocate.
 *
 * @param[in]   addr_type   Peer address type: 0 public, 1 random, 2 RPA-public, 3 RPA-random
 * @param[in]   addr        Peer address, on-air byte order
 * @param[out]  out_exists  Set to true when a bond matches `addr`. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 `out_exists` is set
 *       - ESP_ERR_INVALID_ARG    `addr` or `out_exists` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 */
esp_err_t esp_bt_hub_le_bond_exists(uint8_t addr_type, const uint8_t addr[6], bool *out_exists);

#if CONFIG_BT_BLUEDROID_ENABLED
/**
 * @brief  Record the Bluedroid peer address for an LE ACL handle
 *
 *         `addr` uses the byte order expected by `esp_ble_gap_*` and `esp_ble_set_encryption`. A
 *         Bluedroid GATTC `remote_bda` is already in that order. An `esp_ble_audio` ACL-connect
 *         address is on-air order and must be passed through `esp_bt_hub_le_addr_swap_if_need()`
 *         first. Later disconnect and encryption calls use this record. Not ISR-safe.
 *
 * @param[in]  conn_handle  ACL connection handle
 * @param[in]  addr         Peer address in Bluedroid byte order
 *
 * @return
 *       - ESP_OK                 Record stored
 *       - ESP_ERR_INVALID_ARG    `addr` is NULL
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - ESP_ERR_NO_MEM         Peer table is full
 */
esp_err_t esp_bt_hub_le_peer_set(uint16_t conn_handle, const uint8_t addr[6]);

/**
 * @brief  Drop the Bluedroid peer record for an LE ACL handle
 *
 *         A missing record is ignored. Not ISR-safe. Does not block.
 *
 * @param[in]  conn_handle  ACL connection handle
 *
 * @return
 *       - ESP_OK                 Record removed, or none was stored
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 */
esp_err_t esp_bt_hub_le_peer_clear(uint16_t conn_handle);
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */

/**
 * @brief  Disconnect an LE ACL link
 *
 *         NimBLE passes `reason` to the controller. Bluedroid ignores `reason` and disconnects the
 *         peer recorded by `esp_bt_hub_le_peer_set()`. Not ISR-safe. May block.
 *
 * @param[in]  conn_handle  ACL connection handle
 * @param[in]  reason       HCI disconnect reason. Bluedroid ignores it
 *
 * @return
 *       - ESP_OK                 Disconnect was requested
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NOT_FOUND      Bluedroid has no recorded peer address for `conn_handle`
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_disconnect(uint16_t conn_handle, uint8_t reason);

/**
 * @brief  Start LE encryption on a connection
 *
 *         NimBLE calls `ble_gap_security_initiate()`. Bluedroid requests encryption without MITM for
 *         the peer recorded by `esp_bt_hub_le_peer_set()`. Not ISR-safe. May block.
 *
 * @param[in]  conn_handle  ACL connection handle
 *
 * @return
 *       - ESP_OK                 Encryption was requested
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NOT_FOUND      Bluedroid has no recorded peer address for `conn_handle`
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_security_initiate(uint16_t conn_handle);

/**
 * @brief  Choose the own address type for the next LE procedure
 *
 *         Bluedroid returns `BLE_ADDR_TYPE_RPA_PUBLIC` or `BLE_ADDR_TYPE_PUBLIC` and does not call
 *         the stack. NimBLE calls `ble_hs_id_infer_auto()`. Not ISR-safe. NimBLE may block.
 *
 * @param[in]   privacy        Non-zero to prefer a resolvable private address
 * @param[out]  own_addr_type  Selected address type
 *
 * @return
 *       - ESP_OK                 Address type was written
 *       - ESP_ERR_INVALID_ARG    `own_addr_type` is NULL
 *       - ESP_ERR_INVALID_STATE  On NimBLE, the host is not running
 *       - Others                 NimBLE address selection failed
 */
esp_err_t esp_bt_hub_le_id_infer_auto(int privacy, uint8_t *own_addr_type);

/**
 * @brief  Enable LE privacy on the active host
 *
 *         NimBLE privacy follows its stack configuration, so a running host returns `ESP_OK`.
 *         Bluedroid adds bonded identity keys to the resolving list and enables controller privacy.
 *         Not ISR-safe. On Bluedroid this blocks until the controller command completes.
 *
 * @return
 *       - ESP_OK                 Privacy is enabled, or NimBLE already applies its configuration
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Bluedroid bond-list allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_privacy_enable(void);
#endif  /* ESP_BT_HUB_LE_SUPPORTED */

#if ESP_BT_HUB_LE_LEGACY_SUPPORTED
/**
 * @brief  Take the LE scan lease and start scanning
 *
 *         The same lease is held by `esp_bt_hub_le_external_scan_acquire()`. A finite scan stops in the
 *         controller, and the lease remains until `esp_bt_hub_le_scan_stop()`. NimBLE marks the lease
 *         active before return. Bluedroid leaves it acquiring until the start-complete event and rounds
 *         `duration_ms` up to seconds. Not ISR-safe. May block.
 *
 * @param[in]   client    Client that will own the lease
 * @param[in]   params    Scan parameters
 * @param[out]  out_scan  Scan lease. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Scan was accepted and this client holds the lease
 *       - ESP_ERR_INVALID_ARG    `client`, `params`, or `out_scan` is NULL, or `client` is unknown
 *       - ESP_ERR_INVALID_STATE  Host is not running, or the lease is already held
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_scan_start(esp_bt_hub_client_handle_t client, const esp_bt_hub_le_scan_params_t *params,
                                   esp_bt_hub_le_scan_handle_t *out_scan);

/**
 * @brief  Stop LE scanning and release the lease
 *
 *         The lease must be active or faulted, and it must not be external. On success it is released.
 *         On failure it remains valid and becomes faulted. Not ISR-safe. May block.
 *
 * @param[in]  scan  Scan lease returned by `esp_bt_hub_le_scan_start()`
 *
 * @return
 *       - ESP_OK                 Scanning stopped and the lease was released
 *       - ESP_ERR_INVALID_ARG    `scan` is NULL, stale, external, or neither active nor faulted
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - Others                 Stack call failed; the lease remains valid
 */
esp_err_t esp_bt_hub_le_scan_stop(esp_bt_hub_le_scan_handle_t scan);

/**
 * @brief  Take the legacy advertising lease and start advertising
 *
 *         `data == NULL` with `data_len == 0` is an empty payload. NimBLE marks the lease active before
 *         return and rejects directed or unknown PDU types. Bluedroid leaves the lease acquiring until
 *         the start-complete event. Fails while an external legacy advertising lease is held.
 *         Not ISR-safe. May block.
 *
 * @param[in]   client   Client that will own the lease
 * @param[in]   params   Advertising parameters and payload
 * @param[out]  out_adv  Advertising lease. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Advertising was accepted and this client holds the lease
 *       - ESP_ERR_INVALID_ARG    `client` or `params` is invalid, or NimBLE rejects `adv_type`
 *       - ESP_ERR_INVALID_STATE  Host is not running, or the lease is already held
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_adv_start(esp_bt_hub_client_handle_t client, const esp_bt_hub_le_adv_params_t *params,
                                  esp_bt_hub_le_adv_handle_t *out_adv);

/**
 * @brief  Stop legacy advertising and release the lease
 *
 *         The lease must be active or faulted, and it must not be external. On success it is released.
 *         On failure it remains valid and becomes faulted. Not ISR-safe. May block.
 *
 * @param[in]  adv  Advertising lease returned by `esp_bt_hub_le_adv_start()`
 *
 * @return
 *       - ESP_OK                 Advertising stopped and the lease was released
 *       - ESP_ERR_INVALID_ARG    `adv` is NULL, stale, external, or neither active nor faulted
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - Others                 Stack call failed; the lease remains valid
 */
esp_err_t esp_bt_hub_le_adv_stop(esp_bt_hub_le_adv_handle_t adv);

/**
 * @brief  Reserve the legacy advertising lease without starting advertising
 *
 *         `esp_bt_hub_le_adv_start()` fails until `esp_bt_hub_le_external_adv_release()` drops this
 *         lease. Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]   client   Client that will own the lease
 * @param[out]  out_adv  External advertising lease. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Lease acquired
 *       - ESP_ERR_INVALID_ARG    `client` or `out_adv` is NULL, or `client` is unknown
 *       - ESP_ERR_INVALID_STATE  Host is not running, or a legacy advertising lease is already held
 */
esp_err_t esp_bt_hub_le_external_adv_acquire(esp_bt_hub_client_handle_t client,
                                             esp_bt_hub_le_adv_handle_t *out_adv);

/**
 * @brief  Release an external legacy advertising lease
 *
 *         Advertising is left running. Stop the caller's advertising before releasing the lease.
 *         Not ISR-safe. Blocks only on the host lock.
 *
 * @param[in]  adv  External advertising lease to release
 *
 * @return
 *       - ESP_OK                 Lease released
 *       - ESP_ERR_INVALID_ARG    `adv` is NULL, stale, inactive, or not an external lease
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 */
esp_err_t esp_bt_hub_le_external_adv_release(esp_bt_hub_le_adv_handle_t adv);
#endif  /* ESP_BT_HUB_LE_LEGACY_SUPPORTED */

#if ESP_BT_HUB_LE_EXT_SUPPORTED
/**
 * @brief  Configure one extended advertising instance
 *
 *         `adv` must come from `esp_bt_hub_le_ext_adv_acquire()`. Not ISR-safe. On Bluedroid this
 *         blocks until the controller completes the command.
 *
 * @param[in]  adv     Owned extended-advertising handle
 * @param[in]  params  Extended advertising parameters
 *
 * @return
 *       - ESP_OK                 Parameters were set
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or `params` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_adv_configure(esp_bt_hub_le_ext_adv_handle_t adv,
                                          const esp_bt_hub_le_ext_adv_params_t *params);

/**
 * @brief  Set raw extended advertising data
 *
 *         The host copies `data` before returning. Pass NULL with len 0 to clear the data. Not ISR-safe.
 *         On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv   Owned extended-advertising handle
 * @param[in]  data  Raw advertising payload
 * @param[in]  len   Payload length in bytes
 *
 * @return
 *       - ESP_OK                 Data was set
 *       - ESP_ERR_INVALID_ARG    `adv` is stale, `data` is NULL with non-zero `len`, or `len` exceeds 65535
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_adv_set_data(esp_bt_hub_le_ext_adv_handle_t adv, const uint8_t *data, size_t len);

/**
 * @brief  Set raw extended advertising scan-response data
 *
 *         The host copies `data` before returning. Pass NULL with len 0 to clear the data. Not ISR-safe.
 *         On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv   Owned extended-advertising handle
 * @param[in]  data  Raw scan-response payload
 * @param[in]  len   Payload length in bytes
 *
 * @return
 *       - ESP_OK                 Data was set
 *       - ESP_ERR_INVALID_ARG    `adv` is stale, `data` is NULL with non-zero `len`, or `len` exceeds 65535
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_adv_set_scan_rsp(esp_bt_hub_le_ext_adv_handle_t adv, const uint8_t *data, size_t len);

/**
 * @brief  Start one extended advertising instance
 *
 *         Not ISR-safe. On Bluedroid this blocks until the controller command completes. When the
 *         controller terminates the set, the started flag is cleared and the handle can be released
 *         without `esp_bt_hub_le_ext_adv_stop()`.
 *
 * @param[in]  adv         Owned extended-advertising handle
 * @param[in]  duration    Duration in 10 ms units. 0 means no limit
 * @param[in]  max_events  Maximum advertising events. 0 means no limit
 *
 * @return
 *       - ESP_OK                 Advertising started
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or inactive
 *       - ESP_ERR_INVALID_STATE  Advertising is already active, or the host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_adv_start(esp_bt_hub_le_ext_adv_handle_t adv, int duration, int max_events);

/**
 * @brief  Stop one extended advertising instance
 *
 *         Not ISR-safe. On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv  Owned extended-advertising handle
 *
 * @return
 *       - ESP_OK                 Advertising stopped
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or inactive
 *       - ESP_ERR_INVALID_STATE  Advertising is not active, or the host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_adv_stop(esp_bt_hub_le_ext_adv_handle_t adv);

/**
 * @brief  Start an extended LE scan on a held scan lease
 *
 *         `scan` comes from `esp_bt_hub_le_scan_start()` or `esp_bt_hub_le_external_scan_acquire()`
 *         and must be active. Stopping the scan leaves the lease held. Bluedroid rounds
 *         `duration_ms` up to 10 ms units and blocks until each controller command completes.
 *         Protected by the hub runtime: a concurrent `esp_bt_hub_stop()` waits for this call.
 *         Not ISR-safe.
 *
 * @param[in]  scan    Active LE scan lease
 * @param[in]  params  Extended scan parameters
 *
 * @return
 *       - ESP_OK                 Scan started
 *       - ESP_ERR_INVALID_ARG    `scan` or `params` is NULL, or the duration exceeds the backend range
 *       - ESP_ERR_INVALID_STATE  Host is not running, or `scan` is not an active held lease
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_scan_start(esp_bt_hub_le_scan_handle_t scan,
                                       const esp_bt_hub_le_ext_scan_params_t *params);

/**
 * @brief  Stop the extended LE scan
 *
 *         The LE scan lease stays held and `scan` must be active. A scan that ended by itself
 *         (duration expiry) is stopped again without error. Not ISR-safe. On Bluedroid this
 *         blocks until the controller command completes. Protected by the hub runtime: a
 *         concurrent `esp_bt_hub_stop()` waits for this call.
 *
 * @param[in]  scan  Active LE scan lease
 *
 * @return
 *       - ESP_OK                 Scan stopped, or none was running
 *       - ESP_ERR_INVALID_ARG    `scan` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running, or `scan` is not an active held lease
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_ext_scan_stop(esp_bt_hub_le_scan_handle_t scan);

/**
 * @brief  Open one LE ACL connection
 *
 *         Every selected PHY uses `conn_params`; a zero `phy_mask` preserves 1M-only behavior. Bluedroid opens the
 *         link through `params->gattc_if` and ignores `own_addr_type` and `timeout_ms`. `peer` is on-air byte
 *         order. Not ISR-safe. May block.
 *
 * @param[in]  params  Connect parameters. `conn_params` is required. On Bluedroid, `gattc_if` must not be
 *                     `ESP_GATT_IF_NONE`
 *
 * @return
 *       - ESP_OK                 Connection attempt started
 *       - ESP_ERR_INVALID_ARG    `params` or `params->conn_params` is NULL, or Bluedroid `gattc_if` is
 *                                `ESP_GATT_IF_NONE`
 *       - ESP_ERR_INVALID_STATE  Host is not running, or a connection attempt is already pending
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_connect(const esp_bt_hub_le_connect_params_t *params);

#if CONFIG_BT_BLUEDROID_ENABLED
/**
 * @brief  Complete a Bluedroid LE connection attempt
 *
 *         Call this for every terminal central connect event, whether the connection succeeded or
 *         failed. `peer` uses on-air byte order, matching `esp_bt_hub_le_connect()`. A missing pending
 *         attempt or a different pending peer is safely ignored. Thread-safe with other hub calls.
 *         Not ISR-safe. Does not allocate and blocks only on the host/context lock.
 *
 * @param[in]  peer  Peer address from the terminal connect event, in on-air byte order
 *
 * @return
 *       - ESP_OK                 Matching attempt completed, or no matching attempt was pending
 *       - ESP_ERR_INVALID_ARG    `peer` is NULL
 *       - ESP_ERR_INVALID_STATE  Hub context is unavailable
 */
esp_err_t esp_bt_hub_le_connect_complete(const uint8_t peer[6]);
#endif  /* CONFIG_BT_BLUEDROID_ENABLED */

/**
 * @brief  Cancel the outstanding LE connection attempt
 *
 *         Bluedroid cancels the peer and GATTC interface stored by the last `esp_bt_hub_le_connect()` and the
 *         stack refuses an already-established link. NimBLE treats an already-finished attempt as success.
 *         Not ISR-safe. May block.
 *
 * @return
 *       - ESP_OK                 Attempt was cancelled, or none was outstanding
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_connect_cancel(void);

#if ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED
/**
 * @brief  Configure periodic advertising on one extended advertising instance
 *
 *         Not ISR-safe. On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv     Owned extended-advertising handle
 * @param[in]  params  Periodic advertising parameters
 *
 * @return
 *       - ESP_OK                 Parameters were set
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or `params` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_periodic_adv_configure(esp_bt_hub_le_ext_adv_handle_t adv,
                                               const esp_bt_hub_le_periodic_adv_params_t *params);

/**
 * @brief  Set raw periodic advertising data
 *
 *         The host copies `data` before returning. Pass NULL with len 0 to clear the data. Not ISR-safe.
 *         On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv   Owned extended-advertising handle
 * @param[in]  data  Raw periodic advertising payload
 * @param[in]  len   Payload length in bytes
 *
 * @return
 *       - ESP_OK                 Data was set
 *       - ESP_ERR_INVALID_ARG    `adv` is stale, `data` is NULL with non-zero `len`, or `len` exceeds 65535
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_periodic_adv_set_data(esp_bt_hub_le_ext_adv_handle_t adv,
                                              const uint8_t *data, size_t len);

/**
 * @brief  Start periodic advertising on one extended advertising instance
 *
 *         Not ISR-safe. On Bluedroid this blocks until the controller completes the command. Periodic advertising
 *         has no finite-duration completion and must be explicitly stopped before the owned handle is released.
 *
 * @param[in]  adv  Owned extended-advertising handle
 *
 * @return
 *       - ESP_OK                 Periodic advertising started
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or inactive
 *       - ESP_ERR_INVALID_STATE  Periodic advertising is already active, or the host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_periodic_adv_start(esp_bt_hub_le_ext_adv_handle_t adv);

/**
 * @brief  Stop periodic advertising on one extended advertising instance
 *
 *         Not ISR-safe. On Bluedroid this blocks until the controller completes the command.
 *
 * @param[in]  adv  Owned extended-advertising handle
 *
 * @return
 *       - ESP_OK                 Periodic advertising stopped
 *       - ESP_ERR_INVALID_ARG    `adv` is stale or inactive
 *       - ESP_ERR_INVALID_STATE  Periodic advertising is not active, or the host is not running
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_periodic_adv_stop(esp_bt_hub_le_ext_adv_handle_t adv);

/**
 * @brief  Create a periodic advertising sync to `addr` and `sid`
 *
 *         Not ISR-safe. The sync-established event arrives on the BLE GAP domain.
 *
 * @param[in]  addr_type  Advertiser address type
 * @param[in]  addr       Advertiser address
 * @param[in]  sid        Advertising SID
 * @param[in]  params     Sync parameters
 *
 * @return
 *       - ESP_OK                 Sync creation was requested
 *       - ESP_ERR_INVALID_ARG    `addr` or `params` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_pa_sync_create(uint8_t addr_type, const uint8_t addr[6], uint8_t sid,
                                       const esp_bt_hub_le_periodic_sync_params_t *params);

/**
 * @brief  Terminate a periodic advertising sync
 *
 *         Not ISR-safe.
 *
 * @param[in]  sync_handle  Sync handle from the sync-established event
 *
 * @return
 *       - ESP_OK                 Termination was requested
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_pa_sync_terminate(uint16_t sync_handle);

/**
 * @brief  Cancel an outstanding periodic advertising sync creation
 *
 *         Not ISR-safe.
 *
 * @return
 *       - ESP_OK                 Cancellation was requested
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_pa_sync_create_cancel(void);
#endif  /* ESP_BT_HUB_LE_PERIODIC_ADV_SUPPORTED */
#endif  /* ESP_BT_HUB_LE_EXT_SUPPORTED */

#if ESP_BT_HUB_LE_PAST_SUPPORTED
/**
 * @brief  Accept a periodic advertising sync transfer on `conn_handle`
 *
 *         Not ISR-safe. Bluedroid resolves the peer address recorded by `esp_bt_hub_le_peer_set()`
 *         and blocks until the controller completes the command.
 *
 * @param[in]  conn_handle  ACL connection handle carrying the transfer
 * @param[in]  params       Sync parameters for the transferred sync
 *
 * @return
 *       - ESP_OK                 Receiver was configured
 *       - ESP_ERR_INVALID_ARG    `params` is NULL
 *       - ESP_ERR_INVALID_STATE  Host is not running
 *       - ESP_ERR_NOT_FOUND      Bluedroid has no recorded peer address for `conn_handle`
 *       - ESP_ERR_NO_MEM         Internal allocation failed
 *       - ESP_ERR_TIMEOUT        Bluedroid GAP operation timed out
 *       - Others                 Stack call failed
 */
esp_err_t esp_bt_hub_le_pa_sync_receive(uint16_t conn_handle, const esp_bt_hub_le_periodic_sync_params_t *params);
#endif  /* ESP_BT_HUB_LE_PAST_SUPPORTED */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
