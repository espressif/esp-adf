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
 * @brief  Data partition OTA target configuration
 */
typedef struct {
    bool  pre_erased;  /*!< Set true if the caller has already erased the partition.
                        *   The target will skip the erase step in open(). */
} esp_ota_service_target_data_cfg_t;

/**
 * @brief  Create a raw data-partition OTA target
 *
 *         Erases and writes raw bytes to a named data partition using
 *         esp_partition_find_first/erase/write.  The partition_label argument
 *         passed to open() selects which partition to update.
 *
 *         commit() is a no-op (data is written immediately by write()).
 *         abort() erases the partition to ensure no partial data remains.
 *
 *         This target does NOT support breakpoint resume: open() erases the
 *         whole partition and @c set_write_offset is intentionally left NULL.
 *         Pairing it with @c esp_ota_upgrade_item_t::resumable=true is rejected
 *         at esp_ota_service_set_upgrade_list() time with ESP_ERR_NOT_SUPPORTED.
 *
 * @param[in]   cfg      Configuration. NULL uses all defaults.
 * @param[out]  out_tgt  On success, receives the new target; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_tgt is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_target_data_create(const esp_ota_service_target_data_cfg_t *cfg, esp_ota_service_target_t **out_tgt);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
