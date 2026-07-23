/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Create video capture + dummy sink, register MCP tools, and start UART MCP
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Initialization/registration failure
 */
esp_err_t video_capture_mcp_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
