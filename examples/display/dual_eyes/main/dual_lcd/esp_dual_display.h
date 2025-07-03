/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_lvgl_port.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Display configuration structure
 */
typedef struct {
    lvgl_port_cfg_t  lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t         buffer_size;    /*!< Size of the buffer for the screen in pixels */
    uint32_t         trans_size;     /*!< Number of rows per buffer */
    bool             double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma    : 1;  /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram : 1;  /*!< Allocated LVGL buffer will be in PSRAM */
    } flags;
} display_cfg_t;

/**
 * @brief  Initialize display by call `esp_dual_display_start_with_config()` with a default configuration.
 * 
 * @note  If you do not use lvgl, just call `esp_dual_lcd_driver_init()`
 *
 * @param[out]  disp_left   Pointer to store the handle of the first LCD display
 * @param[out]  disp_right  Pointer to store the handle of the second LCD display
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument is wrong
 *       - ESP_ERR_INVALID_STATE  Invalid state
 *       - ESP_ERR_NO_MEM         Not enough memory to buffer send data
 */
esp_err_t esp_dual_display_start(lv_disp_t **disp_left, lv_disp_t **disp_right);

/**
 * @brief  Initialize display with configuration
 *
 * @note  It will call `lvgl_port_init()` and `esp_dual_lcd_driver_init()` internally.
 *        If you do not use lvgl, just call `esp_dual_lcd_driver_init()`
 *
 * @param[in]   cfg         Display configuration
 * @param[out]  disp_left   Pointer to store the handle of the first LCD display
 * @param[out]  disp_right  Pointer to store the handle of the second LCD display
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument is wrong
 *       - ESP_ERR_INVALID_STATE  Invalid state
 *       - ESP_ERR_NO_MEM         Not enough memory to buffer send data
 */
esp_err_t esp_dual_display_start_with_config(const display_cfg_t *cfg, lv_disp_t **disp_left, lv_disp_t **disp_right);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
