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
 * @brief  Scheduler task names for RTSP service workers
 */
#define ESP_RTSP_SCHED_PUSH_TASK    ESP_RTSP_SERVICE_PUSH_TASK_NAME
#define ESP_RTSP_SCHED_SRC_TASK     ESP_RTSP_SERVICE_SRC_TASK_NAME
#define ESP_RTSP_SCHED_SERVER_TASK  ESP_RTSP_SERVICE_SERVER_TASK_NAME

#ifdef __cplusplus
}
#endif  /* __cplusplus */
