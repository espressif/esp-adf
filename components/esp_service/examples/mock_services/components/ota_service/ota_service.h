/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * OTA service for PC testing — ASYNC mode with domain events.
 */

#pragma once

#include "esp_service.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define OTA_DOMAIN  "ota"
typedef struct ota_service ota_service_t;

enum {
    OTA_EVT_CHECK_START      = 1,  /*!< OTA version check started */
    OTA_EVT_UPDATE_AVAILABLE = 2,  /*!< New firmware found; carries ota_check_result_t */
    OTA_EVT_PROGRESS         = 3,  /*!< Download progress; carries ota_progress_t */
    OTA_EVT_COMPLETE         = 4,  /*!< Single item succeeded (mirrors OTA_EVENT_ITEM_SUCCESS) */
    OTA_EVT_ERROR            = 5,  /*!< Item failed; carries ota_failure_t */
    OTA_EVT_NO_UPDATE        = 6,  /*!< No newer firmware found */
    OTA_EVT_ITEM_BEGIN       = 7,  /*!< Partition upgrade about to start (mirrors OTA_EVENT_ITEM_BEGIN) */
    OTA_EVT_ALL_SUCCESS      = 8,  /*!< All upgrade items completed successfully (mirrors OTA_EVENT_ALL_SUCCESS) */
    OTA_EVT_ABORTED          = 9,  /*!< Upgrade aborted (mirrors OTA_EVENT_ABORTED) */
};

/**
 * @brief  OTA update status
 */
typedef enum {
    OTA_STATUS_IDLE        = 0,  /*!< No OTA activity */
    OTA_STATUS_CHECKING    = 1,  /*!< Checking for updates */
    OTA_STATUS_DOWNLOADING = 2,  /*!< Downloading firmware */
    OTA_STATUS_APPLYING    = 3,  /*!< Applying firmware */
    OTA_STATUS_COMPLETED   = 4,  /*!< Update completed */
    OTA_STATUS_FAILED      = 5,  /*!< Update failed */
} ota_status_t;

/**
 * @brief  OTA check result payload
 */
typedef struct {
    bool      update_available;  /*!< True if a newer firmware was found */
    uint32_t  new_version;       /*!< Version number of available firmware */
} ota_check_result_t;

/**
 * @brief  OTA progress payload — mirrors ota_event_t from the real ADF esp_ota_service
 */
typedef struct {
    int       item_index;           /*!< Upgrade item index (0-based; -1 for session-level events) */
    char      partition_label[17];  /*!< Partition label (e.g. "ota_0"); empty string if unknown */
    uint8_t   percent;              /*!< Download progress 0-100 */
    uint32_t  bytes_written;        /*!< Bytes written so far */
    uint32_t  total_bytes;          /*!< Total firmware size in bytes */
} ota_progress_t;

/**
 * @brief  OTA failure payload
 */
typedef struct {
    esp_err_t  error_code;   /*!< Error code */
    char       message[64];  /*!< Human-readable error message */
} ota_failure_t;

/**
 * @brief  OTA service configuration
 */
typedef struct {
    const char *url;                /*!< Firmware server URL */
    uint32_t    current_version;    /*!< Currently running firmware version */
    uint32_t    check_interval_ms;  /*!< Interval between automatic checks in ms */
    int         sim_max_updates;    /*!< Max simulated OTA updates (0 = no sim) */
} ota_service_cfg_t;

esp_err_t ota_service_create(const ota_service_cfg_t *cfg, ota_service_t **out_svc);
esp_err_t ota_service_destroy(ota_service_t *svc);
esp_err_t ota_service_check_update(ota_service_t *svc, ota_check_result_t *result);
esp_err_t ota_service_start_update(ota_service_t *svc);
esp_err_t ota_service_get_progress(const ota_service_t *svc, ota_progress_t *out);
esp_err_t ota_service_get_status(const ota_service_t *svc, ota_status_t *out);
esp_err_t ota_service_get_version(const ota_service_t *svc, uint32_t *current, uint32_t *latest);
int ota_service_get_events_sent(const ota_service_t *svc);
int ota_service_get_update_count(const ota_service_t *svc);
int ota_service_get_sim_max_updates(const ota_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
