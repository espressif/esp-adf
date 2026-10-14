/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdint.h>

#include "esp_llm_volc.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque application-owned Volc media graph
 *
 *         Owns board audio, codecs, capture, and a 1-stream player. Borrows,
 *         but never destroys, the LLM passed to volc_app_media_start().
 */
typedef struct volc_app_media volc_app_media_t;

/**
 * @brief  Volc application media configuration
 *
 *         Initialize this structure with VOLC_APP_MEDIA_CFG_DEFAULT().
 */
typedef struct {
    uint8_t  output_volume;  /*!< Initial player output volume from 0 through 100 */
} volc_app_media_cfg_t;

/**
 * @brief  Default initializer for volc_app_media_cfg_t
 */
#define VOLC_APP_MEDIA_CFG_DEFAULT() {  \
    .output_volume = 80,                \
}

/**
 * @brief  Create board audio (optional SD, ADC, DAC), default codecs, capture,
 *         and a 1-stream player. Does not link or start services.
 *
 * @param[in]   volc_cfg  Volc adapter configuration; caller retains ownership
 * @param[in]   cfg        Media configuration; caller retains ownership
 * @param[out]  out_media  Receives the created media handle; must point to NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument is NULL or volume is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned by board, codec, or service setup
 */
esp_err_t volc_app_media_create(const esp_llm_volc_cfg_t *volc_cfg,
                                 const volc_app_media_cfg_t *cfg,
                                 volc_app_media_t **out_media);

/**
 * @brief  Get the application-owned audio capture service
 *
 *         The returned borrowed handle remains valid until volc_app_media_destroy().
 *
 * @param[in]   media        Media handle returned by volc_app_media_create()
 * @param[out]  out_capture  Receives the borrowed audio capture handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_capture is NULL
 *       - ESP_ERR_NOT_FOUND    If the audio capture service is unavailable
 */
esp_err_t volc_app_media_get_audio_capture(volc_app_media_t *media,
                                            esp_capture_service_t **out_capture);

/**
 * @brief  Link and start the media graph
 *
 *         Order: link, open codecs, player, LLM, then capture. Failure rolls back.
 *
 * @param[in]  media  Media handle returned by volc_app_media_create()
 * @param[in]  llm    Application-owned LLM service; ownership is not transferred
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p media or @p llm is NULL
 *       - ESP_ERR_INVALID_STATE  If the graph is already active
 *       - Others                 Error returned by link or service startup
 */
esp_err_t volc_app_media_start(volc_app_media_t *media, esp_llm_service_t *llm);

/**
 * @brief  Stop and unlink the media graph
 *
 * @param[in]  media  Media handle returned by volc_app_media_create()
 *
 * @return
 *       - ESP_OK               On success, including when already stopped
 *       - ESP_ERR_INVALID_ARG  If @p media is NULL
 *       - Others               First error returned during stop or unlink
 */
esp_err_t volc_app_media_stop(volc_app_media_t *media);

/**
 * @brief  Destroy all application-owned media resources
 *
 * @note  The borrowed LLM service is never destroyed by this function.
 *
 * @param[in,out]  media  Address of a media handle returned by volc_app_media_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p *media is NULL
 *       - Others               First error returned while destroying a service
 */
esp_err_t volc_app_media_destroy(volc_app_media_t **media);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
