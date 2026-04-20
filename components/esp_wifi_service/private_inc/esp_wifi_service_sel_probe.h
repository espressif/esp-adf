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
 * @brief  Opaque HTTP probe monitor handle
 */
typedef struct wifi_sel_probe *wifi_sel_probe_handle_t;

/**
 * @brief  HTTP probe monitor event identifiers
 */
typedef enum {
    WIFI_SEL_PROBE_EVENT_ACCESS_FAILED = 0,  /*!< HTTP probe failed or returned unexpected status */
    WIFI_SEL_PROBE_EVENT_LATENCY_DEGRADED,   /*!< HTTP probe latency exceeded policy threshold */
} wifi_sel_probe_event_t;

/**
 * @brief  Payload for ::WIFI_SEL_PROBE_EVENT_ACCESS_FAILED
 */
typedef struct {
    esp_err_t  probe_err;    /*!< Probe error code */
    int32_t    status_code;  /*!< HTTP status code when available, else -1 */
} wifi_sel_probe_evt_access_failed_t;

/**
 * @brief  Payload for ::WIFI_SEL_PROBE_EVENT_LATENCY_DEGRADED
 */
typedef struct {
    uint32_t  latency_ms;  /*!< Measured HTTP latency in ms */
    int8_t    rssi_dbm;    /*!< Current station RSSI in dBm */
} wifi_sel_probe_evt_latency_degraded_t;

/**
 * @brief  HTTP probe monitor event callback
 *
 * @param[in]  evt        Probe event identifier
 * @param[in]  payload    Event payload; type depends on @p evt
 * @param[in]  user_data  User context supplied by wifi_sel_probe_set_event_cb()
 */
typedef void (*wifi_sel_probe_event_cb_t)(wifi_sel_probe_event_t evt, void *payload, void *user_data);

/**
 * @brief  Create HTTP probe monitor
 *
 * @param[in]   cfg         Probe policy
 * @param[out]  out_handle  Probe monitor handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid configuration
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t wifi_sel_probe_init(const esp_wifi_service_selector_probe_cfg_t *cfg, wifi_sel_probe_handle_t *out_handle);

/**
 * @brief  Destroy HTTP probe monitor
 *
 * @param[in]  handle  Probe monitor handle
 */
void wifi_sel_probe_deinit(wifi_sel_probe_handle_t handle);

/**
 * @brief  Start HTTP probe monitor worker and timer
 *
 * @param[in]  handle  Probe monitor handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid handle
 *       - ESP_ERR_INVALID_STATE  Handle is being deinitialized
 *       - ESP_ERR_NO_MEM         Worker task creation failed
 */
esp_err_t wifi_sel_probe_start(wifi_sel_probe_handle_t handle);

/**
 * @brief  Stop HTTP probe monitor worker and timer
 *
 * @param[in]  handle  Probe monitor handle
 *
 * @return
 *       - ESP_OK               On success or already stopped
 *       - ESP_ERR_INVALID_ARG  Invalid handle
 */
esp_err_t wifi_sel_probe_stop(wifi_sel_probe_handle_t handle);

/**
 * @brief  Set HTTP probe monitor event callback
 *
 * @param[in]  handle     Probe monitor handle
 * @param[in]  cb         Event callback, NULL to clear
 * @param[in]  user_data  User context passed to @p cb
 */
void wifi_sel_probe_set_event_cb(wifi_sel_probe_handle_t handle, wifi_sel_probe_event_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
