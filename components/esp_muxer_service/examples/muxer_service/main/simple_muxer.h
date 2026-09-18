/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_muxer_service_ops.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Dummy AAC source linked to a TS muxer
 *
 * @param[in]  duration_ms  Run duration
 * @param[in]  mode         Streaming, storage, or both
 */
esp_err_t simple_muxer_audio(uint32_t duration_ms, esp_muxer_service_mode_t mode);

/**
 * @brief  Dummy H264 + AAC source linked to a TS muxer
 *
 * @param[in]  duration_ms  Run duration
 * @param[in]  mode         Streaming, storage, or both
 */
esp_err_t simple_muxer_av(uint32_t duration_ms, esp_muxer_service_mode_t mode);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
