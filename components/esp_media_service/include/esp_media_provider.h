/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_service_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the number of tracks exposed by a provider
 *
 * @param[in]   provider  Provider handle
 * @param[out]  out_num   Output track count
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider or out_num is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_get_track_num(const esp_media_provider_t *provider, uint16_t *out_num);

/**
 * @brief  Get track metadata by provider track index
 *
 * @param[in]   provider  Provider handle
 * @param[in]   index     Track index in the provider
 * @param[out]  out_info  Output track metadata
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider or out_info is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_get_track_info(const esp_media_provider_t *provider, uint16_t index,
                                            esp_media_track_info_t *out_info);

/**
 * @brief  Register a provider event callback
 *
 * @param[in]  provider   Provider handle
 * @param[in]  cb         Event callback, or NULL to clear it
 * @param[in]  event_ctx  User context passed to cb
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_set_event_cb(const esp_media_provider_t *provider, esp_media_provider_event_cb_t cb,
                                          void *event_ctx);

/**
 * @brief  Abort provider-side blocking read operations
 *
 * @param[in]  provider  Provider handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_abort(const esp_media_provider_t *provider);

/**
 * @brief  Acquire the next frame from a provider
 *
 *         The returned frame must be released with esp_media_provider_release_frame().
 *         Set fields such as type or track_id in out_frame before the call to
 *         select a specific track when the provider supports it
 *
 * @param[in]      provider    Provider handle
 * @param[in,out]  out_frame   Input track selector, output acquired frame
 * @param[in]      timeout_ms  Timeout in milliseconds; 0 means no wait, UINT32_MAX means wait forever
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider or out_frame is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_acquire_frame(const esp_media_provider_t *provider, esp_media_frame_t *out_frame,
                                           uint32_t timeout_ms);

/**
 * @brief  Read the next frame into caller-provided storage
 *
 *         Set out_frame->data and out_frame->size before calling. On success,
 *         out_frame->size is updated to the actual payload size
 *
 * @param[in]      provider    Provider handle
 * @param[in,out]  out_frame   Input buffer descriptor, output frame descriptor
 * @param[in]      timeout_ms  Timeout in milliseconds; 0 means no wait, UINT32_MAX means wait forever
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider or out_frame is NULL
 *       - ESP_ERR_INVALID_SIZE   Provided buffer is too small
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_read_frame(const esp_media_provider_t *provider, esp_media_frame_t *out_frame,
                                        uint32_t timeout_ms);

/**
 * @brief  Release a frame acquired from a provider
 *
 * @param[in]  provider  Provider handle
 * @param[in]  frame     Frame returned by esp_media_provider_acquire_frame()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    provider or frame is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not implemented by the provider
 *       - Others                 Error returned by the provider implementation
 */
esp_err_t esp_media_provider_release_frame(const esp_media_provider_t *provider, esp_media_frame_t *frame);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
