/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <sdkconfig.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define RTSP_WIFI_SSID      CONFIG_RTSP_EXAMPLE_WIFI_SSID
#define RTSP_WIFI_PASSWORD  CONFIG_RTSP_EXAMPLE_WIFI_PASSWORD
#define RTSP_WIFI_WAIT_SEC  10
#define RTSP_DURATION_MS    30000  /* 30 seconds */

#define RTSP_PUSH_URL       CONFIG_RTSP_EXAMPLE_PUSH_URL
#define RTSP_PULL_URL       CONFIG_RTSP_EXAMPLE_PULL_URL
#define RTSP_SERVER_URL     CONFIG_RTSP_EXAMPLE_SERVER_URL

#ifdef __cplusplus
}
#endif  /* __cplusplus */
