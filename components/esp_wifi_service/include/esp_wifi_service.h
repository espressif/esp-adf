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

#include "esp_err.h"
#include "esp_service.h"
#include "esp_wifi_service_profile_mgr.h"
#include "esp_wifi_service_prov.h"
#include "esp_wifi_service_selector.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Wi-Fi service event identifiers
 */
typedef enum {
    ESP_WIFI_SERVICE_EVENT_CONNECTED                    = 101,  /*!< Wi-Fi service reports connected */
    ESP_WIFI_SERVICE_EVENT_DISCONNECTED                 = 102,  /*!< Wi-Fi service reports disconnected */
    ESP_WIFI_SERVICE_EVENT_PROV_STARTED                 = 103,  /*!< Provisioning transport started */
    ESP_WIFI_SERVICE_EVENT_PROV_STOPPED                 = 104,  /*!< Provisioning transport stopped */
    ESP_WIFI_SERVICE_EVENT_PROV_PEER_CONNECTED          = 105,  /*!< Provisioning peer connected */
    ESP_WIFI_SERVICE_EVENT_PROV_PEER_DISCONNECTED       = 106,  /*!< Provisioning peer disconnected */
    ESP_WIFI_SERVICE_EVENT_PROV_CREDENTIAL_RECEIVED     = 107,  /*!< Provisioning credential received */
    ESP_WIFI_SERVICE_EVENT_PROV_CUSTOM_DATA_RECEIVED    = 108,  /*!< Provisioning custom data received */
    ESP_WIFI_SERVICE_EVENT_PROV_ERROR                   = 109,  /*!< Provisioning runtime error */
    ESP_WIFI_SERVICE_EVENT_STA_CONFIG                   = 110,  /*!< STA config can be adjusted before connect */
    ESP_WIFI_SERVICE_EVENT_STA_GOT_IP                   = 111,  /*!< Wi-Fi service reports station got IP */
    ESP_WIFI_SERVICE_EVENT_STA_LOST_IP                  = 112,  /*!< Wi-Fi service reports station lost IP */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_CANDIDATE           = 113,  /*!< Selector candidate chosen from scan result */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCHING           = 114,  /*!< Selector decided to switch/connect */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_SWITCH_FAILED       = 115,  /*!< Selector switch/connect failed */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_BLACKLISTED         = 116,  /*!< Selector blacklisted one BSSID */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_ACCESS_FAILED       = 117,  /*!< Selector probe access check failed */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_LATENCY_DEGRADED    = 118,  /*!< Selector latency check degraded */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_THROUGHPUT_DEGRADED = 119,  /*!< Selector throughput check degraded */
    ESP_WIFI_SERVICE_EVENT_SELECTOR_RSSI_LOW            = 120,  /*!< Selector RSSI check is below threshold */
} esp_wifi_service_event_t;

/**
 * @brief  Opaque Wi-Fi service handle
 */
typedef struct esp_wifi_service esp_wifi_service_t;

/**
 * @brief  Wi-Fi service configuration
 */
typedef struct {
    const char                            *name;             /*!< Service instance name for service manager */
    esp_wifi_service_profile_mgr_t         profile_manager;  /*!< Required profile manager handle; created and owned by the caller */
    esp_wifi_service_prov_t               *prov_list;        /*!< Provisioning handle array owned by application */
    size_t                                 prov_num;         /*!< Number of entries in prov_list */
    const esp_wifi_service_selector_cfg_t *selector_policy;  /*!< Selector policy; NULL uses selector built-in defaults */
} esp_wifi_service_config_t;

/**
 * @brief  Create Wi-Fi service instance
 *
 * @note  The caller is responsible for creating the profile manager before calling this function and
 *        destroying it after @c esp_wifi_service_destroy returns. The same handle should be passed to
 *        each provisioning config so that all components share one profile store.
 *
 * @param[in]   cfg          Required configuration; @c profile_manager must be a valid, initialised handle
 * @param[out]  out_service  Created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_NO_MEM       Out of memory
 */
esp_err_t esp_wifi_service_create(const esp_wifi_service_config_t *cfg, esp_wifi_service_t **out_service);

/**
 * @brief  Destroy the service
 *
 * @param[in]  service  Service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_wifi_service_destroy(esp_wifi_service_t *service);

/**
 * @brief  Get profile manager handle owned by service
 *
 * @param[in]   service      Service handle
 * @param[out]  manager_out  Profile manager handle owned by service
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL argument
 */
esp_err_t esp_wifi_service_get_profile_manager(esp_wifi_service_t *service, esp_wifi_service_profile_mgr_t *manager_out);

/**
 * @brief  Start all provisioning instances
 *
 * @param[in]  service  Service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - Others               Provisioning-specific error
 */
esp_err_t esp_wifi_service_start_provisioning(esp_wifi_service_t *service);

/**
 * @brief  Stop all started provisioning instances
 *
 * @param[in]  service  Service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - Others               Provisioning-specific error
 */
esp_err_t esp_wifi_service_stop_provisioning(esp_wifi_service_t *service);

/**
 * @brief  Query provisioning running state
 *
 * @param[in]   service      Service handle
 * @param[out]  running_out  True if any provisioning instance is running
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_wifi_service_is_provisioning_running(esp_wifi_service_t *service, bool *running_out);

/**
 * @brief  Save a Wi-Fi profile and request connection to the specified SSID
 *
 * @note  This API stores or updates the profile as enabled, stops provisioning, starts
 *        the selector if needed, and asks the selector to connect to the specified saved
 *        profile directly without running a scan/re-evaluation cycle.
 *
 * @param[in]  service   Service handle
 * @param[in]  ssid      NUL-terminated SSID
 * @param[in]  password  NUL-terminated password; NULL is treated as an empty password
 * @param[in]  prio      Profile priority, 0 to ::ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX
 * @param[in]  wait_sec  Seconds to wait for STA got IP; 0 returns after the request is accepted
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_TIMEOUT      @p wait_sec is non-zero and STA did not get IP in time
 *       - Others               From profile manager or selector
 */
esp_err_t esp_wifi_service_request_connect(esp_wifi_service_t *service, char *ssid, char *password, uint8_t prio,
                                           uint32_t wait_sec);

/**
 * @brief  Request one Wi-Fi selector re-evaluation cycle
 *
 * @note  This API is asynchronous. It returns after the request is accepted, not after scan
 *        results are evaluated or a connection decision is completed.
 *        If the selector is already running and no re-evaluation is in flight, this API attempts
 *        to start a Wi-Fi scan before returning. If a scan/re-evaluation is already in flight, the
 *        request is marked pending and runs after the current cycle finishes. If the selector is not
 *        running but saved profiles exist, the service starts the selector and schedules evaluation.
 *
 * @param[in]  service  Service handle
 *
 * @return
 *       - ESP_OK               On success or when the request is throttled
 *       - ESP_ERR_INVALID_ARG  service is NULL
 *       - ESP_ERR_NOT_FOUND    No Wi-Fi profile is available to evaluate
 *       - Others               Selector-specific error
 */
esp_err_t esp_wifi_service_request_reeval(esp_wifi_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
