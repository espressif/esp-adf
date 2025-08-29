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

#include "esp_ota_service_checker.h"
#include "esp_ota_service_version_utils.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default byte offset of the embedded @c esp_app_desc_t inside the tone binary
 *
 *         Matches the "format=1" layout produced by upstream
 *         @c tools/audio_tone/mk_audio_tone.py, where the bin starts with an
 *         8-byte tone header (<HHI> = tag | file_cnt | format) and is immediately
 *         followed by a standard @c esp_app_desc_t struct.
 */
#define ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET  8U

/**
 * @brief  Default magic word used by @c mk_audio_tone.py for the tone @c esp_app_desc_t
 *
 *         Distinct from @c ESP_APP_DESC_MAGIC_WORD (0xABCD5AA5) so a tone binary
 *         is not accidentally accepted as an app image and vice versa.
 */
#define ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_MAGIC  0xF55F9876UL

/**
 * @brief  Configuration for @c esp_ota_service_checker_tone_desc_create()
 *
 *         Reuses the same @c esp_app_desc_t header scheme as IDF app images, but
 *         with a configurable byte offset (to skip a container header) and a
 *         configurable magic word (to tell the format apart from an app image).
 *
 *         Typical usage with the default tone layout:
 * @code
 *         esp_ota_service_checker_tone_desc_cfg_t cfg = {
 *             .current_version          = "1.0.0",    // running data version
 *             .expected_project_name    = "ESP_TONE_BIN",
 *             .policy.require_higher_version = true,
 *             .policy.check_project_name     = true,
 *             // desc_offset / expected_magic left 0 → defaults applied
 *         };
 *         esp_ota_service_checker_t *chk = esp_ota_service_checker_tone_desc_create(&cfg);
 * @endcode
 */
typedef struct {
    /** Running data-bin version ("x.y.z"); deep-copied. Required. */
    const char *current_version;

    /** Expected project_name inside the incoming descriptor; deep-copied.
     *  NULL together with @c policy.check_project_name == false disables the check. */
    const char *expected_project_name;

    /** Byte offset of the @c esp_app_desc_t from the start of the data image.
     *  0 → @c ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_OFFSET. */
    uint32_t  desc_offset;

    /** Magic word expected at @c esp_app_desc_t::magic_word.
     *  0 → @c ESP_OTA_SERVICE_CHECKER_TONE_DESC_DEFAULT_MAGIC. */
    uint32_t  expected_magic;

    /** Semver + project-name comparison policy applied to the descriptor. */
    esp_ota_service_version_policy_t  policy;
} esp_ota_service_checker_tone_desc_cfg_t;

/**
 * @brief  Create a data-image update checker that reads an embedded @c esp_app_desc_t
 *
 *         Streams @c desc_offset + sizeof(esp_app_desc_t) bytes from the source,
 *         validates the configured magic word, and compares the incoming
 *         @c esp_app_desc_t against a caller-provided running version (and
 *         optional project_name).
 *
 *         Intended for data partitions carrying an @c esp_app_desc_t-shaped version
 *         header (e.g. tone binaries produced by @c mk_audio_tone.py with @c format=1).
 *         For raw 4-byte packed-semver headers use @c esp_ota_service_checker_data_version_create().
 *         For IDF app images use @c esp_ota_service_checker_app_create().
 *
 * @param[in]  cfg  Configuration; @c current_version is required.
 *
 * @return
 *       - New  checker, or NULL on invalid config / allocation failure.
 */
esp_ota_service_checker_t *esp_ota_service_checker_tone_desc_create(const esp_ota_service_checker_tone_desc_cfg_t *cfg);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
