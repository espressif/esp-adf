/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_capture.h"
#include "esp_capture_sink.h"
#include "esp_capture_service.h"
#include "esp_media_service.h"
#include "esp_media_provider.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Set fixed (raw) capture caps for a registered audio source.
 *
 * @note  Fixed caps shape source negotiation, so they must be set before any
 *        capture opens/negotiates. Returns ESP_ERR_INVALID_STATE when
 *        the service is running.
 *
 * @param[in]  service     Capture service handle
 * @param[in]  fixed_caps  Fixed audio capabilities
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or fixed_caps is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NOT_FOUND      No audio source is configured
 *       - Others                 Error returned by the audio source
 */
esp_err_t esp_capture_service_set_audio_src_fixed_caps(esp_capture_service_t *service,
                                                       const esp_capture_audio_info_t *fixed_caps);

/**
 * @brief  Get native esp_capture handle for advanced capture APIs.
 *
 * @param[in]   service      Capture service handle
 * @param[out]  out_capture  Output capture handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or out_capture is NULL
 *       - ESP_ERR_INVALID_STATE  Service is not configured
 */
esp_err_t esp_capture_service_get_capture_handle(esp_capture_service_t *service,
                                                 esp_capture_handle_t *out_capture);

/**
 * @brief  Get native sink handle for advanced sink APIs.
 *
 * @param[in]   service   Capture service handle
 * @param[in]   stream    Output stream ID
 * @param[out]  out_sink  Output sink handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 */
esp_err_t esp_capture_service_get_sink_handle(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                              esp_capture_sink_handle_t *out_sink);

/**
 * @brief  Get media provider for the specified output stream.
 *
 * @param[in]   service       Capture service handle
 * @param[in]   stream        Output stream ID
 * @param[out]  out_provider  Output media provider
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 */
esp_err_t esp_capture_service_get_provider(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                           esp_media_provider_t *out_provider);

/**
 * @brief  Set a manual storage URL for one stream.
 *
 *         The container/muxer type is resolved in this order:
 *         1. inferred from the URL extension when recognizable;
 *         2. otherwise the muxer type already configured during setup;
 *         3. otherwise the default container.
 *         This lets callers pass signed or extension-less URLs once a muxer type
 *         is known.
 *
 *         The sink muxer is added lazily at capture start, so the URL/type may be
 *         changed any number of times while the service is stopped. After the
 *         muxer is added the URL can still be updated (it applies to the next
 *         record, e.g. stop_record -> set_storage_url -> start_record), but the
 *         container type can no longer change and a mismatching extension returns
 *         ESP_ERR_INVALID_STATE.
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 * @param[in]  url      Storage URL, or NULL to clear the manual URL
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Muxer type cannot be changed
 *       - ESP_ERR_NO_MEM         Out of memory
 *       - Others                 Failed to configure the storage muxer
 */
esp_err_t esp_capture_service_set_storage_url(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                              const char *url);

/**
 * @brief  Get the URL most recently generated for a recording.
 *
 * @param[in]   service  Capture service handle
 * @param[in]   stream   Output stream ID
 * @param[out]  out_url  Internal URL pointer. It remains valid until the stream
 *                       generates another recording URL or the setup/service is
 *                       destroyed. The caller must not free or modify it.
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_NOT_FOUND      No recording URL has been generated
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 */
esp_err_t esp_capture_service_get_last_storage_url(esp_capture_service_t *service,
                                                   esp_media_stream_id_t stream,
                                                   const char **out_url);

/**
 * @brief  Storage file metadata returned by esp_capture_service_get_storage_file_info()
 */
typedef struct {
    int64_t  size;   /*!< File size in bytes */
    int64_t  mtime;  /*!< Last modification time (seconds since Unix epoch) */
    int64_t  ctime;  /*!< Creation / status-change time (seconds since Unix epoch; FS-dependent) */
} esp_capture_service_storage_file_info_t;

/**
 * @brief  Query size and timestamps for a recorded storage file via stat(2)
 *
 *         When `url` is NULL or empty, the stream's last generated storage URL
 *         is used (see esp_capture_service_get_last_storage_url()).
 *
 * @param[in]   service   Capture service handle
 * @param[in]   stream    Output stream ID used when resolving the last URL
 * @param[in]   url       Absolute path to query, or NULL/"" to use last URL
 * @param[out]  out_info  Receives size / mtime / ctime on success
 *
 * @return
 *       - ESP_OK               On success (regular file found)
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_NOT_FOUND    Path missing, not a regular file, or no last URL
 *       - ESP_FAIL             stat() failed for another reason
 */
esp_err_t esp_capture_service_get_storage_file_info(esp_capture_service_t *service,
                                                    esp_media_stream_id_t stream,
                                                    const char *url,
                                                    esp_capture_service_storage_file_info_t *out_info);

/**
 * @brief  Start instant record on one stream by enabling a preconfigured storage muxer.
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Storage muxer is not configured
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_start_record(esp_capture_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Stop instant record on one stream by disabling the storage muxer.
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_stop_record(esp_capture_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Enable or disable an output stream
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 * @param[in]  enable   True to enable the stream
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_enable_stream(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                            bool enable);

/**
 * @brief  Enable or disable a track in an output stream
 *
 * @note  Enabling an already disabled track requires sink re-setup.
 *
 * @param[in]  service     Capture service handle
 * @param[in]  stream      Output stream ID
 * @param[in]  track_type  Track type to change
 * @param[in]  enable      True to enable the track
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 *       - ESP_ERR_NOT_SUPPORTED  Enabling a track is not supported
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_enable_track(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                           esp_media_track_type_t track_type, bool enable);

/**
 * @brief  Trigger one-shot capture.
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_one_shot(esp_capture_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Manually acquire one frame from a stream.
 *
 * @note  `timeout_ms` semantics:
 *          - 0          : non-blocking, return immediately if no frame is ready.
 *          - UINT32_MAX : block until a frame is available or the provider aborts.
 *          - otherwise  : wait up to `timeout_ms`, polling the sink internally.
 *        A finite timeout never blocks indefinitely and always returns once the
 *        provider is aborted.
 *
 * @param[in]      service     Capture service handle
 * @param[in]      stream      Output stream ID
 * @param[in,out]  frame       Input track selector, output acquired frame
 * @param[in]      timeout_ms  Timeout in milliseconds
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_TIMEOUT        No frame was available before timeout
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured or is aborted
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_acquire_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                            esp_media_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief  Read one frame into caller-provided storage.
 *
 *         `frame->data` / `frame->size` describe the caller buffer on input.
 *         On success, metadata is copied from the captured frame and `size`
 *         is set to the payload bytes copied into the caller buffer. The
 *         capture-owned frame is released before this API returns.
 *
 * @param[in]      service     Capture service handle
 * @param[in]      stream      Output stream ID
 * @param[in,out]  frame       Input buffer descriptor, output frame
 * @param[in]      timeout_ms  Timeout in milliseconds
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_TIMEOUT        No frame was available before timeout
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_SIZE   Caller buffer is too small
 *       - ESP_ERR_INVALID_STATE  Stream is not configured or is aborted
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_read_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                         esp_media_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief  Release a frame acquired by esp_capture_service_acquire_frame().
 *
 * @param[in]  service  Capture service handle
 * @param[in]  stream   Output stream ID
 * @param[in]  frame    Frame to release
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument or stream ID
 *       - ESP_ERR_INVALID_STATE  Stream is not configured
 *       - Others                 Error returned by the capture sink
 */
esp_err_t esp_capture_service_release_frame(esp_capture_service_t *service, esp_media_stream_id_t stream,
                                            esp_media_frame_t *frame);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
