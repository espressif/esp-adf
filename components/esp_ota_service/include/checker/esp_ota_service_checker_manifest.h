/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include "esp_ota_service_checker.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Configuration for @c esp_ota_service_checker_manifest_create()
 */
typedef struct {
    const char *manifest_uri;            /*!< URL of the JSON manifest endpoint (deep-copied) */
    const char *current_version;         /*!< Running firmware version "x.y.z" (deep-copied) */
    bool        require_higher_version;  /*!< @c true: reject if incoming semver is not strictly higher */
    bool        check_project_name;      /*!< @c true: reject if project_name differs from @a current_project
                                          *   (requires @a current_project to be set) */
    const char *current_project;         /*!< Running project name for comparison (deep-copied);
                                          *   only used when @a check_project_name is @c true */
    const char *cert_pem;                /*!< PEM certificate for HTTPS; NULL to use the default bundle */
    int         timeout_ms;              /*!< HTTP timeout in ms; 0 = default (5000 ms) */
} esp_ota_service_checker_manifest_cfg_t;

/**
 * @brief  Create a manifest-based update checker
 *
 *         Fetches a JSON manifest from @a manifest_uri, parses version, size, project_name,
 *         and optional @c sha256 / @c md5 hex fields, and determines whether an upgrade is available.
 *
 *         When the manifest contains a "sha256" hex string, the checker populates
 *         @c esp_ota_service_check_result_t::hash and sets @c has_hash = true, enabling the caller
 *         to configure @c esp_ota_service_verifier_sha256_create() with the expected digest.
 *
 *         When the manifest contains an "md5" hex string (32 hex digits), the checker sets
 *         @c has_md5 and @c md5 for use with @c esp_ota_service_verifier_md5_create().
 *
 *         Expected JSON format (all fields optional except "version"):
 * @code{.json}
 *         {
 *             "version": "1.2.3",
 *             "project_name": "my_app",
 *             "size": 1048576,
 *             "sha256": "a1b2c3d4e5f6...",
 *             "md5": "d41d8cd98f00b204e9800998ecf8427e"
 *         }
 *         @endcode
 *
 *         The source parameter in @c check() is ignored; the manifest checker uses its
 *         own HTTP client to fetch @a manifest_uri.
 *
 * @param[in]  cfg  Configuration; must not be NULL.
 *
 * @return
 *       - New  checker, or NULL on allocation failure or invalid config
 */
esp_ota_service_checker_t *esp_ota_service_checker_manifest_create(const esp_ota_service_checker_manifest_cfg_t *cfg);

/**
 * @brief  Parse a manifest JSON body with the same rules as the HTTP manifest checker
 *
 *         Intended for unit tests and host-side tooling. Does not perform any network I/O.
 *
 * @param[in]   json_body               NUL-terminated JSON (same schema as the manifest endpoint)
 * @param[in]   current_version         Running semver string for comparison
 * @param[in]   require_higher_version  Same as @c esp_ota_service_checker_manifest_cfg_t::require_higher_version
 * @param[in]   check_project_name      Same as @c esp_ota_service_checker_manifest_cfg_t::check_project_name
 * @param[in]   current_project         Same as @c esp_ota_service_checker_manifest_cfg_t::current_project (may be NULL)
 * @param[out]  result                  Filled on success
 *
 * @return
 *       - ESP_OK  on success, ESP_ERR_INVALID_ARG, or ESP_FAIL if JSON is invalid or missing version
 */
esp_err_t esp_ota_service_checker_manifest_parse_json(const char *json_body,
                                                      const char *current_version,
                                                      bool require_higher_version,
                                                      bool check_project_name,
                                                      const char *current_project,
                                                      esp_ota_service_check_result_t *result);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
