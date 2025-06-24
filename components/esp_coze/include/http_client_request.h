/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief  Structure to hold HTTP response data
 */
typedef struct {
    char *body;            /*!< Response body (Need to be freed by the caller) */
    int   body_len;        /*!< Response body length */
} http_response_t;

/**
 * @brief  Structure to represent an HTTP request header key-value pair
 */
typedef struct {
    const char *key;       /*!< Header key string */
    const char *value;     /*!< Header value string */
} http_req_header_t;

/**
 * @brief  Send an HTTP POST request
 *
 * @param[in]  url       The URL to which the POST request is sent
 * @param[in]  header    HTTP request headers (terminated by a {NULL, NULL} entry)
 * @param[in]  body      POST request body
 * @param[out] response  HTTP response data (must be freed by the caller)
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  On failure
 */
esp_err_t http_client_post(const char *url, http_req_header_t *header, char *body, http_response_t *response);

#ifdef __cplusplus
}
#endif
