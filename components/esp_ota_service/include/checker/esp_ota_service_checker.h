/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct esp_ota_service_checker;
struct esp_ota_service_source;

/**
 * @brief  Result of a pre-download update check
 *
 *         Filled by @c esp_ota_service_checker_t::check().  The checker determines whether a newer
 *         image is available and optionally provides digests for bridging to streaming
 *         verifiers (@c esp_ota_service_verifier_sha256_create(), @c esp_ota_service_verifier_md5_create()).
 */
typedef struct {
    char      version[32];        /*!< Human-readable incoming version string (e.g. "1.2.3") */
    char      label[32];          /*!< Context label: project_name for app images,
                                   *   partition label or user tag for data images */
    uint32_t  image_size;         /*!< Total image size in bytes; @c UINT32_MAX if unknown */
    bool      upgrade_available;  /*!< @c true when the incoming image should be applied */
    bool      has_hash;           /*!< @c true when @a hash contains a valid SHA-256 digest */
    bool      has_md5;            /*!< @c true when @a md5 contains a valid MD5 digest */
    uint8_t   hash[32];           /*!< SHA-256 digest of the image (e.g. from manifest);
                                   *   valid only when @a has_hash is true */
    uint8_t   md5[16];            /*!< MD5 digest; valid only when @a has_md5 is true */
} esp_ota_service_check_result_t;

/**
 * @brief  OTA pre-download update checker abstraction
 *
 *         A checker answers "should I upgrade?" by inspecting version metadata
 *         (image header, packed semver, JSON manifest, etc.) without performing a
 *         full download or any flash write.
 *
 *         Built-in implementations:
 *           - App image checker (esp_app_desc_t header)
 *           - Data-partition checker (packed semver)
 *           - Manifest checker (JSON: version + sha256 + size)
 *
 *         Custom implementations can be provided by the user for any version format.
 *
 * @note  The checker interface mirrors @c esp_ota_service_verifier_t conventions: named struct
 *        tag, @c destroy + @c priv, and a dedicated layer header under @c include/checker/.
 */
typedef struct esp_ota_service_checker {
    /**
     * @brief  Determine whether an upgrade is needed
     *
     *         Typically fetches version metadata (header-only, no full download)
     *         and compares against the running version.
     *
     * @param[in]   self    Checker instance
     * @param[in]   uri     Image URI (passed to @c source->open)
     * @param[in]   source  Data stream source for fetching header/metadata;
     *                          may be ignored by checkers that fetch from a separate endpoint
     *                          (e.g. manifest checker)
     * @param[out]  result  Filled with version info and upgrade decision
     *
     * @return
     *       - ESP_OK     Check completed (inspect @c result->upgrade_available for decision)
     *       - ESP_ERR_*  Check failed (could not determine availability)
     */
    esp_err_t (*check)(struct esp_ota_service_checker *self, const char *uri,
                       struct esp_ota_service_source *source, esp_ota_service_check_result_t *result);

    /**
     * @brief  Destroy the checker instance and free all allocated memory
     *
     *         Called by the OTA service when the upgrade list is replaced or destroyed.
     *         Set to NULL to prevent the service from freeing this instance (caller owns it).
     *
     * @param[in]  self  Checker instance
     */
    void (*destroy)(struct esp_ota_service_checker *self);

    void *priv;  /*!< Private implementation data — do not access directly */
} esp_ota_service_checker_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
