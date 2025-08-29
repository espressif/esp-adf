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

struct esp_ota_service_source;

/**
 * @brief  OTA data source abstraction
 *
 *         A source provides a sequential stream of bytes to be written to a partition.
 *         Built-in implementations: HTTP/HTTPS, local filesystem.
 *         Custom implementations (BLE, UART, etc.) can be provided by the user.
 *
 * @note  Factory functions (e.g. @c esp_ota_service_source_http_create) return @c esp_err_t only.
 *        Member hooks @c read and @c get_size use integer return types to convey byte counts
 *        or stream state (-1), not @c esp_err_t.
 */
typedef struct esp_ota_service_source {
    /**
     * @brief  Open the source and prepare for reading
     *
     * @param[in]  self  Source instance
     * @param[in]  uri   URI string (e.g., "https://...", "file://sdcard/...")
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific; see each built-in source)
     */
    esp_err_t (*open)(struct esp_ota_service_source *self, const char *uri);

    /**
     * @brief  Read a chunk of data from the source
     *
     * @param[in]   self  Source instance
     * @param[out]  buf   Destination buffer
     * @param[in]   len   Maximum bytes to read
     *
     * @return
     *       - >0  Bytes actually read
     *       - 0   End of stream (success)
     *       - <0  Read error (implementation-specific)
     */
    int (*read)(struct esp_ota_service_source *self, uint8_t *buf, int len);

    /**
     * @brief  Get the total data size
     *
     *         May be called after open(). Returns -1 if the size is unknown
     *         (e.g., HTTP chunked transfer encoding).
     *
     * @param[in]  self  Source instance
     *
     * @return
     *       - >=0  Total bytes in the stream
     *       - -1   Size is unknown (e.g., HTTP chunked transfer encoding)
     */
    int64_t (*get_size)(struct esp_ota_service_source *self);

    /**
     * @brief  Seek to a byte offset in the source stream
     *
     *         Used for OTA resume. After open(), seek to the offset where the
     *         previous download was interrupted.
     *         Set to NULL if the source does not support seeking.
     *
     * @param[in]  self    Source instance
     * @param[in]  offset  Byte offset from the beginning of the stream
     *
     * @return
     *       - ESP_OK                 On success
     *       - ESP_ERR_NOT_SUPPORTED  If the source does not support seeking (or @c seek is NULL)
     *       - ESP_ERR_*              On failure (implementation-specific)
     */
    esp_err_t (*seek)(struct esp_ota_service_source *self, int64_t offset);

    /**
     * @brief  Close the source and release all network/file resources
     *
     * @param[in]  self  Source instance
     *
     * @return
     *       - ESP_OK     On success
     *       - ESP_ERR_*  On failure (implementation-specific)
     */
    esp_err_t (*close)(struct esp_ota_service_source *self);

    /**
     * @brief  Destroy the source instance and free all allocated memory
     *
     *         Called by the OTA service when the upgrade list is replaced or destroyed.
     *         Set to NULL to prevent the service from freeing this instance (caller owns it).
     *
     * @param[in]  self  Source instance
     */
    void (*destroy)(struct esp_ota_service_source *self);

    void *priv;  /*!< Private implementation data — do not access directly */
} esp_ota_service_source_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
