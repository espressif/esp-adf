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
#include "esp_wifi_service_selector.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque throughput monitor handle
 */
typedef struct wifi_sel_throughput *wifi_sel_throughput_handle_t;

/**
 * @brief  Throughput monitor event identifiers
 */
typedef enum {
    WIFI_SEL_THROUGHPUT_EVENT_DEGRADED = 0,  /*!< Throughput dropped below configured threshold */
} wifi_sel_throughput_event_t;

/**
 * @brief  Payload for ::WIFI_SEL_THROUGHPUT_EVENT_DEGRADED
 */
typedef struct {
    int32_t  throughput_kbps;  /*!< Measured throughput in kbps; -1 when probe failed */
    int8_t   rssi_dbm;         /*!< Current station RSSI in dBm */
} wifi_sel_throughput_evt_degraded_t;

/**
 * @brief  Throughput monitor event callback
 *
 * @param[in]  evt        Throughput event identifier
 * @param[in]  payload    Event payload; type depends on @p evt
 * @param[in]  user_data  User context supplied by wifi_sel_throughput_set_event_cb()
 */
typedef void (*wifi_sel_throughput_event_cb_t)(wifi_sel_throughput_event_t evt, void *payload, void *user_data);

/**
 * @brief  Create throughput monitor
 *
 * @param[in]   cfg         Throughput policy
 * @param[out]  out_handle  Throughput monitor handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid configuration
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t wifi_sel_throughput_init(const esp_wifi_service_selector_throughput_cfg_t *cfg, wifi_sel_throughput_handle_t *out_handle);

/**
 * @brief  Destroy throughput monitor
 *
 * @param[in]  handle  Throughput monitor handle
 */
void wifi_sel_throughput_deinit(wifi_sel_throughput_handle_t handle);

/**
 * @brief  Start throughput monitor worker and timer
 *
 * @param[in]  handle  Throughput monitor handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle
 *       - ESP_ERR_INVALID_STATE  Handle is being deinitialized
 *       - ESP_ERR_NO_MEM         Worker task creation failed
 */
esp_err_t wifi_sel_throughput_start(wifi_sel_throughput_handle_t handle);

/**
 * @brief  Stop throughput monitor worker and timer
 *
 * @param[in]  handle  Throughput monitor handle
 *
 * @return
 *       - ESP_OK               On success or already stopped
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t wifi_sel_throughput_stop(wifi_sel_throughput_handle_t handle);

/**
 * @brief  Set throughput monitor event callback
 *
 * @param[in]  handle     Throughput monitor handle
 * @param[in]  cb         Event callback, NULL to clear
 * @param[in]  user_data  User context passed to @p cb
 */
void wifi_sel_throughput_set_event_cb(wifi_sel_throughput_handle_t handle, wifi_sel_throughput_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
