/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_extractor_service_err.h"
#include "esp_extractor_service_ops.h"
#include "esp_extractor_service_priv.h"
#include "esp_extractor_scheduler.h"
#include "esp_fourcc.h"
#include "esp_gmf_obj.h"
#include "esp_gmf_payload.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT
#include "esp_gmf_io_file.h"
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT */

#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT
#include "esp_gmf_io_http.h"
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT */

static const char *TAG = "EXTRACT_SVC";

#define IO_TIMEOUT_MS      5000
#define WRITE_TIMEOUT_MS   100
#define EXIT_TIMEOUT_MS    3000
#define MAP_CODEC(format)  ((esp_media_codec_fourcc_t)format)

#define WAIT_FOR_COND(run_cond, timeout_ms, warn_msg)  do {  \
    int wait_count = timeout_ms / 10;                        \
    while (run_cond) {                                       \
        media_lib_thread_sleep(10);                          \
        if (wait_count > 0) {                                \
            wait_count--;                                    \
            if (wait_count == 0) {                           \
                ESP_LOGW(TAG, warn_msg);                     \
                break;                                       \
            }                                                \
        }                                                    \
    }                                                        \
} while (0)

esp_err_t extractor_err_to_esp(esp_extractor_err_t err)
{
    switch (err) {
        case ESP_EXTRACTOR_ERR_OK:
            return ESP_OK;
        case ESP_EXTRACTOR_ERR_INV_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_EXTRACTOR_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ESP_EXTRACTOR_ERR_NOT_FOUND:
            return ESP_ERR_NOT_FOUND;
        case ESP_EXTRACTOR_ERR_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_EXTRACTOR_ERR_EOS:
            return ESP_ERR_NOT_FINISHED;
        default:
            return ESP_FAIL;
    }
}

void extractor_get_media_thread_cfg(const esp_extractor_service_t *service,
                                    media_lib_thread_cfg_t *out_cfg)
{
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = 8192,
        .priority = 10,
        .core_id = 0,
    };
    esp_service_thread_cfg_t cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name ? service->media.base.name : ESP_EXTRACTOR_SERVICE_NAME,
        .thread_name = ESP_EXTRACTOR_SCHED_SRC_TASK,
    };
    esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &cfg);
    out_cfg->stack_size = cfg.stack_size;
    out_cfg->priority = (uint8_t)cfg.priority;
    out_cfg->core_id = (uint8_t)(cfg.core_id < 0 ? 0 : cfg.core_id);
}

static bool valid_mask(uint8_t mask)
{
    return mask == ESP_EXTRACT_MASK_AUDIO || mask == ESP_EXTRACT_MASK_VIDEO || mask == ESP_EXTRACT_MASK_AV;
}

static esp_err_t ensure_initialized(esp_extractor_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_INITIALIZED,
                        ESP_ERR_INVALID_STATE, TAG, "bad state");
    return ESP_OK;
}

static const char *url_path_end(const char *url)
{
    const char *end = url + strlen(url);
    const char *q = strchr(url, '?');
    const char *h = strchr(url, '#');
    if (q != NULL && q < end) {
        end = q;
    }
    if (h != NULL && h < end) {
        end = h;
    }
    return end;
}

static bool url_ext_eq(const char *ext, const char *path_end, const char *expect)
{
    size_t n = strlen(expect);
    return (size_t)(path_end - ext) == n && strncasecmp(ext, expect, n) == 0;
}

static esp_extractor_type_t type_from_url(const char *url)
{
    if (url == NULL) {
        return ESP_EXTRACTOR_TYPE_NONE;
    }
    /* Strip ?query / #fragment so ".m3u8?format=aac" still matches m3u8. */
    const char *path_end = url_path_end(url);
    const char *dot = NULL;
    for (const char *p = url; p < path_end; p++) {
        if (*p == '.') {
            dot = p;
        }
    }
    if (dot == NULL || dot + 1 >= path_end) {
        return ESP_EXTRACTOR_TYPE_NONE;
    }
    const char *ext = dot + 1;
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    if (url_ext_eq(ext, path_end, "m3u8") || url_ext_eq(ext, path_end, "hls")) {
        return ESP_EXTRACTOR_TYPE_HLS;
    }
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
    if (url_ext_eq(ext, path_end, "wav")) {
        return ESP_EXTRACTOR_TYPE_WAV;
    }
    if (url_ext_eq(ext, path_end, "aac")) {
        return ESP_EXTRACTOR_TYPE_AAC;
    }
    if (url_ext_eq(ext, path_end, "mp3")) {
        return ESP_EXTRACTOR_TYPE_MP3;
    }
    if (url_ext_eq(ext, path_end, "mp4") || url_ext_eq(ext, path_end, "mov")) {
        return ESP_EXTRACTOR_TYPE_MP4;
    }
    if (url_ext_eq(ext, path_end, "ts")) {
        return ESP_EXTRACTOR_TYPE_TS;
    }
    if (url_ext_eq(ext, path_end, "ogg")) {
        return ESP_EXTRACTOR_TYPE_OGG;
    }
    if (url_ext_eq(ext, path_end, "avi")) {
        return ESP_EXTRACTOR_TYPE_AVI;
    }
    if (url_ext_eq(ext, path_end, "flv")) {
        return ESP_EXTRACTOR_TYPE_FLV;
    }
    if (url_ext_eq(ext, path_end, "caf")) {
        return ESP_EXTRACTOR_TYPE_CAF;
    }
    if (url_ext_eq(ext, path_end, "flac")) {
        return ESP_EXTRACTOR_TYPE_FLAC;
    }
    if (url_ext_eq(ext, path_end, "amr")) {
        return ESP_EXTRACTOR_TYPE_AMRNB;
    }
    return ESP_EXTRACTOR_TYPE_NONE;
}

static int io_read(void *buffer, uint32_t size, void *ctx)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)ctx;
    if (service->task_stop) {
        return -1;
    }
    if (service->io) {
        uint8_t *dst = (uint8_t *)buffer;
        uint32_t filled = 0;
        while (filled < size && !service->task_stop) {
            esp_gmf_payload_t payload = {
                .buf = dst + filled,
                .buf_length = size - filled,
            };
            esp_gmf_err_io_t ret = esp_gmf_io_acquire_read(service->io, &payload, size - filled,
                                                           IO_TIMEOUT_MS);
            if (ret != ESP_GMF_IO_OK) {
                return filled > 0 ? (int)filled : -1;
            }
            if (payload.buf != dst + filled && payload.valid_size > 0) {
                memcpy(dst + filled, payload.buf, payload.valid_size);
            }
            filled += payload.valid_size;
            bool done = payload.is_done;
            esp_gmf_io_release_read(service->io, &payload, 0);
            if (done || payload.valid_size == 0) {
                break;
            }
        }
        return (int)filled;
    }
    uint32_t remain = service->src_size - service->src_pos;
    uint32_t read_size = size > remain ? remain : size;
    if (read_size > 0) {
        memcpy(buffer, service->src_data + service->src_pos, read_size);
        service->src_pos += read_size;
    }
    return (int)read_size;
}

static int io_seek(uint32_t position, void *ctx)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)ctx;
    if (service->task_stop) {
        return -1;
    }
    if (service->io != NULL) {
        return esp_gmf_io_seek(service->io, position) == ESP_GMF_ERR_OK ? 0 : -1;
    }
    if (position > service->src_size) {
        return -1;
    }
    service->src_pos = position;
    return 0;
}

static uint32_t src_size(void *ctx)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)ctx;
    if (service->io != NULL) {
        uint64_t total_size = 0;
        esp_gmf_io_get_size(service->io, &total_size);
        return (uint32_t)total_size;
    }
    return service->src_size;
}

static bool is_hls_url(const char *url)
{
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    return type_from_url(url) == ESP_EXTRACTOR_TYPE_HLS;
#else
    (void)url;
    return false;
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
}

static void close_url_io(esp_extractor_service_t *service)
{
    if (service->io != NULL) {
        esp_gmf_io_close(service->io);
        esp_gmf_obj_delete(service->io);
        service->io = NULL;
    }
}

static esp_err_t open_url_io(esp_extractor_service_t *service, const char *url)
{
    ESP_RETURN_ON_FALSE(service->pool != NULL, ESP_ERR_NOT_SUPPORTED, TAG, "no GMF IO pool");
    char *io_tag = NULL;
    esp_gmf_io_handle_t io = NULL;
    ESP_RETURN_ON_FALSE(esp_gmf_pool_get_io_tag_by_url(service->pool, url, ESP_GMF_IO_DIR_READER, &io_tag) == ESP_GMF_ERR_OK &&
                            io_tag != NULL,
                        ESP_ERR_NOT_SUPPORTED, TAG, "unsupported url");
    ESP_RETURN_ON_FALSE(esp_gmf_pool_new_io(service->pool, io_tag, ESP_GMF_IO_DIR_READER, &io) == ESP_GMF_ERR_OK &&
                            io != NULL,
                        ESP_ERR_NO_MEM, TAG, "new io failed");
    service->io = io;
    if (esp_gmf_io_set_uri(io, url) != ESP_GMF_ERR_OK ||
        esp_gmf_io_open(io) != ESP_GMF_ERR_OK) {
        close_url_io(service);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void release_internal_io(esp_extractor_service_t *service)
{
    if (service->own_pool && service->pool != NULL) {
        esp_gmf_pool_deinit(service->pool);
    }
    service->pool = NULL;
    service->own_pool = false;
}

static esp_err_t register_internal_io(esp_extractor_service_t *service)
{
    if (service->pool != NULL) {
        return ESP_OK;
    }

#if defined(CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT) || defined(CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT)
    ESP_RETURN_ON_FALSE(esp_gmf_pool_init(&service->pool) == ESP_GMF_ERR_OK,
                        ESP_ERR_NO_MEM, TAG, "pool init failed");
    service->own_pool = true;

    esp_gmf_io_handle_t io_handle = NULL;
    do {
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT
        io_handle = NULL;
        file_io_cfg_t file_cfg = FILE_IO_CFG_DEFAULT();
        file_cfg.dir = ESP_GMF_IO_DIR_READER;
        if (esp_gmf_io_file_init(&file_cfg, &io_handle) != ESP_GMF_ERR_OK) {
            break;
        }
        if (esp_gmf_pool_register_io(service->pool, io_handle, NULL) != ESP_GMF_ERR_OK) {
            break;
        }
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT */

#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT
        io_handle = NULL;
        http_io_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
        http_cfg.dir = ESP_GMF_IO_DIR_READER;
        if (esp_gmf_io_http_init(&http_cfg, &io_handle) != ESP_GMF_ERR_OK) {
            break;
        }
        if (esp_gmf_pool_register_io(service->pool, io_handle, NULL) != ESP_GMF_ERR_OK) {
            break;
        }
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT */
        return ESP_OK;
    } while (0);
    if (io_handle) {
        esp_gmf_obj_delete(io_handle);
    }
    release_internal_io(service);
    RET_FOR(ESP_ERR_NO_MEM, "register io failed");
#endif  /* defined(CONFIG_ESP_EXTRACTOR_SERVICE_FILE_IO_SUPPORT) || defined(CONFIG_ESP_EXTRACTOR_SERVICE_HTTP_IO_SUPPORT) */
    return ESP_OK;
}

static uint16_t track_id(esp_extractor_stream_type_t type, uint16_t idx)
{
    return (type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO) ?
                                                     (EXTRACTOR_AUDIO_TRACK_ID + idx)
                                                     : (EXTRACTOR_VIDEO_TRACK_ID + idx);
}

static void extractor_frame_release(const esp_media_frame_t *frame, void *ctx)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)ctx;
    if (service == NULL || frame == NULL) {
        return;
    }
    if (frame->data != NULL && service->extractor != NULL) {
        esp_extractor_frame_info_t info = {
            .frame_buffer = frame->data,
            .frame_size = (uint32_t)frame->size,
        };
        esp_extractor_release_frame(service->extractor, &info);
    }
}

static void track_cache_reset(esp_extractor_service_t *service)
{
    memset(service->tracks, 0, sizeof(service->tracks));
    service->track_count = 0;
}

static extractor_track_slot_t *track_cache_find(esp_extractor_service_t *service, uint16_t id)
{
    for (uint8_t i = 0; i < service->track_count; i++) {
        if (service->tracks[i].id == id) {
            return &service->tracks[i];
        }
    }
    return NULL;
}

static void track_cache_upsert(esp_extractor_service_t *service, uint16_t id, uint16_t index,
                               esp_media_track_type_t type, bool ready)
{
    extractor_track_slot_t *slot = track_cache_find(service, id);
    if (slot == NULL) {
        if (service->track_count >= EXTRACTOR_MAX_TRACKS) {
            return;
        }
        slot = &service->tracks[service->track_count++];
        slot->id = id;
        slot->index = index;
    }
    slot->type = type;
    slot->ready = ready;
}

static bool track_info_ready(const esp_media_track_info_t *info)
{
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return info->info.audio.codec != 0;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return info->info.video.codec != 0;
    }
    return false;
}

static esp_err_t add_or_update_track(esp_extractor_service_t *service,
                                     const esp_extractor_stream_info_t *stream)
{
    esp_media_track_info_t info = {
        .id = track_id(stream->stream_type, stream->stream_id),
    };
    if (stream->stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO) {
        info.type = ESP_MEDIA_TRACK_TYPE_AUDIO;
        info.info.audio.codec = MAP_CODEC(stream->audio_info.format);
        info.info.audio.sample_rate = stream->audio_info.sample_rate;
        info.info.audio.bits_per_sample = stream->audio_info.bits_per_sample;
        info.info.audio.channel = stream->audio_info.channel;
        info.info.audio.bitrate = stream->bitrate;
        ESP_LOGI(TAG, "audio info: %x sample_rate: %d bits_per_sample: %d channel: %d bitrate: %d",
                 (int)info.info.audio.codec, info.info.audio.sample_rate,
                 info.info.audio.bits_per_sample, info.info.audio.channel, info.info.audio.bitrate);
    } else if (stream->stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO) {
        info.type = ESP_MEDIA_TRACK_TYPE_VIDEO;
        info.info.video.codec = MAP_CODEC(stream->video_info.format);
        info.info.video.width = stream->video_info.width;
        info.info.video.height = stream->video_info.height;
        info.info.video.fps = stream->video_info.fps;
        info.info.video.bitrate = stream->bitrate;
        ESP_LOGI(TAG, "video info: %x width: %d height: %d fps: %d bitrate: %d",
                 (int)info.info.video.codec, info.info.video.width, info.info.video.height,
                 info.info.video.fps, info.info.video.bitrate);
    } else {
        return ESP_ERR_INVALID_ARG;
    }

    const bool ready = track_info_ready(&info);
    extractor_track_slot_t *slot = track_cache_find(service, info.id);
    if (slot != NULL) {
        esp_err_t ret = esp_media_track_mngr_update_track(service->mngr, slot->index, &info);
        if (ret == ESP_OK) {
            slot->ready = ready;
            slot->type = info.type;
        }
        return ret;
    }

    esp_media_track_mngr_track_cfg_t cfg = {
        .info = info,
        .cache_cfg = {
            .cache_type = ESP_MEDIA_TRACK_CACHE_USER,
            .user_queue = {
                .queue_num = EXTRACTOR_USER_QUEUE_DEPTH,
                .frame_release = extractor_frame_release,
                .release_ctx = service,
            },
        },
    };
    uint16_t index = service->track_count;
    ESP_RETURN_ON_ERROR(esp_media_track_mngr_add_track(service->mngr, &cfg), TAG, "add track failed");
    track_cache_upsert(service, info.id, index, info.type, ready);
    return ESP_OK;
}

static esp_err_t publish_tracks(esp_extractor_service_t *service, esp_extractor_handle_t extractor,
                                esp_extractor_stream_type_t type)
{
    uint16_t stream_num = 0;
    esp_extractor_err_t ret = esp_extractor_get_stream_num(extractor, type, &stream_num);
    if (ret == ESP_EXTRACTOR_ERR_NOT_FOUND) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "stream %d num: %d", type, stream_num);
    ESP_RETURN_ON_FALSE(ret == ESP_EXTRACTOR_ERR_OK, extractor_err_to_esp(ret), TAG, "stream num failed");
    for (uint16_t i = 0; i < stream_num && i < EXTRACTOR_MAX_TRACKS; i++) {
        esp_extractor_stream_info_t stream = {0};
        ret = esp_extractor_get_stream_info(extractor, type, i, &stream);
        if (ret == ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGI(TAG, "stream %d info: %d", type, stream.stream_id);
            ESP_RETURN_ON_ERROR(add_or_update_track(service, &stream), TAG, "add track failed");
        }
    }
    return ESP_OK;
}

static esp_err_t publish_all_tracks(esp_extractor_service_t *service, esp_extractor_handle_t extractor)
{
    esp_err_t ret = ESP_OK;
    if (service->extract_mask & ESP_EXTRACT_MASK_AUDIO) {
        ret = publish_tracks(service, extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO);
    }
    if (ret == ESP_OK && (service->extract_mask & ESP_EXTRACT_MASK_VIDEO)) {
        ret = publish_tracks(service, extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO);
    }
    return ret;
}

static esp_err_t ensure_track_for_frame(esp_extractor_service_t *service,
                                        esp_extractor_handle_t extractor,
                                        const esp_extractor_frame_info_t *frame)
{
    uint16_t id = track_id(frame->stream_type, frame->stream_idx);
    extractor_track_slot_t *slot = track_cache_find(service, id);
    if (slot != NULL && slot->ready) {
        return ESP_OK;
    }
    esp_extractor_stream_info_t stream = {0};
    esp_extractor_err_t ret = esp_extractor_get_stream_info(extractor, frame->stream_type,
                                                            frame->stream_idx, &stream);
    if (ret != ESP_EXTRACTOR_ERR_OK) {
        /* Keep serving frames if a stub track was already published. */
        if (slot != NULL) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "stream info not ready for type:%d idx:%d (%d)",
                 (int)frame->stream_type, (int)frame->stream_idx, (int)ret);
        return ESP_ERR_NOT_FOUND;
    }
    stream.stream_id = frame->stream_idx;
    return add_or_update_track(service, &stream);
}

static esp_err_t write_frame(esp_extractor_service_t *service, const esp_extractor_frame_info_t *in)
{
    if (in->frame_buffer == NULL || in->frame_size == 0) {
        return ESP_FAIL;
    }
    const bool is_eos = EXTRACTOR_IS_EOS(in->frame_flag);
    esp_media_frame_t frame = {
        .track_id = track_id(in->stream_type, in->stream_idx),
        .type = in->stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO ? ESP_MEDIA_TRACK_TYPE_AUDIO : ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = in->frame_buffer,
        .size = in->frame_size,
        .pts = in->pts,
        .dts = in->pts,
        .flags = is_eos ? ESP_MEDIA_FRAME_FLAG_EOS : 0,
    };
    esp_err_t ret = esp_media_track_write_frame(service->mngr, &frame, WRITE_TIMEOUT_MS);
    if (ret == ESP_OK && is_eos) {
        extractor_track_slot_t *slot = track_cache_find(service, frame.track_id);
        if (slot != NULL) {
            slot->eos = true;
        }
        return ESP_OK;
    }
    return ret;
}

static void track_cache_clear_eos(esp_extractor_service_t *service)
{
    for (uint8_t i = 0; i < service->track_count; i++) {
        service->tracks[i].eos = false;
    }
}

static void extractor_cmd_queue_reset(esp_extractor_service_t *service)
{
    extractor_cmd_msg_t msg;
    if (service->cmd_queue == NULL) {
        return;
    }
    while (xQueueReceive(service->cmd_queue, &msg, 0) == pdTRUE) {
    }
}

static esp_err_t extractor_cmd_send(esp_extractor_service_t *service, extractor_cmd_type_t type,
                                    uint32_t position_ms, bool to_front)
{
    if (service->cmd_queue == NULL) {
        return ESP_FAIL;
    }
    extractor_cmd_msg_t msg = {
        .type = type,
        .position_ms = position_ms,
    };
    BaseType_t ok = to_front ? xQueueSendToFront(service->cmd_queue, &msg, 0)
                             : xQueueSend(service->cmd_queue, &msg, 0);
    return ok == pdTRUE ? ESP_OK : ESP_FAIL;
}

static esp_err_t extractor_publish_seek_event(esp_extractor_service_t *service,
                                              esp_extractor_service_event_t event_id,
                                              uint32_t position_ms, esp_err_t err)
{
    esp_extractor_service_event_payload_t payload = {
        .position_ms = position_ms,
        .loop_count = service->loop_count,
        .err = err,
    };
    esp_err_t ret = esp_service_publish_event(ESP_SERVICE_BASE(service), (uint16_t)event_id,
                                              &payload, sizeof(payload), NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "publish seek event %u failed: %s", (unsigned)event_id, esp_err_to_name(ret));
    }
    return ret;
}

static void extractor_abort_read(esp_extractor_service_t *service)
{
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    if (service->extractor != NULL) {
        (void)esp_extractor_ctrl(service->extractor,
                                 (esp_extractor_ctrl_type_t)ESP_EXTRACTOR_CTRL_TYPE_SET_HLS_READ_ABORT,
                                 NULL, 0);
    }
#else
    (void)service;
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
}

static void extractor_handle_seek(esp_extractor_service_t *service, uint32_t position_ms, bool clear_pending)
{
    track_cache_clear_eos(service);
    if (clear_pending && service->mngr != NULL) {
        /* API seek: drain queued USER frames so the consumer does not keep pre-seek data. */
        (void)esp_media_track_write_abort(service->mngr);
    }
    esp_extractor_err_t seek_ret = ESP_EXTRACTOR_ERR_INV_ARG;
    if (service->extractor != NULL && !service->task_stop) {
        seek_ret = esp_extractor_seek(service->extractor, position_ms);
    }
    if (clear_pending) {
        (void)esp_media_track_clear_abort(service->mngr);
    }
    esp_err_t err = extractor_err_to_esp(seek_ret);
    if (seek_ret == ESP_EXTRACTOR_ERR_OK) {
        ESP_LOGI(TAG, "seek done pos=%u", (unsigned)position_ms);
        (void)extractor_publish_seek_event(service, ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE, position_ms, ESP_OK);
    } else {
        ESP_LOGW(TAG, "seek failed pos=%u ret=%d", (unsigned)position_ms, (int)seek_ret);
        (void)extractor_publish_seek_event(service, ESP_EXTRACTOR_SERVICE_EVENT_SEEK_ERROR, position_ms, err);
    }
}

static bool extractor_process_cmds(esp_extractor_service_t *service)
{
    if (service->cmd_queue == NULL) {
        return service->task_stop;
    }
    extractor_cmd_msg_t msg;
    while (xQueueReceive(service->cmd_queue, &msg, 0) == pdTRUE) {
        if (msg.type == EXTRACTOR_CMD_STOP || service->task_stop) {
            service->task_stop = true;
            return true;
        }
        if (msg.type == EXTRACTOR_CMD_SEEK) {
            extractor_handle_seek(service, msg.position_ms, true);
        }
    }
    return service->task_stop;
}

static void write_eos_all_tracks(esp_extractor_service_t *service)
{
    for (uint8_t i = 0; i < service->track_count; i++) {
        extractor_track_slot_t *slot = &service->tracks[i];
        if (!slot->ready || slot->eos) {
            continue;
        }
        esp_media_frame_t frame = {
            .track_id = slot->id,
            .type = slot->type,
            .data = NULL,
            .size = 0,
            .pts = 0,
            .dts = 0,
            .flags = ESP_MEDIA_FRAME_FLAG_EOS,
        };
        esp_err_t ret = ESP_ERR_TIMEOUT;
        while (ret == ESP_ERR_TIMEOUT && !service->task_stop) {
            media_lib_thread_sleep(10);
            ret = esp_media_track_write_frame(service->mngr, &frame, WRITE_TIMEOUT_MS);
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "eos write failed id=%u %s", slot->id, esp_err_to_name(ret));
        } else {
            slot->eos = true;
            ESP_LOGI(TAG, "eos written id=%u type=%d", slot->id, (int)slot->type);
        }
    }
}

static void extractor_task(void *arg)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)arg;

    service->task_running = true;
    service->extractor = NULL;
    bool clean_eos = false;

    if (service->url != NULL) {
        if (is_hls_url(service->url)) {
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
            service->hls_cfg = esp_hls_extractor_io_cfg_init(service->url, service->pool, service->extract_mask,
                                                             service->out_pool_size, EXTRACTOR_DEFAULT_OUTPUT_ALIGN);
            if (service->hls_cfg == NULL ||
                esp_hls_extractor_open_with_cfg(service->hls_cfg, &service->extractor) != ESP_EXTRACTOR_ERR_OK) {
                ESP_LOGE(TAG, "HLS extractor open failed");
                goto exit;
            }
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
        } else {
            if (open_url_io(service, service->url) != ESP_OK) {
                ESP_LOGE(TAG, "open %s failed", service->url);
                goto exit;
            }
        }
    }

    if (service->extractor == NULL) {
        esp_extractor_config_t cfg = {
            .type = service->url ? type_from_url(service->url) : ESP_EXTRACTOR_TYPE_NONE,
            .extract_mask = service->extract_mask,
            .in_read_cb = io_read,
            .in_seek_cb = io_seek,
            .in_size_cb = src_size,
            .in_ctx = service,
            .out_pool_size = service->out_pool_size,
            .out_align = EXTRACTOR_DEFAULT_OUTPUT_ALIGN,
        };
        if (esp_extractor_open(&cfg, &service->extractor) != ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGE(TAG, "extractor open failed");
            goto exit;
        }
    }

    if (!service->task_stop && esp_extractor_parse_stream(service->extractor) == ESP_EXTRACTOR_ERR_OK &&
        publish_all_tracks(service, service->extractor) == ESP_OK) {
        while (!service->task_stop) {
            if (extractor_process_cmds(service)) {
                break;
            }
            esp_extractor_frame_info_t frame = {0};
            esp_extractor_err_t ret = esp_extractor_read_frame(service->extractor, &frame);
            if (ret == ESP_EXTRACTOR_ERR_OK) {
                if (service->task_stop) {
                    esp_extractor_release_frame(service->extractor, &frame);
                    break;
                }
                /* HLS: A/V info may only appear after first media segment/frame. */
                if (ensure_track_for_frame(service, service->extractor, &frame) != ESP_OK) {
                    esp_extractor_release_frame(service->extractor, &frame);
                    continue;
                }
                esp_err_t wr = ESP_ERR_TIMEOUT;
                while (!service->task_stop) {
                    wr = write_frame(service, &frame);
                    if (wr != ESP_ERR_TIMEOUT) {
                        break;
                    }
                    /* Full cache: wait. Mid-seek write_abort fails the write; release then run cmds. */
                    media_lib_thread_sleep(10);
                }
                /* wr==OK transferred ownership to the track queue; abort/consumer free it.
                 * Releasing here races write_abort and double-frees the extractor pool. */
                if (wr != ESP_OK) {
                    esp_extractor_release_frame(service->extractor, &frame);
                    if (service->task_stop || extractor_process_cmds(service)) {
                        break;
                    }
                    continue;
                }
                if (service->task_stop) {
                    break;
                }
            } else if (ret == ESP_EXTRACTOR_ERR_STREAM_CHANGED) {
                publish_all_tracks(service, service->extractor);
            } else if (ret == ESP_EXTRACTOR_ERR_WAITING_OUTPUT) {
                media_lib_thread_sleep(10);
            } else if (ret == ESP_EXTRACTOR_ERR_EOS) {
                service->loop_count++;
                ESP_LOGI(TAG, "EOS play %u auto_loop=%d", (unsigned)service->loop_count,
                         (int)service->auto_loop);
                (void)extractor_publish_seek_event(service, ESP_EXTRACTOR_SERVICE_EVENT_EOS, 0, ESP_OK);
                if (service->auto_loop && !service->task_stop) {
                    ESP_LOGI(TAG, "auto_loop seek 0");
                    extractor_handle_seek(service, 0, false);
                    continue;
                }
                /* Overall extractor EOS: signal every published track, not only the last frame's. */
                write_eos_all_tracks(service);
                clean_eos = true;
                ESP_LOGI(TAG, "read_frame EOS, eos written to all tracks");
                break;
            } else if (ret == ESP_EXTRACTOR_ERR_ABORTED) {
                /* Abort unblocks read for seek/stop; drain cmds then continue. */
                if (extractor_process_cmds(service) || service->task_stop) {
                    break;
                }
                continue;
            } else if (ret != ESP_EXTRACTOR_ERR_SKIPPED) {
                ESP_LOGW(TAG, "read_frame exit ret=%d stop=%d", (int)ret, (int)service->task_stop);
                break;
            }
        }
    }
exit:
    /* Stop already write_abort's; a second drain races acquire_read on the same head. */
    if (!clean_eos && !service->task_stop) {
        esp_media_track_write_abort(service->mngr);
    }
    service->task_running = false;
    media_lib_thread_destroy(NULL);
}

static esp_err_t extractor_ensure_mngr(esp_extractor_service_t *service)
{
    if (service->mngr != NULL) {
        return ESP_OK;
    }
    esp_media_track_mngr_cfg_t cfg = {
        .max_track_num = EXTRACTOR_MAX_TRACKS,
        .use_global_cache = false,
    };
    ESP_RETURN_ON_ERROR(esp_media_track_mngr_create(&cfg, &service->mngr), TAG, "mngr create failed");
    return esp_media_track_mngr_get_provider(service->mngr, &service->provider);
}

static esp_err_t extractor_on_start(esp_service_t *base)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)base;
    ESP_RETURN_ON_FALSE(service->url != NULL || service->src_data != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "no source");
    ESP_RETURN_ON_FALSE(service->cmd_queue != NULL, ESP_ERR_INVALID_STATE, TAG, "no cmd queue");
    ESP_RETURN_ON_ERROR(extractor_ensure_mngr(service), TAG, "no mngr");
    ESP_RETURN_ON_ERROR(esp_media_track_mngr_reset(service->mngr), TAG, "mngr reset failed");
    esp_media_track_clear_abort(service->mngr);
    track_cache_reset(service);
    extractor_cmd_queue_reset(service);
    service->loop_count = 0;
    service->src_pos = 0;

    media_lib_thread_cfg_t cfg = {0};
    extractor_get_media_thread_cfg(service, &cfg);
    service->task_stop = false;
    if (media_lib_thread_create(&service->task, ESP_EXTRACTOR_SCHED_SRC_TASK, extractor_task, service,
                                cfg.stack_size, cfg.priority, cfg.core_id) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t extractor_on_stop(esp_service_t *base)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)base;
    service->task_stop = true;
    (void)extractor_cmd_send(service, EXTRACTOR_CMD_STOP, 0, true);
    if (service->mngr != NULL) {
        esp_media_track_write_abort(service->mngr);
    }
    /* Close extractor will abort ongioing time consume reading */
    esp_extractor_handle_t extractor = service->extractor;
    service->extractor = NULL;
    esp_extractor_close(extractor);
    if (service->task != NULL) {
        WAIT_FOR_COND(service->task_running, EXTRACTOR_TASK_EXIT_TIMEOUT_MS, "task stop timeout");
        service->task = NULL;
    }
    close_url_io(service);
#ifdef CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    if (service->hls_cfg != NULL) {
        esp_hls_extractor_io_cfg_deinit(service->hls_cfg);
        service->hls_cfg = NULL;
    }
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */
    extractor_cmd_queue_reset(service);
    return ESP_OK;
}

static esp_err_t extractor_on_deinit(esp_service_t *base)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)base;
    extractor_on_stop(base);
    if (service->mngr != NULL) {
        esp_media_track_mngr_destroy(service->mngr);
        service->mngr = NULL;
    }
    if (service->url) {
        free(service->url);
        service->url = NULL;
    }
    service->src_data = NULL;
    service->src_size = 0;
    release_internal_io(service);
    if (service->cmd_queue != NULL) {
        vQueueDelete(service->cmd_queue);
        service->cmd_queue = NULL;
    }
    return ESP_OK;
}

static esp_err_t extractor_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    (void)base;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_role = ESP_MEDIA_ROLE_SRC;
    return ESP_OK;
}

static esp_err_t extractor_get_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                        esp_media_provider_t *out_provider)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)base;
    if (service == NULL || out_provider == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(extractor_ensure_mngr(service), TAG, "no mngr");
    *out_provider = service->provider;
    return ESP_OK;
}

static esp_err_t extractor_set_request(esp_service_t *base, esp_media_stream_id_t stream,
                                       const esp_media_service_request_t *request)
{
    esp_extractor_service_t *service = (esp_extractor_service_t *)base;
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(extractor_ensure_mngr(service), TAG, "no mngr");
    /* Prefer out_pool_size over the track-manager 8 KiB default so A/V can coexist. */
    size_t cache_size = request->need_global_cache ? (size_t)service->out_pool_size : 0;
    ESP_RETURN_ON_ERROR(esp_media_track_mngr_set_global_cache(service->mngr,
                                                              request->need_global_cache,
                                                              cache_size),
                        TAG, "set global cache failed");
    return esp_media_track_mngr_get_provider(service->mngr, &service->provider);
}

static const char *extractor_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE:
            return "SEEK_DONE";
        case ESP_EXTRACTOR_SERVICE_EVENT_SEEK_ERROR:
            return "SEEK_ERROR";
        case ESP_EXTRACTOR_SERVICE_EVENT_EOS:
            return "EOS";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_extractor_service_ops = {
    .on_start      = extractor_on_start,
    .on_stop       = extractor_on_stop,
    .on_deinit     = extractor_on_deinit,
    .event_to_name = extractor_event_to_name,
};

static const esp_media_service_ops_t s_extractor_media_ops = {
    .get_role     = extractor_get_role,
    .get_provider = extractor_get_provider,
    .set_request  = extractor_set_request,
};

esp_err_t esp_extractor_service_create(const esp_extractor_service_cfg_t *cfg,
                                       esp_extractor_service_t **out_service)
{
    ESP_RETURN_ON_FALSE(cfg != NULL && out_service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    esp_extractor_service_t *service = calloc(1, sizeof(*service));
    if (service == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "no mem");
    }
    service->extract_mask = ESP_EXTRACT_MASK_AV;
    service->out_pool_size = ESP_EXTRACTOR_SERVICE_DEFAULT_POOL_SIZE;
    service->pool = cfg->pool;

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = cfg->name ? cfg->name : ESP_EXTRACTOR_SERVICE_NAME;
    media_cfg.service_ops = &s_extractor_service_ops;
    media_cfg.media_ops = &s_extractor_media_ops;
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        free(service);
        RET_FOR(ret, "media service init failed");
    }
    const char *err_msg = NULL;
    do {
        ret = register_internal_io(service);
        if (ret != ESP_OK) {
            err_msg = "register io failed";
            break;
        }
        ret = extractor_ensure_mngr(service);
        if (ret != ESP_OK) {
            err_msg = "track manager init failed";
            break;
        }
        service->cmd_queue = xQueueCreate(EXTRACTOR_CMD_QUEUE_LEN, sizeof(extractor_cmd_msg_t));
        if (service->cmd_queue == NULL) {
            ret = ESP_ERR_NO_MEM;
            err_msg = "cmd queue failed";
            break;
        }
        *out_service = service;
    } while (0);
    if (err_msg != NULL) {
        esp_media_service_deinit(ESP_SERVICE_BASE(service));
        free(service);
        RET_ERR_MSG(ret, err_msg);
    }
    return ret;
}

static esp_err_t ensure_running(esp_extractor_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_RUNNING,
                        ESP_ERR_INVALID_STATE, TAG, "not running");
    return ESP_OK;
}

static esp_err_t ensure_initialized_or_running(esp_extractor_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            (state == ESP_SERVICE_STATE_INITIALIZED || state == ESP_SERVICE_STATE_RUNNING),
                        ESP_ERR_INVALID_STATE, TAG, "bad state");
    return ESP_OK;
}

esp_err_t esp_extractor_service_set_extract_mask(esp_extractor_service_t *service, uint8_t mask)
{
    ESP_RETURN_ON_FALSE(valid_mask(mask), ESP_ERR_INVALID_ARG, TAG, "bad mask");
    ESP_RETURN_ON_ERROR(ensure_initialized(service), TAG, "bad state");
    service->extract_mask = mask;
    return ESP_OK;
}

esp_err_t esp_extractor_service_set_out_pool_size(esp_extractor_service_t *service, uint32_t out_pool_size)
{
    ESP_RETURN_ON_ERROR(ensure_initialized(service), TAG, "bad state");
    service->out_pool_size = out_pool_size == 0 ? ESP_EXTRACTOR_SERVICE_DEFAULT_POOL_SIZE : out_pool_size;
    return ESP_OK;
}

esp_err_t esp_extractor_service_set_url(esp_extractor_service_t *service, const char *url)
{
    ESP_RETURN_ON_FALSE(url != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_initialized(service), TAG, "bad state");
    char *copy = strdup(url);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    if (service->url) {
        free(service->url);
    }
    service->url = copy;
    service->src_data = NULL;
    service->src_size = 0;
    return ESP_OK;
}

esp_err_t esp_extractor_service_set_src_data(esp_extractor_service_t *service, const void *data, int size)
{
    ESP_RETURN_ON_FALSE(data != NULL && size > 0, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_initialized(service), TAG, "bad state");
    if (service->url) {
        free(service->url);
    }
    service->url = NULL;
    close_url_io(service);
    service->src_data = (const uint8_t *)data;
    service->src_size = (uint32_t)size;
    service->src_pos = 0;
    return ESP_OK;
}

esp_err_t esp_extractor_service_seek(esp_extractor_service_t *service, uint32_t position_ms)
{
    ESP_RETURN_ON_ERROR(ensure_running(service), TAG, "bad state");
    if (service->mngr != NULL) {
        esp_media_track_write_abort(service->mngr);
    }
    extractor_abort_read(service);
    return extractor_cmd_send(service, EXTRACTOR_CMD_SEEK, position_ms, false);
}

esp_err_t esp_extractor_service_set_auto_loop(esp_extractor_service_t *service, bool auto_loop)
{
    ESP_RETURN_ON_ERROR(ensure_initialized_or_running(service), TAG, "bad state");
    service->auto_loop = auto_loop;
    return ESP_OK;
}
