/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_extractor_id3_parser.h"
#include "esp_player_advance.h"
#include "esp_player_service.h"
#include "esp_player_types.h"
#include "esp_playlist.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Per-stream player event types
 *
 *         Player events are translated by name. TRACK_CHANGED is raised by the
 *         service when a playlist advances.
 */
typedef enum {
    ESP_PLAYER_SERVICE_EVENT_NONE              = 0,   /*!< No event */
    ESP_PLAYER_SERVICE_EVENT_TRACK_INFO_PARSED = 1,   /*!< Container track list parsed */
    ESP_PLAYER_SERVICE_EVENT_AUDIO_INFO_PARSED = 2,   /*!< Audio decoder reported info */
    ESP_PLAYER_SERVICE_EVENT_VIDEO_INFO_PARSED = 3,   /*!< Video decoder reported info */
    ESP_PLAYER_SERVICE_EVENT_PLAYED            = 4,   /*!< Playback started or resumed */
    ESP_PLAYER_SERVICE_EVENT_PAUSED            = 5,   /*!< Playback paused */
    ESP_PLAYER_SERVICE_EVENT_SEEK_DONE         = 6,   /*!< Seek completed */
    ESP_PLAYER_SERVICE_EVENT_STOPPED           = 7,   /*!< Playback stopped by request */
    ESP_PLAYER_SERVICE_EVENT_FINISHED          = 8,   /*!< Reached end of source */
    ESP_PLAYER_SERVICE_EVENT_BUFFERING         = 9,   /*!< Underrun, buffering */
    ESP_PLAYER_SERVICE_EVENT_BUFFERED          = 10,  /*!< Buffering done, ready */
    ESP_PLAYER_SERVICE_EVENT_TRACK_CHANGED     = 11,  /*!< Playlist advanced */
    ESP_PLAYER_SERVICE_EVENT_ERROR             = 12,  /*!< Runtime error */
} esp_player_service_event_type_t;

/**
 * @brief  Static playback priority of a stream within its service
 */
typedef enum {
    ESP_PLAYER_PRIO_BACKGROUND = 0,  /*!< Media / background music */
    ESP_PLAYER_PRIO_NOTIFY     = 1,  /*!< TTS / prompt / notification */
    ESP_PLAYER_PRIO_URGENT     = 2,  /*!< Alarm / critical announcement */
} esp_player_priority_t;

/**
 * @brief  How a higher-priority stream suppresses lower-priority streams
 */
typedef enum {
    ESP_PLAYER_PREEMPT_COEXIST   = 0,  /*!< Lower streams duck and keep playing */
    ESP_PLAYER_PREEMPT_EXCLUSIVE = 1,  /*!< Lower streams fully make way */
} esp_player_preempt_mode_t;

/**
 * @brief  How a stream handles its timeline when exclusively preempted
 *
 *         PAUSE is valid only for URL sources; DROP is valid only for feed/link
 *         sources. Invalid combinations are rejected when the source or mix
 *         configuration is set.
 */
typedef enum {
    ESP_PLAYER_ON_PREEMPT_PAUSE = 0,  /*!< Freeze and resume (URL only) */
    ESP_PLAYER_ON_PREEMPT_DROP  = 1,  /*!< Discard output (feed/link only) */
} esp_player_on_preempt_t;

/**
 * @brief  Event message delivered to the registered callback
 */
typedef struct {
    esp_media_stream_id_t            stream;    /*!< Stream that raised the event */
    esp_player_service_event_type_t  type;      /*!< Event type */
    void                            *data;      /*!< Optional event payload */
    uint32_t                         data_len;  /*!< Payload length in bytes */
} esp_player_service_event_msg_t;

/**
 * @brief  Per-stream mixing and preemption configuration
 */
typedef struct {
    float                      active_gain;    /*!< Level when not suppressed (> 0) */
    float                      duck_gain;      /*!< Level when COEXIST-ducked */
    uint32_t                   transition_ms;  /*!< Smooth transition time */
    esp_player_priority_t      priority;       /*!< Static priority */
    esp_player_preempt_mode_t  preempt_mode;   /*!< Inserter suppression policy */
    esp_player_on_preempt_t    on_preempt;     /*!< Victim timeline policy */
} esp_player_mix_cfg_t;

/**
 * @brief  Player event callback
 *
 * @note  Invoked on internal service/player threads. Do not call blocking
 *        control APIs from the callback.
 *
 * @param[in]  msg  Event message.
 * @param[in]  ctx  User context from set_event_cb().
 *
 * @return
 *       - ESP_OK  Preferred; other values are ignored by the service today
 */
typedef esp_err_t (*esp_player_service_event_cb_t)(const esp_player_service_event_msg_t *msg, void *ctx);

/**
 * @brief  Configure a track for a stream in feed mode
 *
 *         Must be called before writing the first frame. It is not needed for URL
 *         playback or the link path (`esp_media_service_link` / `set_provider`
 *         copies tracks from the SRC).
 *
 *         Limitation: a stream's feed has at most one audio and one video track.
 *         Call once per type; a second call for the same type replaces the previous
 *         description. Extra audio (TTS) uses another stream.
 *
 *         Audio: PCM and headerless ES (raw OPUS, AAC without ADTS, LC3, …) need
 *         `sample_rate`, `channel`, and `bits_per_sample`. Self-framed ES (ADTS AAC,
 *         MP3, FLAC) need only `codec`; leave those fields 0. Video: set `codec`,
 *         `width`, and `height`.
 *
 *         After `stop` (or FINISHED), the first `set_track` starts a new set and
 *         drops the other type: declare only audio to play audio-only even when a
 *         video output is attached. The feed session `av_mask` follows the
 *         declared tracks. Re-feed the same format without calling this; the
 *         previous set is kept.
 *
 *         A type the stream has not declared yet is accepted even while it runs, so
 *         that a source announcing audio and video in separate messages does not
 *         starve the second decoder. Playback restarts once for the new `av_mask`.
 *         Redeclaring a type already in the set needs a `stop` first.
 *
 *         Returns ESP_ERR_NOT_SUPPORTED if the service has no matching output
 *         (video tracks need a video render).
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  track    Track info (`type` selects audio vs video)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If the stream is preparing, playing, or paused and the
 *                                track type is already in the declared set
 *       - ESP_ERR_NOT_SUPPORTED  Output does not match the track type
 */
esp_err_t esp_player_service_set_track(esp_player_service_t *service, esp_media_stream_id_t stream,
                                       const esp_media_track_info_t *track);

/**
 * @brief  Write an encoded or PCM media frame to a stream in feed mode
 *
 *         Routed by `frame->type`. The payload is copied. Invalid while a linked
 *         provider owns the stream.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  frame    Media frame to write
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If a linked provider owns the feed path
 *       - ESP_ERR_NOT_SUPPORTED  Frame type is not available on this output
 */
esp_err_t esp_player_service_write_frame(esp_player_service_t *service, esp_media_stream_id_t stream,
                                         const esp_media_frame_t *frame);

/**
 * @brief  Set the playback URL for a stream
 *
 *         Supports file:///, http(s)://, and HLS. A new URL resets the stream state.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  url      Null-terminated URL string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid, or mix/preempt policy
 *                              conflicts with a URL source
 */
esp_err_t esp_player_service_set_url(esp_player_service_t *service, esp_media_stream_id_t stream,
                                     const char *url);

/**
 * @brief  Start URL playback on a stream
 *
 *         Requires a URL set with `esp_player_service_set_url()`.
 *
 * @note  URL path only, and player transport rather than an input starter: the
 *        feed path starts on the first `esp_player_service_write_frame()`, and a
 *        linked stream starts from its provider bridge.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no player or URL is set
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_play(esp_player_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Pause playback on a stream
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_pause(esp_player_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Resume paused playback on a stream
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_resume(esp_player_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Stop playback on a stream
 *
 *         Player transport only: it stops this stream's `esp_player` and does
 *         not tear down the input. Each path is stopped at its own layer, URL
 *         with this call, feed by no longer writing frames, and a link with
 *         `esp_media_service_unlink()` / `esp_media_service_set_provider(NULL)`
 *         or by stopping the SRC service.
 *
 *         Clears the remembered input kind so the next `set_url` / `set_track`
 *         can switch path. The next `set_track` starts a new feed track set;
 *         re-feed without `set_track` keeps the previous set.
 *
 * @note  On a stream that is still linked, the provider bridge keeps running and
 *        the next SRC frame starts playback again. Unlink (or stop the SRC) when
 *        the input is done. Writing again on the feed path likewise starts a new
 *        session.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_stop(esp_player_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Seek to a time position on a stream
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  time_ms  Target position in milliseconds
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_seek(esp_player_service_t *service, esp_media_stream_id_t stream, uint64_t time_ms);

/**
 * @brief  Set playback speed on a stream
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  speed    Playback speed; must be greater than 0
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_set_speed(esp_player_service_t *service, esp_media_stream_id_t stream, float speed);

/**
 * @brief  Get the duration of the current source on a stream
 *
 * @param[in]   service       Player service handle
 * @param[in]   stream        Target stream id
 * @param[out]  out_duration  Receives duration in milliseconds
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_get_duration(esp_player_service_t *service, esp_media_stream_id_t stream,
                                          uint64_t *out_duration);

/**
 * @brief  Get the current playback position on a stream
 *
 * @param[in]   service       Player service handle
 * @param[in]   stream        Target stream id
 * @param[out]  out_position  Receives position in milliseconds
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_get_position(esp_player_service_t *service, esp_media_stream_id_t stream,
                                          uint64_t *out_position);

/**
 * @brief  Get the playback state of a stream
 *
 * @param[in]   service    Player service handle
 * @param[in]   stream     Target stream id
 * @param[out]  out_state  Receives the current player state
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid
 */
esp_err_t esp_player_service_get_state(esp_player_service_t *service, esp_media_stream_id_t stream,
                                       esp_player_state_t *out_state);

/**
 * @brief  Override demux pool, network read-ahead and buffering gate thresholds
 *
 *         Stored on this stream and applied when its player is created with a
 *         video output. Survives apply_setup(). Pass NULL to drop the override.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  config   Buffer configuration, or NULL to clear the override
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL or stream is invalid
 */
esp_err_t esp_player_service_set_buffer_config(esp_player_service_t *service,
                                               esp_media_stream_id_t stream,
                                               const esp_player_buffer_config_t *config);

/**
 * @brief  Set A/V synchronization mode for a stream
 *
 *         Wraps esp_player_set_sync_mode() on this stream's player. Stored on
 *         the slot (default AUDIO at create) and applied when an A/V player is
 *         created. Survives apply_setup(). Only meaningful when av_mask is
 *         ESP_PLAYER_MASK_AV. Live players that are not IDLE / STOPPED /
 *         FINISHED keep the stored value for the next session.
 *
 * @param[in]  service    Player service handle
 * @param[in]  stream     Target stream id
 * @param[in]  sync_mode  Mode; must be less than ESP_PLAYER_SYNC_MODE_MAX
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service or stream is invalid, or sync_mode is invalid
 *       - Others               If the underlying player fails
 */
esp_err_t esp_player_service_set_sync_mode(esp_player_service_t *service,
                                           esp_media_stream_id_t stream,
                                           esp_player_sync_mode_t sync_mode);

/**
 * @brief  Register a playback event callback
 *
 *         Pass NULL for `cb` to clear it. Invoked on internal threads; do not
 *         call blocking control APIs from the callback.
 *
 * @param[in]  service  Player service handle
 * @param[in]  cb       Event callback, or NULL to clear
 * @param[in]  ctx      User context passed to the callback
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL
 */
esp_err_t esp_player_service_set_event_cb(esp_player_service_t *service, esp_player_service_event_cb_t cb, void *ctx);

/**
 * @brief  Set per-stream volume (ALC gain)
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  volume   Volume 0-100
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_NOT_SUPPORTED  If no GMF pool is installed
 */
esp_err_t esp_player_service_set_volume(esp_player_service_t *service, esp_media_stream_id_t stream, uint8_t volume);

/**
 * @brief  Get per-stream volume
 *
 * @param[in]   service     Player service handle
 * @param[in]   stream      Target stream id
 * @param[out]  out_volume  Receives volume 0-100
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid
 */
esp_err_t esp_player_service_get_volume(esp_player_service_t *service, esp_media_stream_id_t stream,
                                        uint8_t *out_volume);

/**
 * @brief  Set the DAC / output volume for the whole service
 *
 * @param[in]  service  Player service handle
 * @param[in]  volume   Volume 0-100
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL or volume > 100
 *       - ESP_FAIL             If the codec device rejected the volume
 */
esp_err_t esp_player_service_set_output_volume(esp_player_service_t *service, uint8_t volume);

/**
 * @brief  Get the DAC / output volume for the whole service
 *
 * @param[in]   service     Player service handle
 * @param[out]  out_volume  Receives volume 0-100
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid
 */
esp_err_t esp_player_service_get_output_volume(esp_player_service_t *service, uint8_t *out_volume);

/**
 * @brief  Set mixing and preemption policy for a stream
 *
 *         Must be applied before the mixer is running. Invalid PAUSE/DROP
 *         combinations for the current source kind are rejected. On any error the
 *         stream keeps the configuration it had before the call.
 *
 * @note  Only a configured stream takes part in preemption, so the first
 *        successful call also creates the service thread that arbitrates it.
 *        Not thread-safe: serialize this with the other configuration APIs on
 *        the same handle.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  cfg      Mix configuration
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument or policy is invalid
 *       - ESP_ERR_INVALID_STATE  If the mixer is already running
 *       - ESP_ERR_NO_MEM         If the arbitration thread cannot be created
 */
esp_err_t esp_player_service_set_mix_cfg(esp_player_service_t *service, esp_media_stream_id_t stream,
                                         const esp_player_mix_cfg_t *cfg);

/**
 * @brief  Query whether a stream is currently preempted
 *
 * @param[in]   service        Player service handle
 * @param[in]   stream         Target stream id
 * @param[out]  out_preempted  Set to true if the stream is suppressed
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid
 */
esp_err_t esp_player_service_get_preempt_state(esp_player_service_t *service, esp_media_stream_id_t stream,
                                               bool *out_preempted);

/**
 * @brief  Enable or disable ID3 parsing for a bare MP3 URL on a stream
 *
 *         Disabled by default. Call before play. Only applies to raw MP3 URLs.
 *         The preference is stored even if the stream player does not exist yet.
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  enable   true to enable
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service or stream is invalid
 *       - Others               If the underlying player fails
 */
esp_err_t esp_player_service_enable_id3_parse(esp_player_service_t *service,
                                              esp_media_stream_id_t stream,
                                              bool enable);

/**
 * @brief  Get ID3 metadata for the current MP3 on a stream
 *
 *         Available after ESP_PLAYER_SERVICE_EVENT_TRACK_INFO_PARSED. Pointers
 *         remain valid until the source changes, parsing is disabled, or the
 *         player is destroyed. Do not free them.
 *
 * @param[in]   service   Player service handle
 * @param[in]   stream    Target stream id
 * @param[out]  out_info  Receives a pointer to ID3 info
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_player_service_get_id3_info(esp_player_service_t *service,
                                          esp_media_stream_id_t stream,
                                          const esp_extractor_id3_info_t **out_info);

/**
 * @brief  Attach a caller-owned playlist to a stream
 *
 *         Pass NULL for `playlist` to detach. The playlist handle is caller-owned.
 *         Auto-advance reads it from a service thread. Call `esp_playlist_del()`
 *         only after `esp_player_service_destroy()`.
 *
 * @note  Attaching the first playlist also creates that thread. Not thread-safe:
 *        serialize this with the other configuration APIs on the same handle.
 *
 * @param[in]  service   Player service handle
 * @param[in]  stream    Target stream id
 * @param[in]  playlist  Playlist handle, or NULL to detach
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service or stream is invalid
 *       - ESP_ERR_NO_MEM       If the auto-advance thread cannot be created
 */
esp_err_t esp_player_service_set_playlist(esp_player_service_t *service, esp_media_stream_id_t stream,
                                          esp_playlist_handle_t playlist);

/**
 * @brief  Set playlist repeat mode on a stream
 *
 * @param[in]  service      Player service handle
 * @param[in]  stream       Target stream id
 * @param[in]  repeat_mode  Repeat mode
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no playlist is attached
 */
esp_err_t esp_player_service_set_repeat_mode(esp_player_service_t *service, esp_media_stream_id_t stream,
                                             esp_playlist_repeat_mode_t repeat_mode);

/**
 * @brief  Play a playlist item by index
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 * @param[in]  index    Zero-based playlist index
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no playlist is attached
 *       - ESP_ERR_NOT_FOUND      If the item has no URL
 *       - Others                 If play fails
 */
esp_err_t esp_player_service_play_index(esp_player_service_t *service, esp_media_stream_id_t stream, int index);

/**
 * @brief  Advance to the next playlist item
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no playlist is attached
 *       - ESP_ERR_NOT_FOUND      If the next item has no URL
 *       - Others                 If play fails
 */
esp_err_t esp_player_service_next(esp_player_service_t *service, esp_media_stream_id_t stream);

/**
 * @brief  Go to the previous playlist item
 *
 * @param[in]  service  Player service handle
 * @param[in]  stream   Target stream id
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or stream is invalid
 *       - ESP_ERR_INVALID_STATE  If no playlist is attached
 *       - ESP_ERR_NOT_FOUND      If the previous item has no URL
 *       - Others                 If play fails
 */
esp_err_t esp_player_service_prev(esp_player_service_t *service, esp_media_stream_id_t stream);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
