/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Save Wi-Fi credentials and connect
 *
 * @param[in]  ssid      Network name
 * @param[in]  password  Network password; empty string for an open AP
 *
 * @return
 *       - ESP_OK                 Station got an IP address
 *       - ESP_ERR_INVALID_ARG    ssid is empty
 *       - ESP_ERR_INVALID_STATE  Wi-Fi service is unavailable
 *       - Others                 Error returned by the Wi-Fi service
 */
esp_err_t rtsp_example_wifi_connect(const char *ssid, const char *password);

/**
 * @brief  Copy the station IP address into a caller buffer
 *
 *         Writes the "DEVICE_IP" placeholder when no address is assigned yet,
 *         so printed ffplay commands always stay readable.
 *
 * @param[out]  buf      Destination buffer
 * @param[in]   buf_len  Size of buf, at least 16 bytes
 */
void rtsp_example_fill_ip(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
