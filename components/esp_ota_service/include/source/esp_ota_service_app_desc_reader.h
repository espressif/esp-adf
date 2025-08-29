/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_app_desc.h"

#include "esp_ota_service_source.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Read remote @c esp_app_desc_t through @a src (image header only, then close)
 *
 * @param[in]   src             Stream source
 * @param[in]   uri             URI for @c src->open()
 * @param[out]  out_app_desc    Filled from the remote image
 * @param[out]  out_image_size  Total size from @c get_size() when known, else @c UINT32_MAX
 *
 * @return
 *       - ESP_OK                   On success
 *       - ESP_ERR_INVALID_VERSION  If app descriptor magic is invalid
 *       - ESP_ERR_INVALID_SIZE     If the header cannot be read completely
 *       - ESP_ERR_*                From @c src->open() / read / close
 */
esp_err_t esp_ota_service_app_desc_read_from_source(esp_ota_service_source_t *src, const char *uri, esp_app_desc_t *out_app_desc,
                                                    uint32_t *out_image_size);

/**
 * @brief  Read @c esp_app_desc_t from the currently running application partition
 *
 * @param[out]  out_app_desc  Filled from the running slot
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @a out_app_desc is NULL
 *       - ESP_ERR_NOT_FOUND    If no running OTA app partition is reported
 *       - ESP_ERR_*            From @c esp_ota_get_partition_description()
 */
esp_err_t esp_ota_service_app_desc_read_current(esp_app_desc_t *out_app_desc);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
