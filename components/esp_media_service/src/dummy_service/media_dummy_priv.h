/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_media_dummy_service.h"
#include "esp_media_track_mngr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DUMMY_MAX_TRACKS_PER_STREAM  2
#define DUMMY_SEED_QUEUE_DEPTH       3
#define DUMMY_VIDEO_QUEUE_DEPTH      3   /*!< Raw video user-cache slots; venc is slower than audio read */
#define DUMMY_PREBUFFER_FRAMES       DUMMY_VIDEO_QUEUE_DEPTH
#define DUMMY_DEFAULT_VIDEO_FPS      10
#define DUMMY_DEFAULT_AUDIO_SR       16000
#define DUMMY_AAC_SAMPLES_PER_FRAME  1024
#define DUMMY_MP3_MPEG2_SAMPLES      576   /*!< MPEG-2 Layer III samples/frame (s.mp3 @ 16 kHz) */
#define DUMMY_OPUS_FRAME_MS          20    /*!< Opus packet duration used by s.opus */
#define DUMMY_PCM_FRAME_MS           20
#define DUMMY_SINK_TASK_STACK        (4 * 1024)
#define DUMMY_SINK_TASK_PRIORITY     5
#define DUMMY_SINK_IDLE_DELAY_MS     20
#define DUMMY_SINK_BUSY_DELAY_MS     5
#define DUMMY_FRAME_ALIGN            64
#define DUMMY_SRC_DEFAULT_CACHE      (64 * 1024)

typedef struct {
    const uint8_t          *data;               /*!< Length-prefixed blob or raw buffer */
    size_t                  size;               /*!< Total blob size */
    size_t                  frame_size;         /*!< Raw frame size; 0 for encoded data */
    uint32_t                loop_frames;        /*!< Output slots per loop; 0 uses stored frame count */
    uint32_t                frame_duration_ms;  /*!< Duration of one output frame (media PTS unit) */
    esp_media_track_info_t  track_info;         /*!< Canonical track metadata */
    bool                    encoded;            /*!< True for length-prefixed encoded data */
    bool                    owned;              /*!< True if data was allocated by pattern helper */
} media_dummy_pattern_t;

typedef struct media_dummy_release_ctx media_dummy_release_ctx_t;

typedef struct {
    uint8_t  *data;
    size_t    cap;
    size_t    len;
} media_dummy_read_cache_t;

typedef struct {
    esp_media_track_info_t     info;
    media_dummy_pattern_t      pattern;
    media_dummy_release_ctx_t *release_ctx;
    uint32_t                   frame_index;
    uint32_t                   pts_ms;          /*!< Next frame PTS on write (ms) */
    size_t                     max_frame_size;  /*!< Cached max pattern frame payload size */
    uint64_t                   read_pcm_bytes;  /*!< PCM bytes delivered via read_frame */
    uint32_t                   delivered_frames;/*!< Frames delivered via acquire/read (encoded) */
    uint8_t                   *read_buf;       /*!< Heap buffer for one inner read_frame */
    size_t                     read_buf_size;
    media_dummy_read_cache_t   read_cache;     /*!< PCM tail when caller size is not frame-aligned */
    bool                       active;
} media_dummy_track_t;

typedef struct {
    esp_media_track_mngr_t    *mngr;
    esp_media_provider_t       provider;        /*!< Exported provider (may wrap inner) */
    esp_media_provider_t       inner_provider;  /*!< Track-manager provider */
    esp_media_dummy_service_t *svc;
    esp_media_stream_id_t      stream;
    media_dummy_track_t        tracks[DUMMY_MAX_TRACKS_PER_STREAM];
    uint16_t                   track_num;
    int64_t                    start_time_us;   /*!< Set on first acquire; elapsed = now - start */
    bool                       use_global_cache;
} media_dummy_src_stream_t;

typedef struct {
    esp_media_provider_t            provider;
    esp_media_dummy_stream_stats_t  stats;
} media_dummy_sink_stream_t;

struct esp_media_dummy_service {
    esp_media_service_t  media;
    esp_media_role_t     role;
    uint16_t             max_stream_num;
    volatile bool        running;
    char                *name_storage;

    media_dummy_src_stream_t  *src_streams;
    media_dummy_sink_stream_t *sink_streams;
    TaskHandle_t               sink_task;
};

/**
 * @brief  Build a pattern buffer for the requested track
 *
 *         Encoded codecs reuse embedded assets; raw PCM / video generate frames.
 *
 * @param[in]   req  Requested track metadata
 * @param[out]  out  Filled pattern descriptor
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    req or out is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Codec or parameters are unsupported
 *       - ESP_ERR_NO_MEM         Pattern allocation failed
 */
esp_err_t media_dummy_pattern_build(const esp_media_track_info_t *req, media_dummy_pattern_t *out);

/**
 * @brief  Release resources owned by a pattern
 *
 * @param[in,out]  pattern  Pattern to free; safe with NULL or empty pattern
 */
void media_dummy_pattern_release(media_dummy_pattern_t *pattern);

/**
 * @brief  Get one pattern frame by index
 *
 *         Encoded patterns wrap with `index % frame_count`. Raw patterns always
 *         return the single generated buffer.
 *
 * @param[in]   pattern  Built pattern
 * @param[in]   index    Frame index (may wrap for encoded data)
 * @param[out]  data     Pointer to frame payload (not owned by caller)
 * @param[out]  size     Frame payload size in bytes
 * @param[out]  key      Optional; set true for key frames when available
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  pattern, data, or size is NULL
 *       - ESP_ERR_NOT_FOUND    Pattern has no frames
 */
esp_err_t media_dummy_pattern_get_frame(media_dummy_pattern_t *pattern, uint32_t index,
                                        const uint8_t **data, size_t *size, bool *key);

/**
 * @brief  Get number of frames in a pattern
 *
 * @param[in]  pattern  Built pattern
 *
 * @return
 *       - Frame  count, or 0 if pattern is empty / invalid
 */
uint32_t media_dummy_pattern_frame_count(const media_dummy_pattern_t *pattern);

/**
 * @brief  Get the largest frame size in a pattern
 *
 * @param[in]  pattern  Built pattern
 *
 * @return
 *       - Max  frame size in bytes, or 0 if pattern is empty / invalid
 */
size_t media_dummy_pattern_max_frame_size(const media_dummy_pattern_t *pattern);

/**
 * @brief  Get one output frame duration in milliseconds
 *
 *         Prefers `pattern->frame_duration_ms`. Otherwise derives duration from
 *         codec defaults (AAC/MP3 sample counts, Opus 20 ms, PCM frame size,
 *         video fps). Matches `esp_media_frame_t` PTS units (ms).
 *
 * @param[in]  pattern  Built pattern
 *
 * @return
 *       - Frame  duration in ms, or 0 if pattern is NULL / unknown
 */
uint32_t media_dummy_pattern_frame_duration_ms(const media_dummy_pattern_t *pattern);

/**
 * @brief  Allocate source stream state for a dummy source service
 *
 * @param[in,out]  svc  Dummy source service
 *
 * @return
 *       - ESP_OK          On success
 *       - ESP_ERR_NO_MEM  Stream table allocation failed
 */
esp_err_t media_dummy_src_init(esp_media_dummy_service_t *svc);

/**
 * @brief  Free source stream state and tracks
 *
 * @param[in,out]  svc  Dummy source service
 */
void media_dummy_src_deinit(esp_media_dummy_service_t *svc);

/**
 * @brief  Add a pattern track to one source stream
 *
 * @param[in,out]  svc     Dummy source service
 * @param[in]      stream  Stream ID
 * @param[in]      info    Track metadata used to build the pattern
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service, stream, or info
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Track limit or allocation failure
 *       - Others                 Pattern build or track manager error
 */
esp_err_t media_dummy_src_add_track(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                    const esp_media_track_info_t *info);

/**
 * @brief  Remove all tracks from one source stream
 *
 * @param[in,out]  svc     Dummy source service
 * @param[in]      stream  Stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t media_dummy_src_reset_tracks(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream);

/**
 * @brief  Source start hook: reset PTS; pacing clock starts on first acquire
 *
 * @param[in,out]  svc  Dummy source service
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Track manager or seed write error
 */
esp_err_t media_dummy_src_on_start(esp_media_dummy_service_t *svc);

/**
 * @brief  Rewind source timelines for recording after capture pipeline open
 *
 *         Capture open runs one-shot video/audio fetches that consume the first
 *         keyframe and advance PTS. Call after capture start and before record
 *         start when dummy src was started before capture.
 *
 * @param[in,out]  svc     Dummy source service (must be running)
 * @param[in]      stream  Stream ID
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid service or stream
 *       - ESP_ERR_INVALID_STATE  Service is not running
 */
esp_err_t media_dummy_src_sync_record(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream);

/**
 * @brief  Source stop hook: abort pending track writes
 *
 * @param[in,out]  svc  Dummy source service
 *
 * @return
 *       - ESP_OK  Always
 */
esp_err_t media_dummy_src_on_stop(esp_media_dummy_service_t *svc);

/**
 * @brief  Get the media provider for one source stream
 *
 * @param[in]   svc     Dummy source service
 * @param[in]   stream  Stream ID
 * @param[out]  out     Output provider handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid service, stream, or out
 *       - ESP_ERR_NOT_FOUND    Stream has no track manager
 *       - Others               Error returned by the track manager
 */
esp_err_t media_dummy_src_get_provider(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                       esp_media_provider_t *out);

/**
 * @brief  Apply a sink negotiation request to one source stream
 *
 * @param[in,out]  svc      Dummy source service
 * @param[in]      stream   Stream ID
 * @param[in]      request  Negotiation request from the linked sink
 *
 * @return
 *       - ESP_OK               On success (including no-op when no tracks)
 *       - ESP_ERR_INVALID_ARG  Invalid service, stream, or request
 *       - Others               Error returned by the track manager
 */
esp_err_t media_dummy_src_set_request(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                      const esp_media_service_request_t *request);

/**
 * @brief  Allocate sink stream state for a dummy sink service
 *
 * @param[in,out]  svc  Dummy sink service
 *
 * @return
 *       - ESP_OK          On success
 *       - ESP_ERR_NO_MEM  Stream table allocation failed
 */
esp_err_t media_dummy_sink_init(esp_media_dummy_service_t *svc);

/**
 * @brief  Free sink stream state
 *
 * @param[in,out]  svc  Dummy sink service
 */
void media_dummy_sink_deinit(esp_media_dummy_service_t *svc);

/**
 * @brief  Sink start hook: reset stats and create the consume task
 *
 * @param[in,out]  svc  Dummy sink service
 *
 * @return
 *       - ESP_OK          On success
 *       - ESP_ERR_NO_MEM  Task creation failed
 *       - Others          Scheduler configuration error
 */
esp_err_t media_dummy_sink_on_start(esp_media_dummy_service_t *svc);

/**
 * @brief  Sink stop hook: abort providers and wait for the consume task
 *
 * @param[in,out]  svc  Dummy sink service
 *
 * @return
 *       - ESP_OK  Always
 */
esp_err_t media_dummy_sink_on_stop(esp_media_dummy_service_t *svc);

/**
 * @brief  Assign or clear the upstream provider for one sink stream
 *
 * @param[in,out]  svc       Dummy sink service
 * @param[in]      stream    Stream ID
 * @param[in]      provider  Provider to use; NULL clears the binding
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid service or stream
 */
esp_err_t media_dummy_sink_set_provider(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                        const esp_media_provider_t *provider);

/**
 * @brief  Copy sink statistics for one stream
 *
 * @param[in]   svc     Dummy sink service
 * @param[in]   stream  Stream ID
 * @param[out]  stats   Output statistics
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid service, stream, or stats
 */
esp_err_t media_dummy_sink_get_stats(esp_media_dummy_service_t *svc, esp_media_stream_id_t stream,
                                     esp_media_dummy_stream_stats_t *stats);

/**
 * @brief  Clear statistics for all sink streams
 *
 * @param[in,out]  svc  Dummy sink service
 */
void media_dummy_sink_reset_stats(esp_media_dummy_service_t *svc);

#ifdef __cplusplus
}
#endif
