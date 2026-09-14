/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_muxer_service_err.h"
#include "esp_muxer_service_ops.h"
#include "esp_muxer_service_priv.h"
#include "esp_muxer_scheduler.h"

static const char *TAG = "MUXER_SVC";

#define FAKE_STORAGE_DIR_PREFIX  "/fake"
#define STORAGE_DIR_MAX_DEPTH    2
#define MUXER_EXIT_TIMEOUT_MS    3000

#define WAIT_FOR_COND(run_cond, timeout_ms, warn_msg)  do {  \
    int wait_count = timeout_ms / 10;                        \
    while (run_cond) {                                       \
        media_lib_thread_sleep(10);                          \
        if (wait_count > 0) {                                \
            wait_count--;                                    \
            if (wait_count == 0) {                           \
                ESP_LOGW(TAG, warn_msg);                     \
            }                                                \
        }                                                    \
    }                                                        \
} while (0)

typedef union {
    ts_muxer_config_t   ts_cfg;
    mp4_muxer_config_t  mp4_cfg;
    flv_muxer_config_t  flv_cfg;
    wav_muxer_config_t  wav_cfg;
    caf_muxer_config_t  caf_cfg;
    ogg_muxer_config_t  ogg_cfg;
    avi_muxer_config_t  avi_cfg;
} muxer_service_all_cfg_t;

static char *muxer_strdup_or_null(const char *str)
{
    return str == NULL ? NULL : strdup(str);
}

static bool muxer_is_stopped(esp_muxer_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    return esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
           state == ESP_SERVICE_STATE_INITIALIZED;
}

static bool is_fake_storage_dir(const char *dir)
{
    size_t prefix_len = strlen(FAKE_STORAGE_DIR_PREFIX);
    return strncmp(dir, FAKE_STORAGE_DIR_PREFIX, prefix_len) == 0 &&
           (dir[prefix_len] == '\0' || dir[prefix_len] == '/');
}

static esp_err_t ensure_storage_dir(const char *dir)
{
    if (dir == NULL) {
        return ESP_OK;
    }
    if (dir[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (is_fake_storage_dir(dir)) {
        return ESP_OK;
    }

    char *path = strdup(dir);
    ESP_RETURN_ON_FALSE(path != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[--len] = '\0';
    }

    uint8_t depth = 0;
    for (char *p = path; *p != '\0';) {
        while (*p == '/') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        depth++;
        while (*p != '\0' && *p != '/') {
            p++;
        }
    }
    if (depth == 0 || depth > STORAGE_DIR_MAX_DEPTH) {
        ESP_LOGE(TAG, "Storage directory depth %u is unsupported: %s (maximum %u)",
                 depth, dir, STORAGE_DIR_MAX_DEPTH);
        free(path);
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ret = ESP_OK;
    char *component = path + (path[0] == '/' ? 1 : 0);
    while (component != NULL && *component != '\0') {
        char *separator = strchr(component, '/');
        if (separator != NULL) {
            *separator = '\0';
        }
        struct stat st;
        if (stat(path, &st) == 0) {
            if (!S_ISDIR(st.st_mode)) {
                ret = ESP_ERR_INVALID_STATE;
                break;
            }
        } else if (mkdir(path, 0755) != 0 && errno != EEXIST) {
            ESP_LOGE(TAG, "Failed to create storage directory %s: errno %d (%s)",
                     path, errno, strerror(errno));
            ret = ESP_FAIL;
            break;
        }
        if (separator == NULL) {
            break;
        }
        *separator = '/';
        component = separator + 1;
    }
    free(path);
    return ret;
}

static esp_err_t muxer_err_to_esp(esp_muxer_err_t err)
{
    return (esp_err_t)err;
}

static size_t muxer_config_size(esp_muxer_type_t type)
{
    switch (type) {
        case ESP_MUXER_TYPE_TS:
            return sizeof(ts_muxer_config_t);
        case ESP_MUXER_TYPE_MP4:
            return sizeof(mp4_muxer_config_t);
        case ESP_MUXER_TYPE_FLV:
            return sizeof(flv_muxer_config_t);
        case ESP_MUXER_TYPE_WAV:
            return sizeof(wav_muxer_config_t);
        case ESP_MUXER_TYPE_CAF:
            return sizeof(caf_muxer_config_t);
        case ESP_MUXER_TYPE_OGG:
            return sizeof(ogg_muxer_config_t);
        case ESP_MUXER_TYPE_AVI:
            return sizeof(avi_muxer_config_t);
        default:
            return 0;
    }
}

static const char *muxer_extension(esp_muxer_type_t type)
{
    switch (type) {
        case ESP_MUXER_TYPE_TS:
            return "ts";
        case ESP_MUXER_TYPE_MP4:
            return "mp4";
        case ESP_MUXER_TYPE_FLV:
            return "flv";
        case ESP_MUXER_TYPE_WAV:
            return "wav";
        case ESP_MUXER_TYPE_CAF:
            return "caf";
        case ESP_MUXER_TYPE_OGG:
            return "ogg";
        case ESP_MUXER_TYPE_AVI:
            return "avi";
        default:
            return "bin";
    }
}

static esp_muxer_type_t muxer_type_from_extension(const char *url, bool *recognized)
{
    const char *dot = url == NULL ? NULL : strrchr(url, '.');
    if (recognized != NULL) {
        *recognized = true;
    }
    if (dot == NULL || dot[1] == '\0') {
        if (recognized != NULL) {
            *recognized = false;
        }
        return ESP_MUXER_TYPE_TS;
    }
    if (strcasecmp(dot + 1, "ts") == 0) {
        return ESP_MUXER_TYPE_TS;
    }
    if (strcasecmp(dot + 1, "mp4") == 0) {
        return ESP_MUXER_TYPE_MP4;
    }
    if (strcasecmp(dot + 1, "flv") == 0) {
        return ESP_MUXER_TYPE_FLV;
    }
    if (strcasecmp(dot + 1, "wav") == 0) {
        return ESP_MUXER_TYPE_WAV;
    }
    if (strcasecmp(dot + 1, "caf") == 0) {
        return ESP_MUXER_TYPE_CAF;
    }
    if (strcasecmp(dot + 1, "ogg") == 0) {
        return ESP_MUXER_TYPE_OGG;
    }
    if (strcasecmp(dot + 1, "avi") == 0) {
        return ESP_MUXER_TYPE_AVI;
    }
    if (recognized != NULL) {
        *recognized = false;
    }
    return ESP_MUXER_TYPE_TS;
}

static void muxer_get_media_thread_cfg(const esp_muxer_service_t *service, media_lib_thread_cfg_t *out_cfg)
{
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = MUXER_SCHED_TASK_STACK_SIZE,
        .priority = MUXER_SCHED_TASK_PRIORITY,
        .core_id = MUXER_SCHED_TASK_CORE_ID,
    };
    esp_service_thread_cfg_t cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name ? service->media.base.name : ESP_MUXER_SERVICE_NAME,
        .thread_name = ESP_MUXER_SCHED_TASK,
    };
    esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &cfg);
    out_cfg->stack_size = cfg.stack_size;
    out_cfg->priority = cfg.priority;
    out_cfg->core_id = (uint8_t)(cfg.core_id < 0 ? 0 : cfg.core_id);
}

static esp_muxer_audio_codec_t to_muxer_audio_codec(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_FOURCC_AAC:
            return ESP_MUXER_ADEC_AAC;
        case ESP_FOURCC_PCM:
            return ESP_MUXER_ADEC_PCM;
        case ESP_FOURCC_MP3:
            return ESP_MUXER_ADEC_MP3;
        case ESP_FOURCC_ALAW:
            return ESP_MUXER_ADEC_G711_A;
        case ESP_FOURCC_ULAW:
            return ESP_MUXER_ADEC_G711_U;
        case ESP_FOURCC_OPUS:
            return ESP_MUXER_ADEC_OPUS;
        default:
            return ESP_MUXER_ADEC_NONE;
    }
}

static esp_muxer_video_codec_t to_muxer_video_codec(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_FOURCC_H264:
            return ESP_MUXER_VDEC_H264;
        case ESP_FOURCC_MJPG:
            return ESP_MUXER_VDEC_MJPEG;
        default:
            return ESP_MUXER_VDEC_NONE;
    }
}

static muxer_service_track_state_t *track_state(esp_muxer_service_t *service, esp_media_track_type_t type)
{
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return &service->tracks[ESP_MUXER_SERVICE_TRACK_AUDIO];
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return &service->tracks[ESP_MUXER_SERVICE_TRACK_VIDEO];
    }
    return NULL;
}

static int streaming_data_cb(esp_muxer_data_info_t *data, void *ctx)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)ctx;
    if (service == NULL || data == NULL || data->data == NULL || data->size == 0 || service->stream_queue == NULL) {
        return -1;
    }
    void *buffer = NULL;
    int ret = esp_gmf_data_queue_acquire_write(service->stream_queue, &buffer, (int)data->size,
                                               ESP_GMF_DATA_QUEUE_WAIT_FOREVER);
    if (ret != 0 || buffer == NULL) {
        return -1;
    }
    memcpy(buffer, data->data, data->size);
    return esp_gmf_data_queue_release_write(service->stream_queue, (int)data->size) == 0 ? 0 : -1;
}

static int storage_url_pattern(esp_muxer_slice_info_t *info, void *ctx)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)ctx;
    if (service == NULL || info == NULL || info->file_path == NULL || info->len <= 0) {
        return -1;
    }
    const char *base = service->storage_url ? service->storage_url : service->storage_dir;
    if (base == NULL) {
        return -1;
    }
    if (service->storage_url != NULL) {
        if (info->slice_index == 0) {
            return snprintf(info->file_path, info->len, "%s", base) < info->len ? 0 : -1;
        }
        const char *dot = strrchr(base, '.');
        if (dot != NULL && dot > base) {
            int prefix_len = dot - base;
            return snprintf(info->file_path, info->len, "%.*s_%d%s", prefix_len, base,
                            info->slice_index, dot)
                           < info->len
                       ? 0
                       : -1;
        }
        return snprintf(info->file_path, info->len, "%s_%d.%s", base, info->slice_index,
                        muxer_extension(service->muxer_type))
                       < info->len
                   ? 0
                   : -1;
    }
    return snprintf(info->file_path, info->len, "%s/muxed_%d.%s", base, info->slice_index,
                    muxer_extension(service->muxer_type))
                   < info->len
               ? 0
               : -1;
}

static bool storage_configured(const esp_muxer_service_t *service)
{
    return service->storage_url != NULL || service->storage_dir != NULL;
}

static bool storage_enabled(const esp_muxer_service_t *service)
{
    return service->mode == ESP_MUXER_SERVICE_MODE_STORAGE_ONLY ||
           service->mode == ESP_MUXER_SERVICE_MODE_BOTH;
}

static bool streaming_enabled(const esp_muxer_service_t *service)
{
    return service->mode == ESP_MUXER_SERVICE_MODE_STREAMING_ONLY ||
           service->mode == ESP_MUXER_SERVICE_MODE_BOTH;
}

static esp_err_t add_muxer_track(esp_muxer_service_t *service, const esp_media_track_info_t *info)
{
    if (service->muxer == NULL || info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    muxer_service_track_state_t *state = track_state(service, info->type);
    if (state == NULL || state->active) {
        return ESP_OK;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        esp_muxer_audio_stream_info_t audio = {
            .codec = to_muxer_audio_codec(info->info.audio.codec),
            .channel = info->info.audio.channel,
            .bits_per_sample = info->info.audio.bits_per_sample,
            .sample_rate = info->info.audio.sample_rate,
        };
        ESP_RETURN_ON_FALSE(audio.codec != ESP_MUXER_ADEC_NONE, ESP_ERR_NOT_SUPPORTED, TAG, "audio codec");
        ESP_RETURN_ON_ERROR(muxer_err_to_esp(esp_muxer_add_audio_stream(service->muxer, &audio,
                                                                        &state->stream_index)),
                            TAG, "add audio stream");
    } else if (info->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        esp_muxer_video_stream_info_t video = {
            .codec = to_muxer_video_codec(info->info.video.codec),
            .width = info->info.video.width,
            .height = info->info.video.height,
            .fps = (uint8_t)info->info.video.fps,
        };
        ESP_RETURN_ON_FALSE(video.codec != ESP_MUXER_VDEC_NONE, ESP_ERR_NOT_SUPPORTED, TAG, "video codec");
        ESP_RETURN_ON_ERROR(muxer_err_to_esp(esp_muxer_add_video_stream(service->muxer, &video,
                                                                        &state->stream_index)),
                            TAG, "add video stream");
    }
    state->track_id = info->id;
    state->active = true;
    return ESP_OK;
}

static void clear_muxer_track(esp_muxer_service_t *service, const esp_media_track_info_t *info)
{
    muxer_service_track_state_t *state = info ? track_state(service, info->type) : NULL;
    if (state != NULL && (!state->active || state->track_id == info->id)) {
        memset(state, 0, sizeof(*state));
    }
}

static void provider_event_handler(esp_media_provider_event_t event, const esp_media_track_info_t *info, void *ctx)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)ctx;
    if (service == NULL) {
        return;
    }
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT) {
        service->task_stop = true;
        return;
    }
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED) {
        clear_muxer_track(service, info);
        return;
    }
    if ((event == ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED || event == ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED) &&
        service->muxer != NULL && info != NULL) {
        (void)add_muxer_track(service, info);
    }
}

static esp_err_t configure_muxer_tracks(esp_muxer_service_t *service)
{
    uint16_t track_num = 0;
    ESP_RETURN_ON_ERROR(esp_media_provider_get_track_num(&service->provider, &track_num), TAG, "track num");
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        ESP_RETURN_ON_ERROR(esp_media_provider_get_track_info(&service->provider, i, &info), TAG, "track info");
        ESP_RETURN_ON_ERROR(add_muxer_track(service, &info), TAG, "add track");
    }
    return ESP_OK;
}

static esp_err_t open_muxer(esp_muxer_service_t *service)
{
    if (storage_enabled(service) && !storage_configured(service)) {
        return ESP_ERR_INVALID_STATE;
    }
    bool queue_created = false;
    if (streaming_enabled(service) && service->stream_queue == NULL) {
        uint32_t cache_size = service->streaming_cache_size > 0 ? service->streaming_cache_size :
                                                                ESP_MUXER_SERVICE_DEFAULT_QUEUE_SIZE;
        service->stream_queue = esp_gmf_data_queue_create(cache_size);
        ESP_RETURN_ON_FALSE(service->stream_queue != NULL, ESP_ERR_NO_MEM, TAG, "stream queue");
        queue_created = true;
    }

    muxer_service_all_cfg_t cfg = {0};
    esp_muxer_config_t *base_cfg = &cfg.ts_cfg.base_config;
    base_cfg->muxer_type = service->muxer_type;
    base_cfg->slice_duration = service->slice_duration > 0 ? service->slice_duration : ESP_MUXER_SERVICE_DEFAULT_SLICE_DUR;
    base_cfg->ram_cache_size = service->ram_cache_size;
    base_cfg->ctx = service;
    if (storage_enabled(service) && storage_configured(service)) {
        base_cfg->url_pattern_ex = storage_url_pattern;
    }
    if (streaming_enabled(service)) {
        base_cfg->data_cb = streaming_data_cb;
    }

    size_t cfg_size = muxer_config_size(service->muxer_type);
    esp_err_t ret = ESP_OK;
    if (cfg_size == 0) {
        ESP_LOGE(TAG, "muxer type");
        ret = ESP_ERR_NOT_SUPPORTED;
        goto cleanup_queue;
    }
    service->muxer = esp_muxer_open(base_cfg, cfg_size);
    if (service->muxer == NULL) {
        ESP_LOGE(TAG, "open muxer");
        ret = ESP_FAIL;
        goto cleanup_queue;
    }
    ret = configure_muxer_tracks(service);
    if (ret != ESP_OK) {
        esp_muxer_close(service->muxer);
        service->muxer = NULL;
        goto cleanup_queue;
    }
    return ESP_OK;

cleanup_queue:
    if (queue_created && service->stream_queue != NULL) {
        esp_gmf_data_queue_destroy(service->stream_queue);
        service->stream_queue = NULL;
    }
    return ret;
}

static esp_err_t add_frame_packet(esp_muxer_service_t *service, const esp_media_frame_t *frame)
{
    muxer_service_track_state_t *state = track_state(service, frame->type);
    if (state == NULL || !state->active || service->muxer == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    uint32_t pts = frame->pts < 0 ? 0 : (uint32_t)frame->pts;
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        esp_muxer_audio_packet_t packet = {
            .data = frame->data,
            .len = (int)frame->size,
            .pts = pts,
        };
        return muxer_err_to_esp(esp_muxer_add_audio_packet(service->muxer, state->stream_index, &packet));
    }
    esp_muxer_video_packet_t packet = {
        .data = frame->data,
        .len = (int)frame->size,
        .pts = pts,
        .dts = frame->dts < 0 ? pts : (uint32_t)frame->dts,
        .key_frame = (frame->flags & ESP_MEDIA_FRAME_FLAG_KEY) != 0,
    };
    return muxer_err_to_esp(esp_muxer_add_video_packet(service->muxer, state->stream_index, &packet));
}

static void muxer_task(void *arg)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)arg;
    service->task_running = true;
    while (!service->task_stop) {
        esp_media_frame_t frame = {0};
        esp_err_t ret = esp_media_provider_acquire_frame(&service->provider, &frame, 20);
        if (ret != ESP_OK) {
            continue;
        }
        if (!service->task_stop && frame.size > 0 && frame.data != NULL &&
            (frame.type == ESP_MEDIA_TRACK_TYPE_AUDIO || frame.type == ESP_MEDIA_TRACK_TYPE_VIDEO)) {
            ret = add_frame_packet(service, &frame);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Add packet failed: %s", esp_err_to_name(ret));
            }
        }
        esp_media_provider_release_frame(&service->provider, &frame);
    }
    service->task_running = false;
    media_lib_thread_destroy(NULL);
}

static esp_err_t muxer_get_request_impl(esp_muxer_service_t *service, esp_media_stream_id_t stream,
                                        esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(request, 0, sizeof(*request));
    request->need_global_cache = true;
    return ESP_OK;
}

static esp_err_t muxer_set_provider_impl(esp_muxer_service_t *service, esp_media_stream_id_t stream,
                                         const esp_media_provider_t *provider)
{
    if (service == NULL || stream != ESP_MEDIA_DEFAULT_STREAM) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_FALSE(muxer_is_stopped(service), ESP_ERR_INVALID_STATE, TAG, "bad state");
    if (service->provider.ops != NULL) {
        esp_media_provider_set_event_cb(&service->provider, NULL, NULL);
    }
    if (provider == NULL || provider->ops == NULL) {
        memset(&service->provider, 0, sizeof(service->provider));
        return ESP_OK;
    }
    service->provider = *provider;
    return esp_media_provider_set_event_cb(&service->provider, provider_event_handler, service);
}

static esp_err_t muxer_on_start(esp_service_t *base)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)base;
    if (service->provider.ops == NULL) {
        return ESP_OK;
    }
    memset(service->tracks, 0, sizeof(service->tracks));
    ESP_RETURN_ON_ERROR(open_muxer(service), TAG, "open");

    media_lib_thread_cfg_t task_cfg = {0};
    muxer_get_media_thread_cfg(service, &task_cfg);
    service->task_stop = false;
    if (media_lib_thread_create(&service->task, ESP_MUXER_SCHED_TASK, muxer_task, service,
                                task_cfg.stack_size, task_cfg.priority, task_cfg.core_id) != ESP_OK) {
        esp_muxer_close(service->muxer);
        service->muxer = NULL;
        if (service->stream_queue != NULL) {
            esp_gmf_data_queue_destroy(service->stream_queue);
            service->stream_queue = NULL;
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t muxer_on_stop(esp_service_t *base)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)base;
    service->task_stop = true;
    if (service->provider.ops != NULL) {
        esp_media_provider_abort(&service->provider);
    }
    if (service->stream_queue != NULL) {
        esp_gmf_data_queue_wakeup(service->stream_queue);
    }
    if (service->task != NULL) {
        WAIT_FOR_COND(service->task_running, MUXER_EXIT_TIMEOUT_MS, "muxer exit timeout");
        service->task = NULL;
    }
    if (service->muxer != NULL) {
        esp_muxer_close(service->muxer);
        service->muxer = NULL;
    }
    if (service->stream_queue != NULL) {
        esp_gmf_data_queue_t *stream_q = service->stream_queue;
        service->stream_queue = NULL;
        esp_gmf_data_queue_destroy(stream_q);
    }
    return ESP_OK;
}

static esp_err_t muxer_on_deinit(esp_service_t *base)
{
    esp_muxer_service_t *service = (esp_muxer_service_t *)base;
    (void)muxer_on_stop(base);
    if (service->provider.ops != NULL) {
        esp_media_provider_set_event_cb(&service->provider, NULL, NULL);
    }
    if (service->stream_queue != NULL) {
        esp_gmf_data_queue_destroy(service->stream_queue);
        service->stream_queue = NULL;
    }
    if (service->storage_dir != NULL) {
        free(service->storage_dir);
        service->storage_dir = NULL;
    }
    if (service->storage_url != NULL) {
        free(service->storage_url);
        service->storage_url = NULL;
    }
    return ESP_OK;
}

static esp_err_t muxer_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    (void)base;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_role = ESP_MEDIA_ROLE_SINK;
    return ESP_OK;
}

static esp_err_t muxer_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                    const esp_media_provider_t *provider)
{
    return muxer_set_provider_impl((esp_muxer_service_t *)base, stream, provider);
}

static esp_err_t muxer_get_request(esp_service_t *base, esp_media_stream_id_t stream,
                                   esp_media_service_request_t *request)
{
    return muxer_get_request_impl((esp_muxer_service_t *)base, stream, request);
}

static const esp_service_ops_t s_muxer_service_ops = {
    .on_start  = muxer_on_start,
    .on_stop   = muxer_on_stop,
    .on_deinit = muxer_on_deinit,
};

static const esp_media_service_ops_t s_muxer_media_ops = {
    .get_role     = muxer_get_role,
    .set_provider = muxer_set_provider,
    .get_request  = muxer_get_request,
};

esp_err_t esp_muxer_service_create(const esp_muxer_service_cfg_t *cfg,
                                   esp_muxer_service_t **out_service)
{
    ESP_RETURN_ON_FALSE(cfg != NULL && out_service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    *out_service = NULL;

    esp_muxer_service_t *service = calloc(1, sizeof(*service));
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    service->muxer_type = ESP_MUXER_TYPE_TS;
    service->mode = ESP_MUXER_SERVICE_MODE_STORAGE_ONLY;

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = cfg->name ? cfg->name : ESP_MUXER_SERVICE_NAME;
    media_cfg.service_ops = &s_muxer_service_ops;
    media_cfg.media_ops = &s_muxer_media_ops;
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        free(service);
        RET_FOR(ret, "media init failed");
    }

    *out_service = service;
    return ESP_OK;
}

esp_err_t esp_muxer_service_setup(esp_muxer_service_t *service, const esp_muxer_service_setup_t *setup)
{
    ESP_RETURN_ON_FALSE(service != NULL && setup != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(muxer_is_stopped(service), ESP_ERR_INVALID_STATE, TAG, "bad state");
    ESP_RETURN_ON_FALSE(muxer_config_size(setup->muxer_type) > 0, ESP_ERR_NOT_SUPPORTED, TAG, "muxer type");
    ESP_RETURN_ON_FALSE(setup->mode <= ESP_MUXER_SERVICE_MODE_BOTH, ESP_ERR_INVALID_ARG, TAG, "mode");
    ESP_RETURN_ON_ERROR(ensure_storage_dir(setup->storage_dir), TAG, "storage dir");

    char *storage_dir = muxer_strdup_or_null(setup->storage_dir);
    if (setup->storage_dir != NULL && storage_dir == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "no mem");
    }

    free(service->storage_dir);
    service->storage_dir = storage_dir;
    service->muxer_type = setup->muxer_type;
    service->slice_duration = setup->slice_duration;
    service->ram_cache_size = setup->ram_cache_size;
    service->streaming_cache_size = setup->streaming_cache_size;
    service->mode = setup->mode;
    if (!streaming_enabled(service) && service->stream_queue != NULL) {
        esp_gmf_data_queue_destroy(service->stream_queue);
        service->stream_queue = NULL;
    }
    return ESP_OK;
}

esp_err_t esp_muxer_service_set_storage_url(esp_muxer_service_t *service, const char *url)
{
    ESP_RETURN_ON_FALSE(service != NULL && url != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(muxer_is_stopped(service), ESP_ERR_INVALID_STATE, TAG, "bad state");
    char *copy = strdup(url);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    bool recognized = false;
    esp_muxer_type_t muxer_type = muxer_type_from_extension(url, &recognized);
    free(service->storage_url);
    service->storage_url = copy;
    if (recognized) {
        service->muxer_type = muxer_type;
    }
    return ESP_OK;
}

esp_err_t esp_muxer_service_acquire_streaming_data(esp_muxer_service_t *service, const uint8_t **out_data,
                                                   size_t *out_size, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(service != NULL && out_data != NULL && out_size != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(streaming_enabled(service) && service->stream_queue != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "no stream queue");
    void *buffer = NULL;
    int size = 0;
    int ret = esp_gmf_data_queue_acquire_read(service->stream_queue, &buffer, &size,
                                              muxer_queue_timeout_ms(timeout_ms));
    if (ret != 0) {
        RET_FOR(ret == ESP_GMF_IO_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_STATE,
                "acquire streaming data failed");
    }
    *out_data = (const uint8_t *)buffer;
    *out_size = size < 0 ? 0 : (size_t)size;
    return ESP_OK;
}

esp_err_t esp_muxer_service_read_streaming_data(esp_muxer_service_t *service, uint8_t *buffer,
                                                size_t *inout_size, uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(buffer != NULL && inout_size != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    const uint8_t *data = NULL;
    size_t size = 0;
    esp_err_t ret = esp_muxer_service_acquire_streaming_data(service, &data, &size, timeout_ms);
    if (ret != ESP_OK) {
        RET_FOR(ret, "acquire streaming data failed ret %d", ret);
    }
    if (*inout_size < size) {
        esp_muxer_service_release_streaming_data(service);
        *inout_size = size;
        RET_FOR(ESP_ERR_INVALID_SIZE, "buffer too small need:%u", (unsigned)size);
    }
    if (size > 0) {
        memcpy(buffer, data, size);
    }
    *inout_size = size;
    return esp_muxer_service_release_streaming_data(service);
}

esp_err_t esp_muxer_service_release_streaming_data(esp_muxer_service_t *service)
{
    ESP_RETURN_ON_FALSE(service != NULL && service->stream_queue != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid arg");
    return esp_gmf_data_queue_release_read(service->stream_queue) == 0 ? ESP_OK : ESP_FAIL;
}
