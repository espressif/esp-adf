/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_err.h"

#include "esp_extractor_id3_parser.h"
#include "esp_gmf_oal_thread.h"
#include "esp_media_provider.h"
#include "esp_player.h"
#include "esp_player_advance.h"
#include "esp_playlist.h"

#include "esp_player_service.h"
#include "esp_player_service_playback.h"
#include "esp_player_service_setup.h"
#include "esp_player_service_defaults.h"
#include "internal/esp_player_service_subclass.h"
#include "player_out.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef enum {
    PS_SRC_NONE = 0,  /*!< No input selected yet */
    PS_SRC_URL  = 1,  /*!< URL set through set_url() */
    PS_SRC_FEED = 2,  /*!< Application pushes frames with write_frame() */
    PS_SRC_LINK = 3,  /*!< Frames pulled from a linked SRC service */
} player_source_kind_t;

typedef enum {
    PS_MIX_FOREGROUND = 0,  /*!< Plays at its active gain */
    PS_MIX_DUCKED     = 1,  /*!< Plays at its duck gain */
    PS_MIX_SUPPRESSED = 2,  /*!< Made way for an EXCLUSIVE stream */
} player_mix_state_t;

typedef enum {
    PS_DEFER_RUN  = 0,  /*!< Run the pending work on the next wake */
    PS_DEFER_DROP = 1,  /*!< Skip it: a caller is tearing players down */
    PS_DEFER_EXIT = 2,  /*!< Leave the task */
} player_defer_state_t;

/**
 * @brief  Deferred work runner
 *
 *         Playlist auto-advance and mix preemption must not run on the player
 *         state-machine thread, so events only raise the pending flags on the
 *         service / slots and wake this task.
 */
typedef struct {
    esp_gmf_oal_thread_t  task;   /*!< Worker thread; NULL once it has left */
    SemaphoreHandle_t     wake;   /*!< Binary semaphore; extra wakes coalesce */
    player_defer_state_t  state;  /*!< What the worker does on the next wake */
    bool                  busy;   /*!< True while the worker runs one batch */
} player_defer_t;

/**
 * @brief  Per-stream mix and preemption runtime state
 */
typedef struct {
    bool                       cfg_set;         /*!< True after set_mix_cfg() */
    bool                       gain_applied;    /*!< True after mixer gain is programmed */
    esp_player_priority_t      priority;        /*!< Static mix priority */
    esp_player_preempt_mode_t  preempt_mode;    /*!< Inserter suppression policy */
    esp_player_on_preempt_t    on_preempt;      /*!< Victim timeline policy */
    player_source_kind_t       source_kind;     /*!< Current input kind */
    bool                       preempted;       /*!< True while suppressed by a higher stream */
    bool                       preempt_paused;  /*!< True if exclusively paused */
    int8_t                     fade_state;      /*!< Last fade direction; -1 forces apply */
    player_out_mixer_gain_t    gain;            /*!< Mixer gain parameters */
} player_mix_slot_t;

/**
 * @brief  One stream slot. Public id is esp_media_stream_id_t
 */
typedef struct player_stream_slot {
    esp_player_service_t         *service;                                            /*!< Back-pointer; set at create */
    bool                          configured;                                         /*!< True after a feed/link track is declared */
    uint16_t                      track_id;                                           /*!< Last audio track id */
    esp_media_codec_fourcc_t      codec;                                              /*!< Audio codec fourcc */
    esp_player_service_pcm_fmt_t  sample_info;                                        /*!< PCM format for mixer / feed URL */
    void                         *audio_slot;                                         /*!< Mixer slot from player_out; NULL until audio out exists */
    bool                          stop_task;                                          /*!< Provider task stop flag */
    esp_gmf_oal_thread_t          provider_task;                                      /*!< Linked-source audio reader */
    esp_gmf_oal_thread_t          video_provider_task;                                /*!< Linked-source video reader */
    esp_media_provider_t          provider;                                           /*!< Linked provider copy; ops NULL if unused */
    esp_player_handle_t           player;                                             /*!< Per-stream esp_player handle */
    char                         *url;                                                /*!< Duplicated URL; NULL if unused */
    esp_playlist_handle_t         playlist;                                           /*!< Caller-owned playlist; NULL if unused */
    bool                          auto_advance_pending;                               /*!< Defer task should advance this slot */
    uint8_t                       volume;                                             /*!< Stream ALC volume 0-100 */
    bool                          id3_parse_enable;                                   /*!< Stored ID3 parse preference */
    player_mix_slot_t             mix;                                                /*!< Mix / preemption state */
    esp_media_track_info_t        feed_track[ESP_PLAYER_SERVICE_FEED_TRACK_MAX];      /*!< Declared feed tracks */
    bool                          feed_track_set[ESP_PLAYER_SERVICE_FEED_TRACK_MAX];  /*!< Track set flags */
    bool                          feed_session;                                       /*!< fill:// or block:// currently running */
    bool                          feed_session_block;                                 /*!< True when the feed URL uses block:// */
    uint8_t                       feed_session_mask;                                  /*!< av_mask last applied to the running feed session */
    bool                          feed_decl_reset;                                    /*!< Next set_track starts a new track set */
    SemaphoreHandle_t             feed_lock;                                          /*!< Serializes feed session start */
    esp_player_sync_mode_t        sync_mode;                                          /*!< A/V clock for this slot's player */
    esp_player_buffer_config_t    buffer_cfg;                                         /*!< Optional buffer override for this player */
    bool                          buffer_cfg_set;                                     /*!< True when buffer_cfg is valid */
} player_stream_slot_t;

/**
 * @brief  Player service object (media base must remain first)
 *
 *         Shared audio mixer / DAC sit on the service (one speaker). Each
 *         stream is a slot with its own esp_player. Same shape as capture:
 *         service = shared engine + streams[].
 */
struct esp_player_service {
    esp_media_service_t             media;           /*!< Media service base; must be first */
    uint8_t                         max_stream_num;  /*!< Stream slot count */
    void                           *pool;            /*!< GMF pool handle */
    bool                            pool_owned;      /*!< True if this service created the pool */
    bool                            configured;      /*!< True after a successful apply_setup */
    bool                            arb_pending;     /*!< Preemption arbitration left to the defer task */
    player_out_handle_t             out;             /*!< Audio output domain; NULL until apply_setup */
    esp_player_service_pcm_fmt_t    out_fmt;         /*!< Setup-declared output format; feed defaults read it */
    uint8_t                         output_volume;   /*!< Requested device volume 0-100 */
    player_stream_slot_t           *streams;         /*!< Stream slot array */
    esp_player_service_event_cb_t   event_cb;        /*!< Optional playback event callback */
    void                           *event_ctx;       /*!< User context for event_cb */
    player_defer_t                  defer;           /*!< Deferred work runner */
    esp_player_service_deinit_cb_t  deinit_cb;       /*!< Optional subclass deinit callback */
    void                           *deinit_ctx;      /*!< User context for deinit_cb */
    void                           *video_render;    /*!< Not owned: caller or subclass owned_render */
};

player_stream_slot_t *ps_find_slot_by_stream(esp_player_service_t *service, esp_media_stream_id_t stream);
esp_err_t ps_player_err_to_esp(esp_player_err_t err);
void ps_emit_service_event(esp_player_service_t *service, esp_media_stream_id_t stream,
                           esp_player_service_event_type_t type, void *data, uint32_t data_len);
esp_err_t ps_apply_stream_volume(player_stream_slot_t *slot);
esp_err_t ps_ensure_player(esp_player_service_t *service, player_stream_slot_t *slot,
                           esp_media_stream_id_t stream);
esp_err_t ps_open_audio_out(esp_player_service_t *service);
esp_err_t ps_stop_runtime(esp_player_service_t *service);

esp_err_t ps_ensure_defer(esp_player_service_t *service);
void ps_defer_run_pending(esp_player_service_t *service);

void ps_apply_pending_mix_gains(esp_player_service_t *service);
void ps_arbitrate_preemption(esp_player_service_t *service);
esp_err_t ps_validate_preempt_source(const player_stream_slot_t *slot);

esp_err_t ps_try_default_pool(void **pool, bool *pool_owned);
void ps_clear_default_pool(void **pool, bool *pool_owned);

uint8_t ps_stream_av_mask(const player_stream_slot_t *slot);
esp_err_t ps_apply_player_av_mask(player_stream_slot_t *slot, uint8_t mask);

static inline bool ps_has_video_out(const player_stream_slot_t *slot)
{
    return slot != NULL && slot->service != NULL && slot->service->video_render != NULL;
}

static inline bool ps_slot_linked(const player_stream_slot_t *slot)
{
    return slot != NULL && slot->provider.ops != NULL;
}

static inline esp_media_stream_id_t ps_slot_stream(const player_stream_slot_t *slot)
{
    if (slot == NULL || slot->service == NULL || slot->service->streams == NULL) {
        return 0;
    }
    return (esp_media_stream_id_t)(slot - slot->service->streams);
}

static inline bool ps_state_needs_stop(esp_player_state_t st)
{
    return st != ESP_PLAYER_STATE_IDLE && st != ESP_PLAYER_STATE_STOPPED;
}

static inline esp_player_state_t ps_slot_get_play_state(const player_stream_slot_t *slot)
{
    if (slot == NULL || slot->player == NULL) {
        return ESP_PLAYER_STATE_IDLE;
    }
    esp_player_state_t st = ESP_PLAYER_STATE_IDLE;
    if (esp_player_get_state(slot->player, &st) != ESP_PLAYER_ERR_OK) {
        return ESP_PLAYER_STATE_IDLE;
    }
    return st;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
