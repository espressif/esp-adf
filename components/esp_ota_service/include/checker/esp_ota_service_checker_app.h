/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "esp_app_desc.h"
#include "esp_ota_service_checker.h"
#include "esp_ota_service_version_utils.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Optional user callback to override the default semver comparison
 *
 *         When set in @c esp_ota_service_checker_app_cfg_t, this replaces the built-in
 *         @c esp_ota_service_version_compare_descriptors() decision.
 *
 * @param[in]  running   Running firmware descriptor
 * @param[in]  incoming  Incoming image descriptor
 * @param[in]  ctx       User context from @c esp_ota_service_checker_app_cfg_t::should_upgrade_ctx
 *
 * @return
 *       - @c  true if the incoming image should be applied
 */
typedef bool (*esp_ota_service_checker_should_upgrade_fn)(const esp_app_desc_t *running,
                                                          const esp_app_desc_t *incoming,
                                                          void *ctx);

/**
 * @brief  Configuration for @c esp_ota_service_checker_app_create()
 */
typedef struct {
    esp_ota_service_version_policy_t           version_policy;      /*!< Semver + project-name comparison policy */
    esp_ota_service_checker_should_upgrade_fn  should_upgrade;      /*!< NULL: use default semver compare */
    void                                      *should_upgrade_ctx;  /*!< User context for @a should_upgrade */
} esp_ota_service_checker_app_cfg_t;

/**
 * @brief  Create an app-image update checker
 *
 *         Reads the @c esp_app_desc_t from the remote image header (via the item's
 *         @c esp_ota_service_source_t) and compares against the running firmware.
 *
 *         Compatible only with ESP-IDF app images that contain a standard
 *         @c esp_app_desc_t header.
 *
 *         For data partitions or non-@c esp_app_desc_t metadata, use
 *         @c esp_ota_service_checker_data_version_create() or implement @c esp_ota_service_checker_t yourself.
 *
 * @param[in]  cfg  Configuration; NULL uses strict defaults (require_higher_version=true,
 *                  check_project_name=true, no custom hook).
 *
 * @return
 *       - New  checker, or NULL on allocation failure
 */
esp_ota_service_checker_t *esp_ota_service_checker_app_create(const esp_ota_service_checker_app_cfg_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
