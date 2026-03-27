/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Live photo extractor container type
 */
#define ESP_EXTRACTOR_TYPE_LIVE_PHOTO  ((esp_extractor_type_t)EXTRACTOR_4CC('L', 'P', 'H', 'O'))

/**
 * @brief  Register live photo extractor type to esp_extractor
 *
 * @return
 *       - ESP_EXTRACTOR_ERR_OK       On success
 *       - ESP_EXTRACTOR_ERR_INV_ARG  Invalid registration parameters
 *       - ESP_EXTRACTOR_ERR_NO_MEM   No memory for registration table
 */
esp_extractor_err_t esp_live_photo_extractor_register(void);

/**
 * @brief  Unregister live photo extractor type from esp_extractor
 *
 * @return
 *       - ESP_EXTRACTOR_ERR_OK       On success
 *       - ESP_EXTRACTOR_ERR_INV_ARG  Invalid argument
 */
esp_extractor_err_t esp_live_photo_extractor_unregister(void);

/**
 * @brief  Read cover JPEG as extractor frame from output pool
 *
 * @note  The cover frame is stored in extractor pool like audio and video frame
 *        User need call `esp_extractor_release_frame` when frame unused anymore
 *
 * @param[in]   extractor  Extractor handle
 * @param[out]  cover      Output frame information
 *
 * @return
 *       - ESP_EXTRACTOR_ERR_OK         On success
 *       - ESP_EXTRACTOR_ERR_INV_ARG    Invalid input argument
 *       - ESP_EXTRACTOR_ERR_NOT_FOUND  Cover JPEG not found
 *       - ESP_EXTRACTOR_ERR_NO_MEM     No pool memory available
 *       - ESP_EXTRACTOR_ERR_READ       IO read failure
 */
esp_extractor_err_t esp_live_photo_extractor_read_cover_frame(esp_extractor_handle_t extractor,
                                                              esp_extractor_frame_info_t *cover);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
