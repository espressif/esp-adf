/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "tongyi_app_media.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Start the UART CLI (`esp_cli_service`)
 *
 *         Dialog and Vision share the `tongyi>` prompt and built-in `sys_*`
 *         commands. Vision also registers `snap`.
 *
 * @note  Call after media and the LLM are ready.
 *
 * @param[in]  media  Application media graph; may be NULL when snap is unused
 * @param[in]  llm    LLM service; may be NULL when snap is unused
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_STATE  CLI is already running
 *       - Others               Error returned by esp_cli_service
 */
esp_err_t app_cli_start(tongyi_app_media_t *media, esp_llm_service_t *llm);

/**
 * @brief  Stop and destroy the UART CLI
 *
 * @note  Safe when the CLI was never started.
 */
void app_cli_stop(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
