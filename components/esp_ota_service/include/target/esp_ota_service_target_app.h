/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_target.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  App partition OTA target configuration
 */
typedef struct {
    bool  bulk_flash_erase;  /*!< Pre-erase the full OTA partition before writing
                              *   (passes OTA_WITH_SEQUENTIAL_WRITES to esp_ota_begin).
                              *
                              *   When true the entire OTA slot is erased once before
                              *   the first write, eliminating per-sector erase overhead
                              *   during the data transfer.  Combined with the internal-
                              *   SRAM write buffer used by esp_ota_service this yields the
                              *   fastest possible write path.
                              *
                              *   When false (default) esp_ota_begin receives
                              *   OTA_SIZE_UNKNOWN and erases flash on demand, one sector
                              *   at a time, which is slower but avoids the upfront erase
                              *   stall.  Resume mode always uses on-demand erase
                              *   regardless of this flag. */
} esp_ota_service_target_app_cfg_t;

/**
 * @brief  Create an app-partition OTA target
 *
 *         Writes firmware to the next available OTA application slot using the
 *         esp_ota_begin/write/end API.  On commit(), sets the new image as the
 *         active boot partition.  On abort(), calls esp_ota_abort() to invalidate
 *         any partially-written data.
 *
 *         The partition_label argument passed to open() is ignored — the target
 *         always selects the next OTA app slot automatically.
 *
 * @param[in]   cfg      Configuration. NULL uses all defaults.
 * @param[out]  out_tgt  On success, receives the new target; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_tgt is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_target_app_create(const esp_ota_service_target_app_cfg_t *cfg, esp_ota_service_target_t **out_tgt);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
