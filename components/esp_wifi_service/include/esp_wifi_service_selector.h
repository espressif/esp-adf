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
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN  (6)

/**
 * @brief  Bitmask of conditions that may start re-evaluation (scan and scoring)
 */
typedef enum {
    ESP_WIFI_SERVICE_SELECTOR_TRIGGER_RSSI_LOW            = (1u << 0),  /*!< Connected and RSSI below threshold_dbm */
    ESP_WIFI_SERVICE_SELECTOR_TRIGGER_PROBE_FAILED        = (1u << 1),  /*!< Probe access check failed */
    ESP_WIFI_SERVICE_SELECTOR_TRIGGER_LATENCY_DEGRADED    = (1u << 2),  /*!< Latency degraded */
    ESP_WIFI_SERVICE_SELECTOR_TRIGGER_THROUGHPUT_DEGRADED = (1u << 3),  /*!< Throughput degraded */
} esp_wifi_service_selector_trigger_mask_t;

/**
 * @brief  Candidate ranking criterion
 */
typedef enum {
    ESP_WIFI_SERVICE_SELECTOR_CRITERION_QUALITY       = 0,  /*!< Favor RSSI */
    ESP_WIFI_SERVICE_SELECTOR_CRITERION_PRIORITY      = 1,  /*!< Favor saved profile priority */
    ESP_WIFI_SERVICE_SELECTOR_CRITERION_PROBE_TRUSTED = 2,  /*!< Favor last HTTP-OK profile */
    ESP_WIFI_SERVICE_SELECTOR_CRITERION_MAX,                /*!< Number of ranking criteria */
} esp_wifi_service_selector_criterion_t;

/**
 * @brief  RSSI policy
 */
typedef struct {
    int8_t    threshold_dbm;    /*!< Below this RSSI may trigger re-eval if TRIGGER_RSSI */
    uint32_t  check_period_ms;  /*!< RSSI trigger check period; 0 disables */
} esp_wifi_service_selector_rssi_cfg_t;

typedef struct {
    int8_t    task_core;       /*!< Worker task core id, < 0 for no affinity */
    uint32_t  task_stack;      /*!< Worker stack size in bytes, 0 for default */
    uint8_t   task_prio;       /*!< Worker task priority, 0 for default */
    bool      task_ext_stack;  /*!< True to prefer external/PSRAM stack */
} esp_wifi_service_selector_task_cfg_t;

/**
 * @brief  Access-check policy for HTTP Internet availability
 */
typedef struct {
    const char                           *url;                           /*!< Primary HTTP probe URL; empty disables probing */
    uint32_t                              check_period_min;              /*!< Check interval in minutes; 0 means check once on probe start */
    uint32_t                              timeout_ms;                    /*!< HTTP client timeout per request */
    uint16_t                              expected_status;               /*!< Expected HTTP status code (e.g. 204) */
    uint32_t                              blocked_seconds;               /*!< Blacklist duration after probe failure */
    bool                                  latency_check;                 /*!< Whether latency check is enabled */
    uint32_t                              latency_max_ms;                /*!< Maximum acceptable RTT in ms, 0 disables */
    uint8_t                               latency_degraded_consecutive;  /*!< Consecutive degraded checks before event */
    esp_wifi_service_selector_task_cfg_t  task;                          /*!< Worker task configuration */
} esp_wifi_service_selector_probe_cfg_t;

/**
 * @brief  Throughput quality policy
 */
typedef struct {
    const char                           *url;                              /*!< Optional URL for throughput sampling */
    uint32_t                              check_period_min;                 /*!< Check interval minutes; 0 means once on start */
    uint32_t                              min_kbps;                         /*!< Minimum acceptable throughput in kbps */
    uint32_t                              max_bytes;                        /*!< Max bytes to read for throughput sample */
    uint32_t                              timeout_ms;                       /*!< Timeout for throughput request */
    uint32_t                              blocked_seconds;                  /*!< Blacklist duration after throughput degraded */
    uint8_t                               throughput_degraded_consecutive;  /*!< Consecutive degraded checks before event */
    esp_wifi_service_selector_task_cfg_t  task;                             /*!< Worker task configuration */
} esp_wifi_service_selector_throughput_cfg_t;

/**
 * @brief  Roam / selection policy (triggers, ranking, and optional HTTP checks)
 */
typedef struct {
    uint32_t                                    triggers_mask;                                          /*!< Bitwise OR of ::esp_wifi_service_selector_trigger_mask_t */
    esp_wifi_service_selector_criterion_t       select_order[ESP_WIFI_SERVICE_SELECTOR_CRITERION_MAX];  /*!< Selection order */
    esp_wifi_service_selector_rssi_cfg_t        rssi;                                                   /*!< RSSI policy */
    esp_wifi_service_selector_probe_cfg_t       probe;                                                  /*!< Probe policy */
    esp_wifi_service_selector_throughput_cfg_t  throughput;                                             /*!< Throughput policy */
} esp_wifi_service_selector_cfg_t;

/**
 * @brief  Candidate AP payload for candidate event
 */
typedef struct {
    uint8_t           bssid[ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN];  /*!< Candidate BSSID */
    char              ssid[33];                                    /*!< NUL-terminated SSID */
    int8_t            rssi;                                        /*!< RSSI in dBm */
    wifi_auth_mode_t  authmode;                                    /*!< AP authentication mode */
} esp_wifi_service_selector_candidate_t;

/**
 * @brief  Payload for selector switch/connect decision events
 */
typedef struct {
    uint8_t  from_bssid[ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN];  /*!< Previous connected BSSID; all-zero if not connected */
    uint8_t  to_bssid[ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN];    /*!< Target BSSID */
} esp_wifi_service_selector_switch_t;

/**
 * @brief  Payload for selector blacklist events
 */
typedef struct {
    uint8_t  bssid[ESP_WIFI_SERVICE_SELECTOR_BSSID_LEN];  /*!< Blacklisted BSSID */
} esp_wifi_service_selector_blacklisted_t;

/**
 * @brief  Payload for low-RSSI events
 */
typedef struct {
    int8_t  rssi_dbm;  /*!< Current RSSI */
} esp_wifi_service_selector_rssi_low_t;

/**
 * @brief  Payload for access-probe failure events
 */
typedef struct {
    esp_err_t  probe_err;    /*!< Probe error code */
    int32_t    status_code;  /*!< HTTP status code when available, else -1 */
} esp_wifi_service_selector_access_failed_t;

/**
 * @brief  Payload for latency degradation events
 */
typedef struct {
    uint32_t  latency_ms;  /*!< Measured latency */
    int8_t    rssi_dbm;    /*!< RSSI sampled with latency probe */
} esp_wifi_service_selector_latency_degraded_t;

/**
 * @brief  Payload for throughput degradation events
 */
typedef struct {
    int32_t  throughput_kbps;  /*!< Measured throughput; -1 when throughput probe itself fails */
    int8_t   rssi_dbm;         /*!< RSSI sampled with throughput check */
} esp_wifi_service_selector_throughput_degraded_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
