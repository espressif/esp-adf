/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef _BLE_GATTS_MODULE_H_
#define _BLE_GATTS_MODULE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief ble gatt module init
 *
 * @return success:0
 *         fail: others
 */
esp_err_t ble_gatts_module_init(void);

/*
 * @brief ble gatt start adv
 *
 * @return
 */
void ble_gatts_module_start_adv();

/*
 * @brief ble gatt stop adv
 *
 * @return
 */
void ble_gatts_module_stop_adv();

#ifdef __cplusplus
}
#endif

#endif
