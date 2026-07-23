/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Install the example esp_service_scheduler callback.
 *
 *         Capture encoder / source threads are tuned through that callback after
 *         esp_capture_service_create() installs the service-scheduler bridge.
 *         Overlay redraw, test sink, and AI audio tasks also use esp_service_scheduler.
 */
esp_err_t video_capture_scheduler_install(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

