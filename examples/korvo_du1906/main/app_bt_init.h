/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __APP_BT_INIT_H__
#define __APP_BT_INIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_peripherals.h"
#include "esp_gap_bt_api.h"

/**
 * @brief          Initializes the bluetooth
 *
 * @param[in]      set           the specific peripheral set handle
 *
 * @return         esp_periph_handle_t      the bt peripheral handle
 *                 NULL                     failed
 */
esp_periph_handle_t app_bluetooth_init(esp_periph_set_handle_t set);

/**
 * @brief          Deinit the bluetooth
 *
 */
void app_bluetooth_deinit(void);

/**
 * @brief          Enable a2dp function
 *
 * @return         ESP_OK      success
 *                 ESP_FAIL    failed
 */
esp_err_t app_bt_start(void);

/**
 * @brief          Disable a2dp function
 *
 * @param[in]      bda                      remote bt adress (mac address)
 *
 * @return         ESP_OK      success
 *                 ESP_FAIL    failed
 */
esp_err_t app_bt_stop(void);

/**
 * @brief          Set bt bda
 *
 * @param[in]      addr  remote bt adress (mac address)
 */
void app_bt_set_addr(esp_bd_addr_t *addr);

#ifdef __cplusplus
}
#endif

#endif
