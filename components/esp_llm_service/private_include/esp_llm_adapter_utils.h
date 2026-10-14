/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Resolve the adapter context behind a service handle
 *
 *         For backend-specific public APIs such as esp_llm_volc_send_text_to_tts().
 *         adapter_name guards against calling one backend's API on another
 *         backend's service. The returned context is owned by the service and
 *         stays valid until the service is destroyed; the caller is responsible
 *         for any backend-side locking and state checks.
 *
 * @param[in]   service       LLM service handle
 * @param[in]   adapter_name  Expected adapter name, for example ESP_LLM_ADAPTER_VOLC
 * @param[out]  out_ctx       Adapter context on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service, adapter_name, or out_ctx is NULL
 *       - ESP_ERR_NOT_SUPPORTED  If the service runs a different adapter
 *       - ESP_ERR_INVALID_STATE  If the adapter context is not available
 */
esp_err_t esp_llm_adapter_ctx_get(esp_llm_service_t *service,
                                  const char *adapter_name,
                                  void **out_ctx);

/**
 * @brief  Derive a PCM presentation timestamp and advance the byte cursor
 *
 *         Call before each downlink audio frame. Reset *inout_bytes to 0 at the
 *         start of a new utterance (ESP_MEDIA_FRAME_FLAG_KEY).
 *
 * @param[in]      sample_rate      Sample rate in Hz
 * @param[in]      channel          Channel count
 * @param[in]      bits_per_sample  Bits per sample (8 for A-law/U-law)
 * @param[in,out]  inout_bytes      Accumulated PCM bytes before this frame
 * @param[in]      payload_size     Size of this frame in bytes
 *
 * @return
 *       - PTS                 Presentation timestamp in milliseconds
 *       - ESP_LLM_PTS_ABSENT  If the format arguments are invalid
 */
static inline int64_t esp_llm_pts_from_pcm(uint32_t sample_rate, uint8_t channel,
                                           uint8_t bits_per_sample, uint64_t *inout_bytes,
                                           size_t payload_size)
{
    if (inout_bytes == NULL || sample_rate == 0 || channel == 0 || bits_per_sample < 8U) {
        return ESP_LLM_PTS_ABSENT;
    }
    uint32_t bytes_per_sec = sample_rate * (uint32_t)channel * (bits_per_sample / 8U);
    if (bytes_per_sec == 0) {
        return ESP_LLM_PTS_ABSENT;
    }
    int64_t pts = (int64_t)(*inout_bytes * 1000ULL / bytes_per_sec);
    *inout_bytes += payload_size;
    return pts;
}

/**
 * @brief  Derive a video presentation timestamp and advance the frame cursor
 *
 *         Reset *inout_frames to 0 when starting a new video stream
 *         (ESP_MEDIA_FRAME_FLAG_KEY).
 *
 * @param[in]      fps           Frames per second; 0 is treated as 1
 * @param[in,out]  inout_frames  Accumulated frame count before this frame
 *
 * @return
 *       - PTS                 Presentation timestamp in milliseconds
 *       - ESP_LLM_PTS_ABSENT  If inout_frames is NULL
 */
static inline int64_t esp_llm_pts_from_video_fps(uint16_t fps, uint64_t *inout_frames)
{
    if (inout_frames == NULL) {
        return ESP_LLM_PTS_ABSENT;
    }
    uint16_t rate = fps != 0 ? fps : 1U;
    int64_t pts = (int64_t)(*inout_frames * 1000ULL / rate);
    (*inout_frames)++;
    return pts;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
