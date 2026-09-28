/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <limits.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/base64.h"

#include "esp_tongyi_utils.h"

static const char *TAG = "TONGYI_UTIL";

esp_err_t esp_tongyi_base64_encoded_size(size_t data_len, size_t *out_len)
{
    if (out_len == NULL) {
        ESP_LOGE(TAG, "Base64 encoded size failed: out_len is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (data_len > SIZE_MAX - 2U) {
        ESP_LOGE(TAG, "Base64 encoded size failed: encoded size overflow, data_len=%zu", data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    size_t groups = (data_len + 2U) / 3U;
    if (groups > SIZE_MAX / 4U) {
        ESP_LOGE(TAG, "Base64 encoded size failed: encoded size overflow, data_len=%zu", data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    size_t encoded = groups * 4U;
    if (encoded > SIZE_MAX - 1U) {
        ESP_LOGE(TAG, "Base64 encoded size failed: encoded size overflow, data_len=%zu", data_len);
        return ESP_ERR_INVALID_SIZE;
    }
    *out_len = encoded + 1U;
    return ESP_OK;
}

esp_err_t esp_tongyi_base64_encode(const uint8_t *data, size_t data_len, char *out_buf, size_t out_len)
{
    if (data == NULL || data_len == 0) {
        ESP_LOGE(TAG, "Base64 encode failed: data is NULL or empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (out_buf == NULL) {
        ESP_LOGE(TAG, "Base64 encode failed: out_buf is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_len = 0;
    esp_err_t size_ret = esp_tongyi_base64_encoded_size(data_len, &required_len);
    if (size_ret != ESP_OK) {
        return size_ret;  /* esp_tongyi_base64_encoded_size already logged */
    }
    if (out_len < required_len) {
        ESP_LOGE(TAG, "Base64 encode failed: out_len=%zu, need at least %zu", out_len, required_len);
        return ESP_ERR_INVALID_ARG;
    }

    size_t encoded_len = 0;
    int ret = mbedtls_base64_encode((unsigned char *)out_buf, out_len, &encoded_len, data, data_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Base64 encode failed: mbedtls error -0x%x", (unsigned int)-ret);
        return ESP_FAIL;
    }

    out_buf[encoded_len] = '\0';
    return ESP_OK;
}

esp_err_t esp_tongyi_generate_uuid(char *out_buf, size_t out_len)
{
    if (out_buf == NULL) {
        ESP_LOGE(TAG, "Generate uuid failed: out_buf is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (out_len < ESP_TONGYI_UUID_BUF_LEN) {
        ESP_LOGE(TAG, "Generate uuid failed: out_len=%zu, need at least %d",
                 out_len, ESP_TONGYI_UUID_BUF_LEN);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t rand_bytes[16];
    esp_fill_random(rand_bytes, sizeof(rand_bytes));
    /* RFC 4122 UUID v4: version 4, variant 10xx. */
    rand_bytes[6] = (uint8_t)((rand_bytes[6] & 0x0FU) | 0x40U);
    rand_bytes[8] = (uint8_t)((rand_bytes[8] & 0x3FU) | 0x80U);

    static const char hex[] = "0123456789abcdef";
    int idx = 0;
    for (int i = 0; i < 16; i++) {
        if (idx == 8 || idx == 13 || idx == 18 || idx == 23) {
            out_buf[idx++] = '-';
        }
        out_buf[idx++] = hex[(rand_bytes[i] >> 4) & 0x0F];
        out_buf[idx++] = hex[rand_bytes[i] & 0x0F];
    }
    out_buf[idx] = '\0';
    return ESP_OK;
}
