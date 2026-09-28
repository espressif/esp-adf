/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_fourcc.h"
#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_LLM_ADAPTER_MOCK  "mock"

/**
 * @brief  Mock backend configuration
 *
 *         Zero initialization is valid. When emit_start_events is true, start()
 *         emits backend events only: CONNECTED, TEXT, FUNCTION_CALL, AUDIO,
 *         FINISHED. WAKEUP and VAD come from service audio_ai_cfg, not this adapter.
 */
typedef struct {
    esp_llm_service_cfg_t  service;            /*!< Media wiring, audio_ai_cfg, name */
    bool                   emit_start_events;  /*!< True to emit the default event sequence when started */
} esp_llm_mock_cfg_t;

#define ESP_LLM_MOCK_AUDIO_STREAM_CFG() \
    ESP_LLM_STREAM_CFG_AUDIO(true, ESP_FOURCC_PCM, 16000, 16, 1, 0)

#define ESP_LLM_MOCK_CFG_DEFAULT()                                                 \
    ((esp_llm_mock_cfg_t) {                                                        \
        .emit_start_events = true,                                                 \
        .service = {                                                               \
            .streams = {                                                           \
                [ESP_LLM_MEDIA_STREAM_AUDIO_UP] = ESP_LLM_MOCK_AUDIO_STREAM_CFG(), \
                [ESP_LLM_MEDIA_STREAM_AUDIO_DN] = ESP_LLM_MOCK_AUDIO_STREAM_CFG(), \
            },                                                                     \
        },                                                                         \
    })

/**
 * @brief  Create a mock adapter-backed LLM service instance
 *
 * @param[in]   cfg          Mock adapter configuration (backend + `service`)
 * @param[out]  out_service  Created service handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out_service is NULL
 */
esp_err_t esp_llm_mock_create(const esp_llm_mock_cfg_t *cfg,
                              esp_llm_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
