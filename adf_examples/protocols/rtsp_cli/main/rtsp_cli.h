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
 * @brief  Create the CLI service and start the console REPL
 *
 *         Registers the 'rtsp' and 'wifi' commands on the prompt "rtsp>".
 *
 * @return
 *       - ESP_OK  Console is running
 *       - Others  Error returned by the CLI service
 */
esp_err_t rtsp_cli_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
