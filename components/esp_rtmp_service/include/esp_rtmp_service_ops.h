/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_rtmp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Apply optional role setup before start
 *
 *         May be called multiple times while the service is stopped. Fields left
 *         as 0/NULL keep the previous value or built-in default.
 *
 * @param[in]  service  RTMP service handle
 * @param[in]  setup    Role-specific setup
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or setup is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy app_name
 *       - ESP_ERR_NOT_SUPPORTED  Role is invalid
 */
esp_err_t esp_rtmp_service_setup(esp_rtmp_service_t *service, const esp_rtmp_service_setup_t *setup);

/**
 * @brief  Set RTMP URL before start
 *
 *         Server: rtmp[s]://host:port/app — port and app override setup values.
 *         Src/Sink: rtmp[s]://host:port/app/stream
 *
 * @param[in]  service  RTMP service handle
 * @param[in]  url      RTMP URL
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or url is NULL, or server URL is malformed
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy url
 */
esp_err_t esp_rtmp_service_set_url(esp_rtmp_service_t *service, const char *url);

/**
 * @brief  Print server client/session debug information
 *
 *         Only valid for ROLE_SERVER.
 *
 * @param[in]  service  RTMP server service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Role is not SERVER, or server support is disabled
 */
esp_err_t esp_rtmp_service_query(esp_rtmp_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
