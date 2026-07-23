/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Default maximum stream count for dummy services */
#define ESP_MEDIA_DUMMY_SERVICE_DEFAULT_MAX_STREAMS  2

/** Default service name for dummy source */
#define ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SRC_NAME  "media_dummy_src"

/** Default service name for dummy sink */
#define ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SINK_NAME  "media_dummy_sink"

/** Default dummy service configuration */
#define ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT()  {                 \
    .name           = NULL,                                         \
    .role           = ESP_MEDIA_ROLE_SINK,                          \
    .max_stream_num = ESP_MEDIA_DUMMY_SERVICE_DEFAULT_MAX_STREAMS,  \
}

/**
 * @brief  Opaque dummy media service handle
 */
typedef struct esp_media_dummy_service esp_media_dummy_service_t;

/**
 * @brief  Dummy service configuration
 */
typedef struct {
    const char       *name;            /*!< Optional instance name; NULL uses role default */
    esp_media_role_t  role;            /*!< ESP_MEDIA_ROLE_SRC or ESP_MEDIA_ROLE_SINK */
    uint16_t          max_stream_num;  /*!< Maximum supported stream count */
} esp_media_dummy_service_cfg_t;

/**
 * @brief  Per-stream statistics collected by a dummy sink
 */
typedef struct {
    uint32_t  audio_frame_count;  /*!< Consumed audio frames */
    uint32_t  audio_byte_count;   /*!< Consumed audio bytes */
    uint32_t  video_frame_count;  /*!< Consumed video frames */
    uint32_t  video_byte_count;   /*!< Consumed video bytes */
} esp_media_dummy_stream_stats_t;

/**
 * @brief  Create a dummy media source or sink service
 *
 * @param[in]   cfg  Service configuration
 * @param[out]  out  Output service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid role, stream count, or output argument
 *       - ESP_ERR_NO_MEM         Allocation failed
 *       - ESP_ERR_NOT_SUPPORTED  Role support disabled by Kconfig
 *       - Others                 Error returned by esp_media_service_init()
 */
esp_err_t esp_media_dummy_service_create(const esp_media_dummy_service_cfg_t *cfg,
                                         esp_media_dummy_service_t **out);

/**
 * @brief  Destroy a dummy service
 *
 *         Stops the service if still running.
 *
 * @param[in]  svc  Dummy service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  svc is NULL
 *       - Others               Error returned by service deinitialization
 */
esp_err_t esp_media_dummy_service_destroy(esp_media_dummy_service_t *svc);

/**
 * @brief  Add a pattern track to a dummy source stream
 *
 *         Encoded codecs (AAC/OPUS/MP3/H264/MJPG) only need `info->info.*.codec`.
 *         Other fields must be 0 or match the built-in pattern defaults.
 *         Raw PCM / raw video need full track metadata so patterns can be generated.
 *
 * @param[in]  svc     Dummy source service
 * @param[in]  stream  Stream ID
 * @param[in]  info    Track metadata
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service, stream, or track information
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NOT_SUPPORTED  Source support or requested codec is unavailable
 *       - ESP_ERR_NO_MEM         Allocation or track limit failure
 *       - Others                 Error returned by the track manager
 */
esp_err_t esp_media_dummy_service_add_track(esp_media_dummy_service_t *svc,
                                            esp_media_stream_id_t stream,
                                            const esp_media_track_info_t *info);

/**
 * @brief  Remove all tracks from one dummy source stream
 *
 * @param[in]  svc     Dummy source service
 * @param[in]  stream  Stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NOT_SUPPORTED  Source support is disabled
 */
esp_err_t esp_media_dummy_service_reset_tracks(esp_media_dummy_service_t *svc,
                                               esp_media_stream_id_t stream);

/**
 * @brief  Rewind dummy source to the first keyframe before recording
 *
 *         Required when dummy src starts before capture: pipeline open consumes
 *         one video frame (H264 IDR) and some audio during element open. Call
 *         after capture service start and before start_record.
 *
 * @param[in]  svc     Dummy source service (must be running)
 * @param[in]  stream  Stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream
 *       - ESP_ERR_INVALID_STATE  Service is not running
 *       - ESP_ERR_NOT_SUPPORTED  Source support is disabled
 */
esp_err_t esp_media_dummy_service_sync_record(esp_media_dummy_service_t *svc,
                                              esp_media_stream_id_t stream);

/**
 * @brief  Get sink statistics for one stream
 *
 *         Stats are reset automatically on service start. Call after stop to read
 *         the completed run.
 *
 * @param[in]   svc     Dummy sink service
 * @param[in]   stream  Stream ID
 * @param[out]  stats   Output stream statistics
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service, stream, or output argument
 *       - ESP_ERR_NOT_SUPPORTED  Sink support is disabled
 */
esp_err_t esp_media_dummy_service_get_stats(esp_media_dummy_service_t *svc,
                                            esp_media_stream_id_t stream,
                                            esp_media_dummy_stream_stats_t *stats);

/**
 * @brief  Manually clear sink statistics
 *
 * @param[in]  svc  Dummy sink service
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or service role
 *       - ESP_ERR_NOT_SUPPORTED  Sink support is disabled
 */
esp_err_t esp_media_dummy_service_reset_stats(esp_media_dummy_service_t *svc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
