/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#include "power_save_app.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Configure dynamic frequency scaling and create the audio PM lock
 *
 * @param[in,out]  app_ctx  Shared example context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - Others               Errors from power management configuration
 */
esp_err_t power_mgr_configure(power_save_context_t *app_ctx);

/**
 * @brief  Acquire the no-light-sleep lock while audio resources are active
 *
 * @param[in,out]  app_ctx  Shared example context
 * @param[in]      reason   Human-readable reason for acquiring the lock
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - Others               Errors from esp_pm_lock_acquire()
 */
esp_err_t power_mgr_acquire_audio_lock(power_save_context_t *app_ctx, const char *reason);

/**
 * @brief  Release the no-light-sleep lock before entering idle low power
 *
 * @param[in,out]  app_ctx  Shared example context
 * @param[in]      reason   Human-readable reason for releasing the lock
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - Others               Errors from esp_pm_lock_release()
 */
esp_err_t power_mgr_release_audio_lock(power_save_context_t *app_ctx, const char *reason);

/**
 * @brief  Delete PM resources and restore the CPU frequency configuration
 *
 * @param[in,out]  app_ctx  Shared example context
 */
void power_mgr_deinit(power_save_context_t *app_ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
