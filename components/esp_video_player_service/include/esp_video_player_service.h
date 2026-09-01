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
#include "esp_player_scheduler.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_video_player_service_create()
 */
#define ESP_VIDEO_PLAYER_SERVICE_DEFAULT_NAME  "video_player"

/**
 * @brief  Default max stream count used by ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT()
 */
#define ESP_VIDEO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM  2

/**
 * @brief  Default LCD output rate used by ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT()
 */
#define ESP_VIDEO_PLAYER_SERVICE_DEFAULT_RENDER_FPS  30

/**
 * @brief  Video player subclass configuration (create / attach identity)
 */
typedef struct {
    const char *name;            /*!< Service / scheduler name; NULL uses DEFAULT_NAME */
    uint8_t     max_stream_num;  /*!< Stream count; 0 uses DEFAULT_MAX_STREAM_NUM */
    void       *pool;            /*!< Optional GMF pool; NULL uses the parent default audio pool */
    uint8_t     render_fps;      /*!< LCD output rate; 0 uses DEFAULT_RENDER_FPS */
} esp_video_player_service_cfg_t;

/**
 * @brief  Default initializer for esp_video_player_service_cfg_t
 */
#define ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT()  {                       \
    .name           = ESP_VIDEO_PLAYER_SERVICE_DEFAULT_NAME,            \
    .max_stream_num = ESP_VIDEO_PLAYER_SERVICE_DEFAULT_MAX_STREAM_NUM,  \
    .pool           = NULL,                                             \
    .render_fps     = ESP_VIDEO_PLAYER_SERVICE_DEFAULT_RENDER_FPS,      \
}

/**
 * @brief  Create a player service with audio attach and video context
 *
 *         Internally: parent create → audio attach → video attach.
 *         Apply LCD (and DAC if needed) with `esp_video_player_service_apply_setup()`.
 *         Destroy with `esp_player_service_destroy()`.
 *
 *         The application must `esp_board_manager_init_device_by_name()` for the
 *         LCD (and DAC) before apply_setup. This service does not init/deinit panels.
 *
 * @note  Register global extractor / audio / video decoder tables before play.
 *        See `esp_player_service_create()`. This service does not call
 *        `esp_video_dec_register_default()`.
 *
 * @param[in]   cfg          Optional configuration; NULL uses defaults
 * @param[out]  out_service  Receives the parent player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_service is NULL
 *       - Others               Parent create or attach failed
 */
esp_err_t esp_video_player_service_create(const esp_video_player_service_cfg_t *cfg,
                                          esp_player_service_t **out_service);

/**
 * @brief  Attach video output capability to an existing parent player
 *
 *         Use after `esp_audio_player_service_create()` when adding a display.
 *         Do not call `esp_video_player_service_create()` on the same speaker.
 *
 * @param[in]  player  Parent player handle
 * @param[in]  cfg     Optional configuration; NULL uses defaults
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If player is NULL
 *       - ESP_ERR_INVALID_STATE  If video is already attached
 *       - ESP_ERR_NO_MEM         If allocation fails
 *       - Others                 If audio attach or callback setup fails
 */
esp_err_t esp_video_player_service_attach(esp_player_service_t *player,
                                          const esp_video_player_service_cfg_t *cfg);

/**
 * @brief  Get the number of container tracks of one type on a stream
 *
 *         Available after ESP_PLAYER_SERVICE_EVENT_TRACK_INFO_PARSED.
 *         Returns ESP_ERR_NOT_SUPPORTED when video output is missing.
 *
 * @param[in]   service  Parent player handle
 * @param[in]   stream   Target stream id
 * @param[in]   type     Track type (audio or video)
 * @param[out]  out_num  Receives the track count
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - ESP_ERR_NOT_SUPPORTED  If video output is missing
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_video_player_service_get_track_num(esp_player_service_t *service,
                                                 esp_media_stream_id_t stream,
                                                 esp_player_track_type_t type,
                                                 uint16_t *out_num);

/**
 * @brief  Get container / declared metadata for one track
 *
 *         Available after ESP_PLAYER_SERVICE_EVENT_TRACK_INFO_PARSED. This is
 *         the source catalog (type + index), not decoder output: JPEG with no
 *         coded size may report width/height 0.
 *
 *         Returns ESP_ERR_NOT_SUPPORTED when video output is missing.
 *
 * @param[in]   service    Parent player handle
 * @param[in]   stream     Target stream id
 * @param[in]   type       Track type (audio or video)
 * @param[in]   track_idx  Track index within that type
 * @param[out]  out_info   Receives track metadata
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - ESP_ERR_NOT_SUPPORTED  If video output is missing
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_video_player_service_get_track_info(esp_player_service_t *service,
                                                  esp_media_stream_id_t stream,
                                                  esp_player_track_type_t type,
                                                  uint16_t track_idx,
                                                  esp_player_track_info_t *out_info);

/**
 * @brief  Enable or disable one container track on a stream
 *
 *         AV extractor (URL) mode only. Returns ESP_ERR_NOT_SUPPORTED when
 *         video output is missing or the source is feed/link.
 *
 * @param[in]  service    Parent player handle
 * @param[in]  stream     Target stream id
 * @param[in]  type       Track type (audio or video)
 * @param[in]  track_idx  Track index within that type
 * @param[in]  enable     true to enable the track
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If any argument is invalid
 *       - ESP_ERR_INVALID_STATE  If no player exists on the stream
 *       - ESP_ERR_NOT_SUPPORTED  If video output is missing or the source is feed/link
 *       - Others                 If the underlying player fails
 */
esp_err_t esp_video_player_service_enable_track(esp_player_service_t *service,
                                                esp_media_stream_id_t stream,
                                                esp_player_track_type_t type,
                                                uint16_t track_idx,
                                                bool enable);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
