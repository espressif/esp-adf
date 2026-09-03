/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_capture_service_setup.h"
#include "esp_capture_service_priv.h"
#include "capture_service_err.h"

static const char *TAG = "CAPTURE_SERVICE";

#define FAKE_STORAGE_DIR_PREFIX  "/fake"
#define STORAGE_DIR_MAX_DEPTH    2

typedef struct {
    bool                             configured;
    esp_media_track_info_t           tracks[ESP_CAPTURE_SERVICE_MAX_TRACKS_PER_STREAM];
    uint8_t                          track_num;
    esp_capture_service_muxer_cfg_t  muxer;
    char                            *storage_dir;
} capture_stream_setup_t;

struct esp_capture_service_setup {
    esp_capture_service_cfg_t      service_cfg;
    esp_capture_service_src_cfg_t  src_cfg;
    bool                           has_src_cfg;
    capture_stream_setup_t        *streams;
};

static char *setup_strdup(const char *str)
{
    if (str == NULL) {
        return NULL;
    }
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, str, len);
    }
    return copy;
}

static bool is_fake_storage_dir(const char *dir)
{
    size_t prefix_len = strlen(FAKE_STORAGE_DIR_PREFIX);
    return strncmp(dir, FAKE_STORAGE_DIR_PREFIX, prefix_len) == 0 &&
           (dir[prefix_len] == '\0' || dir[prefix_len] == '/');
}

esp_err_t capture_service_ensure_storage_dir(const char *dir)
{
    if (dir == NULL) {
        return ESP_OK;
    }
    if (dir[0] == '\0') {
        ESP_LOGE(TAG, "Storage directory is empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (is_fake_storage_dir(dir)) {
        return ESP_OK;
    }

    char *path = setup_strdup(dir);
    if (path == NULL) {
        return ESP_ERR_NO_MEM;
    }
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
                ESP_LOGE(TAG, "Storage path exists but is not a directory: %s", path);
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

esp_err_t capture_service_ensure_storage_url_parent(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_OK;
    }
    if (is_fake_storage_dir(url)) {
        return ESP_OK;
    }
    const char *slash = strrchr(url, '/');
    if (slash == NULL || slash == url) {
        return ESP_OK;
    }
    size_t parent_len = (size_t)(slash - url);
    char *parent = malloc(parent_len + 1);
    if (parent == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(parent, url, parent_len);
    parent[parent_len] = '\0';
    esp_err_t ret = capture_service_ensure_storage_dir(parent);
    free(parent);
    return ret;
}

static esp_err_t validate_stream(const esp_capture_service_setup_t *setup, esp_media_stream_id_t stream)
{
    if (setup == NULL || stream >= setup->service_cfg.max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static bool setup_has_track_type(const esp_capture_service_setup_t *setup, esp_media_track_type_t type)
{
    for (uint16_t i = 0; i < setup->service_cfg.max_stream_num; i++) {
        const capture_stream_setup_t *stream_setup = &setup->streams[i];
        for (uint8_t j = 0; j < stream_setup->track_num; j++) {
            if (stream_setup->tracks[j].type == type) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief  Infer the capture sync mode for an instance from its configured tracks.
 *
 * @note  Sync is an instance-level policy: when the same capture instance
 *        contains any audio track and any video track (even on different streams),
 *        audio-master sync is enabled for the whole instance. Audio and video do
 *        not need to live on the same stream. Instances that are audio-only or
 *        video-only run without sync.
 */
static esp_capture_sync_mode_t infer_sync_mode(const esp_capture_service_setup_t *setup)
{
    bool has_audio = setup_has_track_type(setup, ESP_MEDIA_TRACK_TYPE_AUDIO);
    bool has_video = setup_has_track_type(setup, ESP_MEDIA_TRACK_TYPE_VIDEO);
    return (has_audio && has_video) ? ESP_CAPTURE_SYNC_MODE_AUDIO : ESP_CAPTURE_SYNC_MODE_NONE;
}

static esp_err_t fill_sink_track_info(const esp_media_track_info_t *track, esp_capture_sink_cfg_t *sink_cfg)
{
    if (track->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        sink_cfg->audio_info.format_id = (esp_capture_format_id_t)track->info.audio.codec;
        sink_cfg->audio_info.sample_rate = track->info.audio.sample_rate;
        sink_cfg->audio_info.channel = track->info.audio.channel;
        sink_cfg->audio_info.bits_per_sample = track->info.audio.bits_per_sample;
        return ESP_OK;
    }
    if (track->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        sink_cfg->video_info.format_id = (esp_capture_format_id_t)track->info.video.codec;
        sink_cfg->video_info.width = track->info.video.width;
        sink_cfg->video_info.height = track->info.video.height;
        sink_cfg->video_info.fps = track->info.video.fps;
        return ESP_OK;
    }
    if (track->type == ESP_MEDIA_TRACK_TYPE_MUXER) {
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static void setup_storage_for_stream(capture_stream_t *stream, const capture_stream_setup_t *stream_setup)
{
    if (!stream_setup->muxer.auto_record && !stream_setup->muxer.streaming && stream_setup->muxer.storage_dir == NULL) {
        stream->storage_pending = false;
        return;
    }
    stream->storage_pending = true;
}

static esp_err_t setup_sink_for_stream(esp_capture_service_t *service, uint16_t stream_idx,
                                       const capture_stream_setup_t *stream_setup)
{
    capture_stream_t *stream = &service->streams[stream_idx];
    esp_capture_sink_cfg_t sink_cfg = {0};

    if (stream_setup->track_num == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0; i < stream_setup->track_num; i++) {
        esp_err_t ret = fill_sink_track_info(&stream_setup->tracks[i], &sink_cfg);
        if (ret != ESP_OK) {
            return ret;
        }
        stream->tracks[i] = stream_setup->tracks[i];
    }
    esp_err_t ret = capture_err_to_esp(
        esp_capture_sink_setup(service->capture, stream_idx, &sink_cfg, &stream->sink));
    if (ret != ESP_OK) {
        return ret;
    }
    stream->configured = true;
    stream->service = service;
    stream->enabled = true;
    stream->muxer = stream_setup->muxer;
    stream->track_num = stream_setup->track_num;
    if (stream_setup->storage_dir != NULL) {
        stream->storage_dir = setup_strdup(stream_setup->storage_dir);
        if (stream->storage_dir == NULL) {
            return ESP_ERR_NO_MEM;
        }
        stream->muxer.storage_dir = stream->storage_dir;
    }
    setup_storage_for_stream(stream, stream_setup);
    return capture_err_to_esp(esp_capture_sink_enable(stream->sink, ESP_CAPTURE_RUN_MODE_ALWAYS));
}

esp_capture_service_setup_t *esp_capture_service_setup_create(const esp_capture_service_cfg_t *service_cfg)
{
    if (service_cfg == NULL || service_cfg->max_stream_num == 0) {
        ESP_LOGE(TAG, "%s:%d Invalid service configuration", __func__, __LINE__);
        return NULL;
    }
    esp_capture_service_setup_t *setup = calloc(1, sizeof(*setup));
    if (setup == NULL) {
        ESP_LOGE(TAG, "%s:%d Allocate setup failed", __func__, __LINE__);
        return NULL;
    }
    setup->service_cfg = *service_cfg;
    setup->streams = calloc(service_cfg->max_stream_num, sizeof(capture_stream_setup_t));
    if (setup->streams == NULL) {
        free(setup);
        ESP_LOGE(TAG, "%s:%d Allocate setup streams failed", __func__, __LINE__);
        return NULL;
    }
    return setup;
}

esp_err_t esp_capture_service_setup_destroy(esp_capture_service_setup_t *setup)
{
    if (setup == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid setup");
    }
    for (uint16_t i = 0; i < setup->service_cfg.max_stream_num; i++) {
        free(setup->streams[i].storage_dir);
    }
    free(setup->streams);
    free(setup);
    return ESP_OK;
}

esp_err_t esp_capture_service_setup_src(esp_capture_service_setup_t *setup,
                                        const esp_capture_service_src_cfg_t *src_cfg)
{
    if (setup == NULL || src_cfg == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid source configuration");
    }
    setup->src_cfg = *src_cfg;
    setup->has_src_cfg = true;
    return ESP_OK;
}

esp_err_t esp_capture_service_setup_add_track(esp_capture_service_setup_t *setup,
                                              esp_media_stream_id_t stream,
                                              const esp_media_track_info_t *track)
{
    esp_err_t ret = validate_stream(setup, stream);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Invalid stream");
    }
    if (track == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid track");
    }
    if (track->type != ESP_MEDIA_TRACK_TYPE_AUDIO && track->type != ESP_MEDIA_TRACK_TYPE_VIDEO &&
        track->type != ESP_MEDIA_TRACK_TYPE_MUXER) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Unsupported track type");
    }
    capture_stream_setup_t *stream_setup = &setup->streams[stream];
    /* A stream maps each stream type to a single track and the provider resolves by
       type, so duplicate track types are ambiguous and rejected. */
    for (uint8_t i = 0; i < stream_setup->track_num; i++) {
        if (stream_setup->tracks[i].type == track->type) {
            RET_FOR(ESP_ERR_INVALID_ARG, "Duplicate track type");
        }
    }
    if (stream_setup->track_num >= ESP_CAPTURE_SERVICE_MAX_TRACKS_PER_STREAM) {
        RET_FOR(ESP_ERR_NO_MEM, "Too many tracks");
    }
    stream_setup->tracks[stream_setup->track_num++] = *track;
    stream_setup->configured = true;
    return ESP_OK;
}

/* Muxer types are FourCC values, so ESP_MUXER_TYPE_MAX is not an upper bound. */
static bool is_muxer_type_valid(esp_muxer_type_t type)
{
    return (type > 0 && type != ESP_MUXER_TYPE_MAX);
}

esp_err_t esp_capture_service_setup_set_muxer_cfg(esp_capture_service_setup_t *setup,
                                                  esp_media_stream_id_t stream,
                                                  const esp_capture_service_muxer_cfg_t *cfg)
{
    esp_err_t ret = validate_stream(setup, stream);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Invalid stream");
    }
    if (cfg == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid muxer configuration");
    }
    if (!is_muxer_type_valid(cfg->muxer_type)) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid muxer type");
    }
    ret = capture_service_ensure_storage_dir(cfg->storage_dir);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Prepare storage directory failed");
    }
    char *copy = setup_strdup(cfg->storage_dir);
    if (cfg->storage_dir != NULL && copy == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Copy storage directory failed");
    }
    capture_stream_setup_t *stream_setup = &setup->streams[stream];
    free(stream_setup->storage_dir);
    stream_setup->storage_dir = copy;
    stream_setup->muxer = *cfg;
    stream_setup->muxer.storage_dir = stream_setup->storage_dir;
    stream_setup->configured = true;
    return ESP_OK;
}

esp_err_t esp_capture_service_setup_apply(esp_capture_service_t *service,
                                          const esp_capture_service_setup_t *setup)
{
    if (service == NULL || setup == NULL || setup->service_cfg.max_stream_num > service->max_stream_num) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid setup arguments");
    }
    /* Setup is not allowed while the service is running. Re-applying tears down
       and recreates the capture, which would free capture_stream_t memory that a
       linked provider's ctx still points at. Linking before start stays valid as
       long as the provider has not requested data yet. */
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
        state == ESP_SERVICE_STATE_RUNNING) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Service is running");
    }
    if (service->configured) {
        esp_err_t ret = capture_service_teardown(service);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Tear down service failed");
        }
    }

    bool has_audio = setup_has_track_type(setup, ESP_MEDIA_TRACK_TYPE_AUDIO);
    bool has_video = setup_has_track_type(setup, ESP_MEDIA_TRACK_TYPE_VIDEO);
    esp_capture_audio_src_if_t *audio_src = has_audio && setup->has_src_cfg ? setup->src_cfg.audio_src : NULL;
    esp_capture_video_src_if_t *video_src = has_video && setup->has_src_cfg ? setup->src_cfg.video_src : NULL;
    if ((has_audio && audio_src == NULL) || (has_video && video_src == NULL)) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Required source unavailable");
    }

    service->streams = calloc(service->max_stream_num, sizeof(capture_stream_t));
    if (service->streams == NULL) {
        RET_FOR(ESP_ERR_NO_MEM, "Allocate streams failed");
    }
    service->stream_num = service->max_stream_num;

    esp_capture_cfg_t capture_cfg = {
        .sync_mode = infer_sync_mode(setup),
        .audio_src = audio_src,
        .video_src = video_src,
        .share_overlay = setup->has_src_cfg ? setup->src_cfg.share_overlay : false,
        .full_speed_decode = setup->has_src_cfg ? setup->src_cfg.full_speed_decode : false,
    };
    esp_err_t ret = capture_err_to_esp(esp_capture_open(&capture_cfg, &service->capture));
    if (ret != ESP_OK) {
        free(service->streams);
        service->streams = NULL;
        service->stream_num = 0;
        RET_FOR(ret, "Open capture failed");
    }

    for (uint16_t i = 0; i < setup->service_cfg.max_stream_num; i++) {
        if (!setup->streams[i].configured || setup->streams[i].track_num == 0) {
            continue;
        }
        ret = setup_sink_for_stream(service, i, &setup->streams[i]);
        if (ret != ESP_OK) {
            if (service->capture != NULL) {
                esp_capture_close(service->capture);
            }
            for (uint16_t j = 0; j < service->max_stream_num; j++) {
                free(service->streams[j].storage_url);
                free(service->streams[j].storage_dir);
                free(service->streams[j].last_storage_url);
            }
            free(service->streams);
            service->capture = NULL;
            service->streams = NULL;
            service->stream_num = 0;
            RET_FOR(ret, "Configure stream failed");
        }
    }
    service->configured = true;
    service->audio_src = audio_src;
    service->video_src = video_src;
    return ESP_OK;
}
