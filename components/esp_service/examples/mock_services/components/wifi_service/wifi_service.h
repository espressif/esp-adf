/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_service.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define WIFI_DOMAIN  "wifi"
typedef struct wifi_service wifi_service_t;

enum {
    WIFI_EVT_CONNECTING     = 1,  /*!< Attempting to connect */
    WIFI_EVT_CONNECTED      = 2,  /*!< Successfully connected */
    WIFI_EVT_DISCONNECTED   = 3,  /*!< Disconnected from AP; carries wifi_disconnected_payload_t */
    WIFI_EVT_GOT_IP         = 4,  /*!< IP address obtained; carries wifi_ip_info_t */
    WIFI_EVT_SCAN_DONE      = 5,  /*!< AP scan completed; carries wifi_scan_result_t */
    WIFI_EVT_RSSI_UPDATE    = 6,  /*!< RSSI measurement update */
    WIFI_EVT_CONNECT_FAILED = 7,  /*!< Connection failed after all retries; carries wifi_connect_failed_payload_t */
};

/**
 * @brief  Disconnection / connection-failure reason — mirrors wifi_service_disconnect_reason_t
 *         from the real ADF wifi_service component
 */
typedef enum {
    WIFI_DISCONNECT_REASON_UNKNOWN      = 0,  /*!< Unknown reason */
    WIFI_DISCONNECT_REASON_COM_ERROR    = 1,  /*!< Communication/radio error */
    WIFI_DISCONNECT_REASON_AUTH_ERROR   = 2,  /*!< Authentication failed (wrong password) */
    WIFI_DISCONNECT_REASON_AP_NOT_FOUND = 3,  /*!< AP SSID not found */
    WIFI_DISCONNECT_REASON_BY_USER      = 4,  /*!< Disconnected explicitly by caller */
} wifi_disconnect_reason_t;

/**
 * @brief  Payload for WIFI_EVT_DISCONNECTED
 */
typedef struct {
    wifi_disconnect_reason_t  reason;  /*!< Why the connection was lost */
} wifi_disconnected_payload_t;

/**
 * @brief  Payload for WIFI_EVT_CONNECT_FAILED
 */
typedef struct {
    uint8_t                   retry_count;  /*!< Number of connection attempts made */
    wifi_disconnect_reason_t  reason;       /*!< Reason the last attempt failed */
} wifi_connect_failed_payload_t;

/**
 * @brief  WiFi connection status
 */
typedef enum {
    WIFI_SVC_STATUS_IDLE         = 0,  /*!< Not connected */
    WIFI_SVC_STATUS_SCANNING     = 1,  /*!< Scanning for APs */
    WIFI_SVC_STATUS_CONNECTING   = 2,  /*!< Connecting to AP */
    WIFI_SVC_STATUS_CONNECTED    = 3,  /*!< Connected to AP */
    WIFI_SVC_STATUS_DISCONNECTED = 4,  /*!< Disconnected */
} wifi_svc_status_t;

/**
 * @brief  Single AP scan record
 */
typedef struct {
    char     ssid[33];  /*!< AP SSID (null-terminated) */
    int8_t   rssi;      /*!< Signal strength in dBm */
    uint8_t  channel;   /*!< WiFi channel number */
} wifi_service_ap_record_t;

/**
 * @brief  WiFi scan result
 */
typedef struct {
    uint8_t                   count;   /*!< Number of APs found */
    wifi_service_ap_record_t  aps[8];  /*!< AP records (up to 8) */
} wifi_scan_result_t;

/**
 * @brief  IP address information
 */
typedef struct {
    char  ip[16];       /*!< IP address string */
    char  gateway[16];  /*!< Gateway address string */
    char  netmask[16];  /*!< Netmask string */
} wifi_ip_info_t;

/**
 * @brief  WiFi service configuration
 */
typedef struct {
    const char *ssid;        /*!< Target AP SSID */
    const char *password;    /*!< AP password */
    uint8_t     max_retry;   /*!< Maximum connection retry attempts */
    int         sim_rounds;  /*!< Random connect/disconnect cycles (0 = no sim) */
} wifi_service_cfg_t;

esp_err_t wifi_service_create(const wifi_service_cfg_t *cfg, wifi_service_t **out_svc);
esp_err_t wifi_service_destroy(wifi_service_t *svc);
esp_err_t wifi_service_connect(wifi_service_t *svc);
esp_err_t wifi_service_disconnect(wifi_service_t *svc);
esp_err_t wifi_service_scan(wifi_service_t *svc, wifi_scan_result_t *out);
esp_err_t wifi_service_get_status(const wifi_service_t *svc, wifi_svc_status_t *out);
esp_err_t wifi_service_get_ssid(const wifi_service_t *svc, const char **out_ssid);
esp_err_t wifi_service_get_ip(const wifi_service_t *svc, wifi_ip_info_t *out);
esp_err_t wifi_service_get_rssi(const wifi_service_t *svc, int8_t *out_rssi);
esp_err_t wifi_service_set_connecting(wifi_service_t *svc);
esp_err_t wifi_service_set_connected(wifi_service_t *svc, const wifi_ip_info_t *ip, int8_t rssi);
esp_err_t wifi_service_set_disconnected(wifi_service_t *svc, wifi_disconnect_reason_t reason);
esp_err_t wifi_service_reconnect(wifi_service_t *svc, const char *ssid, const char *password);
int wifi_service_get_events_sent(const wifi_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
