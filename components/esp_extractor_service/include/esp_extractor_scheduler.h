/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_extractor_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Scheduler task name for the extractor source worker
 *
 *         Applications may override thread config via esp_service_scheduler_set_cb()
 *         using this name with the service name from create cfg.
 */
#define ESP_EXTRACTOR_SCHED_SRC_TASK  "extractor_src"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
