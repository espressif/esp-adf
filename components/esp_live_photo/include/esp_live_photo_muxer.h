/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_muxer.h"
#include "mp4_muxer.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Live photo muxer container type
 */
#define ESP_MUXER_TYPE_LIVE_PHOTO  ((esp_muxer_type_t)ESP_MUXER_FOURCC('L', 'P', 'H', 'O'))

/**
 * @brief  Live photo muxer configuration
 */
typedef struct {
    mp4_muxer_config_t  mp4_config;          /*!< MP4 muxer configuration settings */
    uint32_t            reserve_cover_size;  /*!< Reserved space (in bytes) for the cover JPEG before MP4 data
                                                  Must be larger than the actual JPEG size to avoid corruption */
} esp_live_photo_muxer_config_t;

/**
 * @brief  Basic EXIF fields used by live photo cover JPEG
 */
typedef struct {
    const char *make;         /*!< e.g. "espressif" */
    const char *model;        /*!< e.g. "esp32p4" */
    const char *date_time;    /*!< "YYYY:MM:DD HH:MM:SS" */
    uint16_t    orientation;  /*!< TIFF orientation, 1 = normal */
} esp_live_photo_exif_info_t;

/**
 * @brief  Cover JPEG information for live photo muxer
 */
typedef struct {
    uint8_t  *jpeg;      /*!< JPEG cover data */
    uint32_t  jpeg_len;  /*!< Length of JPEG cover */
    uint32_t  jpeg_pts;  /*!< Presentation timestamp for JPEG cover */
} esp_live_photo_muxer_cover_info_t;

/**
 * @brief  Register live photo muxer type to esp_muxer
 *
 * @return
 *       - ESP_MUXER_ERR_OK           On success
 *       - ESP_MUXER_ERR_INVALID_ARG  Invalid registration data
 *       - ESP_MUXER_ERR_NO_MEM       No memory for registration table
 */
esp_muxer_err_t esp_live_photo_muxer_register(void);

/**
 * @brief  Unregister live photo muxer type from esp_muxer
 */
void esp_live_photo_muxer_unregister(void);

/**
 * @brief  Set or replace cover JPEG data
 *
 * @param[in]  muxer       Live photo muxer handle
 * @param[in]  cover_info  Cover JPEG buffer and metadata
 *
 * @return
 *       - ESP_MUXER_ERR_OK           On success
 *       - ESP_MUXER_ERR_INVALID_ARG  Invalid input argument
 *       - ESP_MUXER_ERR_NO_MEM       Not enough memory
 *       - ESP_MUXER_ERR_FAIL         Internal write/seek failure
 */
esp_muxer_err_t esp_live_photo_muxer_set_cover_jpeg(esp_muxer_handle_t muxer, esp_live_photo_muxer_cover_info_t *cover_info);

/**
 * @brief  Set basic EXIF fields used when rewriting cover JPEG
 *
 * @param[in]  muxer      Live photo muxer handle
 * @param[in]  exif_info  EXIF information to apply
 *
 * @return
 *       - ESP_MUXER_ERR_OK           On success
 *       - ESP_MUXER_ERR_INVALID_ARG  Invalid input argument
 */
esp_muxer_err_t esp_live_photo_muxer_set_exif_info(esp_muxer_handle_t muxer, const esp_live_photo_exif_info_t *exif_info);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
