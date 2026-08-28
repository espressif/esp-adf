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
 * @brief  Scheduler task names for RTMP service workers
 *
 *         Applications may override thread config via esp_service_scheduler_set_cb()
 *         using these names with the service name from create cfg.
 */
#define ESP_RTMP_SERVICE_PUSH_TASK_NAME    "rtmp_push"
#define ESP_RTMP_SERVICE_SRC_TASK_NAME     "rtmp_src"
#define ESP_RTMP_SERVICE_SERVER_TASK_NAME  "rtmp_server"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
