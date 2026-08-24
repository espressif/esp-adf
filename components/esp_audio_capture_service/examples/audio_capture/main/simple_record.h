/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_err_t simple_record_stream(uint32_t duration_ms);
esp_err_t simple_record_direct(uint32_t duration_ms);
esp_err_t simple_record_ai_direct(uint32_t duration_ms);
esp_err_t simple_record_storage(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
