/*  Audio sleep wakeup example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef __AUDIO_SLEEP_WAKEUP_H__
#define __AUDIO_SLEEP_WAKEUP_H__

#include "esp_sleep.h"
#include "esp_pm.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Power manage init
 * 
 * This function reset gpio wakeup source firstly to ensure gpio can be apply rightly by application.
 * Create and acquire a power management lock to prevent the system from entering auto light sleep.
 * Configure dynamic frequency scaling (DFS) relevant parameters and register uart0 wakeup source.
 * You can also register uart wakeup source in enter_power_manage for release uart0 resources.
 */
void power_manage_init(void);

/**
 * @brief Enter power manage
 * 
 * This function will register gpio and timer wakeup source and release power management lock to 
 * enter auto light sleep mode. After wake up from light sleep, it will get power management lock
 * and exit auto light sleep mode.
 */
void enter_power_manage(void);

/**
 * @brief This function realize dynamic frequency scaling related configuration
 *
 * @param max_freq Dynamic frequency scaling (DFS) maximum frequency. Maximum frequency of the CPU by default
 * @param min_freq Dynamic frequency scaling (DFS) minimum frequency. Use 40MHz by default
 * @param enable   Whether to enable automatic light sleep mode, enable it by set 1.
 */
void power_manage_config(int max_freq, int min_freq, bool enable);

/**
 * @brief Get global power manage lock handle
 *
 * @return Power manage lock handle
 */
esp_pm_lock_handle_t get_power_manage_lock_handle(void);

/**
 * @brief Get and show wakeup source from light sleep or deep sleep
 *
 * @return Cause of wake up from last sleep (deep sleep or light sleep)
 */
esp_sleep_wakeup_cause_t get_wakeup_cause(void);

/**
 * @brief This function register Wake Source by param and enter deep sleep
 *
 * @param wakeup_mode The wakeup mode for deep sleep
 */
void enter_deep_sleep(esp_sleep_source_t wakeup_mode);

#ifdef __cplusplus
}
#endif

#endif // __AUDIO_SLEEP_WAKEUP_H__
