/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_capture_types.h"
#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define RET_FOR(_err, _fmt, ...)  do {                                \
    ESP_LOGE(TAG, "%s:%d " _fmt, __func__, __LINE__, ##__VA_ARGS__);  \
    return (_err);                                                    \
} while (0)

#define RET_CHK(_expr, _fmt, ...)  do {                                   \
    esp_err_t _capture_err = (_expr);                                     \
    if (_capture_err != ESP_OK) {                                         \
        ESP_LOGE(TAG, "%s:%d " _fmt, __func__, __LINE__, ##__VA_ARGS__);  \
        return _capture_err;                                              \
    }                                                                     \
} while (0)

static inline esp_err_t capture_err_to_esp(esp_capture_err_t err)
{
    switch (err) {
        case ESP_CAPTURE_ERR_OK:
            return ESP_OK;
        case ESP_CAPTURE_ERR_INVALID_ARG:
            return ESP_ERR_INVALID_ARG;
        case ESP_CAPTURE_ERR_NO_MEM:
        case ESP_CAPTURE_ERR_NO_RESOURCES:
            return ESP_ERR_NO_MEM;
        case ESP_CAPTURE_ERR_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        case ESP_CAPTURE_ERR_NOT_FOUND:
            return ESP_ERR_NOT_FOUND;
        case ESP_CAPTURE_ERR_TIMEOUT:
            return ESP_ERR_TIMEOUT;
        case ESP_CAPTURE_ERR_INVALID_STATE:
            return ESP_ERR_INVALID_STATE;
        default:
            return ESP_FAIL;
    }
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
