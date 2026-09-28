/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_capture_service.h"
#include "esp_llm_tongyi.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque application-owned Tongyi media graph
 *
 *         Owns board audio, codecs, capture, and a 1-stream player. Vision mode
 *         also owns LCD preview and JPEG snapshots; video is never linked into
 *         Tongyi. Borrows, but never destroys, the LLM passed to
 *         tongyi_app_media_start().
 */
typedef struct tongyi_app_media tongyi_app_media_t;

/**
 * @brief  Tongyi application media configuration
 *
 *         Initialize this structure with TONGYI_APP_MEDIA_CFG_DEFAULT().
 */
typedef struct {
    uint8_t  output_volume;  /*!< Initial player output volume from 0 through 100 */
} tongyi_app_media_cfg_t;

/**
 * @brief  Default initializer for #tongyi_app_media_cfg_t
 */
#define TONGYI_APP_MEDIA_CFG_DEFAULT() {  \
    .output_volume = 80,                  \
}

/**
 * @brief  Create board audio (optional SD, ADC, DAC), default codecs, capture,
 *         and a 1-stream player. Vision mode adds LCD preview and JPEG snap when
 *         the board has camera and LCD. Does not link or start services.
 *
 * @param[in]   tongyi_cfg  Tongyi adapter configuration; caller retains ownership
 * @param[in]   cfg         Media configuration; caller retains ownership
 * @param[out]  out_media   Receives the created media handle; must point to NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument is NULL or volume is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned by board, codec, or service setup
 */
esp_err_t tongyi_app_media_create(const esp_llm_tongyi_cfg_t *tongyi_cfg,
                                 const tongyi_app_media_cfg_t *cfg,
                                 tongyi_app_media_t **out_media);

/**
 * @brief  Get the application-owned audio capture service
 *
 *         The returned borrowed handle remains valid until tongyi_app_media_destroy().
 *
 * @param[in]   media        Media handle returned by tongyi_app_media_create()
 * @param[out]  out_capture  Receives the borrowed audio capture handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p out_capture is NULL
 *       - ESP_ERR_NOT_FOUND    If the audio capture service is unavailable
 */
esp_err_t tongyi_app_media_get_audio_capture(tongyi_app_media_t *media,
                                            esp_capture_service_t **out_capture);

/**
 * @brief  Copy one JPEG from the vision cache (or the next live frame)
 *
 *         Caller owns @p out_data and must free it. Dialog builds return
 *         ESP_ERR_NOT_SUPPORTED.
 *
 * @param[in]   media     Media handle returned by tongyi_app_media_create()
 * @param[out]  out_data  Receives a heap JPEG buffer; caller must free()
 * @param[out]  out_size  Receives the JPEG size in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If an argument is NULL
 *       - ESP_ERR_INVALID_STATE  If the camera is not running
 *       - ESP_ERR_NO_MEM         If the copy buffer cannot be allocated
 *       - ESP_ERR_NOT_SUPPORTED  If the example was built without vision mode
 */
esp_err_t tongyi_app_media_capture_jpeg(tongyi_app_media_t *media,
                                        uint8_t **out_data, size_t *out_size);

/**
 * @brief  Link and start the media graph
 *
 *         Order: link, open codecs, player, LLM, then capture. Vision mode then
 *         starts LCD, camera, and preview. Failure rolls back.
 *
 * @param[in]  media  Media handle returned by tongyi_app_media_create()
 * @param[in]  llm    Application-owned LLM service; ownership is not transferred
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p media or @p llm is NULL
 *       - ESP_ERR_INVALID_STATE  If the graph is already active
 *       - Others                 Error returned by link or service startup
 */
esp_err_t tongyi_app_media_start(tongyi_app_media_t *media, esp_llm_service_t *llm);

/**
 * @brief  Stop and unlink the media graph
 *
 * @param[in]  media  Media handle returned by tongyi_app_media_create()
 *
 * @return
 *       - ESP_OK               On success, including when already stopped
 *       - ESP_ERR_INVALID_ARG  If @p media is NULL
 *       - Others               First error returned during stop or unlink
 */
esp_err_t tongyi_app_media_stop(tongyi_app_media_t *media);

/**
 * @brief  Destroy all application-owned media resources
 *
 * @note  The borrowed LLM service is never destroyed by this function.
 *
 * @param[in,out]  media  Address of a media handle returned by tongyi_app_media_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p media or @p *media is NULL
 *       - Others               First error returned while destroying a service
 */
esp_err_t tongyi_app_media_destroy(tongyi_app_media_t **media);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
