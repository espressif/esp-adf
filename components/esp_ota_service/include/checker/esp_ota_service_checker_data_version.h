/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_checker.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Configuration for @c esp_ota_service_checker_data_version_create()
 */
typedef struct {
    const char *current_version;  /*!< Current data version string "x.y.z" (deep-copied) */
} esp_ota_service_checker_data_cfg_t;

/**
 * @brief  Create a data-partition version checker
 *
 *         Reads the first 4 bytes of the remote image via the source stream as a
 *         packed semver (little-endian uint32: major<<16 | minor<<8 | patch) and
 *         compares it against @a current_version.
 *
 *         This convention matches @c esp_ota_service_version_pack_semver() and the raw-data
 *         image layout used in the ota_fs example.
 *
 *         For non-semver data formats, implement @c esp_ota_service_checker_t yourself.
 *
 * @param[in]  cfg  Configuration; must not be NULL.
 *
 * @return
 *       - New  checker, or NULL on allocation failure or invalid config
 */
esp_ota_service_checker_t *esp_ota_service_checker_data_version_create(const esp_ota_service_checker_data_cfg_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
