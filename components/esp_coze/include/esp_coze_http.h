/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  HTTP response buffer returned by #esp_coze_http_post().
 *
 *         On success the caller takes ownership of body and must `free()` it.
 */
typedef struct {
    char *body;      /*!< Response body bytes; NUL-terminated for convenience */
    int   body_len;  /*!< Length of body in bytes, excluding the trailing NUL */
} esp_coze_http_response_t;

/**
 * @brief  One HTTP header field for #esp_coze_http_post().
 *
 *         Pass an array terminated by a `{NULL, NULL}` sentinel entry.
 */
typedef struct {
    const char *key;    /*!< Header name (for example, "Authorization") */
    const char *value;  /*!< Header value corresponding to key */
} esp_coze_http_header_t;

/**
 * @brief  Issue a synchronous HTTP POST and capture the response body.
 *
 *         Blocks the calling task until the response is fully received or the
 *         internal timeout elapses. On success the response body is stored in
 *         response->body (caller takes ownership and must `free()` it) and
 *         response->body_len holds its byte length excluding the trailing NUL.
 *
 * @note  Not ISR-safe; call only from a task context.
 *
 * @param[in]   url       Target URL
 * @param[in]   header    Optional header list terminated by `{NULL, NULL}`, or NULL
 * @param[in]   body      Optional NUL-terminated request body, or NULL
 * @param[out]  response  Filled with the response body on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  url or response is NULL
 *       - ESP_ERR_NO_MEM       Allocation failure
 *       - ESP_ERR_TIMEOUT      Request did not complete in time
 *       - ESP_FAIL             Transport-level failure
 */
esp_err_t esp_coze_http_post(const char *url,
                             esp_coze_http_header_t *header,
                             char *body,
                             esp_coze_http_response_t *response);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
