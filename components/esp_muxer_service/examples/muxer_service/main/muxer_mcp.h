/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Create dummy SRC + muxer, register MCP tools, and start UART MCP
 */
esp_err_t muxer_mcp_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
