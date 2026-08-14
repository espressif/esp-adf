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
#include "esp_media_service.h"
#include "esp_player.h"
#include "esp_player_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_player_service_create()
 *
 *         Scheduler and manager requests use this when cfg->name is NULL.
 */
#define ESP_PLAYER_SERVICE_DEFAULT_NAME  "player"

/**
 * @brief  Default max stream count used by ESP_PLAYER_SERVICE_CFG_DEFAULT()
 *
 *         Override per instance via esp_player_service_cfg_t.max_stream_num
 *         (e.g. set to 2 for TTS + BGM mix).
 */
#define ESP_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM  1

typedef struct esp_player_service esp_player_service_t;

/**
 * @brief  Callback invoked when the player service is deinitialized
 *
 * @param[in]  service    Player service handle
 * @param[in]  user_data  User context from esp_player_service_set_deinit_cb()
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Error returned by the callback implementation
 */
typedef esp_err_t (*esp_player_service_deinit_cb_t)(esp_player_service_t *service, void *user_data);

/**
 * @brief  Advanced service-wide handle snapshot from get_info()
 *
 *         Not a day-to-day status query. Use get_state / get_position /
 *         get_volume for playback status. Typical use: take `audio_render` and
 *         attach extra GMF processors. Per-stream handles come from
 *         esp_player_service_get_stream_info().
 *
 *         Handles are typed as void* so this header pulls neither codec_dev nor
 *         the render components; cast to the type named on each field. They stay
 *         owned by the service and remain valid until the next apply_setup that
 *         rebuilds output, or until destroy.
 */
typedef struct {
    void    *codec_dev;       /*!< esp_codec_dev_handle_t; NULL if unused */
    void    *audio_render;    /*!< esp_audio_render_handle_t; NULL without audio output */
    void    *video_render;    /*!< esp_video_render_handle_t; NULL without video output */
    uint8_t  max_stream_num;  /*!< Stream slot count; valid stream ids are 0..max_stream_num-1 */
} esp_player_service_info_t;

/**
 * @brief  Advanced per-stream handle snapshot from get_stream_info()
 *
 *         Typical use: take `mixer_stream` and attach extra GMF processors, or
 *         use `player` with `esp_player_set_custom_elements()` /
 *         `esp_player_set_dec_cfg()`.
 *
 *         Both handles are created lazily and are NULL until the stream has an
 *         output / input. They stay owned by the service and remain valid until
 *         the next apply_setup that rebuilds output, or until destroy.
 */
typedef struct {
    void                *mixer_stream;  /*!< esp_audio_render_stream_handle_t; NULL until audio output exists */
    esp_player_handle_t  player;        /*!< Per-stream esp_player; NULL until the stream is used */
} esp_player_service_stream_info_t;

/**
 * @brief  Player service configuration (create-time only)
 *
 *         Output format / device settings live in esp_player_service_setup_t
 *         (apply_setup).
 *
 *         `name` is for esp_service / esp_service_scheduler.
 *         `pool` is typed as void* so this header does not pull GMF; NULL lets
 *         create() build a default audio pool (decoder + convert + ALC + sonic).
 */
typedef struct {
    const char *name;            /*!< Service / scheduler name; NULL uses DEFAULT_NAME */
    uint8_t     max_stream_num;  /*!< Maximum streams (public stream ids) */
    void       *pool;            /*!< Optional GMF pool handle; NULL builds a default audio pool */
} esp_player_service_cfg_t;

/**
 * @brief  Default initializer for esp_player_service_cfg_t
 */
#define ESP_PLAYER_SERVICE_CFG_DEFAULT()  {                       \
    .name           = ESP_PLAYER_SERVICE_DEFAULT_NAME,            \
    .max_stream_num = ESP_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM,  \
    .pool           = NULL,                                       \
}

/**
 * @brief  Create a player service
 *
 *         The service is created in an uninitialized state. Call
 *         esp_player_service_apply_setup() then esp_service_start().
 *
 * @note  Global extractor and decoder tables are owned by the application.
 *        Register them before play (typical: `esp_extractor_register_default()`
 *        and `esp_audio_dec_register_default()`). The default GMF pool only
 *        installs elements; this service does not call register_default or
 *        unregister_default.
 *
 * @param[in]   cfg          Service configuration
 * @param[out]  out_service  Pointer to receive the created service handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If cfg or out_service is NULL, or mandatory fields are invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 */
esp_err_t esp_player_service_create(const esp_player_service_cfg_t *cfg,
                                    esp_player_service_t **out_service);

/**
 * @brief  Destroy the player service
 *
 *         Stops the service if running, joins provider bridge tasks, and
 *         releases all resources. After this returns ESP_OK, any caller-owned
 *         playlist previously attached with `esp_player_service_set_playlist()`
 *         is no longer used and may be freed. The handle is then invalid.
 *
 * @note  Blocks until linked provider bridge tasks exit (same contract as
 *        `esp_player_stop()`). Custom providers must honor acquire timeout and
 *        `abort()` so those tasks can leave. `esp_service_stop()` uses the same
 *        join.
 *
 * @param[in]  service  Service handle to destroy
 *
 * @return
 *       - ESP_OK               On success; handle is invalid afterwards
 *       - ESP_ERR_INVALID_ARG  If service is NULL
 */
esp_err_t esp_player_service_destroy(esp_player_service_t *service);

/**
 * @brief  Set the callback invoked when the service is deinitialized
 *
 *         Subclasses use this to detach board-level DAC / LCD context.
 *
 * @param[in]  service    Player service handle
 * @param[in]  deinit_cb  Callback to invoke, or NULL to clear it
 * @param[in]  user_data  User context passed to deinit_cb
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service is NULL
 */
esp_err_t esp_player_service_set_deinit_cb(esp_player_service_t *service,
                                           esp_player_service_deinit_cb_t deinit_cb,
                                           void *user_data);

/**
 * @brief  Get an advanced snapshot of this service's codec / render handles
 *
 *         Daily playback status is `get_state` / `get_position` / `get_volume`.
 *         Use this to inspect or extend the render pipeline.
 *
 * @param[in]   service   Player service handle
 * @param[out]  out_info  Pointer to receive info; handles owned by the service
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument is invalid
 */
esp_err_t esp_player_service_get_info(esp_player_service_t *service,
                                      esp_player_service_info_t *out_info);

/**
 * @brief  Get the mixer stream and esp_player handles of one stream
 *
 *         Use this to reach a per-stream `esp_player` for custom elements or
 *         decoder config, or the mixer stream for extra GMF processors. Reapply
 *         custom elements after a stop / start cycle.
 *
 * @note  Both handles may be NULL before the stream is set up. Check them
 *        instead of assuming a successful call yields usable handles.
 *
 * @param[in]   service   Player service handle
 * @param[in]   stream    Target stream id
 * @param[out]  out_info  Pointer to receive info; handles owned by the service
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If any argument or stream id is invalid
 */
esp_err_t esp_player_service_get_stream_info(esp_player_service_t *service,
                                             esp_media_stream_id_t stream,
                                             esp_player_service_stream_info_t *out_info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
