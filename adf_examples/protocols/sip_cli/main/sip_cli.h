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
 * @brief  Create the console service and register the 'sip' and 'wifi' commands
 *
 * @return
 *       - ESP_OK  Console is running
 *       - Others  Error from the CLI service create, register or start
 */
esp_err_t sip_cli_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
