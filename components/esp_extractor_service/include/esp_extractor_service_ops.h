/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_extractor_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default extractor output frame pool size in bytes
 *
 *         Used when esp_extractor_service_set_out_pool_size() is never called
 *         or is called with 0. Must fit the largest audio/video access unit.
 */
#define ESP_EXTRACTOR_SERVICE_DEFAULT_POOL_SIZE  (64 * 1024)

/**
 * @brief  Set extract mask before start (AUDIO / VIDEO / AV)
 *
 *         Default is ESP_EXTRACT_MASK_AV when never set.
 *
 * @param[in]  service  Extractor service handle
 * @param[in]  mask     ESP_EXTRACT_MASK_AUDIO, ESP_EXTRACT_MASK_VIDEO, or ESP_EXTRACT_MASK_AV
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    mask is not a supported combination
 *       - ESP_ERR_INVALID_STATE  Service is not in INITIALIZED
 */
esp_err_t esp_extractor_service_set_extract_mask(esp_extractor_service_t *service, uint8_t mask);

/**
 * @brief  Set extractor output pool size before start
 *
 *         0 or never set uses ESP_EXTRACTOR_SERVICE_DEFAULT_POOL_SIZE.
 *
 * @param[in]  service        Extractor service handle
 * @param[in]  out_pool_size  Pool size in bytes; 0 restores the default
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not in INITIALIZED
 */
esp_err_t esp_extractor_service_set_out_pool_size(esp_extractor_service_t *service, uint32_t out_pool_size);

/**
 * @brief  Set media URL (file://, http(s)://, or path). Opens via GMF IO / HLS helper.
 *
 *         Clears any in-memory buffer previously set by set_src_data().
 *
 * @param[in]  service  Extractor service handle
 * @param[in]  url      File path, file://, http(s)://, or .m3u8 URI
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or url is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not in INITIALIZED
 *       - ESP_ERR_NO_MEM         Failed to copy url
 */
esp_err_t esp_extractor_service_set_url(esp_extractor_service_t *service, const char *url);

/**
 * @brief  Set in-memory media buffer as input (clears URL)
 *
 *         The buffer is referenced, not copied. `data` must remain valid until
 *         the service is stopped, or until set_url() / set_src_data() replaces it.
 *
 * @param[in]  service  Extractor service handle
 * @param[in]  data     Borrowed media buffer
 * @param[in]  size     Buffer size in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    data is NULL or size is not positive
 *       - ESP_ERR_INVALID_STATE  Service is not in INITIALIZED
 */
esp_err_t esp_extractor_service_set_src_data(esp_extractor_service_t *service, const void *data, int size);

/**
 * @brief  Seek to a media timestamp while running
 *
 *         Aborts outstanding track writes, posts a seek command to the
 *         extractor task, then the task clears abort, seeks, and publishes
 *         ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE or SEEK_ERROR.
 *
 * @param[in]  service      Extractor service handle
 * @param[in]  position_ms  Target position in milliseconds
 *
 * @return
 *       - ESP_OK                 Command queued
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not RUNNING
 *       - ESP_FAIL               Command queue full or unavailable
 */
esp_err_t esp_extractor_service_seek(esp_extractor_service_t *service, uint32_t position_ms);

/**
 * @brief  Enable or disable auto loop on EOS
 *
 *         When enabled, each EOS publishes ESP_EXTRACTOR_SERVICE_EVENT_EOS,
 *         seeks to 0, and continues. The service does not stop after N plays;
 *         the application counts EOS events and calls stop when it is done.
 *         When disabled, EOS writes per-track EOS and the extractor task exits.
 *         Stop still quits the task even when auto loop is enabled.
 *         May be set before start or while running.
 *
 * @param[in]  service    Extractor service handle
 * @param[in]  auto_loop  true to loop on EOS
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not INITIALIZED or RUNNING
 */
esp_err_t esp_extractor_service_set_auto_loop(esp_extractor_service_t *service, bool auto_loop);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
