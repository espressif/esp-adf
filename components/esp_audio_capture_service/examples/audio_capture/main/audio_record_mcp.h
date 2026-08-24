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
 * @brief  Create capture + dummy sink, register MCP tools, and start UART MCP
 *
 *         Services remain alive for PC-side MCP scripts. No-op when MCP is disabled.
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Initialization/registration failure
 */
esp_err_t audio_record_mcp_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
