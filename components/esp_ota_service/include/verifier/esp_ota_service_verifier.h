/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct esp_ota_service_verifier;

/**
 * @brief  OTA image verifier abstraction
 *
 *         A verifier inspects image data as it streams in to determine whether the
 *         upgrade should proceed. The OTA service calls the three hooks in sequence:
 *         verify_begin → [verify_update × N] → verify_finish
 *
 *         Built-in implementations: SHA-256 checksum.
 *         Custom implementations can add signature validation, CRC, etc.
 *
 *         Returning any non-ESP_OK error from any hook aborts the upgrade for that item.
 *
 * @note  Factory functions return @c esp_err_t only. Verifier hooks @c verify_begin,
 *        @c verify_update, and @c verify_finish return @c esp_err_t; @c destroy returns void.
 */
typedef struct esp_ota_service_verifier {
    /**
     * @brief  Called once before writing begins
     *
     *         Allocate any internal buffers here.
     *
     * @param[in]  self  Verifier instance
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific; see built-in verifiers)
     */
    esp_err_t (*verify_begin)(struct esp_ota_service_verifier *self);

    /**
     * @brief  Called once per data chunk as it is read from the source
     *
     *         Used to accumulate image header bytes, hash state, or signature data.
     *         The verifier may return ESP_OK for all chunks and defer the decision to
     *         verify_finish(), or return an error early to abort.
     *
     * @param[in]  self  Verifier instance
     * @param[in]  data  Data chunk
     * @param[in]  len   Chunk length in bytes
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*verify_update)(struct esp_ota_service_verifier *self, const uint8_t *data, size_t len);

    /**
     * @brief  Called after all data has been read, before target->commit()
     *
     *         Perform final validation (signature check, full hash comparison, etc.).
     *
     * @param[in]  self  Verifier instance
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*verify_finish)(struct esp_ota_service_verifier *self);

    /**
     * @brief  Destroy the verifier instance and free all allocated memory
     *
     *         Called by the OTA service when the upgrade list is replaced or destroyed.
     *         Set to NULL to prevent the service from freeing this instance (caller owns it).
     *
     * @param[in]  self  Verifier instance
     */
    void (*destroy)(struct esp_ota_service_verifier *self);

    void *priv;  /*!< Private implementation data — do not access directly */
} esp_ota_service_verifier_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
