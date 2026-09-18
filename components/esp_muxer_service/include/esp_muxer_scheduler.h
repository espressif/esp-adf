/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_muxer_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Scheduler task names for muxer service workers
 *
 *         Applications may override thread config via esp_service_scheduler_set_cb()
 *         using this name with the service name from create cfg.
 */
#define ESP_MUXER_SCHED_TASK  "muxer_sink"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
