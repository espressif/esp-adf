/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Media frame flags
 */
#define ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED  (1U << 0)  /*!< Zero-size control frame carrying updated track info */
#define ESP_MEDIA_FRAME_FLAG_KEY            (1U << 1)  /*!< Key frame */
#define ESP_MEDIA_FRAME_FLAG_EOS            (1U << 2)  /*!< End of stream */
#define ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED  (1U << 3)  /*!< Zero-size control frame indicating track removal */

/**
 * @brief  FourCC-style media codec value
 */
typedef uint32_t esp_media_codec_fourcc_t;

/**
 * @brief  Media track type
 */
typedef enum {
    ESP_MEDIA_TRACK_TYPE_UNKNOWN = 0,  /*!< Unknown track type */
    ESP_MEDIA_TRACK_TYPE_AUDIO   = 1,  /*!< Audio track type */
    ESP_MEDIA_TRACK_TYPE_VIDEO   = 2,  /*!< Video track type */
    ESP_MEDIA_TRACK_TYPE_MUXER   = 3,  /*!< Muxer track type */
} esp_media_track_type_t;

/**
 * @brief  Audio track metadata
 */
typedef struct {
    esp_media_codec_fourcc_t  codec;            /*!< Encoded or raw audio codec */
    uint32_t                  sample_rate;      /*!< Sample rate in Hz */
    uint8_t                   bits_per_sample;  /*!< Bits per sample */
    uint8_t                   channel;          /*!< Channel count */
    uint32_t                  bitrate;          /*!< Bitrate in bit/s, 0 if unknown */
} esp_media_audio_info_t;

/**
 * @brief  Video track metadata
 */
typedef struct {
    esp_media_codec_fourcc_t  codec;    /*!< Encoded or raw video codec */
    uint16_t                  width;    /*!< Frame width in pixels */
    uint16_t                  height;   /*!< Frame height in pixels */
    uint16_t                  fps;      /*!< Frame-rate */
    uint32_t                  bitrate;  /*!< Bitrate in bit/s, 0 if unknown */
} esp_media_video_info_t;

/**
 * @brief  Media track metadata
 */
typedef struct {
    uint16_t                id;    /*!< Service-local track id */
    esp_media_track_type_t  type;  /*!< Track type */
    union {
        esp_media_audio_info_t  audio;  /*!< Audio metadata when type is AUDIO */
        esp_media_video_info_t  video;  /*!< Video metadata when type is VIDEO */
    } info;
} esp_media_track_info_t;

/**
 * @brief  Media frame descriptor
 */
typedef struct {
    uint16_t                track_id;   /*!< Track id that owns this frame */
    esp_media_track_type_t  type;       /*!< Frame media type */
    void                   *data;       /*!< Frame payload */
    size_t                  size;       /*!< Payload size in bytes */
    int64_t                 pts;        /*!< Presentation timestamp in us */
    int64_t                 dts;        /*!< Decode timestamp in us, or pts when not separate */
    uint32_t                flags;      /*!< Codec/container-specific frame flags, check ESP_MEDIA_FRAME_FLAG_XXX for details */
    void                   *user_data;  /*!< Optional owner-specific metadata */
} esp_media_frame_t;

/**
 * @brief  Media provider event type for dynamic track changes
 */
typedef enum {
    ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED   = 1,  /*!< New track is available */
    ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED = 2,  /*!< Existing track metadata changed */
    ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED = 3,  /*!< Track is no longer available */
    ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT  = 4,  /*!< All tracks aborted; info is NULL */
} esp_media_provider_event_t;

/**
 * @brief  Provider event callback
 *
 * @note  The track info pointer is only valid during the callback
 *        info is NULL for ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT
 */
typedef void (*esp_media_provider_event_cb_t)(esp_media_provider_event_t event, const esp_media_track_info_t *info,
                                              void *ctx);

/**
 * @brief  Provider operations implemented by a source/provider
 */
typedef struct {
    esp_err_t (*get_track_num)(void *ctx, uint16_t *out_num);                                  /*!< Get available track count */
    esp_err_t (*get_track_info)(void *ctx, uint16_t index, esp_media_track_info_t *out_info);  /*!< Get track metadata by index */
    esp_err_t (*set_event_cb)(void *ctx, esp_media_provider_event_cb_t cb, void *event_ctx);   /*!< Register dynamic track event callback */
    esp_err_t (*acquire_frame)(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms);  /*!< Acquire a frame without copying payload */
    esp_err_t (*read_frame)(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms);     /*!< Read a frame into caller-provided storage */
    esp_err_t (*release_frame)(void *ctx, esp_media_frame_t *frame);                           /*!< Release an acquired frame */
    esp_err_t (*abort)(void *ctx);                                                             /*!< Abort from sink: will wakeup source side write blocking */
} esp_media_provider_ops_t;

/**
 * @brief  Media provider handle
 */
typedef struct {
    const esp_media_provider_ops_t *ops;  /*!< Provider operations */
    void                           *ctx;  /*!< Provider implementation context */
} esp_media_provider_t;

/**
 * @brief  Track manager queue statistics
 */
typedef struct {
    int  q_num;   /*!< Number of queued blocks */
    int  q_size;  /*!< Total queued byte size */
} esp_media_track_mngr_queue_info_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
