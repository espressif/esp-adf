/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_gmf_data_queue.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_muxer.h"
#include "esp_muxer_default.h"
#include "esp_muxer_service.h"
#include "esp_service_scheduler.h"
#include "media_lib_os.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_MUXER_SERVICE_DEFAULT_QUEUE_SIZE  (32 * 1024)  /*!< Default streaming queue size in bytes */
#define ESP_MUXER_SERVICE_MAX_PATH            256          /*!< Max storage path length including NUL */
#define ESP_MUXER_SERVICE_TRACK_AUDIO         0            /*!< Index of the audio slot in tracks[] */
#define ESP_MUXER_SERVICE_TRACK_VIDEO         1            /*!< Index of the video slot in tracks[] */
#define ESP_MUXER_SERVICE_TRACK_MAX           2            /*!< Audio + video slots */
#define ESP_MUXER_SERVICE_DEFAULT_SLICE_DUR   (300000)     /*!< Default slice duration: 5 minutes in ms */

#define MUXER_SCHED_TASK_STACK_SIZE  (6144)  /*!< Default muxer_sink stack */
#define MUXER_SCHED_TASK_PRIORITY    (10)    /*!< Default muxer_sink priority */
#define MUXER_SCHED_TASK_CORE_ID     (0)     /*!< Default muxer_sink core */

/**
 * @brief  Binding of one incoming media track to a muxer stream index
 */
typedef struct {
    int       stream_index;  /*!< Muxer stream index once added; -1 if unused */
    uint16_t  track_id;      /*!< Provider track id */
    bool      active;        /*!< True after the track is added to the muxer */
} muxer_service_track_state_t;

/**
 * @brief  Muxer sink service instance (opaque as esp_muxer_service_t)
 */
struct esp_muxer_service {
    esp_media_service_t          media;                                /*!< Base media service (must be first) */
    esp_muxer_type_t             muxer_type;                           /*!< Container type from setup / URL */
    char                        *storage_dir;                          /*!< Copied storage directory; NULL if unused */
    char                        *storage_url;                          /*!< Copied file URL/path; NULL if unused */
    esp_muxer_service_mode_t     mode;                                 /*!< Storage / streaming / both */
    uint32_t                     slice_duration;                       /*!< Slice length in ms; 0 uses muxer default */
    uint32_t                     ram_cache_size;                       /*!< File-write RAM cache; 0 uses muxer default */
    uint32_t                     streaming_cache_size;                 /*!< Streaming queue size; 0 uses default */
    esp_media_provider_t         provider;                             /*!< Linked source provider */
    esp_muxer_handle_t           muxer;                                /*!< Active muxer handle while running */
    esp_gmf_data_queue_t        *stream_queue;                         /*!< Muxed-byte queue when streaming */
    media_lib_thread_handle_t    task;                                 /*!< Muxer worker thread */
    volatile bool                task_stop;                            /*!< Set to request worker exit */
    volatile bool                task_running;                         /*!< True while worker is alive */
    muxer_service_track_state_t  tracks[ESP_MUXER_SERVICE_TRACK_MAX];  /*!< Audio / video bind state */
};

/**
 * @brief  Map API timeout_ms to an esp_gmf_data_queue wait value
 *
 *         0 means no wait; UINT32_MAX means wait forever.
 */
static inline uint32_t muxer_queue_timeout_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return ESP_GMF_DATA_QUEUE_NO_WAIT;
    }
    if (timeout_ms == UINT32_MAX) {
        return ESP_GMF_DATA_QUEUE_WAIT_FOREVER;
    }
    return timeout_ms;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
