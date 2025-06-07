/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BLE_GATTS_MODULE_H_
#define _BLE_GATTS_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Ble gatts module init
 *
 * @return
 *       - ESP_OK    On sucess
 *       - ESP_FAIL  Init failed
 */
esp_err_t ble_gatts_module_init();

/**
 * @brief  Ble gatts start adv
 *
 * @return
 */
void ble_gatts_module_start_adv();

/**
 * @brief  Ble gatts stop adv
 *
 * @return
 */
void ble_gatts_module_stop_adv();

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* _BLE_GATTS_MODULE_H_ */
