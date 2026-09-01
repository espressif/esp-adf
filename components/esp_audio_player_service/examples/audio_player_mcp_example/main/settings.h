/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Optional demo MP3 on SD card for remote set_url / play via MCP. */
#define EXAMPLE_MCP_DEMO_MP3_FILENAME  "test.mp3"

#define EXAMPLE_WIFI_SSID      CONFIG_EXAMPLE_WIFI_SSID
#define EXAMPLE_WIFI_PASSWORD  CONFIG_EXAMPLE_WIFI_PASSWORD
#define EXAMPLE_MCP_HTTP_PORT  CONFIG_EXAMPLE_MCP_HTTP_PORT
#define EXAMPLE_MCP_HTTP_URI   CONFIG_EXAMPLE_MCP_HTTP_URI

#ifdef __cplusplus
}
#endif  /* __cplusplus */
