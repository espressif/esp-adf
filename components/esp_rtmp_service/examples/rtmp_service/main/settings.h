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

#define RTMP_WIFI_SSID      CONFIG_RTMP_EXAMPLE_WIFI_SSID
#define RTMP_WIFI_PASSWORD  CONFIG_RTMP_EXAMPLE_WIFI_PASSWORD
#define RTMP_WIFI_WAIT_SEC  CONFIG_RTMP_EXAMPLE_WIFI_WAIT_SEC
#define RTMP_DURATION_MS    30000  /* 30 seconds */

#define RTMP_PUSH_URL       CONFIG_RTMP_EXAMPLE_PUSH_URL
#define RTMP_PULL_URL       CONFIG_RTMP_EXAMPLE_PULL_URL
#define RTMP_SERVER_URL     CONFIG_RTMP_EXAMPLE_SERVER_URL
#define RTMP_SERVER_STREAM  CONFIG_RTMP_EXAMPLE_SERVER_STREAM

#ifdef __cplusplus
}
#endif  /* __cplusplus */
