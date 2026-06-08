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
 * @brief  Configure enabled wakeup sources
 *
 * @param[in,out]  app_ctx  Shared example context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - Others               Errors from timer, GPIO, or UART wakeup setup
 */
esp_err_t wakeup_mgr_configure_sources(power_save_context_t *app_ctx);

/**
 * @brief  Enter idle low power and wait for a wakeup event
 *
 * @param[in,out]  app_ctx  Shared example context
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid context
 *       - ESP_ERR_TIMEOUT      No wakeup source fired before the wait timeout
 *       - Others               Errors from PM lock or Wi-Fi power save mode transitions
 */
esp_err_t wakeup_mgr_wait(power_save_context_t *app_ctx);

/**
 * @brief  Get a printable name for wakeup source bits
 *
 * @param[in]   bits  Wakeup event bits
 * @param[out]  name  Wakeup source name string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If name is NULL
 */
esp_err_t wakeup_mgr_wakeup_source_name(EventBits_t bits, const char **name);

/**
 * @brief  Release wakeup manager resources
 *
 * @param[in,out]  app_ctx  Shared example context
 */
void wakeup_mgr_deinit(power_save_context_t *app_ctx);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
