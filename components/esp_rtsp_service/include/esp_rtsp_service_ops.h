/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_rtsp_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Apply optional stream/session setup before start
 *
 *         May be called while stopped. If never called, defaults are used.
 *         For SINK/SERVER, A/V enable and codecs are taken from linked
 *         provider tracks at start; setup.audio/video_enable apply to SRC.
 *         aud_frame_size and vid_frame_size of 0 keep the default RTP buffers.
 *
 * @param[in]  service  RTSP service handle
 * @param[in]  setup    Setup to apply; NULL restores defaults
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 */
esp_err_t esp_rtsp_service_setup(esp_rtsp_service_t *service, const esp_rtsp_service_setup_t *setup);

/**
 * @brief  Set RTSP URL before start
 *
 *         Server: rtsp://host:port/live
 *         Src/Sink: rtsp://host:port/live
 *
 * @param[in]  service  RTSP service handle
 * @param[in]  url      RTSP URL
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service or url is NULL
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy url
 */
esp_err_t esp_rtsp_service_set_url(esp_rtsp_service_t *service, const char *url);

/**
 * @brief  Set local IP advertised/bound by the protocol stack
 *
 *         Must be called while stopped, before start. Not taken from set_url.
 *         Pass NULL to clear; the stack then uses 0.0.0.0.
 *
 * @param[in]  service  RTSP service handle
 * @param[in]  ip       IPv4 string (e.g. "192.168.1.10"); NULL clears
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL, or ip is empty
 *       - ESP_ERR_INVALID_STATE  Service is running
 *       - ESP_ERR_NO_MEM         Failed to copy ip
 */
esp_err_t esp_rtsp_service_set_ip(esp_rtsp_service_t *service, const char *ip);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
