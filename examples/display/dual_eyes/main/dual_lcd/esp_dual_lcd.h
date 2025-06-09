/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Display */
#define BSP_LCD_DATA0  (GPIO_NUM_38)
#define BSP_LCD_PCLK   (GPIO_NUM_45)
#define BSP_LCD_DC     (GPIO_NUM_47)
#define BSP_LCD_RST    (GPIO_NUM_21)
#define BSP_LCD_CS_0   (GPIO_NUM_41)
#define BSP_LCD_CS_1   (GPIO_NUM_48)
#define BSP_LCD_BL     (GPIO_NUM_39)
#define BSP_LCD_POWER  (GPIO_NUM_NC) // Control power(VCC) for the LCD, if not used, set to GPIO_NUM_NC

#define BSP_LCD_H_RES  (240)
#define BSP_LCD_V_RES  (240)
#define TRANS_SIZE     (BSP_LCD_H_RES * BSP_LCD_V_RES / 10)

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT    (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN       (1)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL  (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE     (LCD_RGB_ELEMENT_ORDER_BGR)
// Bit number used to represent command and parameter
#define LCD_CMD_BITS            (8)
#define LCD_PARAM_BITS          (8)
#define LCD_LEDC_CH             (LEDC_CHANNEL_0)
#define LCD_LEDC_SPEED_MODE     (LEDC_LOW_SPEED_MODE)
// SPI bus parameter
#define BSP_LCD_PIXEL_CLOCK_HZ  (80 * 1000 * 1000)  // Need to check if your display supports this frequency
#define BSP_LCD_SPI_NUM         (SPI2_HOST)
#define SPI_ISR_CPU_ID          (ESP_INTR_CPU_AFFINITY_TO_CORE_ID(ESP_INTR_CPU_AFFINITY_1))

/**
 * @brief  Initialize display's brightness
 *
 * @note  Brightness is controlled with PWM signal to a pin controlling backlight
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t esp_dual_display_brightness_init(void);

/**
 * @brief  Set display's brightness
 *
 * @note  Brightness must be already initialized by `calling esp_dual_display_brightness_init()` or `esp_dual_lcd_driver_init()`
 *
 * @param[in]  brightness_percent  Brightness in [%]
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Parameter error
 */
esp_err_t esp_dual_display_brightness_set(int brightness_percent);

/**
 * @brief  Initialize the dual LCD display driver
 *
 * @param[out]  left_panel   Pointer to store the handle of the first LCD panel
 * @param[out]  left_io      Pointer to store the handle of the first LCD panel IO
 * @param[out]  right_panel  Pointer to store the handle of the second LCD panel
 * @param[out]  right_io     Pointer to store the handle of the second LCD panel IO
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument is wrong
 *       - ESP_ERR_INVALID_STATE  Invalid state
 *       - ESP_ERR_NO_MEM         Not enough memory to buffer send data
 */
esp_err_t esp_dual_lcd_driver_init(esp_lcd_panel_handle_t *left_panel, esp_lcd_panel_io_handle_t *left_io, esp_lcd_panel_handle_t *right_panel, esp_lcd_panel_io_handle_t *right_io);

/**
 * @brief  Deinitialize the dual LCD display driver
 *
 * @note  It will deinitialize and free all resources used by the driver (including brightness control)
 */
void esp_dual_lcd_driver_deinit(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
