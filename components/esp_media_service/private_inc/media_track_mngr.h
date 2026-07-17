/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_data_queue.h"
#include "esp_media_track_mngr.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct {
    esp_media_frame_t       frame;
    size_t                  payload_size;
    size_t                  payload_offset;
    size_t                  alloc_size;
    bool                    has_track_info;
    esp_media_track_info_t  track_info[0];
} track_frame_node_t;

typedef struct {
    esp_media_track_info_t        info;           /*!< Media track information */
    esp_gmf_data_queue_t         *queue;          /*!< Data queue to store media data */
    track_frame_node_t           *write_node;     /*!< Current write node */
    track_frame_node_t           *read_node;      /*!< Current read node */
    esp_media_track_cache_type_t  cache_type;     /*!< Cache type */
    uint16_t                      addr_align;     /*!< Frame address alignment */
    uint16_t                      size_align;     /*!< Frame size alignment */
    esp_media_frame_release_cb_t  frame_release;  /*!< User frame release callback */
    void                         *release_ctx;    /*!< User frame release context */
} media_track_t;

struct esp_media_track_mngr {
    media_track_t                 *tracks;             /*!< Media tracks */
    uint16_t                       max_track_num;      /*!< Maximum track number */
    uint16_t                       track_num;          /*!< Added track number */
    esp_gmf_data_queue_t          *global_queue;       /*!< Global frame cache data queue for multiple tracks */
    bool                           use_global_cache;   /*!< Whether use global cache or not */
    size_t                         global_cache_size;  /*!< Global cache size */
    esp_media_provider_event_cb_t  event_cb;           /*!< Media provider event callback */
    void                          *event_ctx;          /*!< Media provider event context */
    bool                           aborted;            /*!< Whether aborted or not */
};

static inline uint32_t queue_timeout_ms(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return ESP_GMF_DATA_QUEUE_NO_WAIT;
    }
    if (timeout_ms == UINT32_MAX) {
        return ESP_GMF_DATA_QUEUE_WAIT_FOREVER;
    }
    return timeout_ms;
}

static inline media_track_t *find_track_by_frame(const esp_media_track_mngr_t *mngr, const esp_media_frame_t *frame)
{
    uint16_t index = 0;
    uint16_t track_num = 0;
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        if (mngr->tracks[i].info.type == frame->type) {
            if (frame->track_id == mngr->tracks[i].info.id) {
                return &mngr->tracks[i];
            }
            index = i;
            track_num++;
        }
    }
    // When only one track return directly
    if (track_num == 1) {
        return &mngr->tracks[index];
    }
    return NULL;
}

static inline esp_media_track_cache_type_t track_cache_type(const esp_media_track_mngr_t *mngr,
                                                            const media_track_t *track)
{
    (void)mngr;
    return track->cache_type;
}

static esp_gmf_data_queue_t *track_queue(esp_media_track_mngr_t *mngr, media_track_t *track)
{
    return mngr->use_global_cache ? mngr->global_queue : track->queue;
}

static track_frame_node_t *acquire_frame_node(esp_gmf_data_queue_t *queue, int size, uint32_t timeout_ms, int *ret)
{
    void *buffer = NULL;
    if (queue != NULL) {
        *ret = esp_gmf_data_queue_acquire_write(queue, &buffer, size, queue_timeout_ms(timeout_ms));
    } else {
        *ret = ESP_ERR_INVALID_ARG;
        return NULL;
    }
    if (*ret == ESP_GMF_IO_ABORT) {
        *ret = ESP_ERR_INVALID_STATE;
        return NULL;
    }
    if (*ret != 0) {
        *ret = (*ret == ESP_GMF_IO_TIMEOUT) ? ESP_ERR_TIMEOUT : ESP_FAIL;
        return NULL;
    }
    return (track_frame_node_t *)buffer;
}

static uint8_t *node_payload(track_frame_node_t *node)
{
    return (uint8_t *)node + node->payload_offset;
}

static inline esp_err_t wakeup_tracks(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        if (mngr->tracks[i].queue != NULL) {
            esp_gmf_data_queue_wakeup(mngr->tracks[i].queue);
        }
    }
    if (mngr->global_queue != NULL) {
        esp_gmf_data_queue_wakeup(mngr->global_queue);
    }
    return ESP_OK;
}

void track_mngr_reset_data_queue(esp_media_track_mngr_t *mngr);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
