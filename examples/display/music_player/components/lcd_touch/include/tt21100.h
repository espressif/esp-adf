/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Init TT21100 touch panel
 * 
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t tt21100_tp_init(void);

/**
 * @brief Read packet from TT21100
 * 
 * @return 
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t tt21100_tp_read(void);

/**
 * @brief Get the touch point position
 * 
 * @param p_tp_num      Pointer to touch number
 * @param p_x           Pointer to X direction
 * @param p_y           Pointer to y direction
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t tt21100_get_touch_point(uint8_t *p_tp_num, uint16_t *p_x, uint16_t *p_y);

/**
 * @brief Get button value
 * 
 * @param p_btn_val    Pointer to button value
 * @param p_btn_signal Pointer to button singal
 *
 * @return
 *    - ESP_OK: Success
 *    - Others: Fail
 */
esp_err_t tt21100_get_btn_val(uint8_t *p_btn_val, uint16_t *p_btn_signal);


#ifdef __cplusplus
}
#endif

