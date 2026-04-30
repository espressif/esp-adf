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
 * @brief  Bootloader OTA target configuration
 *
 *         Downloads the new bootloader image to a staging partition first, then
 *         copies it to the bootloader area (offset 0x0) on commit.
 *
 *         WARNING: Bootloader OTA is inherently risky. A power failure during the
 *         final copy step can brick the device. Use only with a recovery bootloader
 *         or in controlled factory environments.
 */
typedef struct {
    const char *staging_label;  /*!< Partition label used as temporary staging area.
                                 *   Must be large enough to hold the bootloader image.
                                 *   If NULL, the next inactive OTA app partition is used. */
} esp_ota_service_target_bootloader_cfg_t;

/**
 * @brief  Create a bootloader OTA target
 *
 *         The partition_label passed to open() is ignored — the staging partition
 *         is determined by the cfg->staging_label at creation time.
 *
 *         Flow:
 *         open()   — erases the staging partition
 *         write()  — writes data to the staging partition sequentially
 *         commit() — validates the bootloader image header in staging, then
 *         copies the image from staging to bootloader area (offset 0x0)
 *         abort()  — erases staging partition to discard partial data
 *
 * @param[in]   cfg      Configuration. NULL uses defaults (staging = next OTA app slot).
 * @param[out]  out_tgt  On success, receives the new target; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_tgt is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_target_bootloader_create(const esp_ota_service_target_bootloader_cfg_t *cfg, esp_ota_service_target_t **out_tgt);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
