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

struct esp_ota_service_target;

/**
 * @brief  OTA write target abstraction
 *
 *         A target accepts a stream of bytes and writes them to a flash partition.
 *         Built-in implementations: app partition (esp_ota_* API), raw data partition.
 *
 * @note  Factory functions (e.g. @c esp_ota_service_target_app_create) return @c esp_err_t only.
 *        The @c get_write_offset hook returns a byte offset as @c int64_t, not @c esp_err_t.
 */
typedef struct esp_ota_service_target {
    /**
     * @brief  Open the target partition and prepare for writing
     *
     * @param[in]  self             Target instance
     * @param[in]  partition_label  Partition label string, or NULL for the next OTA app slot
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific; see each built-in target)
     */
    esp_err_t (*open)(struct esp_ota_service_target *self, const char *partition_label);

    /**
     * @brief  Write a data chunk to the target partition
     *
     * @param[in]  self  Target instance
     * @param[in]  data  Source data buffer
     * @param[in]  len   Number of bytes to write
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*write)(struct esp_ota_service_target *self, const uint8_t *data, size_t len);

    /**
     * @brief  Finalize the write and mark the partition as valid
     *
     *         For app partitions this sets the new boot partition.
     *         For data partitions this is a no-op (data is already committed).
     *
     * @param[in]  self  Target instance
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*commit)(struct esp_ota_service_target *self);

    /**
     * @brief  Abort the write and discard any partially written data
     *
     * @param[in]  self  Target instance
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*abort)(struct esp_ota_service_target *self);

    /**
     * @brief  Get the current write offset (bytes written so far)
     *
     *         Used for OTA resume to determine where to continue writing.
     *         Set to NULL if not supported (assumed 0).
     *
     * @param[in]  self  Target instance
     *
     * @return
     *       - Current  write offset in bytes (non-negative for built-in targets)
     */
    int64_t (*get_write_offset)(struct esp_ota_service_target *self);

    /**
     * @brief  Set the write offset for resume
     *
     *         When called with a non-zero offset before open(), the target will
     *         skip its initial erase and resume writing at the given byte position.
     *         Set to NULL if the target does not support resume.
     *
     * @param[in]  self    Target instance
     * @param[in]  offset  Byte offset to resume from (must be sector-aligned)
     *
     * @return
     *       - ESP_OK                 On success
     *       - ESP_ERR_NOT_SUPPORTED  If the target does not support resume
     */
    esp_err_t (*set_write_offset)(struct esp_ota_service_target *self, int64_t offset);

    /**
     * @brief  Destroy the target instance and free all allocated memory
     *
     *         Called by the OTA service when the upgrade list is replaced or destroyed.
     *         Set to NULL to prevent the service from freeing this instance (caller owns it).
     *
     * @param[in]  self  Target instance
     */
    void (*destroy)(struct esp_ota_service_target *self);

    void *priv;  /*!< Private implementation data — do not access directly */
} esp_ota_service_target_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
