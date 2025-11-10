/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_thread.h"
#include "esp_log_level.h"
#include "storage.h"
#include "mmap_generate_assets.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Start LVGL UI interface
 */
void dual_eye_ui_start(void);

/**
 * @brief  Start AVI video player
 */
void avi_player_start(void);

/**
 * @brief  Start LVGL UI interface with AVI video player
 */
void dual_eye_ui_avi_start(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
