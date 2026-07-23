/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_media_provider.h"
#include "esp_media_track.h"

#include "media_dummy_priv.h"

void track_mngr_reset_data_queue(esp_media_track_mngr_t *mngr);

static const char *TAG = "DUMMY_SRC";

/** Feed every active track (start / global-cache top-up). */
#define DUMMY_FEED_ALL_TRACKS  UINT16_MAX

struct media_dummy_release_ctx {
    esp_media_dummy_service_t *svc;
    esp_media_stream_id_t      stream;
    uint16_t                   track_index;
};

static media_dummy_src_stream_t *src_stream(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream)
{
    if (svc == NULL || svc->src_streams == NULL || stream >= svc->max_stream_num) {
        return NULL;
    }
    return &svc->src_streams[stream];
}

static bool track_use_user_cache(const media_dummy_pattern_t *pattern)
{
    return pattern != NULL &&
           !pattern->encoded &&
           pattern->track_info.type == ESP_MEDIA_TRACK_TYPE_VIDEO;
}

static bool track_is_user_cache_video(const media_dummy_track_t *track)
{
    return track != NULL &&
           track->info.type == ESP_MEDIA_TRACK_TYPE_VIDEO &&
           track_use_user_cache(&track->pattern);
}

static uint16_t track_queue_capacity(const media_dummy_track_t *track)
{
    return track_is_user_cache_video(track) ? DUMMY_VIDEO_QUEUE_DEPTH : DUMMY_SEED_QUEUE_DEPTH;
}

static uint32_t track_frame_duration_ms(const media_dummy_track_t *track)
{
    if (track == NULL) {
        return 0;
    }
    return media_dummy_pattern_frame_duration_ms(&track->pattern);
}

static uint32_t track_video_fps(const media_dummy_track_t *track)
{
    uint32_t fps = track->pattern.track_info.info.video.fps;
    return fps ? fps : DUMMY_DEFAULT_VIDEO_FPS;
}

static void track_advance_pts(media_dummy_track_t *track)
{
    track->frame_index++;
    if (track->info.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        uint32_t fps = track_video_fps(track);
        track->pts_ms = (uint32_t)((uint64_t)track->frame_index * 1000U / fps);
    } else {
        track->pts_ms += track_frame_duration_ms(track);
    }
}

static uint32_t track_pcm_bytes_to_ms(const media_dummy_track_t *track, uint64_t bytes)
{
    if (track == NULL || bytes == 0) {
        return 0;
    }
    const esp_media_audio_info_t *audio = &track->pattern.track_info.info.audio;
    uint32_t byte_rate = audio->sample_rate * audio->channel * (audio->bits_per_sample / 8);
    if (byte_rate == 0) {
        return 0;
    }
    return (uint32_t)((bytes * 1000ULL) / byte_rate);
}

static int64_t stream_elapsed_ms(const media_dummy_src_stream_t *st)
{
    return (esp_timer_get_time() - st->start_time_us) / 1000;
}

static void ensure_stream_clock(media_dummy_src_stream_t *st)
{
    if (st != NULL && st->start_time_us == 0 && st->svc != NULL && st->svc->running) {
        st->start_time_us = esp_timer_get_time();
    }
}

/**
 * Sleep one slot if media timeline is more than one slot ahead of wall clock.
 * deliver_pts_ms: PTS (ms) of the payload about to be returned to the consumer.
 */
static void pace_before_deliver(media_dummy_src_stream_t *st, uint32_t deliver_pts_ms, uint32_t slot_ms)
{
    if (st == NULL || st->start_time_us == 0 || slot_ms == 0) {
        return;
    }
    if ((int64_t)deliver_pts_ms > stream_elapsed_ms(st) + (int64_t)slot_ms) {
        vTaskDelay(pdMS_TO_TICKS(slot_ms));
    }
}

static esp_err_t write_pattern_frame(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                     uint16_t track_index, uint32_t timeout_ms)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || track_index >= st->track_num || st->mngr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    media_dummy_track_t *track = &st->tracks[track_index];
    if (!track->active) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t *data = NULL;
    size_t size = 0;
    bool key = false;
    ESP_RETURN_ON_ERROR(media_dummy_pattern_get_frame(&track->pattern, track->frame_index, &data, &size, &key),
                        TAG, "get pattern frame");

    esp_media_frame_t frame = {
        .track_id = track->info.id,
        .type = track->info.type,
        .data = (void *)data,
        .size = size,
        .pts = track->pts_ms,
        .dts = track->pts_ms,
        .flags = key ? ESP_MEDIA_FRAME_FLAG_KEY : 0,
        .user_data = NULL,
    };
    esp_err_t ret = esp_media_track_write_frame(st->mngr, &frame, timeout_ms);
    if (ret == ESP_OK) {
        track_advance_pts(track);
    }
    return ret;
}

/** Write at most one pattern frame per feed call when running. */
static void feed_track(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream, uint16_t track_index)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || track_index >= st->track_num) {
        return;
    }
    media_dummy_track_t *track = &st->tracks[track_index];
    if (!track->active) {
        return;
    }

    if (!svc->running || st->start_time_us == 0) {
        return;
    }

    uint32_t duration_ms = track_frame_duration_ms(track);
    if (duration_ms == 0) {
        return;
    }

    uint16_t queue_cap = track_queue_capacity(track);
    /* Allow enough timeline lead to keep the user-cache queue full for slow venc. */
    uint32_t lead_ms = duration_ms * queue_cap;

    for (uint16_t i = 0; i < queue_cap; i++) {
        if ((int64_t)track->pts_ms > stream_elapsed_ms(st) + (int64_t)lead_ms) {
            break;
        }
        if (track_is_user_cache_video(track)) {
            esp_media_track_mngr_queue_info_t qi = {0};
            if (esp_media_track_mngr_query(st->mngr, ESP_MEDIA_TRACK_TYPE_VIDEO, &qi) == ESP_OK &&
                qi.q_num >= (int)queue_cap) {
                break;
            }
        }
        if (write_pattern_frame(svc, stream, track_index, 0) != ESP_OK) {
            break;
        }
        if (queue_cap == 1) {
            break;
        }
    }
}

static void feed_stream(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream, uint16_t track_index)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (svc == NULL || st == NULL) {
        return;
    }
    if (st->use_global_cache || track_index == DUMMY_FEED_ALL_TRACKS) {
        for (uint16_t t = 0; t < st->track_num; t++) {
            feed_track(svc, stream, t);
        }
    } else if (track_index < st->track_num) {
        feed_track(svc, stream, track_index);
    }
}

static void pattern_frame_release(const esp_media_frame_t *frame, void *ctx)
{
    (void)frame;
    media_dummy_release_ctx_t *rctx = ctx;
    if (rctx == NULL || rctx->svc == NULL || !rctx->svc->running) {
        return;
    }
    feed_stream(rctx->svc, rctx->stream, rctx->track_index);
}

static esp_err_t wrap_get_track_num(void *ctx, uint16_t *out_num)
{
    media_dummy_src_stream_t *st = ctx;
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->get_track_num == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return st->inner_provider.ops->get_track_num(st->inner_provider.ctx, out_num);
}

static esp_err_t wrap_get_track_info(void *ctx, uint16_t index, esp_media_track_info_t *out_info)
{
    media_dummy_src_stream_t *st = ctx;
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->get_track_info == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return st->inner_provider.ops->get_track_info(st->inner_provider.ctx, index, out_info);
}

static esp_err_t wrap_set_event_cb(void *ctx, esp_media_provider_event_cb_t cb, void *event_ctx)
{
    media_dummy_src_stream_t *st = ctx;
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->set_event_cb == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return st->inner_provider.ops->set_event_cb(st->inner_provider.ctx, cb, event_ctx);
}

static uint16_t feed_track_target(media_dummy_src_stream_t *st, const esp_media_frame_t *frame)
{
    if (st->use_global_cache || frame == NULL) {
        return DUMMY_FEED_ALL_TRACKS;
    }
    if (frame->type != ESP_MEDIA_TRACK_TYPE_UNKNOWN) {
        uint16_t typed = st->track_num;
        uint16_t typed_num = 0;
        for (uint16_t i = 0; i < st->track_num; i++) {
            if (st->tracks[i].info.type != frame->type) {
                continue;
            }
            if (frame->track_id == st->tracks[i].info.id) {
                return i;
            }
            typed = i;
            typed_num++;
        }
        if (typed_num == 1) {
            return typed;
        }
    }
    for (uint16_t i = 0; i < st->track_num; i++) {
        if (st->tracks[i].info.id == frame->track_id) {
            return i;
        }
    }
    return DUMMY_FEED_ALL_TRACKS;
}

static void free_track_read_storage(media_dummy_track_t *track)
{
    if (track == NULL) {
        return;
    }
    free(track->read_buf);
    free(track->read_cache.data);
    track->read_buf = NULL;
    track->read_buf_size = 0;
    track->read_cache.data = NULL;
    track->read_cache.cap = 0;
    track->read_cache.len = 0;
}

static esp_err_t ensure_read_cache_cap(media_dummy_read_cache_t *cache, size_t need)
{
    if (need <= cache->cap) {
        return ESP_OK;
    }
    size_t cap = need < 256 ? 256 : need;
    uint8_t *data = realloc(cache->data, cap);
    if (data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    cache->data = data;
    cache->cap = cap;
    return ESP_OK;
}

static esp_err_t ensure_track_read_buf(media_dummy_track_t *track)
{
    if (track->max_frame_size == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (track->read_buf != NULL) {
        return ESP_OK;
    }
    track->read_buf = malloc(track->max_frame_size);
    if (track->read_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    track->read_buf_size = track->max_frame_size;
    return ESP_OK;
}

static void prep_feed(media_dummy_src_stream_t *st, uint16_t target)
{
    ensure_stream_clock(st);
    feed_stream(st->svc, st->stream, target);
}

#define DUMMY_INNER_WAIT_SLICE_MS  20

static int64_t inner_wait_deadline_us(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return 0;
    }
    return esp_timer_get_time() + (int64_t)timeout_ms * 1000;
}

static uint32_t inner_wait_slice_ms(uint32_t timeout_ms, int64_t deadline_us)
{
    if (timeout_ms == 0) {
        return 0;
    }
    if (deadline_us <= 0) {
        return DUMMY_INNER_WAIT_SLICE_MS;
    }
    int64_t remain_us = deadline_us - esp_timer_get_time();
    if (remain_us <= 0) {
        return 0;
    }
    uint32_t remain_ms = (uint32_t)((remain_us + 999) / 1000);
    return remain_ms < DUMMY_INNER_WAIT_SLICE_MS ? remain_ms : DUMMY_INNER_WAIT_SLICE_MS;
}

static esp_err_t read_inner_frame(media_dummy_src_stream_t *st, uint16_t target,
                                  esp_media_frame_t *out_frame, uint32_t timeout_ms,
                                  bool feed_after)
{
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->read_frame == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    int64_t deadline_us = inner_wait_deadline_us(timeout_ms);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    while (true) {
        prep_feed(st, target);
        ret = st->inner_provider.ops->read_frame(st->inner_provider.ctx, out_frame,
                                               inner_wait_slice_ms(timeout_ms, deadline_us));
        if (ret == ESP_OK) {
            if (feed_after && st->svc != NULL && st->svc->running) {
                feed_stream(st->svc, st->stream, target);
            }
            return ESP_OK;
        }
        if (ret != ESP_ERR_TIMEOUT) {
            return ret;
        }
        if (timeout_ms == 0 || (deadline_us > 0 && esp_timer_get_time() >= deadline_us)) {
            break;
        }
    }
    return ret;
}

static esp_err_t acquire_inner_frame(media_dummy_src_stream_t *st, uint16_t target,
                                     esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->acquire_frame == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    int64_t deadline_us = inner_wait_deadline_us(timeout_ms);
    esp_err_t ret = ESP_ERR_TIMEOUT;
    while (true) {
        prep_feed(st, target);
        ret = st->inner_provider.ops->acquire_frame(st->inner_provider.ctx, out_frame,
                                                  inner_wait_slice_ms(timeout_ms, deadline_us));
        if (ret == ESP_OK) {
            if (st->svc != NULL && st->svc->running) {
                feed_stream(st->svc, st->stream, target);
            }
            return ESP_OK;
        }
        if (ret != ESP_ERR_TIMEOUT) {
            return ret;
        }
        if (timeout_ms == 0 || (deadline_us > 0 && esp_timer_get_time() >= deadline_us)) {
            break;
        }
    }
    return ret;
}

static esp_err_t wrap_read_encoded_frame(media_dummy_src_stream_t *st, uint16_t target,
                                         media_dummy_track_t *track, esp_media_frame_t *out_frame,
                                         uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(ensure_track_read_buf(track), TAG, "read buf");

    ensure_stream_clock(st);

    size_t want = out_frame->size;
    uint32_t slot_ms = track_frame_duration_ms(track);
    if (slot_ms == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t wait_ms = timeout_ms;
    while (st->svc != NULL) {
        pace_before_deliver(st, track->delivered_frames * slot_ms, slot_ms);

        esp_media_frame_t chunk = {
            .type = track->info.type,
            .track_id = track->info.id,
            .data = track->read_buf,
            .size = track->read_buf_size,
        };
        esp_err_t ret = read_inner_frame(st, target, &chunk, wait_ms, true);
        wait_ms = 0;
        if (ret == ESP_ERR_TIMEOUT) {
            return ret;
        }
        if (ret != ESP_OK || chunk.size == 0) {
            continue;
        }
        if (chunk.size > want) {
            ESP_LOGW(TAG, "read buffer %u too small for encoded frame %u, skip",
                     (unsigned)want, (unsigned)chunk.size);
            continue;
        }

        memcpy(out_frame->data, track->read_buf, chunk.size);
        out_frame->size = chunk.size;
        out_frame->pts = chunk.pts;
        out_frame->type = track->info.type;
        out_frame->track_id = track->info.id;
        track->delivered_frames++;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

static esp_err_t wrap_read_pcm_frame(media_dummy_src_stream_t *st, uint16_t target,
                                     media_dummy_track_t *track, esp_media_frame_t *out_frame,
                                     uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(ensure_track_read_buf(track), TAG, "read buf");

    ensure_stream_clock(st);

    size_t want = out_frame->size;
    uint32_t slot_ms = track_pcm_bytes_to_ms(track, want);
    if (slot_ms == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    pace_before_deliver(st, track_pcm_bytes_to_ms(track, track->read_pcm_bytes), slot_ms);

    size_t filled = 0;
    media_dummy_read_cache_t *cache = &track->read_cache;
    uint32_t out_pts = track_pcm_bytes_to_ms(track, track->read_pcm_bytes);

    if (cache->len > 0) {
        size_t n = cache->len < want ? cache->len : want;
        memcpy(out_frame->data, cache->data, n);
        if (n < cache->len) {
            memmove(cache->data, cache->data + n, cache->len - n);
        }
        cache->len -= n;
        filled += n;
    }

    while (filled < want) {
        esp_media_frame_t chunk = {
            .type = track->info.type,
            .track_id = track->info.id,
            .data = track->read_buf,
            .size = track->read_buf_size,
        };
        /* Keep caller timeout for every inner read while aggregating to want bytes. */
        esp_err_t ret = read_inner_frame(st, target, &chunk, timeout_ms, false);
        if (ret != ESP_OK) {
            return ret;
        }
        if (chunk.size == 0) {
            return ESP_ERR_INVALID_SIZE;
        }

        size_t need = want - filled;
        if (chunk.size <= need) {
            memcpy((uint8_t *)out_frame->data + filled, track->read_buf, chunk.size);
            filled += chunk.size;
            continue;
        }

        memcpy((uint8_t *)out_frame->data + filled, track->read_buf, need);
        filled += need;
        size_t rem = chunk.size - need;
        ESP_RETURN_ON_ERROR(ensure_read_cache_cap(cache, cache->len + rem), TAG, "read cache");
        memcpy(cache->data + cache->len, track->read_buf + need, rem);
        cache->len += rem;
    }
    out_frame->size = want;
    out_frame->pts = out_pts;
    out_frame->type = track->info.type;
    out_frame->track_id = track->info.id;
    track->read_pcm_bytes += want;
    if (st->svc != NULL && st->svc->running) {
        feed_stream(st->svc, st->stream, target);
    }
    return ESP_OK;
}

static esp_err_t wrap_read_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    media_dummy_src_stream_t *st = ctx;
    if (out_frame == NULL || out_frame->data == NULL || out_frame->size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->read_frame == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t target = feed_track_target(st, out_frame);
    if (target >= st->track_num) {
        prep_feed(st, target);
        esp_err_t ret = st->inner_provider.ops->read_frame(st->inner_provider.ctx, out_frame, timeout_ms);
        if (ret == ESP_ERR_TIMEOUT) {
            prep_feed(st, target);
            ret = st->inner_provider.ops->read_frame(st->inner_provider.ctx, out_frame, 0);
        }
        if (ret == ESP_OK && st->svc != NULL && st->svc->running) {
            feed_stream(st->svc, st->stream, target);
        }
        return ret;
    }

    media_dummy_track_t *track = &st->tracks[target];
    if (track->pattern.encoded) {
        return wrap_read_encoded_frame(st, target, track, out_frame, timeout_ms);
    }
    return wrap_read_pcm_frame(st, target, track, out_frame, timeout_ms);
}

static esp_err_t wrap_acquire_frame(void *ctx, esp_media_frame_t *out_frame, uint32_t timeout_ms)
{
    media_dummy_src_stream_t *st = ctx;
    if (out_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->acquire_frame == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t target = feed_track_target(st, out_frame);
    if (target < st->track_num) {
        media_dummy_track_t *track = &st->tracks[target];
        uint32_t slot_ms = track_frame_duration_ms(track);
        if (slot_ms == 0) {
            return ESP_ERR_INVALID_STATE;
        }
        ensure_stream_clock(st);
        uint32_t deliver_pts = track->delivered_frames * slot_ms;
        pace_before_deliver(st, deliver_pts, slot_ms);
    }

    esp_err_t ret = acquire_inner_frame(st, target, out_frame, timeout_ms);
    if (ret == ESP_OK && st->svc != NULL && st->svc->running && target < st->track_num &&
        st->tracks[target].info.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        st->tracks[target].delivered_frames++;
    }
    return ret;
}

static esp_err_t wrap_release_frame(void *ctx, esp_media_frame_t *frame)
{
    media_dummy_src_stream_t *st = ctx;
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->release_frame == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret = st->inner_provider.ops->release_frame(st->inner_provider.ctx, frame);
    feed_stream(st->svc, st->stream, feed_track_target(st, frame));
    return ret;
}

static esp_err_t wrap_abort(void *ctx)
{
    media_dummy_src_stream_t *st = ctx;
    if (st->inner_provider.ops == NULL || st->inner_provider.ops->abort == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return st->inner_provider.ops->abort(st->inner_provider.ctx);
}

static const esp_media_provider_ops_t s_wrap_ops = {
    .get_track_num  = wrap_get_track_num,
    .get_track_info = wrap_get_track_info,
    .set_event_cb   = wrap_set_event_cb,
    .acquire_frame  = wrap_acquire_frame,
    .read_frame     = wrap_read_frame,
    .release_frame  = wrap_release_frame,
    .abort          = wrap_abort,
};

static esp_err_t install_provider_wrap(media_dummy_src_stream_t *st)
{
    if (st == NULL || st->mngr == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (st->provider.ops != &s_wrap_ops || st->inner_provider.ops == NULL) {
        ESP_RETURN_ON_ERROR(esp_media_track_mngr_get_provider(st->mngr, &st->inner_provider),
                            TAG, "get provider");
        st->provider.ops = &s_wrap_ops;
        st->provider.ctx = st;
    }
    return ESP_OK;
}

static esp_err_t ensure_mngr(media_dummy_src_stream_t *st)
{
    if (st == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (st->mngr != NULL) {
        return install_provider_wrap(st);
    }
    esp_media_track_mngr_cfg_t cfg = {
        .max_track_num = DUMMY_MAX_TRACKS_PER_STREAM,
    };
    esp_err_t ret = esp_media_track_mngr_create(&cfg, &st->mngr);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = install_provider_wrap(st);
    if (ret != ESP_OK) {
        esp_media_track_mngr_destroy(st->mngr);
        st->mngr = NULL;
        memset(&st->provider, 0, sizeof(st->provider));
        memset(&st->inner_provider, 0, sizeof(st->inner_provider));
    }
    return ret;
}

static void reset_track_timeline(media_dummy_track_t *track)
{
    track->frame_index = 0;
    track->pts_ms = 0;
    track->read_pcm_bytes = 0;
    track->delivered_frames = 0;
    track->read_cache.len = 0;
}

esp_err_t media_dummy_src_init(esp_media_dummy_service_t *svc)
{
    svc->src_streams = calloc(svc->max_stream_num, sizeof(*svc->src_streams));
    if (svc->src_streams == NULL) {
        return ESP_ERR_NO_MEM;
    }
    for (uint16_t i = 0; i < svc->max_stream_num; i++) {
        svc->src_streams[i].svc = svc;
        svc->src_streams[i].stream = i;
        esp_err_t ret = ensure_mngr(&svc->src_streams[i]);
        if (ret != ESP_OK) {
            for (uint16_t j = 0; j < i; j++) {
                if (svc->src_streams[j].mngr != NULL) {
                    esp_media_track_mngr_destroy(svc->src_streams[j].mngr);
                    svc->src_streams[j].mngr = NULL;
                }
            }
            free(svc->src_streams);
            svc->src_streams = NULL;
            return ret;
        }
    }
    return ESP_OK;
}

void media_dummy_src_deinit(esp_media_dummy_service_t *svc)
{
    if (svc == NULL || svc->src_streams == NULL) {
        return;
    }
    for (uint16_t i = 0; i < svc->max_stream_num; i++) {
        (void)media_dummy_src_reset_tracks(svc, i);
    }
    free(svc->src_streams);
    svc->src_streams = NULL;
}

esp_err_t media_dummy_src_add_track(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                    const esp_media_track_info_t *info)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (svc->running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (st->track_num >= DUMMY_MAX_TRACKS_PER_STREAM) {
        return ESP_ERR_NO_MEM;
    }

    media_dummy_pattern_t pattern = {0};
    ESP_RETURN_ON_ERROR(media_dummy_pattern_build(info, &pattern), TAG, "build pattern");
    esp_err_t ret = ensure_mngr(st);
    if (ret != ESP_OK) {
        media_dummy_pattern_release(&pattern);
        return ret;
    }

    media_dummy_track_t *track = &st->tracks[st->track_num];
    memset(track, 0, sizeof(*track));
    track->info = pattern.track_info;
    track->info.id = st->track_num;
    track->pattern = pattern;
    track->max_frame_size = media_dummy_pattern_max_frame_size(&track->pattern);
    track->active = true;

    bool use_user = track_use_user_cache(&track->pattern);
    esp_media_track_mngr_track_cfg_t track_cfg = {
        .info = track->info,
    };
    if (use_user) {
        media_dummy_release_ctx_t *rctx = calloc(1, sizeof(*rctx));
        if (rctx == NULL) {
            media_dummy_pattern_release(&track->pattern);
            track->active = false;
            return ESP_ERR_NO_MEM;
        }
        rctx->svc = svc;
        rctx->stream = stream;
        rctx->track_index = st->track_num;
        track->release_ctx = rctx;
        track_cfg.cache_cfg = (esp_media_track_mngr_cache_cfg_t) {
            .cache_type = ESP_MEDIA_TRACK_CACHE_USER,
            .user_queue = {
                .queue_num = DUMMY_VIDEO_QUEUE_DEPTH,
                .frame_release = pattern_frame_release,
                .release_ctx = rctx,
            },
        };
    } else {
        size_t cache_size = (track->max_frame_size + 128 + DUMMY_FRAME_ALIGN) * DUMMY_SEED_QUEUE_DEPTH;
        if (cache_size < 16 * 1024) {
            cache_size = 16 * 1024;
        }
        track_cfg.cache_cfg = (esp_media_track_mngr_cache_cfg_t) {
            .cache_type = ESP_MEDIA_TRACK_CACHE_INTERNAL,
            .track_cache = {
                .cache_size = cache_size,
                .addr_align = DUMMY_FRAME_ALIGN,
                .size_align = 0,
            },
        };
    }

    ret = esp_media_track_mngr_add_track(st->mngr, &track_cfg);
    if (ret != ESP_OK) {
        free(track->release_ctx);
        track->release_ctx = NULL;
        media_dummy_pattern_release(&track->pattern);
        track->active = false;
        return ret;
    }
    st->track_num++;
    return install_provider_wrap(st);
}

esp_err_t media_dummy_src_reset_tracks(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (svc->running) {
        return ESP_ERR_INVALID_STATE;
    }
    if (st->mngr != NULL) {
        esp_media_track_mngr_destroy(st->mngr);
        st->mngr = NULL;
    }
    for (uint16_t i = 0; i < st->track_num; i++) {
        free_track_read_storage(&st->tracks[i]);
        free(st->tracks[i].release_ctx);
        st->tracks[i].release_ctx = NULL;
        media_dummy_pattern_release(&st->tracks[i].pattern);
        memset(&st->tracks[i], 0, sizeof(st->tracks[i]));
    }
    st->track_num = 0;
    memset(&st->provider, 0, sizeof(st->provider));
    memset(&st->inner_provider, 0, sizeof(st->inner_provider));
    return ESP_OK;
}

esp_err_t media_dummy_src_on_start(esp_media_dummy_service_t *svc)
{
    for (uint16_t s = 0; s < svc->max_stream_num; s++) {
        media_dummy_src_stream_t *st = &svc->src_streams[s];
        if (st->mngr == NULL) {
            continue;
        }
        ESP_RETURN_ON_ERROR(esp_media_track_clear_abort(st->mngr), TAG, "clear abort");
        track_mngr_reset_data_queue(st->mngr);
        st->start_time_us = 0;
        for (uint16_t t = 0; t < st->track_num; t++) {
            reset_track_timeline(&st->tracks[t]);
        }
        ESP_RETURN_ON_ERROR(install_provider_wrap(st), TAG, "wrap provider");
    }
    return ESP_OK;
}

esp_err_t media_dummy_src_sync_record(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream)
{
    if (svc == NULL || !svc->running) {
        return ESP_ERR_INVALID_STATE;
    }
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || st->mngr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    track_mngr_reset_data_queue(st->mngr);
    st->start_time_us = 0;
    for (uint16_t t = 0; t < st->track_num; t++) {
        reset_track_timeline(&st->tracks[t]);
    }
    return ESP_OK;
}

esp_err_t media_dummy_src_on_stop(esp_media_dummy_service_t *svc)
{
    for (uint16_t s = 0; s < svc->max_stream_num; s++) {
        media_dummy_src_stream_t *st = &svc->src_streams[s];
        if (st->mngr != NULL) {
            (void)esp_media_track_write_abort(st->mngr);
        }
        for (uint16_t t = 0; t < st->track_num; t++) {
            reset_track_timeline(&st->tracks[t]);
        }
        st->start_time_us = 0;
    }
    return ESP_OK;
}

esp_err_t media_dummy_src_get_provider(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                       esp_media_provider_t *out)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(ensure_mngr(st), TAG, "ensure mngr");
    *out = st->provider;
    return ESP_OK;
}

esp_err_t media_dummy_src_set_request(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                      const esp_media_service_request_t *request)
{
    media_dummy_src_stream_t *st = src_stream(svc, stream);
    if (st == NULL || request == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (st->mngr == NULL) {
        return ESP_OK;
    }
    if (request->need_global_cache) {
        size_t cache = 0;
        for (uint16_t i = 0; i < st->track_num; i++) {
            if (st->tracks[i].max_frame_size > cache) {
                cache = st->tracks[i].max_frame_size;
            }
        }
        cache *= DUMMY_SEED_QUEUE_DEPTH;
        if (cache == 0) {
            cache = DUMMY_SRC_DEFAULT_CACHE;
        }
        esp_err_t ret = esp_media_track_mngr_set_global_cache(st->mngr, true, cache);
        if (ret == ESP_ERR_INVALID_STATE) {
            return ESP_ERR_NOT_SUPPORTED;
        }
        st->use_global_cache = true;
        return ret;
    }
    return ESP_OK;
}
