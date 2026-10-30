/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_bt_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Classic discoverable level voted by one client
 *
 *         Values match `esp_bt_discovery_mode_t` and increase from none through limited to general.
 */
typedef enum {
    ESP_BT_HUB_DISCOVERABLE_NONE = 0,  /*!< Not discoverable */
    ESP_BT_HUB_DISCOVERABLE_LIMITED,   /*!< Limited discoverable */
    ESP_BT_HUB_DISCOVERABLE_GENERAL,   /*!< General discoverable */
} esp_bt_hub_discoverable_t;

/** @brief Opaque exclusive Classic discovery lease */
typedef struct esp_bt_hub_discovery_lease *esp_bt_hub_discovery_handle_t;

#if ESP_BT_HUB_CLASSIC_SUPPORTED
/**
 * @brief  One Classic inquiry result
 *
 *         Filled by `esp_bt_hub_classic_inquiry_result_parse()`.
 */
typedef struct {
    uint8_t   addr[6];                            /*!< Peer address */
    char      name[ESP_BT_HUB_MAX_DEV_NAME_LEN];  /*!< Complete EIR name, else short EIR or BDNAME */
    uint32_t  cod;                                /*!< Class of Device when `has_cod` is true */
    int8_t    rssi;                               /*!< RSSI in dBm when `has_rssi` is true */
    bool      has_cod;                            /*!< True when Class of Device was present */
    bool      has_rssi;                           /*!< True when RSSI was present */
} esp_bt_hub_classic_inquiry_result_t;

/**
 * @brief  Set one client's Classic scan-mode vote
 *
 *         Connectable votes are combined with OR. Discoverable votes use the highest level. The merged
 *         mode is written to the controller. `esp_bt_hub_client_close()` drops the vote and applies the
 *         new mode. A failed update restores the previous vote. If that restore also fails, deinit stays
 *         blocked until a later scan-mode update or client close succeeds. Not ISR-safe. May block.
 *
 * @param[in]  client        Client that owns this vote
 * @param[in]  connectable   True to vote connectable
 * @param[in]  discoverable  Discoverable level to vote
 *
 * @return
 *       - ESP_OK                 Vote is stored and the merged mode was applied
 *       - ESP_ERR_INVALID_ARG    `client` is NULL or unknown, or `discoverable` is invalid
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - Others                 Controller update failed; the previous vote is restored when possible
 */
esp_err_t esp_bt_hub_classic_scan_mode_set(esp_bt_hub_client_handle_t client, bool connectable,
                                           esp_bt_hub_discoverable_t discoverable);

/**
 * @brief  Start Classic inquiry for one client
 *
 *         Only one client may own inquiry. Results are delivered to every Classic GAP subscriber.
 *         When `inquiry_len` elapses the controller stops inquiry, and the lease remains until
 *         `esp_bt_hub_classic_discovery_stop()`. Not ISR-safe. May block.
 *
 * @param[in]   client         Client that will own the lease
 * @param[in]   inquiry_mode   `esp_bt_inq_mode_t` value: 0 general, 1 limited
 * @param[in]   inquiry_len    Inquiry length in 1.28 s units
 * @param[in]   num_responses  Maximum responses. 0 means no limit
 * @param[out]  out_discovery  Exclusive discovery lease. Unchanged on failure
 *
 * @return
 *       - ESP_OK                 Inquiry started and this client owns it
 *       - ESP_ERR_INVALID_ARG    `client` or `out_discovery` is NULL, or `client` is unknown
 *       - ESP_ERR_INVALID_STATE  Host is not running, or another client already owns inquiry
 *       - Others                 Controller update failed
 */
esp_err_t esp_bt_hub_classic_discovery_start(esp_bt_hub_client_handle_t client, uint8_t inquiry_mode,
                                             uint8_t inquiry_len, uint8_t num_responses,
                                             esp_bt_hub_discovery_handle_t *out_discovery);

/**
 * @brief  Stop Classic inquiry and release its lease
 *
 *         The lease must be active or faulted. On success it is released. On failure it remains valid
 *         and becomes faulted. Not ISR-safe. May block.
 *
 * @param[in]  discovery  Discovery lease returned by start
 *
 * @return
 *       - ESP_OK                 Inquiry was stopped and the lease was released
 *       - ESP_ERR_INVALID_ARG    `discovery` is NULL, stale, or neither active nor faulted
 *       - ESP_ERR_INVALID_STATE  Host context does not exist
 *       - Others                 Controller update failed; the lease remains valid
 */
esp_err_t esp_bt_hub_classic_discovery_stop(esp_bt_hub_discovery_handle_t discovery);

/**
 * @brief  Parse an `ESP_BT_GAP_DISC_RES_EVT` parameter into an inquiry result
 *
 *         `param` is valid only until the Classic GAP callback returns. A complete EIR local name
 *         overrides a short EIR name and BDNAME. Not ISR-safe.
 *
 * @param[in]   param  `esp_bt_gap_cb_param_t` from `ESP_BT_GAP_DISC_RES_EVT`
 * @param[out]  out    Parsed result
 *
 * @return
 *       - ESP_OK               Result was filled
 *       - ESP_ERR_INVALID_ARG  `param` or `out` is NULL
 */
esp_err_t esp_bt_hub_classic_inquiry_result_parse(const void *param, esp_bt_hub_classic_inquiry_result_t *out);
#endif  /* ESP_BT_HUB_CLASSIC_SUPPORTED */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
