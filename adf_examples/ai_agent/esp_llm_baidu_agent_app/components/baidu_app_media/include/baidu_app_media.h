/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdint.h>

#include "sdkconfig.h"

#include "esp_llm_baidu.h"
#include "esp_media_service.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define BAIDU_APP_MEDIA_TTS_STREAM          ((esp_media_stream_id_t)0)  /*!< Linked LLM downlink */
#define BAIDU_APP_MEDIA_MUSIC_STREAM        ((esp_media_stream_id_t)1)  /*!< HTTP / file URL mix */
#define BAIDU_APP_MEDIA_WAKEUP_STREAM       ((esp_media_stream_id_t)2)  /*!< Local wake-up prompt */
#define BAIDU_APP_MEDIA_PLAYER_STREAM_NUM   3                           /*!< TTS + music + wake-up */

/**
 * @brief  Opaque application-owned Baidu media graph
 *
 *         Owns board audio, codecs, capture, and a 3-stream player. Vision
 *         scenes also own camera and LCD; those links are not exposed through
 *         getters. Borrows, but never destroys, the LLM passed to
 *         baidu_app_media_start().
 */
typedef struct baidu_app_media baidu_app_media_t;

/**
 * @brief  Baidu application media configuration
 *
 *         Initialize this structure with BAIDU_APP_MEDIA_CFG_DEFAULT().
 */
typedef struct {
    uint8_t  output_volume;  /*!< Initial player output volume from 0 through 100 */
} baidu_app_media_cfg_t;

/**
 * @brief  Default initializer for baidu_app_media_cfg_t
 */
#define BAIDU_APP_MEDIA_CFG_DEFAULT() {             \
    .output_volume = CONFIG_BAIDU_OUTPUT_VOLUME,    \
}

/**
 * @brief  Create board audio (optional SD, ADC, DAC), default codecs, capture,
 *         and a 3-stream player. Vision scenes add camera (JPEG uplink) and LCD
 *         (VIDEO_DN) when the board has those devices. Does not link or start
 *         services.
 *
 * @param[in]   baidu_cfg  Baidu adapter configuration; caller retains ownership
 * @param[in]   cfg        Media configuration; caller retains ownership
 * @param[out]  out_media  Receives the created media handle; must point to NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument is NULL or volume is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned by board, codec, or service setup
 */
esp_err_t baidu_app_media_create(const esp_llm_baidu_cfg_t *baidu_cfg,
                                 const baidu_app_media_cfg_t *cfg,
                                 baidu_app_media_t **out_media);

/**
 * @brief  Get the application-owned audio capture service
 *
 *         The returned borrowed handle remains valid until baidu_app_media_destroy().
 *
 * @param[in]   media        Media handle returned by baidu_app_media_create()
 * @param[out]  out_capture  Receives the borrowed audio capture handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_capture is NULL
 *       - ESP_ERR_NOT_FOUND    If the audio capture service is unavailable
 */
esp_err_t baidu_app_media_get_audio_capture(baidu_app_media_t *media,
                                            esp_capture_service_t **out_capture);

/**
 * @brief  Get the application-owned player service
 *
 *         The handle is an audio player or a video player, depending on the
 *         scene. Valid until baidu_app_media_destroy(). Stream 0 is LLM
 *         downlink, stream 1 is URL mix, stream 2 is the local wake-up prompt.
 *
 * @param[in]   media       Media handle returned by baidu_app_media_create()
 * @param[out]  out_player  Receives the borrowed player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_player is NULL
 *       - ESP_ERR_NOT_FOUND    If the player service is unavailable
 */
esp_err_t baidu_app_media_get_player(baidu_app_media_t *media,
                                     esp_player_service_t **out_player);

/**
 * @brief  Link and start the media graph
 *
 *         Order: link, open codecs, player, LLM, then audio capture. Vision
 *         scenes also start camera capture. Failure rolls back.
 *
 * @param[in]  media  Media handle returned by baidu_app_media_create()
 * @param[in]  llm    Application-owned LLM service; ownership is not transferred
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p media or @p llm is NULL
 *       - ESP_ERR_INVALID_STATE  If the graph is already active
 *       - Others                 Error returned by link or service startup
 */
esp_err_t baidu_app_media_start(baidu_app_media_t *media, esp_llm_service_t *llm);

/**
 * @brief  Stop and unlink the media graph
 *
 * @param[in]  media  Media handle returned by baidu_app_media_create()
 *
 * @return
 *       - ESP_OK               On success, including when already stopped
 *       - ESP_ERR_INVALID_ARG  If @p media is NULL
 *       - Others               First error returned during stop or unlink
 */
esp_err_t baidu_app_media_stop(baidu_app_media_t *media);

/**
 * @brief  Destroy all application-owned media resources
 *
 * @note  The borrowed LLM service is never destroyed by this function.
 *
 * @param[in,out]  media  Address of a media handle returned by baidu_app_media_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p *media is NULL
 *       - Others               First error returned while destroying a service
 */
esp_err_t baidu_app_media_destroy(baidu_app_media_t **media);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
