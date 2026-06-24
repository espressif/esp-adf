/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_track_mngr.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Acquire writable storage for a frame payload
 *
 *         The returned frame is committed by esp_media_track_write_frame() or
 *         discarded by esp_media_track_release_frame()
 *
 * @param[in]   mngr        Track manager handle
 * @param[out]  out_frame   Output writable frame descriptor
 * @param[in]   size        Payload size to allocate
 * @param[in]   timeout_ms  Timeout in milliseconds; 0 means no wait, UINT32_MAX means wait forever
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Track manager or out_frame is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not supported for the track cache mode
 *       - Others                 Error returned by the track manager implementation
 */
esp_err_t esp_media_track_acquire_frame(esp_media_track_mngr_t *mngr, esp_media_frame_t *out_frame, size_t size,
                                        uint32_t timeout_ms);

/**
 * @brief  Release an acquired write frame without committing it
 *
 * @param[in]  mngr   Track manager handle
 * @param[in]  frame  Frame returned by esp_media_track_acquire_frame()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    mngr or frame is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not supported for the track cache mode
 *       - Others                 Error returned by the track manager implementation
 */
esp_err_t esp_media_track_release_frame(esp_media_track_mngr_t *mngr, esp_media_frame_t *frame);

/**
 * @brief  Write a frame to a track manager
 *
 *         When frame was acquired from esp_media_track_acquire_frame(), this
 *         commits that acquired buffer. Otherwise, the manager may copy or queue
 *         the caller-provided frame according to its cache mode
 *
 * @param[in]  mngr        Track manager handle
 * @param[in]  frame       Frame to write
 * @param[in]  timeout_ms  Timeout in milliseconds; 0 means no wait, UINT32_MAX means wait forever
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Track manager or frame is NULL
 *       - ESP_ERR_INVALID_SIZE   Frame is larger than the acquired buffer
 *       - ESP_ERR_NOT_SUPPORTED  Operation is not supported for the track cache mode
 *       - Others                 Error returned by the track manager implementation
 */
esp_err_t esp_media_track_write_frame(esp_media_track_mngr_t *mngr, const esp_media_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief  Abort from the write side
 *
 *         Use when no more frames will be produced but consumers keep waiting.
 *         This abort wakes blocked consumers; further reads return an error.
 *         The queued data is kept until esp_media_track_clear_abort() is called
 *
 * @param[in]  mngr  Track manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager is NULL
 */
esp_err_t esp_media_track_write_abort(esp_media_track_mngr_t *mngr);

/**
 * @brief  Clear track manager abort state
 *
 *         If the manager was aborted, queued data is reset before new reads or
 *         writes can proceed. Typical use cases: stop and replay loops
 *
 * @param[in]  mngr  Track manager handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Track manager is NULL
 */
esp_err_t esp_media_track_clear_abort(esp_media_track_mngr_t *mngr);

/**
 * @brief  Query queued frame statistics for a track type
 *
 * @param[in]   mngr      Track manager handle
 * @param[in]   type      Track type to query. Ignored when global cache is enabled
 * @param[out]  out_info  Queue block count and total queued byte size
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Arguments are invalid
 *       - ESP_ERR_NOT_FOUND    No track matches type
 *       - ESP_FAIL             Underlying queue query failed
 */
esp_err_t esp_media_track_mngr_query(esp_media_track_mngr_t *mngr, esp_media_track_type_t type,
                                     esp_media_track_mngr_queue_info_t *out_info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
