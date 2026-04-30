/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  One persisted resume point (one per upgrade item index)
 *
 *         The store keys records by @c item_index so multiple concurrent
 *         resumable items do not overwrite each other.
 */
typedef struct {
    int8_t    item_index;     /*!< Upgrade item index this record belongs to */
    uint32_t  written_bytes;  /*!< Bytes already written to the target */
    char      uri[256];       /*!< Source URI (truncated to 255 chars + NUL; the
                               *   service refuses longer URIs to avoid silent
                               *   truncation). */
} esp_ota_service_resume_point_t;

/**
 * @brief  Save / overwrite the resume point for @c point->item_index
 *
 * @param[in]  point  Resume point data to persist. @c item_index must be >= 0.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @a point is NULL, item_index is negative, or URI is too long
 *       - Other                Error code returned by NVS
 */
esp_err_t esp_ota_service_resume_save(const esp_ota_service_resume_point_t *point);

/**
 * @brief  Load the resume point saved for a specific item index
 *
 * @param[in]   item_index  Upgrade item index to look up (must be >= 0)
 * @param[out]  point       Receives the loaded record (zeroed before read)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @a point is NULL or @a item_index is negative
 *       - ESP_ERR_NVS_NOT_FOUND  If no record exists for this index (first time / cleared)
 *       - Other                  Error code returned by NVS
 */
esp_err_t esp_ota_service_resume_load(int item_index, esp_ota_service_resume_point_t *point);

/**
 * @brief  Clear the resume record for one item index
 *
 *         Used after a successful commit for that item, or when the caller
 *         decides to restart that item from scratch.
 *
 * @param[in]  item_index  Upgrade item index whose record should be erased
 *
 * @return
 *       - ESP_OK  On success, or if no record existed
 *       - Other   Error code returned by NVS
 */
esp_err_t esp_ota_service_resume_clear(int item_index);

/**
 * @brief  Clear every resume record in the namespace
 *
 *         Typically called by the OTA session before a brand-new run, so
 *         stale records from a previous configuration cannot be picked up
 *         by mistake.
 *
 * @return
 *       - ESP_OK  On success, or if the resume namespace does not exist
 *       - Other   Error code returned by NVS
 */
esp_err_t esp_ota_service_resume_clear_all(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
