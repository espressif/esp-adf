/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_ota_service_source.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  HTTP/HTTPS OTA source configuration
 */
typedef struct {
    const char *cert_pem;    /*!< Server CA certificate in PEM format (not copied).
                              *   NULL: use the IDF certificate bundle if available. */
    int         timeout_ms;  /*!< Read timeout in ms. 0 = use default (10 000 ms). */
    int         buf_size;    /*!< Internal ring-buffer size in bytes. 0 = use default (4096 B). */
} esp_ota_service_source_http_cfg_t;

/**
 * @brief  Create an HTTP/HTTPS OTA source
 *
 *         Handles both http:// and https:// URIs.  The actual connection is established
 *         in open(), so this call succeeds when memory allocation succeeds.
 *
 * @param[in]   cfg      Configuration. NULL uses all defaults.
 * @param[out]  out_src  On success, receives the new source; cleared to NULL on failure.
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If out_src is NULL
 *       - ESP_ERR_NO_MEM       On allocation failure
 */
esp_err_t esp_ota_service_source_http_create(const esp_ota_service_source_http_cfg_t *cfg, esp_ota_service_source_t **out_src);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
