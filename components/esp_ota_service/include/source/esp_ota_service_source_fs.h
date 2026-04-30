/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_source.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Local filesystem OTA source configuration
 */
typedef struct {
    int  buf_size;  /*!< Internal cache size in bytes. 0 = use default. */
} esp_ota_service_source_fs_cfg_t;

/**
 * @brief  Create a local filesystem OTA source
 *
 *         Accepts file:// URIs (e.g., "file://sdcard/firmware.bin").
 *         The filesystem must be already mounted before open() is called.
 *         Supports any VFS-backed filesystem: FAT, SPIFFS, LittleFS, etc.
 *
 * @param[in]   cfg      Configuration. NULL uses all defaults.
 * @param[out]  out_src  On success, receives the new source; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_src is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_source_fs_create(const esp_ota_service_source_fs_cfg_t *cfg, esp_ota_service_source_t **out_src);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
