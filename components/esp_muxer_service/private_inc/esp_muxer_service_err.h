/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Log and return a specific error
 */
#define RET_FOR(_err, _fmt, ...)  do {                                \
    ESP_LOGE(TAG, "%s:%d " _fmt, __func__, __LINE__, ##__VA_ARGS__);  \
    return (_err);                                                    \
} while (0)

/**
 * @brief  Evaluate _expr; on failure log and return its esp_err_t
 */
#define RET_CHK(_expr, _fmt, ...)  do {                                   \
    esp_err_t _muxer_err = (_expr);                                       \
    if (_muxer_err != ESP_OK) {                                           \
        ESP_LOGE(TAG, "%s:%d " _fmt, __func__, __LINE__, ##__VA_ARGS__);  \
        return _muxer_err;                                                \
    }                                                                     \
} while (0)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
