/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_llm_service.h"

typedef struct esp_player_service esp_player_service_t;

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Parse a Baidu FunctionCall payload and apply volume changes
 *
 *         Recognizes `adjust_volume` with mode `up`, `down`, or `set`.
 *         After the player volume is updated, the Baidu TTS path speaks
 *         a confirmation such as "音量已调到80". Other function names are ignored.
 *
 * @param[in]  llm       Running Baidu LLM service
 * @param[in]  player    Application-owned audio player
 * @param[in,out]  volume  Last applied volume; updated on success
 * @param[in]  json_str  FunctionCall JSON from the agent; ignored if NULL
 */
void app_agent_parse_fc_volume_adjust(esp_llm_service_t *llm,
                                      esp_player_service_t *player,
                                      int *volume,
                                      const char *json_str);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
