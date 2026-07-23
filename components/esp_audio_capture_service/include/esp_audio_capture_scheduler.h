/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_audio_capture_service_create()
 *
 *         Capture pipeline and AI audio tasks request scheduler overrides under
 *         this name when cfg->service_name is not set.
 */
#define ESP_AUDIO_CAPTURE_SERVICE_NAME       "audio-rec"

/** Audio source read thread (esp_capture gmf_audio_src) */
#define ESP_AUDIO_CAPTURE_TASK_AUD_SRC       "AUD_SRC"
/** Shared audio source pipeline worker (multi-stream only) */
#define ESP_AUDIO_CAPTURE_TASK_AUD_SRC_PIPE  "aud_src"
/** Primary audio encode pipeline */
#define ESP_AUDIO_CAPTURE_TASK_AENC_0        "aenc_0"
/** Secondary audio encode pipeline */
#define ESP_AUDIO_CAPTURE_TASK_AENC_1        "aenc_1"
/** AI audio GMF pipeline worker */
#define ESP_AUDIO_CAPTURE_TASK_AI_PIPE       "ai_audio_pipe"
/** AFE manager feed task (esp_gmf_afe_manager) */
#define ESP_AUDIO_CAPTURE_TASK_AFE_FEED      "afe_feed"
/** AFE manager fetch task (esp_gmf_afe_manager) */
#define ESP_AUDIO_CAPTURE_TASK_AFE_FETCH     "afe_fetch"

/**
 * @brief  Scheduler thread / pipeline task names for esp_audio_capture_service
 *
 * Capture task flow (data moves left to right):
 *
 * Single stream (one encode stream):
 *
 *     mic/codec --> AUD_SRC --> aenc_0 --> stream
 *                 (gmf_audio_src read thread inside aud_src element;
 *                  aud_src element is embedded in aenc_0 pipeline)
 *
 * Multiple streams (two or more encode streams):
 *
 *     mic/codec --> AUD_SRC --> aud_src --+--> aenc_0 --> stream 0
 *                 (read thread)  (shared  +--> aenc_1 --> stream 1
 *                                 source pipeline with share_copier)
 *
 * AI audio enabled (AI replaces hardware codec as capture source):
 *
 *     Without AFE (standalone GMF elements, e.g. ai_aec / ai_wn):
 *
 *         ai_audio_pipe --> AUD_SRC --> (capture paths above)
 *
 *     With AFE (compact esp_gmf_afe_manager):
 *
 *         ai_audio_pipe --> afe_feed --> afe_fetch --> AUD_SRC --> ...
 *                           (feed mic)   (fetch PCM)
 *
 * Name mapping:
 *   AUD_SRC       - gmf_audio_src read thread (esp_capture)
 *   aud_src       - shared source pipeline worker (multi-stream only)
 *   aenc_0/aenc_1 - audio encode pipeline workers
 *   ai_audio_pipe - AI GMF pipeline worker
 *   afe_feed      - AFE manager feed task
 *   afe_fetch     - AFE manager fetch task
 */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
