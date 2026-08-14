/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

esp_err_t example_wifi_start(void);
esp_err_t example_wifi_get_ip_str(char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
