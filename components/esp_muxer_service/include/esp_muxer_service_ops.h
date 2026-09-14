/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_muxer_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Muxer output mode
 */
typedef enum {
    ESP_MUXER_SERVICE_MODE_STORAGE_ONLY   = 0,  /*!< Write to storage only (default) */
    ESP_MUXER_SERVICE_MODE_STREAMING_ONLY = 1,  /*!< Streaming queue only, no file storage, mux type need support streaming */
    ESP_MUXER_SERVICE_MODE_BOTH           = 2,  /*!< Storage and streaming together */
} esp_muxer_service_mode_t;

/**
 * @brief  Optional muxer setup before start
 *
 *         Default when never called: STORAGE_ONLY with built-in container defaults.
 *         Missing storage_dir components are created (max depth 2), same as capture.
 *         May be called only while the service is stopped.
 */
typedef struct {
    esp_muxer_type_t          muxer_type;            /*!< Container type; may be overridden by set_storage_url() */
    esp_muxer_service_mode_t  mode;                  /*!< Storage / streaming / both */
    const char               *storage_dir;           /*!< Storage directory; NULL disables file storage unless URL set */
    uint32_t                  slice_duration;        /*!< Slice duration in ms, 0 uses muxer default */
    uint32_t                  ram_cache_size;        /*!< Muxer file-write RAM cache size, speedup write speed when set */
    uint32_t                  streaming_cache_size;  /*!< Streaming queue cache size for buffering streaming output data */
} esp_muxer_service_setup_t;

/**
 * @brief  Default setup: TS container, storage-only, no directory / cache overrides
 */
#define ESP_MUXER_SERVICE_SETUP_DEFAULT()  {                      \
    .muxer_type           = ESP_MUXER_TYPE_TS,                    \
    .mode                 = ESP_MUXER_SERVICE_MODE_STORAGE_ONLY,  \
    .storage_dir          = NULL,                                 \
    .slice_duration       = 0,                                    \
    .ram_cache_size       = 0,                                    \
    .streaming_cache_size = 0,                                    \
}

/**
 * @brief  Apply optional muxer setup while stopped
 *
 *         If never called, defaults to STORAGE_ONLY with default container settings.
 *
 * @param[in]  service  Muxer service handle
 * @param[in]  setup    Setup to apply
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service, setup, or mode is invalid
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NOT_SUPPORTED  muxer_type is not compiled in
 *       - ESP_ERR_NO_MEM         Failed to copy storage_dir
 *       - Others                 Failed to create storage_dir
 */
esp_err_t esp_muxer_service_setup(esp_muxer_service_t *service, const esp_muxer_service_setup_t *setup);

/**
 * @brief  Set storage URL/path before start
 *
 *         Infers muxer_type from a recognizable extension when possible; otherwise
 *         keeps the type from setup (or default). Overrides the next slice path.
 *         Only valid while stopped.
 *
 * @param[in]  service  Muxer service handle
 * @param[in]  url      File path or URL with a container extension
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or url is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy url
 */
esp_err_t esp_muxer_service_set_storage_url(esp_muxer_service_t *service, const char *url);

/**
 * @brief  Acquire muxed streaming data (mode STREAMING_ONLY or BOTH)
 *
 *         Pair every successful acquire with esp_muxer_service_release_streaming_data().
 *
 * @param[in]   service     Muxer service handle
 * @param[out]  out_data    Receives a pointer to queue memory (not copied)
 * @param[out]  out_size    Receives the byte count
 * @param[in]   timeout_ms  Wait time; 0 returns immediately
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid arguments
 *       - ESP_ERR_INVALID_STATE  Streaming is not enabled or queue is missing
 *       - ESP_ERR_TIMEOUT        No data within timeout_ms
 */
esp_err_t esp_muxer_service_acquire_streaming_data(esp_muxer_service_t *service, const uint8_t **out_data,
                                                   size_t *out_size, uint32_t timeout_ms);

/**
 * @brief  Read muxed streaming data into a caller buffer
 *
 *         It will not read the exact size of the data but read one block of muxed data and return immediately.
 *
 * @param[in]      service     Muxer service handle
 * @param[out]     buffer      Destination buffer
 * @param[in,out]  inout_size  In: buffer capacity. Out: bytes copied, or required size on ESP_ERR_INVALID_SIZE
 * @param[in]      timeout_ms  Wait time; 0 returns immediately
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    buffer or inout_size is NULL
 *       - ESP_ERR_INVALID_SIZE   Buffer is too small; *inout_size is the required size
 *       - ESP_ERR_INVALID_STATE  Streaming is not enabled
 *       - ESP_ERR_TIMEOUT        No data within timeout_ms
 */
esp_err_t esp_muxer_service_read_streaming_data(esp_muxer_service_t *service, uint8_t *buffer,
                                                size_t *inout_size, uint32_t timeout_ms);

/**
 * @brief  Release muxed streaming data acquired from the output queue
 *
 * @param[in]  service  Muxer service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service is NULL or queue is missing
 *       - ESP_FAIL             Queue release failed
 */
esp_err_t esp_muxer_service_release_streaming_data(esp_muxer_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
