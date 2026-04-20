/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Roam blacklist helpers (private, not public API).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque blacklist handle
 */
typedef struct wifi_sel_blacklist *wifi_sel_blacklist_handle_t;

/**
 * @brief  Create blacklist handle
 *
 * @param[out]  out_handle  Blacklist handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t wifi_sel_blacklist_init(wifi_sel_blacklist_handle_t *out_handle);

/**
 * @brief  Destroy blacklist handle
 *
 * @param[in]  handle  Blacklist handle
 */
void wifi_sel_blacklist_deinit(wifi_sel_blacklist_handle_t handle);

/**
 * @brief  Whether BSSID is currently blacklisted (non-expired slot)
 *
 * @param[in]  handle  Blacklist handle
 * @param[in]  bssid   BSSID (6 bytes)
 *
 * @return
 *       - true  if blacklisted
 */
bool wifi_sel_blacklisted(wifi_sel_blacklist_handle_t handle, const uint8_t *bssid);

/**
 * @brief  Add or refresh blacklist entry for a BSSID
 *
 * @param[in]  handle           Blacklist handle
 * @param[in]  bssid            BSSID (6 bytes)
 * @param[in]  blocked_seconds  Duration until expiry from now
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_INVALID_STATE  Blacklist is full
 */
esp_err_t wifi_sel_blacklist_add(wifi_sel_blacklist_handle_t handle, const uint8_t *bssid, uint32_t blocked_seconds);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
