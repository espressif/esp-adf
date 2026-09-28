/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_service.h"

#include "esp_llm_adapter.h"
#include "esp_llm_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_llm_service esp_llm_service_t;

/**
 * @brief  One LLM media stream: enable flag and track format
 *
 *         Set enabled and fill info before create. Adapter create() validates
 *         that the backend accepts the format. Disabled streams keep
 *         info unused. Typed adapter defaults pre-fill formats so the
 *         application can enable a stream without rewriting the track.
 */
typedef struct {
    bool                    enabled;  /*!< True to enable this stream at create */
    esp_media_track_info_t  info;     /*!< Track type and codec parameters */
} esp_llm_stream_cfg_t;

/**
 * @brief  Audio stream initializer for esp_llm_service_cfg_t.streams[]
 */
#define ESP_LLM_STREAM_CFG_AUDIO(en, codec_, rate, bits, ch, br)     \
    { .enabled = (en), .info = { .type = ESP_MEDIA_TRACK_TYPE_AUDIO, \
      .info.audio = { .codec = (codec_), .sample_rate = (rate),      \
                      .bits_per_sample = (bits), .channel = (ch), .bitrate = (br) } } }

/**
 * @brief  Video stream initializer for esp_llm_service_cfg_t.streams[]
 */
#define ESP_LLM_STREAM_CFG_VIDEO(en, codec_, w, h, fps_)             \
    { .enabled = (en), .info = { .type = ESP_MEDIA_TRACK_TYPE_VIDEO, \
      .info.video = { .codec = (codec_), .width = (w), .height = (h), .fps = (fps_) } } }

/**
 * @brief  LLM service creation configuration
 *
 *         Capture and player are application-owned. Fill streams[] with the
 *         formats those endpoints must use, then create capture/player from
 *         the same cfg. audio_ai_cfg.capture is borrowed only to attach AFE
 *         features. downlink_cb is an optional tap: the service still writes
 *         linked player-route frames to the shared provider. Filter by
 *         frame->type. Frame payload is valid only during the callback.
 */
typedef struct esp_llm_service_cfg {
    const char                   *name;                                 /*!< NULL to use adapter name */
    esp_llm_stream_cfg_t          streams[ESP_LLM_MEDIA_STREAM_COUNT];  /*!< Per-stream enable and format */
    esp_llm_audio_ai_cfg_t        audio_ai_cfg;                         /*!< Optional AFE policy and borrowed capture */
    esp_llm_downlink_frame_cb_t   downlink_cb;                          /*!< Optional audio and video downlink tap */
    void                         *downlink_cb_ctx;                      /*!< Context passed to downlink_cb */
} esp_llm_service_cfg_t;

/**
 * @brief  Default LLM service configuration (all streams disabled)
 */
#define ESP_LLM_SERVICE_CFG_DEFAULT()  {0}

/**
 * @brief  Create an LLM service instance
 *
 * @note  Prefer a typed wrapper such as esp_llm_volc_create(). Adapter descriptor
 *         must remain valid for the service lifetime. adapter_cfg is valid only
 *         during this call. Control-plane APIs must run on one application thread
 *         and must not overlap (`send_*`, `cancel_response`, `trigger_wakeup`,
 *         `trigger_sleep`, `keep_awake`). `enable_uplink_track` is thread-safe
 *         and may be called from an event-hub handler. Other event callbacks
 *         run on the event-hub context.
 *
 * @param[in]   adapter           Adapter descriptor
 * @param[in]   adapter_cfg       Typed adapter configuration
 * @param[in]   adapter_cfg_size  Size of adapter_cfg
 * @param[in]   cfg               Service configuration; caller retains ownership
 * @param[out]  out_service       Created handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If adapter, cfg, or out_service is invalid
 *       - ESP_ERR_NO_MEM         If memory allocation failed
 *       - ESP_ERR_NOT_SUPPORTED  If cfg requests an unsupported output path
 *       - Others                 Error returned by the adapter or media path
 */
esp_err_t esp_llm_service_create(const esp_llm_adapter_desc_t *adapter,
                                 const void *adapter_cfg,
                                 size_t adapter_cfg_size,
                                 const esp_llm_service_cfg_t *cfg,
                                 esp_llm_service_t **out_service);

/**
 * @brief  Destroy an LLM service instance
 *
 * @note  Does not stop the instance. Call esp_service_stop() first.
 *         esp_media_service_deinit() releases the adapter. On success, *service
 *         is set to NULL. On failure the handle is retained for retry.
 *
 * @param[in,out]  service  Pointer to a handle from esp_llm_service_create()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or *service is NULL
 *       - ESP_ERR_INVALID_STATE  If an uplink task is still running
 *       - ESP_FAIL               If the adapter context was not released
 */
esp_err_t esp_llm_service_destroy(esp_llm_service_t **service);

/**
 * @brief  Send text to the backend
 *
 * @param[in]  service  Running service handle
 * @param[in]  text     Null-terminated text
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL, or text is NULL or empty
 *       - ESP_ERR_INVALID_STATE  If service is not running
 *       - ESP_ERR_NOT_SUPPORTED  If backend does not support text input
 *       - Others                 Error returned by adapter send_text()
 */
esp_err_t esp_llm_service_send_text(esp_llm_service_t *service, const char *text);

/**
 * @brief  Send one audio, video, or one-shot vision frame
 *
 * @note  Do not also push frames that a linked capture provider is already
 *         consuming. VISION backends may accept a VIDEO frame without video_up.
 *         PTS is not required on this uplink path. frame->data and
 *         frame->user_data are valid only during this call.
 *
 * @param[in]  service  Running service handle
 * @param[in]  frame    Audio or video frame
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service, frame, or frame->data is invalid
 *       - ESP_ERR_INVALID_STATE  If service is not running
 *       - ESP_ERR_NOT_SUPPORTED  If backend does not support this track type
 *       - Others                 Error returned by adapter send_frame()
 */
esp_err_t esp_llm_service_send_frame(esp_llm_service_t *service, const esp_media_frame_t *frame);

/**
 * @brief  Send a backend-native custom payload
 *
 * @note  Forwarded verbatim. The string is valid only during this call.
 *
 * @param[in]  service  Running service handle
 * @param[in]  data     Custom payload string
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL, or data is NULL or empty
 *       - ESP_ERR_INVALID_STATE  If service is not running
 *       - ESP_ERR_NOT_SUPPORTED  If backend does not support custom payloads
 *       - Others                 Error returned by adapter send_custom_data()
 */
esp_err_t esp_llm_service_send_custom_data(esp_llm_service_t *service, const char *data);

/**
 * @brief  Cancel the current response, keep the session
 *
 * @param[in]  service  Running service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If service is not running
 *       - ESP_ERR_NOT_SUPPORTED  If backend does not support response cancellation
 *       - Others                 Error returned by adapter cancel_response()
 */
esp_err_t esp_llm_service_cancel_response(esp_llm_service_t *service);

/**
 * @brief  Enable or gate uplink frames of one media type
 *
 *         Thread-safe and may be called from an event-hub handler. Applies to
 *         capture consume tasks and to esp_llm_service_send_frame() of the same
 *         type. Gated frames are discarded and the call still returns ESP_OK.
 *         Capture and local preview keep running. The gate persists across
 *         stop/start; audio AI attach/detach does not reset it.
 *
 * @param[in]  service  Service handle
 * @param[in]  type     ESP_MEDIA_TRACK_TYPE_AUDIO or ESP_MEDIA_TRACK_TYPE_VIDEO
 * @param[in]  enabled  True to send frames of this type to the adapter
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service is NULL or type is not audio/video
 */
esp_err_t esp_llm_service_enable_uplink_track(esp_llm_service_t *service,
                                             esp_media_track_type_t type,
                                             bool enabled);

/**
 * @brief  Cancel the current response when a wake-up starts
 *
 * @note  Requires audio AI with ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN in
 *         feature_mask. Call after create. Not ISR-safe.
 *
 * @param[in]  service  Service handle
 * @param[in]  enable   True to cancel on wake-up start
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If audio AI is unused or WN is not enabled
 */
esp_err_t esp_llm_service_set_wakeup_cancel_response(esp_llm_service_t *service, bool enable);

/**
 * @brief  Set the downlink track-manager cache size for one media type
 *
 * @note  Must run while the service is not RUNNING, and before the downlink
 *         provider is exported (`esp_media_service_link()` / get_provider).
 *         cache_size 0 restores the default (16 KB audio, 128 KB video).
 *
 * @param[in]  service     Service handle
 * @param[in]  type        ESP_MEDIA_TRACK_TYPE_AUDIO or ESP_MEDIA_TRACK_TYPE_VIDEO
 * @param[in]  cache_size  Cache size in bytes, or 0 for the default
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL or type is not audio/video
 *       - ESP_ERR_INVALID_STATE  If the service is RUNNING or the provider was exported
 *       - Others                 Error returned while rebuilding the downlink
 */
esp_err_t esp_llm_service_set_downlink_cache_size(esp_llm_service_t *service,
                                                  esp_media_track_type_t type,
                                                  uint32_t cache_size);

/**
 * @brief  Manually enter the AFE wakeup state
 *
 * @note  Same path as a wake word. Capture must already be running.
 *         Not ISR-safe. Control-plane API; do not overlap with other control calls.
 *
 * @param[in]  service  Running service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If service is not running or audio AI is unused
 *       - ESP_ERR_NOT_SUPPORTED  If the AFE element is not present
 *       - Others                 Error returned by the capture AFE command
 */
esp_err_t esp_llm_service_trigger_wakeup(esp_llm_service_t *service);

/**
 * @brief  Manually leave the AFE wakeup state
 *
 * @note  Capture must already be running. Not ISR-safe.
 *
 * @param[in]  service  Running service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If service is not running or audio AI is unused
 *       - ESP_ERR_NOT_SUPPORTED  If the AFE element is not present
 *       - Others                 Error returned by the capture AFE command
 */
esp_err_t esp_llm_service_trigger_sleep(esp_llm_service_t *service);

/**
 * @brief  Keep the AFE in wakeup state without automatic timeout
 *
 * @note  Hold-to-talk: enable after trigger_wakeup on button press, disable
 *         before trigger_sleep on release. Capture must already be running.
 *         Not ISR-safe.
 *
 * @param[in]  service  Running service handle
 * @param[in]  enable   True to stay awake, false to allow wakeup timeout
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If service is not running or audio AI is unused
 *       - ESP_ERR_NOT_SUPPORTED  If the AFE element is not present
 *       - Others                 Error returned by the capture AFE command
 */
esp_err_t esp_llm_service_keep_awake(esp_llm_service_t *service, bool enable);

/**
 * @brief  Get effective instance capabilities
 *
 *         Adapter static bits intersected with streams enabled in
 *         esp_llm_service_cfg_t at create. The mask does not change with
 *         enable_uplink_track() or other runtime gates.
 *
 * @param[in]   service   Service handle
 * @param[out]  out_caps  Capability bitmask
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service or out_caps is NULL
 */
esp_err_t esp_llm_service_get_capabilities(esp_llm_service_t *service, esp_llm_capability_t *out_caps);

/**
 * @brief  Get the create-time format of one media stream
 *
 *         Available after create, independent of start(). Returns the track
 *         info stored from cfg.streams[stream]. Does not query the backend.
 *
 * @param[in]   service   Service handle
 * @param[in]   stream    Media stream
 * @param[out]  out_info  Track format on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If service or out_info is invalid, or stream is out of range
 *       - ESP_ERR_NOT_FOUND    If the stream is disabled
 */
esp_err_t esp_llm_service_get_stream_info(esp_llm_service_t *service,
                                          esp_llm_media_stream_t stream,
                                          esp_media_track_info_t *out_info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
