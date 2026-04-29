/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Default Coze streaming WebSocket base URL (path `/v1/chat`). */
#define COZE_DEFAULT_WS_BASE_URL  "wss://ws.coze.cn/v1/chat"

/**
 * @brief  Pack four bytes into a FOURCC uint32_t (same layout as ESP_AUDIO_FOURCC_TO_INT
 *         in esp_audio_types.h).
 */
#define ESP_COZE_AUDIO_FOURCC_TO_INT(a, b, c, d)  \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/**
 * @brief  Audio codec identifiers for uplink/downlink in Coze streaming protocols.
 */
typedef enum {
    ESP_COZE_AUDIO_TYPE_OPUS  = ESP_COZE_AUDIO_FOURCC_TO_INT('O', 'P', 'U', 'S'),  /*!< OPUS */
    ESP_COZE_AUDIO_TYPE_G711A = ESP_COZE_AUDIO_FOURCC_TO_INT('A', 'L', 'A', 'W'),  /*!< G.711 A-law */
    ESP_COZE_AUDIO_TYPE_G711U = ESP_COZE_AUDIO_FOURCC_TO_INT('U', 'L', 'A', 'W'),  /*!< G.711 mu-law */
    ESP_COZE_AUDIO_TYPE_PCM   = ESP_COZE_AUDIO_FOURCC_TO_INT('P', 'C', 'M', ' '),  /*!< PCM */
} esp_coze_audio_type_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
