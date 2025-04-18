/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

 #pragma once

/**
 * @brief Structure representing an HTTP response
 */
typedef struct {
    char *body;      /**< Pointer to the response body content */
    int   body_len;  /**< Length of the response body in bytes */
} http_response_t;

/**
 * @brief Structure representing a single HTTP request header key-value pair
 */
typedef struct {
    const char *key;    /**< The header field name (e.g., "Content-Type") */
    const char *value;  /**< The header field value (e.g., "application/json") */
} http_req_header_t;

 /**
 * @brief Send an HTTP POST request to the specified URL
 *
 * @note This function sends an HTTP POST request with the given headers and body to the specified URL,
 *       and fills the provided response structure with the response data
 *
 * @param[in]  url       The target URL to send the POST request to
 * @param[in]  header    Pointer to the HTTP request header structure. Can be NULL if no custom headers are needed
 * @param[in]  body      The request body to send. Can be NULL if body length is 0
 * @param[out] response  Pointer to the response structure to be filled with the server's response
 *
 * @return 0 on success, or a negative error code on failure
 */
 int http_client_post(const char *url, http_req_header_t *header , char *body, http_response_t *response);
  