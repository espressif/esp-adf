/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_media_track_mngr.h"
#include "media_track_mngr.h"
#include "media_service_err.h"

static const char *TAG = "MEDIA_TRACK";

#define ALIGN_UP(size, align)  ((size + align - 1) & ~(align - 1))

static size_t aligned_offset(uintptr_t base, size_t offset, size_t align_size)
{
    uintptr_t start = base + offset;
    uintptr_t aligned = ((start + align_size - 1) / align_size) * align_size;
    return (size_t)(aligned - base);
}

static size_t node_alloc_size_for(esp_media_track_cache_type_t cache_type, size_t align_size, size_t payload_size)
{
    if (cache_type == ESP_MEDIA_TRACK_CACHE_USER) {
        return sizeof(track_frame_node_t);
    }
    return sizeof(track_frame_node_t) + payload_size + align_size - 1;
}

static void reset_data_queue(esp_media_track_mngr_t *mngr)
{
    track_mngr_reset_data_queue(mngr);
}

esp_err_t esp_media_track_acquire_frame(esp_media_track_mngr_t *mngr, esp_media_frame_t *out_frame, size_t size,
                                        uint32_t timeout_ms)
{
    if (mngr == NULL || out_frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid arguments mngr:%p out_frame:%p", mngr, out_frame);
    }
    if (mngr->aborted) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Track mngr aborted mngr:%p", mngr);
    }
    media_track_t *track = find_track_by_frame(mngr, out_frame);
    if (track == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Track not found mngr:%p type:%d", mngr, out_frame->type);
    }
    if (track_cache_type(mngr, track) != ESP_MEDIA_TRACK_CACHE_INTERNAL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Track cache type not supported mngr:%p type:%d", mngr, out_frame->type);
    }
    if (track->write_node != NULL) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Track write node not null mngr:%p", mngr);
    }

    uint16_t align_size = track->addr_align;
    if (track->size_align) {
        size = ALIGN_UP(size, track->size_align);
    }
    size_t alloc_size = node_alloc_size_for(ESP_MEDIA_TRACK_CACHE_INTERNAL, align_size, size);
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    int ret = ESP_OK;
    track_frame_node_t *node = acquire_frame_node(queue, (int)alloc_size, timeout_ms, &ret);
    if (node == NULL) {
        return mngr->aborted ? ESP_ERR_INVALID_STATE : ret;
    }
    memset(node, 0, sizeof(*node));
    node->payload_size = size;
    node->payload_offset = aligned_offset((uintptr_t)node, sizeof(*node), align_size);
    node->alloc_size = alloc_size;
    node->frame.track_id = track->info.id;
    node->frame.type = track->info.type;
    node->frame.data = node_payload(node);
    node->frame.size = size;
    track->write_node = node;
    *out_frame = node->frame;
    return ESP_OK;
}

esp_err_t esp_media_track_write_frame(esp_media_track_mngr_t *mngr, const esp_media_frame_t *frame, uint32_t timeout_ms)
{
    if (mngr == NULL || frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid arguments mngr:%p frame:%p", mngr, frame);
    }
    if (frame->size > 0 && frame->data == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid frame data or size");
    }
    if (mngr->aborted) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Track mngr aborted mngr:%p", mngr);
    }
    media_track_t *track = find_track_by_frame(mngr, frame);
    if (track == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Track not found mngr:%p type:%d", mngr, frame->type);
    }
    esp_media_track_cache_type_t cache_type = track_cache_type(mngr, track);
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    int ret = ESP_OK;
    if (cache_type == ESP_MEDIA_TRACK_CACHE_USER) {
        track_frame_node_t *node = acquire_frame_node(queue, sizeof(*node), timeout_ms, &ret);
        if (node == NULL) {
            return mngr->aborted ? ESP_ERR_INVALID_STATE : ret;
        }
        memset(node, 0, sizeof(*node));
        node->frame = *frame;
        node->payload_size = frame->size;
        node->alloc_size = sizeof(*node);
        return esp_gmf_data_queue_release_write(queue, sizeof(*node)) == 0 ? ESP_OK : ESP_FAIL;
    }

    if (track->write_node != NULL) {
        if (frame->size > track->write_node->payload_size) {
            RET_FOR(ESP_ERR_INVALID_SIZE, "Frame size is too large for write node mngr:%p frame:%p", mngr, frame);
        }
        void *payload = node_payload(track->write_node);
        track->write_node->frame = *frame;
        track->write_node->frame.data = payload;
        track->write_node->payload_size = frame->size;
        size_t alloc_size = track->write_node->alloc_size;
        esp_err_t ret = esp_gmf_data_queue_release_write(queue, (int)alloc_size) == 0 ? ESP_OK : ESP_FAIL;
        track->write_node = NULL;
        return ret;
    }

    size_t frame_size = frame->size;
    uint16_t align_size = track->addr_align;
    if (track->size_align) {
        frame_size = ALIGN_UP(frame_size, track->size_align);
    }
    size_t alloc_size = node_alloc_size_for(ESP_MEDIA_TRACK_CACHE_INTERNAL, align_size, frame_size);
    track_frame_node_t *node = acquire_frame_node(queue, (int)alloc_size, timeout_ms, &ret);
    if (node == NULL) {
        return mngr->aborted ? ESP_ERR_INVALID_STATE : ret;
    }
    memset(node, 0, sizeof(*node));
    node->frame = *frame;
    node->payload_size = frame->size;
    node->payload_offset = aligned_offset((uintptr_t)node, sizeof(*node), align_size);
    node->alloc_size = alloc_size;
    node->frame.data = node_payload(node);
    if (frame->size > 0) {
        memcpy(node->frame.data, frame->data, frame->size);
    }
    return esp_gmf_data_queue_release_write(queue, (int)alloc_size) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_media_track_release_frame(esp_media_track_mngr_t *mngr, esp_media_frame_t *frame)
{
    if (mngr == NULL || frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid arguments mngr:%p frame:%p", mngr, frame);
    }
    media_track_t *track = find_track_by_frame(mngr, frame);
    if (track == NULL || track->write_node == NULL) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Invalid track or write node %p", track ? track->write_node : NULL);
    }
    track->write_node = NULL;
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    return queue != NULL && esp_gmf_data_queue_release_write(queue, 0) == 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t esp_media_track_write_abort(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid write abort args mngr:%p", mngr);
    }
    bool notify = !mngr->aborted;
    mngr->aborted = true;
    if (notify && mngr->event_cb != NULL) {
        mngr->event_cb(ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT, NULL, mngr->event_ctx);
    }
    RET_CHK(wakeup_tracks(mngr), "Failed to wake track for write abort");
    return ESP_OK;
}

esp_err_t esp_media_track_clear_abort(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args mngr:%p", mngr);
    }
    if (!mngr->aborted) {
        return ESP_OK;
    }
    mngr->aborted = false;
    reset_data_queue(mngr);
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_query(esp_media_track_mngr_t *mngr, esp_media_track_type_t type,
                                     esp_media_track_mngr_queue_info_t *out_info)
{
    if (mngr == NULL || out_info == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid query args mngr:%p out_info:%p",
                mngr, out_info);
    }
    out_info->q_num = 0;
    out_info->q_size = 0;
    if (mngr->use_global_cache) {
        RET_CHK(esp_gmf_data_queue_query(mngr->global_queue, &out_info->q_num,
                                         &out_info->q_size),
                "Failed to query global queue");
        return ESP_OK;
    }
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        media_track_t *track = &mngr->tracks[i];
        if (track->info.type != type) {
            continue;
        }
        RET_CHK(esp_gmf_data_queue_query(track->queue, &out_info->q_num, &out_info->q_size),
                "Failed to query track queue type:%d", (int)type);
        return ESP_OK;
    }
    RET_FOR(ESP_ERR_NOT_FOUND, "Track type not found type:%d", (int)type);
}
