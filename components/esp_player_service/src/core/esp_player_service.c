/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "esp_fourcc.h"
#include "esp_gmf_oal_thread.h"
#include "esp_media_provider.h"
#include "esp_player.h"
#include "esp_player_advance.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"

#include "esp_player_scheduler.h"
#include "esp_player_service.h"
#include "esp_player_service_defaults.h"
#include "esp_player_service_priv.h"
#include "esp_player_service_setup.h"
#include "internal/esp_player_service_subclass.h"
#include "player_out.h"

#define ESP_PLAYER_SERVICE_CODEC_PCM  ESP_FOURCC_PCM

/**
 * @brief  Linked-provider reader task context
 */
typedef struct {
    esp_player_service_t   *service;  /*!< Owning player service */
    player_stream_slot_t   *slot;     /*!< Stream slot being read */
    esp_media_stream_id_t   stream;   /*!< Public stream id */
    esp_media_track_type_t  type;     /*!< Audio or video reader */
} player_provider_ctx_t;

static const char *TAG = "PLAYER_SERVICE";

static esp_err_t get_thread_cfg(esp_player_service_t *service,
                                uint16_t service_inst_idx,
                                const char *thread_name,
                                const esp_service_thread_cfg_t *default_cfg,
                                esp_service_thread_cfg_t *out_cfg)
{
    if (service == NULL || thread_name == NULL || default_cfg == NULL || out_cfg == NULL) {
        ESP_LOGE(TAG, "Get thread cfg failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    *out_cfg = *default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name != NULL
                            ? service->media.base.name
                            : ESP_PLAYER_SERVICE_DEFAULT_NAME,
        .service_inst_idx = service_inst_idx,
        .thread_name = thread_name,
    };
    return esp_service_scheduler_get_thread_cfg(&request, default_cfg, out_cfg);
}

static void overlay_gmf_task_from_scheduler(esp_player_service_t *service,
                                            uint16_t stream_idx,
                                            const char *thread_name,
                                            esp_gmf_task_config_t *task_cfg)
{
    if (task_cfg == NULL) {
        return;
    }
    esp_service_thread_cfg_t default_thread_cfg = {
        .stack_size = task_cfg->stack,
        .priority = task_cfg->prio,
        .core_id = task_cfg->core,
        .is_ext = (task_cfg->stack_in_ext != 0),
    };
    esp_service_thread_cfg_t thread_cfg = default_thread_cfg;
    if (get_thread_cfg(service, stream_idx, thread_name, &default_thread_cfg, &thread_cfg) != ESP_OK) {
        return;
    }
    task_cfg->stack = thread_cfg.stack_size;
    task_cfg->prio = thread_cfg.priority;
    task_cfg->core = thread_cfg.core_id;
    task_cfg->stack_in_ext = thread_cfg.is_ext ? 1 : 0;
}

static void ps_defer_task(void *arg)
{
    esp_player_service_t *service = (esp_player_service_t *)arg;
    player_defer_t *defer = &service->defer;
    while (1) {
        xSemaphoreTake(defer->wake, portMAX_DELAY);
        if (defer->state == PS_DEFER_EXIT) {
            break;
        }
        /* Set busy before reading the state: a quiesce that misses this batch
           has already cleared the pending flags, so the batch is a no-op. */
        defer->busy = true;
        if (defer->state == PS_DEFER_RUN) {
            ps_defer_run_pending(service);
        }
        defer->busy = false;
    }
    defer->task = NULL;
    esp_gmf_oal_thread_delete(NULL);
}

/* Ask the defer task to look at the pending flags; extra wakes coalesce. */
static void ps_defer_notify(esp_player_service_t *service)
{
    if (service != NULL && service->defer.wake != NULL) {
        xSemaphoreGive(service->defer.wake);
    }
}

static void ps_defer_wait_idle(const esp_player_service_t *service, const char *what)
{
    unsigned polls = 0;
    while (service->defer.busy) {
        vTaskDelay(pdMS_TO_TICKS(ESP_PLAYER_SERVICE_TASK_JOIN_POLL_MS));
        polls++;
        if ((polls % ESP_PLAYER_SERVICE_TASK_JOIN_WARN_EVERY) == 0) {
            ESP_LOGW(TAG, "Still waiting for deferred work: %s", what);
        }
    }
}

/* Drop pending work and wait for the running batch, so the caller can tear
   down players without racing an auto-advance. */
static void ps_defer_quiesce(esp_player_service_t *service)
{
    if (service->defer.state != PS_DEFER_EXIT) {
        service->defer.state = PS_DEFER_DROP;
    }
    service->arb_pending = false;
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        service->streams[i].auto_advance_pending = false;
    }
    ps_defer_wait_idle(service, "quiesce");
}

static void ps_defer_stop(esp_player_service_t *service)
{
    if (service->defer.task != NULL) {
        ps_defer_quiesce(service);
        service->defer.state = PS_DEFER_EXIT;
        ps_defer_notify(service);
        unsigned polls = 0;
        while (service->defer.task != NULL) {
            vTaskDelay(pdMS_TO_TICKS(ESP_PLAYER_SERVICE_TASK_JOIN_POLL_MS));
            polls++;
            if ((polls % ESP_PLAYER_SERVICE_TASK_JOIN_WARN_EVERY) == 0) {
                ESP_LOGW(TAG, "Still waiting for deferred work task join");
            }
        }
    }
    if (service->defer.wake != NULL) {
        vSemaphoreDelete(service->defer.wake);
        service->defer.wake = NULL;
    }
}

static void apply_out_task_cfg(esp_player_service_t *service)
{
    player_out_task_cfg_t out_task = {0};
    player_out_audio_default_task_cfg(&out_task);
    esp_service_thread_cfg_t default_thread_cfg = {
        .stack_size = out_task.stack,
        .priority = out_task.prio,
        .core_id = out_task.core,
        .is_ext = out_task.stack_in_ext,
    };
    esp_service_thread_cfg_t thread_cfg = default_thread_cfg;
    if (get_thread_cfg(service, 0, ESP_PLAYER_SERVICE_MIXER_TASK_NAME,
                       &default_thread_cfg, &thread_cfg) == ESP_OK) {
        out_task.stack = thread_cfg.stack_size;
        out_task.prio = (uint8_t)thread_cfg.priority;
        out_task.core = (int8_t)thread_cfg.core_id;
        out_task.stack_in_ext = thread_cfg.is_ext;
    }
    (void)player_out_audio_set_task_cfg(service->out, &out_task);
}

static esp_err_t apply_player_scheduler_tasks(esp_player_service_t *service,
                                              esp_media_stream_id_t stream,
                                              esp_player_handle_t player)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    bool want_video = ps_has_video_out(slot);
    esp_player_task_config_t task_cfg = {
        .extractor = {
            .stack = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_STACK,
            .prio = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_PRIO,
            .core = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_EXTRACTOR_CORE,
            .stack_in_ext = 1,
        },
        .audio_decoder = {
            .stack = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_STACK,
            .prio = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_PRIO,
            .core = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_AUDIO_DECODER_CORE,
            .stack_in_ext = 1,
        },
        .audio_render = {
            .stack = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_STACK,
            .prio = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_PRIO,
            .core = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_AUDIO_RENDER_CORE,
            .stack_in_ext = 1,
        },
        .video_decoder = {
            .stack = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_STACK,
            .prio = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_PRIO,
            .core = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_VIDEO_DECODER_CORE,
            .stack_in_ext = 1,
        },
        .video_render = {
            .stack = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_STACK,
            .prio = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_TASK_PRIO,
            .core = ESP_PLAYER_SERVICE_DEFAULT_PLAYER_VIDEO_RENDER_CORE,
            .stack_in_ext = 1,
        },
    };
    uint16_t stream_idx = (uint16_t)stream;
    overlay_gmf_task_from_scheduler(service, stream_idx,
                                    ESP_PLAYER_SERVICE_PLAYER_EXTRACTOR_TASK_NAME,
                                    &task_cfg.extractor);
    overlay_gmf_task_from_scheduler(service, stream_idx,
                                    ESP_PLAYER_SERVICE_PLAYER_AUDIO_DECODER_TASK_NAME,
                                    &task_cfg.audio_decoder);
    overlay_gmf_task_from_scheduler(service, stream_idx,
                                    ESP_PLAYER_SERVICE_PLAYER_AUDIO_RENDER_TASK_NAME,
                                    &task_cfg.audio_render);
    if (want_video) {
        overlay_gmf_task_from_scheduler(service, stream_idx,
                                        ESP_PLAYER_SERVICE_PLAYER_VIDEO_DECODER_TASK_NAME,
                                        &task_cfg.video_decoder);
        overlay_gmf_task_from_scheduler(service, stream_idx,
                                        ESP_PLAYER_SERVICE_PLAYER_VIDEO_RENDER_TASK_NAME,
                                        &task_cfg.video_render);
    }
    return ps_player_err_to_esp(esp_player_set_task_config(player, &task_cfg));
}

static player_stream_slot_t *find_slot_by_track(esp_player_service_t *service,
                                                uint16_t track_id)
{
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (slot->configured && slot->track_id == track_id) {
            return slot;
        }
    }
    return NULL;
}

static esp_err_t configure_track(esp_player_service_t *service, esp_media_stream_id_t stream,
                                 const esp_media_track_info_t *track)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL || track == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (track->type != ESP_MEDIA_TRACK_TYPE_AUDIO && track->type != ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return ESP_ERR_INVALID_ARG;
    }
    if (track->type == ESP_MEDIA_TRACK_TYPE_VIDEO && service->video_render == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* Busy only in active states; idle/finished player can be reconfigured. A type
       the slot has not declared yet is the exception: a live source may announce
       audio and video in separate messages, and refusing the second one would
       starve that decoder for the rest of the session. The next write rebuilds
       when the declared mask no longer matches the running feed session. */
    int track_slot = (track->type == ESP_MEDIA_TRACK_TYPE_AUDIO)
                         ? ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO
                         : ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO;
    esp_player_state_t st = ps_slot_get_play_state(slot);
    bool active = (st == ESP_PLAYER_STATE_PREPARING || st == ESP_PLAYER_STATE_PLAYING || st == ESP_PLAYER_STATE_PAUSED);
    bool late = active && !slot->feed_track_set[track_slot];
    if (slot->player != NULL && active && !late) {
        ESP_LOGW(TAG, "Configure track: stream %u busy (state=%d)", (unsigned)stream, (int)st);
        return ESP_ERR_INVALID_STATE;
    }
    /* After stop/FINISHED, the first set_track starts a new set (A/V then
       audio-only). Same-format re-feed without set_track keeps the previous set. */
    if (slot->feed_session &&
        (st == ESP_PLAYER_STATE_FINISHED || st == ESP_PLAYER_STATE_STOPPED ||
         st == ESP_PLAYER_STATE_IDLE || st == ESP_PLAYER_STATE_ERROR)) {
        slot->feed_decl_reset = true;
        slot->feed_session = false;
    }
    if (slot->feed_decl_reset) {
        memset(slot->feed_track_set, 0, sizeof(slot->feed_track_set));
        slot->configured = false;
        slot->feed_decl_reset = false;
    }
    if (track->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        slot->configured = true;
        slot->track_id = track->id;
        slot->codec = track->info.audio.codec;
        bool encoded = track->info.audio.codec != 0 && track->info.audio.codec != ESP_PLAYER_SERVICE_CODEC_PCM;
        if (!encoded) {
            slot->sample_info.sample_rate = track->info.audio.sample_rate != 0
                                                ? track->info.audio.sample_rate
                                                : service->out_fmt.sample_rate;
            slot->sample_info.bits_per_sample = track->info.audio.bits_per_sample != 0
                                                    ? track->info.audio.bits_per_sample
                                                    : service->out_fmt.bits_per_sample;
            slot->sample_info.channel = track->info.audio.channel != 0
                                            ? track->info.audio.channel
                                            : service->out_fmt.channel;
        } else {
            slot->sample_info.sample_rate = track->info.audio.sample_rate;
            slot->sample_info.bits_per_sample = track->info.audio.bits_per_sample;
            slot->sample_info.channel = track->info.audio.channel;
        }
        slot->feed_track[ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO] = *track;
        slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO] = true;
    } else {
        slot->configured = true;
        slot->feed_track[ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO] = *track;
        slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO] = true;
    }
    slot->feed_session = false;
    if (late) {
        ESP_LOGI(TAG, "Stream %u took a late %s track, restarting the feed session",
                 (unsigned)stream,
                 track->type == ESP_MEDIA_TRACK_TYPE_AUDIO ? "audio" : "video");
    }
    return ESP_OK;
}

static const char *codec_to_url_ext(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_FOURCC_AAC:
            return "aac";
        case ESP_FOURCC_MP3:
            return "mp3";
        case ESP_FOURCC_FLAC:
            return "flac";
        case ESP_FOURCC_OPUS:
            return "opus";
        case ESP_FOURCC_PCM:
            return "pcm";
        case ESP_FOURCC_ALAW:
            return "g711a";
        case ESP_FOURCC_ULAW:
            return "g711u";
        case ESP_FOURCC_ADPCM:
            return "adpcm";
        case ESP_FOURCC_SBC:
            return "sbc";
        case ESP_FOURCC_LC3:
            return "lc3";
        case ESP_FOURCC_ALAC:
            return "alac";
        case ESP_FOURCC_VORBIS:
            return "vorbis";
        case ESP_FOURCC_AMRNB:
            return "amr";
        case ESP_FOURCC_AMRWB:
            return "amrwb";
        case ESP_FOURCC_WAV:
            return "wav";
        default:
            return NULL;
    }
}

static bool feed_url_is_block(const char *url)
{
    return url != NULL && strncmp(url, "block://", 8) == 0;
}

static esp_err_t build_feed_url(const player_stream_slot_t *slot, bool block, char **out_url)
{
    const char *ext = codec_to_url_ext(slot->codec);
    if (ext == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const char *scheme = block ? "block" : "fill";
    const esp_player_service_pcm_fmt_t *si = &slot->sample_info;
    char *url = NULL;
    int n;
    if (si->sample_rate != 0 && si->channel != 0) {
        n = asprintf(&url, "%s:///feed.%s?sr=%u&ch=%u&bits=%u", scheme, ext,
                     (unsigned)si->sample_rate, (unsigned)si->channel,
                     (unsigned)si->bits_per_sample);
    } else {
        n = asprintf(&url, "%s:///feed.%s", scheme, ext);
    }
    if (n < 0 || url == NULL) {
        return ESP_ERR_NO_MEM;
    }
    *out_url = url;
    return ESP_OK;
}

static esp_player_service_event_type_t player_event_to_service(esp_player_event_type_t event)
{
    switch (event) {
        case ESP_PLAYER_EVENT_PLAYED:
            return ESP_PLAYER_SERVICE_EVENT_PLAYED;
        case ESP_PLAYER_EVENT_PAUSED:
            return ESP_PLAYER_SERVICE_EVENT_PAUSED;
        case ESP_PLAYER_EVENT_STOPPED:
            return ESP_PLAYER_SERVICE_EVENT_STOPPED;
        case ESP_PLAYER_EVENT_FINISHED:
            return ESP_PLAYER_SERVICE_EVENT_FINISHED;
        case ESP_PLAYER_EVENT_SEEK_DONE:
            return ESP_PLAYER_SERVICE_EVENT_SEEK_DONE;
        case ESP_PLAYER_EVENT_BUFFERING:
            return ESP_PLAYER_SERVICE_EVENT_BUFFERING;
        case ESP_PLAYER_EVENT_BUFFERED:
            return ESP_PLAYER_SERVICE_EVENT_BUFFERED;
        case ESP_PLAYER_EVENT_ERROR:
            return ESP_PLAYER_SERVICE_EVENT_ERROR;
        case ESP_PLAYER_EVENT_TRACK_INFO_PARSED:
            return ESP_PLAYER_SERVICE_EVENT_TRACK_INFO_PARSED;
        case ESP_PLAYER_EVENT_AUDIO_INFO_PARSED:
            return ESP_PLAYER_SERVICE_EVENT_AUDIO_INFO_PARSED;
        case ESP_PLAYER_EVENT_VIDEO_INFO_PARSED:
            return ESP_PLAYER_SERVICE_EVENT_VIDEO_INFO_PARSED;
        default:
            return ESP_PLAYER_SERVICE_EVENT_NONE;
    }
}

static esp_player_err_t player_event_bridge(esp_player_event_msg_t *msg, void *ctx)
{
    player_stream_slot_t *slot = (player_stream_slot_t *)ctx;
    if (slot == NULL || msg == NULL) {
        return ESP_PLAYER_ERR_OK;
    }
    esp_player_service_t *service = slot->service;
    esp_media_stream_id_t stream = ps_slot_stream(slot);
    esp_player_service_event_type_t type = player_event_to_service(msg->event_type);
    if (type == ESP_PLAYER_SERVICE_EVENT_NONE) {
        return ESP_PLAYER_ERR_OK;
    }
    ps_emit_service_event(service, stream, type, msg->data, msg->data_len);
    /* Defer pause/resume/run off the player SM thread onto the defer task. */
    bool wake_defer = false;
    if (type == ESP_PLAYER_SERVICE_EVENT_PLAYED ||
        type == ESP_PLAYER_SERVICE_EVENT_FINISHED ||
        type == ESP_PLAYER_SERVICE_EVENT_STOPPED ||
        type == ESP_PLAYER_SERVICE_EVENT_ERROR) {
        if (service != NULL) {
            service->arb_pending = true;
            wake_defer = true;
        }
    }
    if (type == ESP_PLAYER_SERVICE_EVENT_FINISHED && slot->playlist != NULL) {
        slot->auto_advance_pending = true;
        wake_defer = true;
    }
    if (wake_defer) {
        ps_defer_notify(service);
    }
    return ESP_PLAYER_ERR_OK;
}

static esp_err_t write_audio_frame(esp_player_service_t *service, esp_media_stream_id_t stream,
                                   const esp_media_frame_t *frame)
{
    if (service == NULL || frame == NULL || frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO ||
        frame->data == NULL || frame->size == 0 || stream >= service->max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = NULL;
    slot = &service->streams[stream];
    if (slot == NULL || !slot->configured || slot->track_id != frame->track_id) {
        slot = find_slot_by_track(service, frame->track_id);
    }
    if (slot == NULL || !slot->configured) {
        return ESP_ERR_NOT_FOUND;
    }
    if (slot->stop_task) {
        return ESP_ERR_INVALID_STATE;
    }
    /* DROP: discard frames while preempted; always pass EOS. */
    if (slot->mix.preempted && slot->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_DROP &&
        !(frame->flags & ESP_MEDIA_FRAME_FLAG_EOS)) {
        return ESP_OK;
    }
    esp_media_stream_id_t slot_stream = (esp_media_stream_id_t)(slot - service->streams);
    esp_err_t ret = ps_ensure_player(service, slot, slot_stream);
    if (ret != ESP_OK) {
        return ret;
    }
    bool block = (slot->mix.source_kind == PS_SRC_LINK);
    esp_player_state_t st = ps_slot_get_play_state(slot);
    bool active = (st == ESP_PLAYER_STATE_PLAYING || st == ESP_PLAYER_STATE_PREPARING || st == ESP_PLAYER_STATE_PAUSED);
    if (!active || (block != feed_url_is_block(slot->url))) {
        if (slot->stop_task) {
            return ESP_ERR_INVALID_STATE;
        }
        char *url = NULL;
        ret = build_feed_url(slot, block, &url);
        if (ret != ESP_OK) {
            return ret;
        }
        if (ps_state_needs_stop(st)) {
            esp_player_stop(slot->player);
        }
        ret = ps_player_err_to_esp(esp_player_set_url(slot->player, url));
        if (ret != ESP_OK) {
            free(url);
            return ret;
        }
        free(slot->url);
        slot->url = url;
        ret = ps_player_err_to_esp(esp_player_run(slot->player));
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (slot->stop_task) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_player_frame_t pf = {
        .data = frame->data,
        .data_len = (uint32_t)frame->size,
        .pts = frame->pts > 0 ? (uint64_t)frame->pts : 0,
        .eos = (frame->flags & ESP_MEDIA_FRAME_FLAG_EOS) != 0,
    };
    uint32_t timeout_ms = block ? 0 : ESP_PLAYER_SERVICE_FEED_TIMEOUT_MS;
    return ps_player_err_to_esp(esp_player_submit_frame(slot->player, &pf, timeout_ms));
}

static int feed_track_slot_from_media(esp_media_track_type_t type)
{
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO;
    }
    return ESP_PLAYER_SERVICE_FEED_TRACK_MAX;
}

static esp_err_t apply_feed_track_info(player_stream_slot_t *slot, int track_slot)
{
    const esp_media_track_info_t *track = &slot->feed_track[track_slot];
    esp_player_track_info_t info = {0};
    if (track_slot == ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO) {
        info.track_type = ESP_PLAYER_TRACK_TYPE_AUDIO;
        info.audio_info.format = track->info.audio.codec;
        info.audio_info.sample_rate = track->info.audio.sample_rate;
        info.audio_info.channels = track->info.audio.channel;
        info.audio_info.bits_per_sample = track->info.audio.bits_per_sample;
        info.audio_info.bitrate = track->info.audio.bitrate;
    } else {
        info.track_type = ESP_PLAYER_TRACK_TYPE_VIDEO;
        info.video_info.format = track->info.video.codec;
        info.video_info.width = track->info.video.width;
        info.video_info.height = track->info.video.height;
        info.video_info.fps = track->info.video.fps;
        info.video_info.bitrate = track->info.video.bitrate;
    }
    return ps_player_err_to_esp(esp_player_set_track_info(slot->player, &info));
}

static uint8_t ps_feed_session_mask(const player_stream_slot_t *slot)
{
    if (slot == NULL) {
        return 0;
    }
    uint8_t ceiling = ps_stream_av_mask(slot);
    uint8_t mask = 0;
    if (slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO] &&
        (ceiling & ESP_PLAYER_MASK_AUDIO) != 0) {
        mask |= ESP_PLAYER_MASK_AUDIO;
    }
    if (slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO] &&
        (ceiling & ESP_PLAYER_MASK_VIDEO) != 0) {
        mask |= ESP_PLAYER_MASK_VIDEO;
    }
    return mask;
}

static esp_err_t ensure_av_feed_session(esp_player_service_t *service,
                                        player_stream_slot_t *slot, esp_media_stream_id_t stream)
{
    if (slot->stop_task) {
        return ESP_ERR_INVALID_STATE;
    }
    bool want_block = ps_slot_linked(slot);
    uint8_t mask = ps_feed_session_mask(slot);
    if (mask == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    esp_player_state_t st = ps_slot_get_play_state(slot);
    bool active = (st == ESP_PLAYER_STATE_PLAYING || st == ESP_PLAYER_STATE_PREPARING || st == ESP_PLAYER_STATE_PAUSED);
    /* feed_session alone is not enough: an in-flight session start can set it
       true again after a late track cleared it. Rebuild when the declared
       tracks no longer match the running player, either order. */
    if (active && slot->feed_session && slot->feed_session_block == want_block &&
        slot->feed_session_mask == mask) {
        return ESP_OK;
    }
    esp_err_t ret = ps_ensure_player(service, slot, stream);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ps_state_needs_stop(st)) {
        esp_player_stop(slot->player);
    }
    ret = ps_apply_player_av_mask(slot, mask);
    if (ret != ESP_OK) {
        return ret;
    }
    const char *url = want_block ? "block:///" : "fill:///";
    ret = ps_player_err_to_esp(esp_player_set_url(slot->player, url));
    if (ret != ESP_OK) {
        return ret;
    }
    for (int i = 0; i < ESP_PLAYER_SERVICE_FEED_TRACK_MAX; i++) {
        uint8_t track_mask = (i == ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO) ? ESP_PLAYER_MASK_AUDIO
                                                                        : ESP_PLAYER_MASK_VIDEO;
        if (!slot->feed_track_set[i] || (mask & track_mask) == 0) {
            continue;
        }
        ret = apply_feed_track_info(slot, i);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (mask == ESP_PLAYER_MASK_AV) {
        ret = ps_player_err_to_esp(esp_player_set_sync_mode(slot->player, slot->sync_mode));
        if (ret != ESP_OK) {
            return ret;
        }
    }
    ret = ps_player_err_to_esp(esp_player_run(slot->player));
    if (ret != ESP_OK) {
        return ret;
    }
    slot->feed_session = true;
    slot->feed_session_block = want_block;
    slot->feed_session_mask = mask;
    return ESP_OK;
}

static esp_err_t write_av_frame(esp_player_service_t *service, esp_media_stream_id_t stream,
                                const esp_media_frame_t *frame)
{
    if (service == NULL || frame == NULL || frame->data == NULL || frame->size == 0 ||
        stream >= service->max_stream_num) {
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    int track_slot = feed_track_slot_from_media(frame->type);
    if (track_slot == ESP_PLAYER_SERVICE_FEED_TRACK_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t mask = ps_stream_av_mask(slot);
    uint8_t track_mask = (track_slot == ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO) ? ESP_PLAYER_MASK_AUDIO
                                                                             : ESP_PLAYER_MASK_VIDEO;
    if ((mask & track_mask) == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!slot->feed_track_set[track_slot]) {
        return ESP_ERR_NOT_FOUND;
    }
    if (slot->stop_task) {
        return ESP_ERR_INVALID_STATE;
    }
    if (slot->mix.preempted && slot->mix.on_preempt == ESP_PLAYER_ON_PREEMPT_DROP &&
        !(frame->flags & ESP_MEDIA_FRAME_FLAG_EOS) && frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        return ESP_OK;
    }
    if (slot->feed_lock != NULL) {
        xSemaphoreTake(slot->feed_lock, portMAX_DELAY);
    }
    esp_err_t ret = ensure_av_feed_session(service, slot, stream);
    if (slot->feed_lock != NULL) {
        xSemaphoreGive(slot->feed_lock);
    }
    if (ret != ESP_OK) {
        return ret;
    }
    if (slot->stop_task) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_player_frame_t pf = {
        .data = frame->data,
        .data_len = (uint32_t)frame->size,
        .pts = frame->pts > 0 ? (uint64_t)frame->pts : 0,
        .eos = (frame->flags & ESP_MEDIA_FRAME_FLAG_EOS) != 0,
        .track_type = (track_slot == ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO) ? ESP_PLAYER_TRACK_TYPE_AUDIO
                                                                          : ESP_PLAYER_TRACK_TYPE_VIDEO,
    };
    uint32_t timeout_ms = ps_slot_linked(slot) ? 0 : ESP_PLAYER_SERVICE_FEED_TIMEOUT_MS;
    return ps_player_err_to_esp(esp_player_submit_frame(slot->player, &pf, timeout_ms));
}

static esp_err_t feed_media_frame(esp_player_service_t *service, esp_media_stream_id_t stream,
                                  const esp_media_frame_t *frame)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Write frame failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    if (ps_has_video_out(slot)) {
        return write_av_frame(service, stream, frame);
    }
    if (frame->type != ESP_MEDIA_TRACK_TYPE_AUDIO) {
        ESP_LOGE(TAG, "Write frame failed: audio-only stream %u got non-audio frame", (unsigned)stream);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return write_audio_frame(service, stream, frame);
}

static esp_media_stream_id_t resolve_provider_track_stream(esp_player_service_t *service,
                                                           esp_media_stream_id_t provider_stream,
                                                           uint16_t track_id)
{
    player_stream_slot_t *target = find_slot_by_track(service, track_id);
    if (target == NULL) {
        return provider_stream;
    }
    esp_media_stream_id_t mapped = (esp_media_stream_id_t)(target - service->streams);
    if (mapped == provider_stream) {
        return mapped;
    }
    if (target->mix.source_kind == PS_SRC_NONE && !ps_slot_linked(target)) {
        return mapped;
    }
    return provider_stream;
}

static void configure_tracks_from_provider(esp_player_service_t *service,
                                           esp_media_stream_id_t stream)
{
    if (stream >= service->max_stream_num) {
        return;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    if (!ps_slot_linked(slot)) {
        return;
    }
    /* SRC owns the track set. Replace whatever feed leftover was on this slot. */
    memset(slot->feed_track_set, 0, sizeof(slot->feed_track_set));
    slot->configured = false;
    slot->feed_decl_reset = false;
    slot->feed_session = false;
    uint16_t track_num = 0;
    if (esp_media_provider_get_track_num(&slot->provider, &track_num) != ESP_OK) {
        return;
    }
    for (uint16_t i = 0; i < track_num; i++) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(&slot->provider, i, &info) != ESP_OK) {
            continue;
        }
        if (info.type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
            esp_media_stream_id_t audio_stream =
                resolve_provider_track_stream(service, stream, info.id);
            configure_track(service, audio_stream, &info);
            if (audio_stream != stream && !slot->configured) {
                configure_track(service, stream, &info);
            }
        } else if (info.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
            if (!ps_has_video_out(slot)) {
                continue;
            }
            configure_track(service, stream, &info);
        }
    }
}

static void player_provider_task(void *arg)
{
    player_provider_ctx_t *task_ctx = (player_provider_ctx_t *)arg;
    esp_player_service_t *service = task_ctx->service;
    player_stream_slot_t *slot = task_ctx->slot;
    esp_media_stream_id_t stream = task_ctx->stream;
    esp_media_track_type_t type = task_ctx->type;
    free(task_ctx);

    while (!slot->stop_task) {
        if (!ps_slot_linked(slot)) {
            break;
        }
        uint16_t track_id = slot->track_id;
        if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
            track_id = slot->feed_track[ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO].id;
        }
        esp_media_frame_t frame = {
            .track_id = track_id,
            .type = type,
        };
        esp_err_t ret = esp_media_provider_acquire_frame(&slot->provider, &frame,
                                                         ESP_PLAYER_SERVICE_ACQUIRE_FRAME_TIMEOUT_MS);
        if (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_NOT_FOUND) {
            if (slot->stop_task) {
                break;
            }
            continue;
        }
        if (ret == ESP_ERR_INVALID_STATE) {
            break;
        }
        if (ret != ESP_OK) {
            continue;
        }
        if ((frame.flags & ESP_MEDIA_FRAME_FLAG_TRACK_REMOVED) != 0) {
            esp_media_provider_release_frame(&slot->provider, &frame);
            break;
        }
        if (frame.type != type) {
            esp_media_provider_release_frame(&slot->provider, &frame);
            continue;
        }
        if (type == ESP_MEDIA_TRACK_TYPE_AUDIO && slot->track_id != 0 && frame.track_id != slot->track_id) {
            esp_media_provider_release_frame(&slot->provider, &frame);
            continue;
        }
        if (slot->stop_task) {
            esp_media_provider_release_frame(&slot->provider, &frame);
            break;
        }
        if ((frame.flags & ESP_MEDIA_FRAME_FLAG_TRACK_CHANGED) == 0 &&
            frame.data != NULL && frame.size != 0) {
            ret = feed_media_frame(service, stream, &frame);
            if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED && !slot->stop_task) {
                ESP_LOGW(TAG, "Failed to feed %s frame: %s",
                         type == ESP_MEDIA_TRACK_TYPE_AUDIO ? "audio" : "video", esp_err_to_name(ret));
            }
        }
        esp_media_provider_release_frame(&slot->provider, &frame);
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        slot->video_provider_task = NULL;
    } else {
        slot->provider_task = NULL;
    }
    esp_gmf_oal_thread_delete(NULL);
}

static esp_err_t start_one_provider_task(esp_player_service_t *service, player_stream_slot_t *slot,
                                         esp_media_stream_id_t stream, esp_media_track_type_t type)
{
    esp_gmf_oal_thread_t *task_slot = (type == ESP_MEDIA_TRACK_TYPE_VIDEO)
                                          ? &slot->video_provider_task
                                          : &slot->provider_task;
    if (*task_slot != NULL) {
        return ESP_OK;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_VIDEO && !slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_VIDEO]) {
        return ESP_OK;
    }
    if (type == ESP_MEDIA_TRACK_TYPE_AUDIO && !slot->feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_AUDIO]) {
        return ESP_OK;
    }
    player_provider_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->service = service;
    ctx->slot = slot;
    ctx->stream = stream;
    ctx->type = type;

    const char *name = (type == ESP_MEDIA_TRACK_TYPE_VIDEO)
                           ? ESP_PLAYER_SERVICE_VIDEO_READER_TASK_NAME
                           : ESP_PLAYER_SERVICE_READER_TASK_NAME;
    esp_service_thread_cfg_t default_thread_cfg = {
        .stack_size = ESP_PLAYER_SERVICE_DEFAULT_TASK_STACK_SIZE,
        .priority = ESP_PLAYER_SERVICE_DEFAULT_TASK_PRIORITY,
        .core_id = ESP_PLAYER_SERVICE_DEFAULT_TASK_CORE_ID,
        .is_ext = true,
    };
    esp_service_thread_cfg_t thread_cfg = default_thread_cfg;
    (void)get_thread_cfg(service, (uint16_t)stream, name, &default_thread_cfg, &thread_cfg);

    esp_gmf_err_t gret = esp_gmf_oal_thread_create(task_slot, name, player_provider_task, ctx,
                                                   thread_cfg.stack_size, thread_cfg.priority,
                                                   thread_cfg.is_ext,
                                                   (thread_cfg.core_id >= 0) ? thread_cfg.core_id : tskNO_AFFINITY);
    if (gret != ESP_GMF_ERR_OK) {
        *task_slot = NULL;
        free(ctx);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t start_slot_provider_task(esp_player_service_t *service,
                                          player_stream_slot_t *slot, esp_media_stream_id_t stream)
{
    if (!service->configured || !ps_slot_linked(slot) || !slot->configured) {
        return ESP_OK;
    }
    slot->stop_task = false;
    esp_err_t ret = start_one_provider_task(service, slot, stream, ESP_MEDIA_TRACK_TYPE_AUDIO);
    if (ret != ESP_OK) {
        return ret;
    }
    if (ps_has_video_out(slot)) {
        ret = start_one_provider_task(service, slot, stream, ESP_MEDIA_TRACK_TYPE_VIDEO);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static void wake_slot_player(player_stream_slot_t *slot)
{
    if (slot->player != NULL && ps_state_needs_stop(ps_slot_get_play_state(slot))) {
        (void)esp_player_stop(slot->player);
    }
}

static bool ps_slot_provider_running(const player_stream_slot_t *slot)
{
    return slot->provider_task != NULL || slot->video_provider_task != NULL;
}

static void ps_wait_slot_provider_join(const player_stream_slot_t *slot, unsigned stream)
{
    unsigned polls = 0;
    while (ps_slot_provider_running(slot)) {
        vTaskDelay(pdMS_TO_TICKS(ESP_PLAYER_SERVICE_TASK_JOIN_POLL_MS));
        polls++;
        if ((polls % ESP_PLAYER_SERVICE_TASK_JOIN_WARN_EVERY) == 0) {
            ESP_LOGW(TAG, "Still waiting for provider bridge join on stream %u", stream);
        }
    }
}

static void stop_slot_provider_task(player_stream_slot_t *slot, unsigned stream)
{
    slot->stop_task = true;
    if (ps_slot_linked(slot)) {
        esp_media_provider_abort(&slot->provider);
    }
    wake_slot_player(slot);
    ps_wait_slot_provider_join(slot, stream);
}

static esp_err_t start_provider_tasks(esp_player_service_t *service)
{
    if (!service->configured) {
        return ESP_OK;
    }
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (!ps_slot_linked(slot) || !slot->configured) {
            continue;
        }
        esp_err_t ret = start_slot_provider_task(service, slot, i);
        if (ret != ESP_OK) {
            for (int j = (int)i; j >= 0; j--) {
                player_stream_slot_t *started = &service->streams[j];
                if (!ps_slot_provider_running(started)) {
                    continue;
                }
                stop_slot_provider_task(started, (unsigned)j);
            }
            return ret;
        }
    }
    return ESP_OK;
}

static void handle_provider_track_added(esp_player_service_t *service,
                                        player_stream_slot_t *slot, esp_media_stream_id_t stream,
                                        const esp_media_track_info_t *info)
{
    if (service == NULL || slot == NULL || info == NULL || !ps_slot_linked(slot)) {
        return;
    }
    if (info->type != ESP_MEDIA_TRACK_TYPE_AUDIO && info->type != ESP_MEDIA_TRACK_TYPE_VIDEO) {
        return;
    }
    if (info->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        esp_media_stream_id_t target_stream =
            resolve_provider_track_stream(service, stream, info->id);
        configure_track(service, target_stream, info);
        if (target_stream != stream && !slot->configured) {
            configure_track(service, stream, info);
        }
    } else {
        configure_track(service, stream, info);
    }
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
        state == ESP_SERVICE_STATE_RUNNING) {
        start_slot_provider_task(service, slot, stream);
    }
}

static void audio_provider_event_handler(esp_media_provider_event_t event,
                                         const esp_media_track_info_t *info, void *ctx)
{
    player_stream_slot_t *slot = (player_stream_slot_t *)ctx;
    if (slot == NULL) {
        return;
    }
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT) {
        slot->stop_task = true;
        return;
    }
    /* Stop only for this stream's track (or unknown id). */
    if (event == ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED) {
        if (info == NULL || slot->track_id == 0 || info->id == slot->track_id) {
            slot->stop_task = true;
        }
        return;
    }
    if (info == NULL) {
        return;
    }
    if (event != ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED && event != ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED) {
        return;
    }
    handle_provider_track_added(slot->service, slot, ps_slot_stream(slot), info);
}

static esp_err_t media_get_role(esp_service_t *service, esp_media_role_t *out_role)
{
    esp_player_service_t *render_service = (esp_player_service_t *)service;
    if (out_role == NULL || render_service == NULL) {
        ESP_LOGE(TAG, "Get role failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    if (!render_service->configured) {
        ESP_LOGE(TAG, "Get role failed: service is not configured");
        return ESP_ERR_INVALID_STATE;
    }
    *out_role = ESP_MEDIA_ROLE_SINK;
    return ESP_OK;
}

static esp_err_t media_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                    const esp_media_provider_t *provider)
{
    esp_player_service_t *service = (esp_player_service_t *)base;
    if (service == NULL || (provider != NULL && provider->ops == NULL) ||
        stream >= service->max_stream_num) {
        ESP_LOGE(TAG, "Set provider failed: invalid argument on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = &service->streams[stream];
    if (provider == NULL) {
        if (ps_slot_linked(slot)) {
            stop_slot_provider_task(slot, (unsigned)stream);
            esp_media_provider_set_event_cb(&slot->provider, NULL, NULL);
        }
        slot->provider.ops = NULL;
        slot->provider.ctx = NULL;
        slot->stop_task = false;
        if (slot->mix.source_kind == PS_SRC_LINK) {
            slot->mix.source_kind = PS_SRC_NONE;
        }
        return ESP_OK;
    }
    if (ps_slot_linked(slot)) {
        stop_slot_provider_task(slot, (unsigned)stream);
        esp_media_provider_set_event_cb(&slot->provider, NULL, NULL);
    }
    slot->provider = *provider;
    slot->mix.source_kind = PS_SRC_LINK;
    if (ps_validate_preempt_source(slot) != ESP_OK) {
        slot->provider.ops = NULL;
        slot->provider.ctx = NULL;
        slot->mix.source_kind = PS_SRC_NONE;
        ESP_LOGE(TAG, "Set provider failed: invalid preempt policy on stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    configure_tracks_from_provider(service, stream);
    esp_media_provider_set_event_cb(&slot->provider, audio_provider_event_handler, slot);

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
        state == ESP_SERVICE_STATE_RUNNING) {
        return start_slot_provider_task(service, slot, stream);
    }
    return ESP_OK;
}

static esp_err_t audio_player_service_on_start(esp_service_t *base)
{
    /* Arm provider bridges; URL/feed players start lazily. */
    esp_player_service_t *service = (esp_player_service_t *)base;
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        configure_tracks_from_provider(service, i);
    }
    return start_provider_tasks(service);
}

static esp_err_t audio_player_service_on_stop(esp_service_t *base)
{
    esp_player_service_t *service = (esp_player_service_t *)base;
    return ps_stop_runtime(service);
}

static esp_err_t audio_player_service_on_deinit(esp_service_t *base)
{
    esp_player_service_t *service = (esp_player_service_t *)base;

    ps_defer_stop(service);

    /* Drop provider cbs before freeing slots (unlink may be skipped). */
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (ps_slot_linked(slot)) {
            esp_media_provider_set_event_cb(&slot->provider, NULL, NULL);
        }
    }
    (void)ps_stop_runtime(service);
    if (service->streams != NULL) {
        for (uint8_t i = 0; i < service->max_stream_num; i++) {
            if (service->streams[i].feed_lock != NULL) {
                vSemaphoreDelete(service->streams[i].feed_lock);
                service->streams[i].feed_lock = NULL;
            }
        }
    }
    player_out_audio_destroy(&service->out);
    if (service->deinit_cb != NULL) {
        (void)service->deinit_cb(service, service->deinit_ctx);
        service->deinit_cb = NULL;
        service->deinit_ctx = NULL;
    }
    service->video_render = NULL;
    free(service->streams);
    service->streams = NULL;

    if (service->pool_owned) {
        ps_clear_default_pool(&service->pool, &service->pool_owned);
    }
    return ESP_OK;
}

static esp_err_t ps_ensure_audio_slot(esp_player_service_t *service,
                                      player_stream_slot_t *slot)
{
    /* AUDIO streams only. VIDEO-only leaves slot->audio_slot NULL (no mixer). */
    if (service == NULL || slot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ps_open_audio_out(service);
    if (ret != ESP_OK) {
        return ret;
    }
    ps_apply_pending_mix_gains(service);
    uint8_t stream_idx = (uint8_t)(slot - service->streams);
    ret = player_out_slot_acquire(service->out, stream_idx, &slot->audio_slot);
    if (ret != ESP_OK) {
        slot->audio_slot = NULL;
        return ret;
    }
    (void)ps_apply_stream_volume(slot);
    return ESP_OK;
}

static const esp_media_service_ops_t s_media_ops = {
    .get_role     = media_get_role,
    .set_provider = media_set_provider,
};

static const esp_service_ops_t s_service_ops = {
    .on_deinit = audio_player_service_on_deinit,
    .on_start  = audio_player_service_on_start,
    .on_stop   = audio_player_service_on_stop,
};

uint8_t ps_stream_av_mask(const player_stream_slot_t *slot)
{
    if (slot == NULL || slot->service == NULL) {
        return 0;
    }
    /* Runtime capability, not the build option alone: a deferred output has no
       sink yet, and claiming AUDIO would make the mixer open fail on a build
       that only ever wanted silent video. */
    uint8_t mask = (PLAYER_OUT_AUDIO_SUPPORTED && player_out_audio_has_sink(slot->service->out))
                       ? ESP_PLAYER_MASK_AUDIO
                       : 0;
    if (ps_has_video_out(slot)) {
        mask |= ESP_PLAYER_MASK_VIDEO;
    }
    return mask;
}

esp_err_t ps_apply_player_av_mask(player_stream_slot_t *slot, uint8_t mask)
{
    if (slot == NULL || slot->player == NULL || mask == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return ps_player_err_to_esp(esp_player_set_av_mask(slot->player, mask));
}

player_stream_slot_t *ps_find_slot_by_stream(esp_player_service_t *service,
                                             esp_media_stream_id_t stream)
{
    if (service == NULL || stream >= service->max_stream_num) {
        return NULL;
    }
    return &service->streams[stream];
}

esp_err_t ps_player_err_to_esp(esp_player_err_t err)
{
    switch (err) {
        case ESP_PLAYER_ERR_OK:
            return ESP_OK;
        case ESP_PLAYER_ERR_INVALID_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_PLAYER_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ESP_PLAYER_ERR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case ESP_PLAYER_ERR_NOT_SUPPORT:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_PLAYER_ERR_INVALID_STATE:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_FAIL;
    }
}

void ps_emit_service_event(esp_player_service_t *service, esp_media_stream_id_t stream,
                           esp_player_service_event_type_t type, void *data, uint32_t data_len)
{
    if (service == NULL || service->event_cb == NULL ||
        type == ESP_PLAYER_SERVICE_EVENT_NONE) {
        return;
    }
    esp_player_service_event_msg_t out = {
        .stream = stream,
        .type = type,
        .data = data,
        .data_len = data_len,
    };
    service->event_cb(&out, service->event_ctx);
}

esp_err_t ps_apply_stream_volume(player_stream_slot_t *slot)
{
    if (slot == NULL || slot->service == NULL || slot->audio_slot == NULL) {
        return ESP_OK;
    }
    return player_out_slot_set_volume(slot->service->out, (uint8_t)ps_slot_stream(slot),
                                      slot->volume, slot->sample_info.channel);
}

esp_err_t ps_ensure_player(esp_player_service_t *service, player_stream_slot_t *slot,
                           esp_media_stream_id_t stream)
{
    if (slot->player != NULL) {
        return ESP_OK;
    }
    uint8_t mask = ps_stream_av_mask(slot);
    if (mask == 0) {
        ESP_LOGE(TAG, "No player on stream %u: neither an audio sink nor a video render is installed",
                 (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    if ((mask & ESP_PLAYER_MASK_AUDIO) != 0) {
        esp_err_t ret = ps_ensure_audio_slot(service, slot);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    esp_player_config_t cfg = ESP_PLAYER_CONFIG_DEFAULT();
    if (mask & ESP_PLAYER_MASK_AUDIO) {
        cfg.audio_render_hd = slot->audio_slot;
    }
    if (mask & ESP_PLAYER_MASK_VIDEO) {
        cfg.video_render_hd = service->video_render;
    }
    if (esp_player_init(&cfg, &slot->player) != ESP_PLAYER_ERR_OK || slot->player == NULL) {
        slot->player = NULL;
        return ESP_FAIL;
    }
    esp_err_t ret = ps_player_err_to_esp(esp_player_set_av_mask(slot->player, mask));
    if (ret != ESP_OK) {
        esp_player_deinit(slot->player);
        slot->player = NULL;
        return ret;
    }
    if (mask == ESP_PLAYER_MASK_AV) {
        ret = ps_player_err_to_esp(esp_player_set_sync_mode(slot->player, slot->sync_mode));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set sync mode: %s", esp_err_to_name(ret));
        }
    }
    if (slot->buffer_cfg_set && (mask & ESP_PLAYER_MASK_VIDEO) != 0) {
        ret = ps_player_err_to_esp(esp_player_set_buffer_config(slot->player, &slot->buffer_cfg));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set buffer config: %s", esp_err_to_name(ret));
        }
    }
    ret = apply_player_scheduler_tasks(service, stream, slot->player);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to apply player scheduler tasks: %s", esp_err_to_name(ret));
    }
    if (slot->id3_parse_enable) {
        esp_player_err_t id3_ret = esp_player_enable_id3_parse(slot->player, true);
        if (id3_ret != ESP_PLAYER_ERR_OK) {
            ESP_LOGW(TAG, "Failed to enable ID3 parse: %d", (int)id3_ret);
        }
    }
    esp_player_set_event_cb(slot->player, player_event_bridge, slot);
    return ESP_OK;
}

/* Built on demand: without a playlist or a mix policy nothing is ever deferred,
   so the plain single-stream player pays neither the task nor its stack. */
esp_err_t ps_ensure_defer(esp_player_service_t *service)
{
    if (service->defer.task != NULL) {
        return ESP_OK;
    }
    service->defer.wake = xSemaphoreCreateBinary();
    if (service->defer.wake == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_service_thread_cfg_t default_thread_cfg = {
        .stack_size = ESP_PLAYER_SERVICE_DEFAULT_DEFER_TASK_STACK,
        .priority = ESP_PLAYER_SERVICE_DEFAULT_DEFER_TASK_PRIO,
        .core_id = ESP_PLAYER_SERVICE_DEFAULT_DEFER_TASK_CORE,
        .is_ext = true,
    };
    esp_service_thread_cfg_t thread_cfg = default_thread_cfg;
    (void)get_thread_cfg(service, 0, ESP_PLAYER_SERVICE_DEFER_TASK_NAME,
                         &default_thread_cfg, &thread_cfg);
    esp_gmf_err_t gret = esp_gmf_oal_thread_create(&service->defer.task,
                                                   ESP_PLAYER_SERVICE_DEFER_TASK_NAME,
                                                   ps_defer_task, service,
                                                   thread_cfg.stack_size, thread_cfg.priority,
                                                   thread_cfg.is_ext,
                                                   (thread_cfg.core_id >= 0) ? thread_cfg.core_id : tskNO_AFFINITY);
    if (gret != ESP_GMF_ERR_OK) {
        service->defer.task = NULL;
        vSemaphoreDelete(service->defer.wake);
        service->defer.wake = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t ps_open_audio_out(esp_player_service_t *service)
{
    if (service->out == NULL) {
        ESP_LOGE(TAG, "Codec device or custom writer is required to create audio render");
        return ESP_ERR_INVALID_STATE;
    }
    if (player_out_audio_is_open(service->out)) {
        return ESP_OK;
    }
    /* The mixer task is created here, so read the scheduler now: a stop / start
       cycle must pick up a scheduler callback registered in between. */
    apply_out_task_cfg(service);
    return player_out_audio_open(service->out);
}

esp_err_t ps_stop_runtime(esp_player_service_t *service)
{
    /* No deferred work may run while the players below are being destroyed. */
    ps_defer_quiesce(service);
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        service->streams[i].stop_task = true;
    }
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        if (ps_slot_linked(slot)) {
            esp_media_provider_abort(&slot->provider);
        }
        wake_slot_player(slot);
    }
    for (uint8_t i = 0; service->streams != NULL && i < service->max_stream_num; i++) {
        player_stream_slot_t *slot = &service->streams[i];
        ps_wait_slot_provider_join(slot, i);
        if (slot->player != NULL) {
            esp_player_handle_t player = slot->player;
            esp_player_set_event_cb(player, NULL, NULL);
            slot->player = NULL;
            esp_player_stop(player);
            esp_player_deinit(player);
        }
        slot->audio_slot = NULL;
        player_out_slot_release(service->out, i);
        slot->feed_session = false;
        slot->feed_decl_reset = false;
        free(slot->url);
        slot->url = NULL;
        slot->stop_task = false;
        slot->auto_advance_pending = false;
        slot->configured = false;
        memset(slot->feed_track_set, 0, sizeof(slot->feed_track_set));
        /* Keep mix cfg/volume; reset runtime mix flags for next render. */
        slot->mix.gain_applied = false;
        slot->mix.fade_state = -1;
        slot->mix.preempted = false;
        slot->mix.preempt_paused = false;
    }
    service->arb_pending = false;
    player_out_audio_close(service->out);
    /* Players are gone, so deferred work is harmless again. */
    if (service->defer.state == PS_DEFER_DROP) {
        service->defer.state = PS_DEFER_RUN;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_create(const esp_player_service_cfg_t *cfg,
                                    esp_player_service_t **out_service)
{
    if (cfg == NULL || out_service == NULL || cfg->max_stream_num == 0) {
        ESP_LOGE(TAG, "Create failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_player_service_t *service = calloc(1, sizeof(*service));
    if (service == NULL) {
        ESP_LOGE(TAG, "Create failed: alloc service");
        return ESP_ERR_NO_MEM;
    }
    service->max_stream_num = cfg->max_stream_num;
    service->pool = cfg->pool;
    esp_err_t ret = ps_try_default_pool(&service->pool, &service->pool_owned);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: default pool: %s", esp_err_to_name(ret));
        free(service);
        return ret;
    }

    service->streams = calloc(service->max_stream_num, sizeof(player_stream_slot_t));
    if (service->streams == NULL) {
        ESP_LOGE(TAG, "Create failed: alloc streams");
        ps_clear_default_pool(&service->pool, &service->pool_owned);
        free(service);
        return ESP_ERR_NO_MEM;
    }
    service->configured = false;
    service->output_volume = ESP_PLAYER_SERVICE_DEFAULT_OUTPUT_VOLUME;
    for (uint8_t i = 0; i < service->max_stream_num; i++) {
        service->streams[i].service = service;
        service->streams[i].volume = 100;
        service->streams[i].mix.fade_state = -1;
        service->streams[i].sync_mode = ESP_PLAYER_SYNC_MODE_AUDIO;
        service->streams[i].feed_lock = xSemaphoreCreateMutex();
        if (service->streams[i].feed_lock == NULL) {
            for (uint8_t j = 0; j < i; j++) {
                vSemaphoreDelete(service->streams[j].feed_lock);
            }
            free(service->streams);
            service->streams = NULL;
            ps_clear_default_pool(&service->pool, &service->pool_owned);
            free(service);
            ESP_LOGE(TAG, "Create failed: alloc feed lock");
            return ESP_ERR_NO_MEM;
        }
    }

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = cfg->name != NULL ? cfg->name : ESP_PLAYER_SERVICE_DEFAULT_NAME;
    media_cfg.user_data = service;
    media_cfg.service_ops = &s_service_ops;
    media_cfg.media_ops = &s_media_ops;
    ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        audio_player_service_on_deinit((esp_service_t *)service);
        if (service->streams != NULL) {
            free(service->streams);
            service->streams = NULL;
        }
        if (service->pool_owned) {
            ps_clear_default_pool(&service->pool, &service->pool_owned);
        }
        free(service);
        ESP_LOGE(TAG, "Create failed: media service init: %s", esp_err_to_name(ret));
        return ret;
    }
    *out_service = service;
    ESP_LOGI(TAG, "Create '%s': player service ready, streams=%u",
             media_cfg.name, (unsigned)service->max_stream_num);
    return ESP_OK;
}

esp_err_t esp_player_service_destroy(esp_player_service_t *service)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Destroy failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    const char *name = service->media.base.name != NULL
                           ? service->media.base.name
                           : ESP_PLAYER_SERVICE_DEFAULT_NAME;
    ESP_LOGI(TAG, "Destroy '%s': player service releasing", name);
    (void)esp_media_service_deinit(ESP_SERVICE_BASE(service));
    if (service->streams != NULL) {
        for (uint8_t i = 0; i < service->max_stream_num; i++) {
            ps_wait_slot_provider_join(&service->streams[i], i);
        }
        for (uint8_t i = 0; i < service->max_stream_num; i++) {
            if (service->streams[i].feed_lock != NULL) {
                vSemaphoreDelete(service->streams[i].feed_lock);
                service->streams[i].feed_lock = NULL;
            }
        }
        free(service->streams);
        service->streams = NULL;
        if (service->pool_owned) {
            ps_clear_default_pool(&service->pool, &service->pool_owned);
        }
    }
    free(service);
    return ESP_OK;
}

esp_err_t esp_player_service_get_info(esp_player_service_t *service,
                                      esp_player_service_info_t *out_info)
{
    if (service == NULL || out_info == NULL) {
        ESP_LOGE(TAG, "Get info failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_info, 0, sizeof(*out_info));
    out_info->codec_dev = player_out_get_codec_dev(service->out);
    out_info->audio_render = player_out_get_render(service->out);
    out_info->video_render = service->video_render;
    out_info->max_stream_num = service->max_stream_num;
    return ESP_OK;
}

esp_err_t esp_player_service_get_stream_info(esp_player_service_t *service,
                                             esp_media_stream_id_t stream,
                                             esp_player_service_stream_info_t *out_info)
{
    if (service == NULL || out_info == NULL) {
        ESP_LOGE(TAG, "Get stream info failed: invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Get stream info failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    memset(out_info, 0, sizeof(*out_info));
    out_info->mixer_stream = slot->audio_slot;
    out_info->player = slot->player;
    return ESP_OK;
}

void *esp_player_service_get_pool(esp_player_service_t *service)
{
    if (service == NULL) {
        return NULL;
    }
    return service->pool;
}

esp_err_t esp_player_service_set_video_render(esp_player_service_t *service, void *render)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set video render failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(ESP_SERVICE_BASE(service), &state) != ESP_OK ||
        state != ESP_SERVICE_STATE_INITIALIZED) {
        ESP_LOGE(TAG, "Set video render failed: service is not INITIALIZED");
        return ESP_ERR_INVALID_STATE;
    }
    service->video_render = render;
    return ESP_OK;
}

esp_err_t esp_player_service_set_deinit_cb(esp_player_service_t *service,
                                           esp_player_service_deinit_cb_t deinit_cb,
                                           void *user_data)
{
    if (service == NULL) {
        ESP_LOGE(TAG, "Set deinit callback failed: service is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    service->deinit_cb = deinit_cb;
    service->deinit_ctx = user_data;
    return ESP_OK;
}

esp_err_t esp_player_service_set_track(esp_player_service_t *service,
                                       esp_media_stream_id_t stream,
                                       const esp_media_track_info_t *track)
{
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot == NULL) {
        ESP_LOGE(TAG, "Set track failed: invalid stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_ARG;
    }
    player_source_kind_t old_source_kind = slot->mix.source_kind;
    slot->mix.source_kind = PS_SRC_FEED;
    esp_err_t ret = ps_validate_preempt_source(slot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set track failed: invalid preempt policy on stream %u", (unsigned)stream);
        slot->mix.source_kind = old_source_kind;
        return ret;
    }
    ret = configure_track(service, stream, track);
    if (ret != ESP_OK) {
        slot->mix.source_kind = old_source_kind;
        /* configure_track logs busy state; log other failures here */
        if (ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Set track failed on stream %u: %s", (unsigned)stream, esp_err_to_name(ret));
        }
        return ret;
    }
    return ESP_OK;
}

esp_err_t esp_player_service_write_frame(esp_player_service_t *service,
                                         esp_media_stream_id_t stream,
                                         const esp_media_frame_t *frame)
{
    if (service == NULL || frame == NULL) {
        ESP_LOGE(TAG, "Write frame failed: service or frame is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    /* A linked provider owns the feed path. */
    player_stream_slot_t *slot = ps_find_slot_by_stream(service, stream);
    if (slot != NULL && ps_slot_linked(slot)) {
        ESP_LOGE(TAG, "Write frame failed: linked provider owns stream %u", (unsigned)stream);
        return ESP_ERR_INVALID_STATE;
    }
    return feed_media_frame(service, stream, frame);
}
