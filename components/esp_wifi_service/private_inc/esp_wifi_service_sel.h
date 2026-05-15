/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi_service_profile_mgr.h"
#include "esp_wifi_service_selector.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Selector handle
 */
typedef void *esp_wifi_service_selector_handle_t;

/**
 * @brief  Selector internal event IDs
 */
typedef enum {
    ESP_WIFI_SERVICE_SELECTOR_EVT_CANDIDATE = 0,        /*!< Candidate selected from scan result */
    ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCHING,            /*!< Selector decided to switch/connect */
    ESP_WIFI_SERVICE_SELECTOR_EVT_SWITCH_FAILED,        /*!< Switch/connect failed */
    ESP_WIFI_SERVICE_SELECTOR_EVT_BLACKLISTED,          /*!< BSSID blacklisted */
    ESP_WIFI_SERVICE_SELECTOR_EVT_RSSI_LOW,             /*!< RSSI below threshold */
    ESP_WIFI_SERVICE_SELECTOR_EVT_ACCESS_FAILED,        /*!< Access check failed */
    ESP_WIFI_SERVICE_SELECTOR_EVT_LATENCY_DEGRADED,     /*!< Latency degraded */
    ESP_WIFI_SERVICE_SELECTOR_EVT_THROUGHPUT_DEGRADED,  /*!< Throughput degraded */
} esp_wifi_service_selector_event_t;

/**
 * @brief  Selector -> service event callback
 *
 *         The selector emits only selection-decision related events
 *         (candidate, switching, blacklist, switch failure, connectivity).
 *
 * @param[in]  evt        Selector event id
 * @param[in]  data       Payload pointer; lifetime is bounded by the callback
 * @param[in]  user_data  User data supplied in ::esp_wifi_service_selector_config_t
 */
typedef void (*esp_wifi_service_selector_event_cb_t)(esp_wifi_service_selector_event_t evt, void *data, void *user_data);

/**
 * @brief  Initialization parameters for ::esp_wifi_service_selector_init
 */
typedef struct {
    esp_wifi_service_profile_mgr_t         profile_manager;  /*!< Profile manager from ::esp_wifi_service_profile_mgr_init */
    const esp_wifi_service_selector_cfg_t *selector_cfg;     /*!< Selector configuration; NULL uses built-in defaults */
    esp_wifi_service_selector_event_cb_t   event_cb;         /*!< Selector event callback */
    void                                  *user_data;        /*!< User data passed to @c event_cb */
} esp_wifi_service_selector_config_t;

/**
 * @brief  Allocate selector and load profiles via profile manager
 *
 * @param[in]   cfg         Configuration (must provide @c profile_manager; @c selector_cfg may be NULL for defaults).
 * @param[out]  out_handle  Selector handle on success
 *
 * @return
 *       - ESP_OK     On success
 *       - ESP_ERR_*  On invalid argument, load failure, or out of memory
 */
esp_err_t esp_wifi_service_selector_init(const esp_wifi_service_selector_config_t *cfg,
                                         esp_wifi_service_selector_handle_t *out_handle);

/**
 * @brief  Stop selector if started, unregister handlers, free controller
 *
 * @param[in]  handle  Selector handle from ::esp_wifi_service_selector_init
 */
void esp_wifi_service_selector_deinit(esp_wifi_service_selector_handle_t handle);

/**
 * @brief  Start selector logic (register WIFI / IP handlers, timers, optional connectivity probe).
 *
 * @param[in]  handle  Selector handle
 *
 * @return
 *       - ESP_OK     On success
 *       - ESP_ERR_*  On failure
 */
esp_err_t esp_wifi_service_selector_start(esp_wifi_service_selector_handle_t handle);

/**
 * @brief  Stop timers, unregister WIFI / IP handlers, and tear down connectivity probe
 *
 * @param[in]  handle  Selector handle
 */
void esp_wifi_service_selector_stop(esp_wifi_service_selector_handle_t handle);

/**
 * @brief  Query whether selector logic is started
 *
 * @param[in]   handle       Selector handle
 * @param[out]  started_out  True if selector is started
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_wifi_service_selector_is_started(esp_wifi_service_selector_handle_t handle, bool *started_out);

/**
 * @brief  Request one scan + selection cycle
 *
 * @param[in]  handle  Selector handle
 *
 * @return
 *       - ESP_OK     On success or throttled
 *       - ESP_ERR_*  On invalid state
 */
esp_err_t esp_wifi_service_selector_request_reeval(esp_wifi_service_selector_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
