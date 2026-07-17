/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_gmf_data_queue.h"
#include "esp_media_track_mngr.h"
#include "media_track_mngr.h"
#include "media_service_err.h"

static const char *TAG = "MEDIA_TRACK_MNGR";

#define MEDIA_TRACK_DEFAULT_QUEUE_NUM   (4)
#define MEDIA_TRACK_DEFAULT_QUEUE_SIZE  (8 * 1024)

static inline esp_err_t io_err_to_esp(int ret)
{
    if (ret == 0) {
        return ESP_OK;
    }
    if (ret == ESP_GMF_IO_ABORT) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_ERR_TIMEOUT;
}

static inline size_t normalize_align_size(size_t align_size)
{
    return align_size == 0 ? sizeof(void *) : align_size;
}

static size_t track_queue_size(esp_media_track_mngr_t *mngr, const esp_media_track_mngr_cache_cfg_t *cfg)
{
    if (cfg->cache_type == ESP_MEDIA_TRACK_CACHE_USER) {
        uint16_t queue_num = cfg->user_queue.queue_num == 0 ? MEDIA_TRACK_DEFAULT_QUEUE_NUM : cfg->user_queue.queue_num;
        size_t track_info_size = sizeof(esp_media_track_info_t) * mngr->max_track_num;
        return queue_num * (sizeof(track_frame_node_t) + sizeof(int)) + track_info_size;
    }
    return cfg->track_cache.cache_size == 0 ? MEDIA_TRACK_DEFAULT_QUEUE_SIZE : cfg->track_cache.cache_size;
}

static inline size_t global_queue_size(size_t cache_size)
{
    return cache_size == 0 ? MEDIA_TRACK_DEFAULT_QUEUE_SIZE : cache_size;
}

static inline bool queue_have_data(esp_gmf_data_queue_t *queue)
{
    bool have_data = false;
    return queue != NULL && esp_gmf_data_queue_have_data(queue, &have_data) == 0 && have_data;
}

static media_track_t *find_read_track_by_frame(esp_media_track_mngr_t *mngr, const esp_media_frame_t *frame)
{
    media_track_t *track = find_track_by_frame(mngr, frame);
    if (track == NULL || track->read_node == NULL) {
        return NULL;
    }
    if (frame->data != NULL && track->read_node->frame.data != frame->data) {
        return NULL;
    }
    return track;
}

static media_track_t *select_read_track(esp_media_track_mngr_t *mngr, const esp_media_frame_t *request)
{
    if (mngr->use_global_cache) {
        return mngr->track_num > 0 ? &mngr->tracks[0] : NULL;
    }
    if (request != NULL) {
        media_track_t *track = find_track_by_frame(mngr, request);
        if (track != NULL) {
            return track;
        }
        if (request->type != ESP_MEDIA_TRACK_TYPE_UNKNOWN) {
            return NULL;
        }
    }
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        if (queue_have_data(mngr->tracks[i].queue)) {
            return &mngr->tracks[i];
        }
    }
    return mngr->track_num > 0 ? &mngr->tracks[0] : NULL;
}

static void release_pending_user_frames(esp_media_track_mngr_t *mngr, media_track_t *track)
{
    (void)mngr;
    if (track->cache_type != ESP_MEDIA_TRACK_CACHE_USER || track->frame_release == NULL) {
        return;
    }
    while (queue_have_data(track->queue)) {
        void *buffer = NULL;
        int size = 0;
        if (esp_gmf_data_queue_acquire_read(track->queue, &buffer, &size, ESP_GMF_DATA_QUEUE_WAIT_FOREVER) != 0) {
            break;
        }
        if (size >= (int)sizeof(track_frame_node_t)) {
            track_frame_node_t *node = (track_frame_node_t *)buffer;
            if (!node->has_track_info) {
                track->frame_release(&node->frame, track->release_ctx);
            }
        }
        esp_gmf_data_queue_release_read(track->queue);
    }
}

static void release_pending_global_user_frames(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL || mngr->global_queue == NULL) {
        return;
    }
    while (queue_have_data(mngr->global_queue)) {
        void *buffer = NULL;
        int size = 0;
        if (esp_gmf_data_queue_acquire_read(mngr->global_queue, &buffer, &size, ESP_GMF_DATA_QUEUE_WAIT_FOREVER) != 0) {
            break;
        }
        if (size >= (int)sizeof(track_frame_node_t)) {
            track_frame_node_t *node = (track_frame_node_t *)buffer;
            media_track_t *track = find_track_by_frame(mngr, &node->frame);
            if (track != NULL && track->cache_type == ESP_MEDIA_TRACK_CACHE_USER &&
                track->frame_release != NULL && !node->has_track_info) {
                track->frame_release(&node->frame, track->release_ctx);
            }
        }
        esp_gmf_data_queue_release_read(mngr->global_queue);
    }
}

void track_mngr_reset_data_queue(esp_media_track_mngr_t *mngr)
{
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        media_track_t *track = &mngr->tracks[i];
        release_pending_user_frames(mngr, track);
        track->write_node = NULL;
        track->read_node = NULL;
        if (track->queue != NULL) {
            esp_gmf_data_queue_reset(track->queue);
        }
    }
    if (mngr->global_queue != NULL) {
        release_pending_global_user_frames(mngr);
        esp_gmf_data_queue_reset(mngr->global_queue);
    }
}

static esp_err_t provider_get_track_num(void *ctx, uint16_t *out_num)
{
    const esp_media_track_mngr_t *mngr = (const esp_media_track_mngr_t *)ctx;
    if (mngr == NULL || out_num == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid get track num args mngr:%p out_num:%p",
                mngr, out_num);
    }
    *out_num = mngr->track_num;
    return ESP_OK;
}

static esp_err_t provider_get_track_info(void *ctx, uint16_t index, esp_media_track_info_t *out_info)
{
    const esp_media_track_mngr_t *mngr = (const esp_media_track_mngr_t *)ctx;
    if (mngr == NULL || out_info == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid get track info args mngr:%p out_info:%p",
                mngr, out_info);
    }
    if (index >= mngr->track_num) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Track index not found index:%u track_num:%u",
                index, mngr->track_num);
    }
    *out_info = mngr->tracks[index].info;
    return ESP_OK;
}

static esp_err_t provider_set_cb(void *ctx, esp_media_provider_event_cb_t cb, void *event_ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_track_mngr_t *mngr = (esp_media_track_mngr_t *)ctx;
    mngr->event_cb = cb;
    mngr->event_ctx = event_ctx;
    return ESP_OK;
}

static esp_err_t provider_release_frame(void *ctx, esp_media_frame_t *frame)
{
    if (ctx == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_track_mngr_t *mngr = (esp_media_track_mngr_t *)ctx;
    media_track_t *track = find_read_track_by_frame(mngr, frame);
    if (track == NULL || track->read_node == NULL) {
        ESP_LOGE(TAG, "Failed to release frame type:%d size:%u track:%p read_node:%p",
                 (int)frame->type, (unsigned)frame->size, (void *)track,
                 track != NULL ? (void *)track->read_node : NULL);
        return ESP_ERR_INVALID_STATE;
    }
    if (track_cache_type(mngr, track) == ESP_MEDIA_TRACK_CACHE_USER && track->frame_release != NULL &&
        !track->read_node->has_track_info) {
        track->frame_release(&track->read_node->frame, track->release_ctx);
    }
    track->read_node = NULL;
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    return queue != NULL && esp_gmf_data_queue_release_read(queue) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t provider_acquire_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    if (ctx == NULL || out_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_track_mngr_t *mngr = (esp_media_track_mngr_t *)ctx;
    if (mngr->aborted) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mngr->track_num == 0) {
        return timeout_ms == 0 ? ESP_ERR_NOT_FOUND : ESP_ERR_TIMEOUT;
    }
    media_track_t *track = select_read_track(mngr, out_frame);
    if (track == NULL) {
        return timeout_ms == 0 ? ESP_ERR_NOT_FOUND : ESP_ERR_TIMEOUT;
    }
    void *buffer = NULL;
    int size = 0;
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    int qret = ESP_GMF_IO_FAIL;
    if (queue != NULL) {
        qret = esp_gmf_data_queue_acquire_read(queue, &buffer, &size, queue_timeout_ms(timeout_ms));
    }
    if (qret != 0) {
        return io_err_to_esp(qret);
    }
    if (size < (int)sizeof(track_frame_node_t)) {
        esp_gmf_data_queue_release_read(queue);
        return ESP_FAIL;
    }
    track_frame_node_t *node = (track_frame_node_t *)buffer;
    if (mngr->use_global_cache) {
        media_track_t *node_track = find_track_by_frame(mngr, &node->frame);
        if (node_track != NULL) {
            track = node_track;
        }
    }
    if (track->read_node != NULL) {
        esp_gmf_data_queue_release_read(queue);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGD(TAG, "Acquire frame, track:%p %d, read_node:%p", track, (int)track->info.type, node);
    track->read_node = node;
    if (node->has_track_info) {
        if ((node->frame.flags & ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED) != 0) {
            esp_media_track_info_t removed = node->track_info[0];
            track->read_node = NULL;
            esp_gmf_data_queue_release_read(queue);
            if (mngr->event_cb != NULL) {
                mngr->event_cb(ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED, &removed, mngr->event_ctx);
            }
            return mngr->aborted ? ESP_ERR_INVALID_STATE : ESP_ERR_NOT_FOUND;
        }
        track->info = node->track_info[0];
        if (mngr->event_cb != NULL) {
            mngr->event_cb(ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED, &track->info, mngr->event_ctx);
        }
    }
    *out_frame = node->frame;
    if (track_cache_type(mngr, track) == ESP_MEDIA_TRACK_CACHE_INTERNAL && !node->has_track_info) {
        out_frame->data = node_payload(node);
    }
    return ESP_OK;
}

static esp_err_t provider_read_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    if (ctx == NULL || out_frame == NULL || out_frame->data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    void *buffer = out_frame->data;
    size_t buffer_size = out_frame->size;
    esp_err_t ret = provider_acquire_frame(ctx, out_frame, timeout_ms);
    if (ret != ESP_OK) {
        return ret;
    }
    if (out_frame->size > buffer_size) {
        ESP_LOGW(TAG, "Frame size over read buffer %d > %d", (int)out_frame->size, (int)buffer_size);
        provider_release_frame(ctx, out_frame);
        return ESP_ERR_INVALID_SIZE;
    }
    if (out_frame->size > 0 && out_frame->data != NULL) {
        memcpy(buffer, out_frame->data, out_frame->size);
    }
    size_t actual_size = out_frame->size;
    ret = provider_release_frame(ctx, out_frame);
    if (ret != ESP_OK) {
        return ret;
    }
    out_frame->data = buffer;
    out_frame->size = actual_size;
    return ESP_OK;
}

static esp_err_t provider_abort(void *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_media_track_mngr_t *mngr = (esp_media_track_mngr_t *)ctx;
    mngr->aborted = true;
    return wakeup_tracks(mngr);
}

esp_err_t esp_media_track_mngr_create(const esp_media_track_mngr_cfg_t *cfg, esp_media_track_mngr_t **out_mngr)
{
    if (cfg == NULL || cfg->max_track_num == 0 || out_mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args cfg:%p out_mngr:%p",
                cfg, out_mngr);
    }
    esp_media_track_mngr_t *mngr = calloc(1, sizeof(esp_media_track_mngr_t));
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate track mngr");
    }
    mngr->tracks = calloc(cfg->max_track_num, sizeof(*mngr->tracks));
    if (mngr->tracks == NULL) {
        free(mngr);
        RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate tracks max:%u", cfg->max_track_num);
    }

    mngr->max_track_num = cfg->max_track_num;
    mngr->use_global_cache = cfg->use_global_cache;
    mngr->global_cache_size = global_queue_size(cfg->global_cache.cache_size);
    if (mngr->use_global_cache) {
        mngr->global_queue = esp_gmf_data_queue_create((int)mngr->global_cache_size);
        if (mngr->global_queue == NULL) {
            size_t global_cache_size = mngr->global_cache_size;
            free(mngr->tracks);
            free(mngr);
            RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate global queue size:%u",
                    (unsigned)global_cache_size);
        }
    }

    *out_mngr = mngr;
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_destroy(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args mngr:%p", mngr);
    }
    RET_CHK(esp_media_track_mngr_reset(mngr), "Failed to reset provider before destroy");
    if (mngr->global_queue != NULL) {
        esp_gmf_data_queue_wakeup(mngr->global_queue);
        esp_gmf_data_queue_destroy(mngr->global_queue);
        mngr->global_queue = NULL;
    }
    if (mngr->tracks != NULL) {
        free(mngr->tracks);
        mngr->tracks = NULL;
    }
    free(mngr);
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_reset(esp_media_track_mngr_t *mngr)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args mngr:%p", mngr);
    }
    mngr->aborted = false;
    release_pending_global_user_frames(mngr);
    for (uint16_t i = 0; i < mngr->track_num; i++) {
        media_track_t *track = &mngr->tracks[i];
        track->write_node = NULL;
        if (track->queue != NULL) {
            release_pending_user_frames(mngr, track);
            esp_gmf_data_queue_wakeup(track->queue);
            esp_gmf_data_queue_destroy(track->queue);
        }
        memset(track, 0, sizeof(*track));
    }
    mngr->track_num = 0;
    if (mngr->global_queue != NULL) {
        esp_gmf_data_queue_wakeup(mngr->global_queue);
        esp_gmf_data_queue_reset(mngr->global_queue);
    }
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_set_global_cache(esp_media_track_mngr_t *mngr, bool enable, size_t cache_size)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args mngr:%p", mngr);
    }
    if (mngr->track_num != 0) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Cannot set global cache after tracks added track_num:%u",
                mngr->track_num);
    }
    if (mngr->global_queue != NULL) {
        esp_gmf_data_queue_wakeup(mngr->global_queue);
        esp_gmf_data_queue_destroy(mngr->global_queue);
        mngr->global_queue = NULL;
    }
    mngr->use_global_cache = enable;
    mngr->global_cache_size = global_queue_size(cache_size);
    if (enable) {
        mngr->global_queue = esp_gmf_data_queue_create((int)mngr->global_cache_size);
        if (mngr->global_queue == NULL) {
            mngr->use_global_cache = false;
            RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate global queue size:%u",
                    (unsigned)mngr->global_cache_size);
        }
    }
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_add_track(esp_media_track_mngr_t *mngr, const esp_media_track_mngr_track_cfg_t *cfg)
{
    if (mngr == NULL || cfg == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid add track args mngr:%p cfg:%p", mngr, cfg);
    }
    if (mngr->track_num >= mngr->max_track_num) {
        RET_FOR(ESP_ERR_NO_MEM, "Track limit reached track_num:%u max:%u",
                mngr->track_num, mngr->max_track_num);
    }

    media_track_t *track = &mngr->tracks[mngr->track_num];
    if (mngr->use_global_cache) {
        if (mngr->global_queue == NULL) {
            mngr->global_queue = esp_gmf_data_queue_create((int)mngr->global_cache_size);
            if (mngr->global_queue == NULL) {
                RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate global queue size:%u",
                        (unsigned)mngr->global_cache_size);
            }
        }
    } else {
        track->queue = esp_gmf_data_queue_create((int)track_queue_size(mngr, &cfg->cache_cfg));
        if (track->queue == NULL) {
            RET_FOR(ESP_ERR_NO_MEM, "Failed to allocate track queue index:%u",
                    mngr->track_num);
        }
    }
    track->info = cfg->info;
    track->cache_type = cfg->cache_cfg.cache_type;
    if (track->cache_type == ESP_MEDIA_TRACK_CACHE_USER) {
        track->addr_align = sizeof(void *);
        track->frame_release = cfg->cache_cfg.user_queue.frame_release;
        track->release_ctx = cfg->cache_cfg.user_queue.release_ctx;
    } else {
        track->addr_align = normalize_align_size(cfg->cache_cfg.track_cache.addr_align);
        track->size_align = cfg->cache_cfg.track_cache.size_align;
    }
    mngr->track_num++;
    if (mngr->event_cb != NULL) {
        mngr->event_cb(ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED, &cfg->info, mngr->event_ctx);
    }
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_update_track(esp_media_track_mngr_t *mngr, uint16_t index,
                                            const esp_media_track_info_t *info)
{
    if (mngr == NULL || info == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args mngr:%p info:%p",
                mngr, info);
    }
    if (index >= mngr->track_num) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Track index not found index:%u track_num:%u",
                index, mngr->track_num);
    }
    media_track_t *track = &mngr->tracks[index];
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    if (queue == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Track queue unavailable index:%u", index);
    }
    int size = sizeof(track_frame_node_t) + sizeof(esp_media_track_info_t);
    int ret = ESP_OK;
    track_frame_node_t *node = acquire_frame_node(queue, size, ESP_GMF_DATA_QUEUE_WAIT_FOREVER, &ret);
    if (node == NULL) {
        RET_FOR(ret, "Failed to queue track update index:%u", index);
    }
    memset(node, 0, size);
    node->frame.track_id = track->info.id;
    node->frame.type = info->type;
    node->frame.flags = ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED;
    node->has_track_info = true;
    node->track_info[0] = *info;
    node->alloc_size = size;
    if (esp_gmf_data_queue_release_write(queue, size) != 0) {
        RET_FOR(ESP_FAIL, "Failed to commit track update index:%u", index);
    }
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_remove_track(esp_media_track_mngr_t *mngr, uint16_t index)
{
    if (mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid remove track args mngr:%p", mngr);
    }
    if (index >= mngr->track_num) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Track index not found index:%u track_num:%u",
                index, mngr->track_num);
    }
    media_track_t *track = &mngr->tracks[index];
    esp_gmf_data_queue_t *queue = track_queue(mngr, track);
    if (queue == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Track queue unavailable index:%u", index);
    }
    int size = sizeof(track_frame_node_t) + sizeof(esp_media_track_info_t);
    int ret = ESP_OK;
    track_frame_node_t *node = acquire_frame_node(queue, size, ESP_GMF_DATA_QUEUE_WAIT_FOREVER, &ret);
    if (node == NULL) {
        RET_FOR(ret, "Failed to queue track removal index:%u", index);
    }
    memset(node, 0, size);
    node->frame.track_id = track->info.id;
    node->frame.type = track->info.type;
    node->frame.flags = ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED;
    node->has_track_info = true;
    node->track_info[0] = track->info;
    node->alloc_size = size;
    if (esp_gmf_data_queue_release_write(queue, size) != 0) {
        RET_FOR(ESP_FAIL, "Failed to commit track removal index:%u", index);
    }
    return ESP_OK;
}

esp_err_t esp_media_track_mngr_get_provider(esp_media_track_mngr_t *mngr, esp_media_provider_t *provider)
{
    if (provider == NULL || mngr == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid get provider args mngr:%p provider:%p",
                mngr, provider);
    }
    static const esp_media_provider_ops_t s_provider_ops = {
        .get_track_num = provider_get_track_num,
        .get_track_info = provider_get_track_info,
        .set_event_cb = provider_set_cb,
        .acquire_frame = provider_acquire_frame,
        .read_frame = provider_read_frame,
        .release_frame = provider_release_frame,
        .abort = provider_abort,
    };
    provider->ops = &s_provider_ops;
    provider->ctx = mngr;
    return ESP_OK;
}
