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
#include "esp_app_desc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Version comparison policy
 *
 *         Controls the semantic checks performed by @c esp_ota_service_version_compare_descriptors().
 *         The magic word in @c esp_app_desc_t is always validated regardless of these flags.
 */
typedef struct {
    bool  require_higher_version;  /*!< @c true: reject when incoming semver is not strictly higher */
    bool  check_project_name;      /*!< @c true: reject when project_name differs from running */
} esp_ota_service_version_policy_t;

/**
 * @brief  Compare running vs incoming @c esp_app_desc_t using the given policy
 *
 * @param[in]  running_desc   Running firmware descriptor; NULL skips project/version compare
 *                            (incoming magic is still validated).
 * @param[in]  incoming_desc  Candidate image descriptor.
 * @param[in]  policy         Comparison policy; NULL defaults to strict (both flags @c true).
 *
 * @return
 *       - ESP_OK                   Incoming image should be accepted
 *       - ESP_ERR_INVALID_ARG      @a incoming_desc is NULL
 *       - ESP_ERR_INVALID_VERSION  Rejected by policy (version not higher, project mismatch,
 *                                  bad magic, unparsable version string)
 */
esp_err_t esp_ota_service_version_compare_descriptors(const esp_app_desc_t *running_desc,
                                                      const esp_app_desc_t *incoming_desc,
                                                      const esp_ota_service_version_policy_t *policy);

/**
 * @brief  Pack a semver string into a uint32 for ordering comparison
 *
 *         Accepts @c "x.y.z" or @c "Vx.y.z" with each component in 0..255.
 *
 *         **Raw data-partition convention:** the first four bytes of a userdata image may store
 *         this value in little-endian order (byte0 = patch, byte1 = minor, byte2 = major,
 *         byte3 = 0), matching ESP32's native byte order.
 *
 * @param[in]  version  Semver string; NULL or invalid form yields @c UINT32_MAX.
 *
 * @return
 *       - Packed  value @c (major<<16)|(minor<<8)|patch, or @c UINT32_MAX on parse failure.
 */
uint32_t esp_ota_service_version_pack_semver(const char *version);

/**
 * @brief  Unpack a packed semver uint32 into a human-readable "x.y.z" string
 *
 * @param[in]   packed   Packed version from @c esp_ota_service_version_pack_semver()
 * @param[out]  buf      Output buffer (must be at least 12 bytes for "255.255.255\\0")
 * @param[in]   buf_len  Size of @a buf
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   If @a buf is NULL or @a buf_len < 12
 *       - ESP_ERR_INVALID_SIZE  If @a packed is @c UINT32_MAX (invalid)
 */
esp_err_t esp_ota_service_version_unpack_semver(uint32_t packed, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
