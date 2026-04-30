/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_ota_service_verifier.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Configuration for @c esp_ota_service_verifier_md5_create()
 */
typedef struct {
    uint8_t  expected_md5[16];  /*!< Expected MD5 digest of the complete image */
} esp_ota_service_verifier_md5_cfg_t;

/**
 * @brief  Create a streaming MD5 integrity verifier
 *
 *         Computes the MD5 hash of all data passed through @c verify_update()
 *         and compares it against @a expected_md5 in @c verify_finish().
 *
 * @note  MD5 is weak for security; prefer SHA-256 when possible.
 *
 * @param[in]   cfg      Configuration with expected digest
 * @param[out]  out_ver  On success, receives the new verifier; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If cfg or out_ver is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_verifier_md5_create(const esp_ota_service_verifier_md5_cfg_t *cfg, esp_ota_service_verifier_t **out_ver);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
