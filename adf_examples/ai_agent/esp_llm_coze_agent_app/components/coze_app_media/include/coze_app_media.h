/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdint.h>

#include "esp_llm_coze.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define COZE_APP_MEDIA_TTS_STREAM           ((esp_media_stream_id_t)0)  /*!< Linked LLM downlink */
#define COZE_APP_MEDIA_WAKEUP_STREAM        ((esp_media_stream_id_t)1)  /*!< Local wake-up prompt */
#define COZE_APP_MEDIA_PLAYER_STREAM_NUM    2                           /*!< TTS + wake-up */

/**
 * @brief  Opaque application-owned Coze media graph
 *
 *         Owns board audio, codecs, capture, and a 2-stream player. Stream 0
 *         is LLM downlink. Stream 1 is the local wake-up prompt. Borrows, but
 *         never destroys, the LLM passed to coze_app_media_start().
 */
typedef struct coze_app_media coze_app_media_t;

/**
 * @brief  Coze application media configuration
 *
 *         Initialize this structure with COZE_APP_MEDIA_CFG_DEFAULT().
 */
typedef struct {
    uint8_t  output_volume;  /*!< Initial player output volume from 0 through 100 */
} coze_app_media_cfg_t;

/**
 * @brief  Default initializer for coze_app_media_cfg_t
 */
#define COZE_APP_MEDIA_CFG_DEFAULT() {  \
    .output_volume = 80,                \
}

/**
 * @brief  Create board audio (optional SD, ADC, DAC), default codecs, capture,
 *         and a 2-stream player. Does not link or start services.
 *
 * @param[in]   coze_cfg  Coze adapter configuration; caller retains ownership
 * @param[in]   cfg        Media configuration; caller retains ownership
 * @param[out]  out_media  Receives the created media handle; must point to NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument is NULL or volume is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned by board, codec, or service setup
 */
esp_err_t coze_app_media_create(const esp_llm_coze_cfg_t *coze_cfg,
                                 const coze_app_media_cfg_t *cfg,
                                 coze_app_media_t **out_media);

/**
 * @brief  Get the application-owned audio capture service
 *
 *         The returned borrowed handle remains valid until coze_app_media_destroy().
 *
 * @param[in]   media        Media handle returned by coze_app_media_create()
 * @param[out]  out_capture  Receives the borrowed audio capture handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_capture is NULL
 *       - ESP_ERR_NOT_FOUND    If the audio capture service is unavailable
 */
esp_err_t coze_app_media_get_audio_capture(coze_app_media_t *media,
                                            esp_capture_service_t **out_capture);

/**
 * @brief  Get the application-owned player service
 *
 *         Valid until coze_app_media_destroy(). Stream 0 is LLM downlink.
 *         Stream 1 is the local wake-up prompt.
 *
 * @param[in]   media       Media handle returned by coze_app_media_create()
 * @param[out]  out_player  Receives the borrowed player handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_player is NULL
 *       - ESP_ERR_NOT_FOUND    If the player service is unavailable
 */
esp_err_t coze_app_media_get_player(coze_app_media_t *media,
                                    esp_player_service_t **out_player);

/**
 * @brief  Link and start the media graph
 *
 *         Order: link, open codecs, player, LLM, then capture. Failure rolls back.
 *
 * @param[in]  media  Media handle returned by coze_app_media_create()
 * @param[in]  llm    Application-owned LLM service; ownership is not transferred
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p media or @p llm is NULL
 *       - ESP_ERR_INVALID_STATE  If the graph is already active
 *       - Others                 Error returned by link or service startup
 */
esp_err_t coze_app_media_start(coze_app_media_t *media, esp_llm_service_t *llm);

/**
 * @brief  Stop and unlink the media graph
 *
 * @param[in]  media  Media handle returned by coze_app_media_create()
 *
 * @return
 *       - ESP_OK               On success, including when already stopped
 *       - ESP_ERR_INVALID_ARG  If @p media is NULL
 *       - Others               First error returned during stop or unlink
 */
esp_err_t coze_app_media_stop(coze_app_media_t *media);

/**
 * @brief  Destroy all application-owned media resources
 *
 * @note  The borrowed LLM service is never destroyed by this function.
 *
 * @param[in,out]  media  Address of a media handle returned by coze_app_media_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p *media is NULL
 *       - Others               First error returned while destroying a service
 */
esp_err_t coze_app_media_destroy(coze_app_media_t **media);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
