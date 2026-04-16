/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Show heap memory information
 */
void app_utils_show_memory(void);

/**
 * @brief  Start task status monitor
 */
void app_utils_task_status_monitor_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
