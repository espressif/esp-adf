/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_TONGYI_UUID_STR_LEN  36
#define ESP_TONGYI_UUID_BUF_LEN  (ESP_TONGYI_UUID_STR_LEN + 1)

/**
 * @brief  Compute the buffer size needed by #esp_tongyi_base64_encode
 *
 *         Result includes the trailing NUL. Rejects overflow.
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   data_len  Input length in bytes
 * @param[out]  out_len   Receives the required size in bytes
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   If @p out_len is NULL
 *       - ESP_ERR_INVALID_SIZE  If the encoded size would overflow size_t
 */
esp_err_t esp_tongyi_base64_encoded_size(size_t data_len, size_t *out_len);

/**
 * @brief  Encode binary data to a base64 string
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   data      Input data
 * @param[in]   data_len  Input length in bytes
 * @param[out]  out_buf   Caller-provided buffer
 * @param[in]   out_len   Size of @p out_buf in bytes
 *
 * @return
 *       - ESP_OK                On success
 *       - ESP_ERR_INVALID_ARG   If an argument is invalid or @p out_len is too small
 *       - ESP_ERR_INVALID_SIZE  If the encoded size would overflow
 *       - ESP_FAIL              If mbedtls base64 encoding failed
 */
esp_err_t esp_tongyi_base64_encode(const uint8_t *data, size_t data_len, char *out_buf, size_t out_len);

/**
 * @brief  Generate a random UUID v4 string
 *
 *         Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 *
 * @note  This function is not ISR-safe.
 *
 * @param[out]  out_buf  Caller-provided buffer; must hold at least #ESP_TONGYI_UUID_BUF_LEN bytes
 * @param[in]   out_len  Size of @p out_buf in bytes
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p out_buf is NULL or @p out_len is too small
 */
esp_err_t esp_tongyi_generate_uuid(char *out_buf, size_t out_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
