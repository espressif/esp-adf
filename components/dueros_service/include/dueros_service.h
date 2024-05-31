/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#ifndef _DUEROS_SERVICE_H_
#define _DUEROS_SERVICE_H_

#include "audio_service.h"
#include "duer_wifi_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status of WiFi connection
 */
typedef struct {
    int status;     /*!< Please refer to: `enum duer_dipb_client_status_e` */
    int err;        /*!< Please refer to: `enum duer_wifi_connect_error_code_e` */
} dueros_wifi_st_t;

/**
 * @brief Create the dueros service
 *
 * @return
 *     - NULL, Fail
 *     - Others, Success
 */
audio_service_handle_t dueros_service_create();

/**
 * @brief Get dueros service state
 *
 * @return The state of service
 *
 */
service_state_t dueros_service_state_get();

/**
 * @brief Upload voice to backend server
 *
 * @param handle  dueros service handle
 * @param buf     Data buffer
 * @param len     Size of buffer
 *
 * @return
 *         ESP_OK
 *         ESP_FAIL
 */
esp_err_t dueros_voice_upload(audio_service_handle_t handle, void *buf, int len);

/**
 * @brief Cancel the current session
 *
 * @param handle  dueros service handle
 *
 * @return
 *         ESP_OK
 *         ESP_FAIL
 */
esp_err_t dueros_voice_cancel(audio_service_handle_t handle);

/**
 * @brief Start the wifi configure process
 *
 * @param handle  Dueros service handle
 * @param cfg     Configuration
 *
 * @return
 *         ESP_OK
 *         ESP_FAIL
 */
esp_err_t dueros_start_wifi_cfg(audio_service_handle_t handle, duer_wifi_cfg_t *cfg);

/**
 * @brief Stop the wifi configure process
 *
 * @param handle  Dueros service handle
 *
 * @return
 *         ESP_OK
 *         ESP_FAIL
 */
esp_err_t dueros_stop_wifi_cfg(audio_service_handle_t handle);

/**
 * @brief Report the wifi status to dipb
 *
 * @param handle  Dueros service handle
 * @param st      WiFi status and error code
 *
 * @return
 *         ESP_OK
 *         ESP_FAIL
 */
esp_err_t dueros_wifi_status_report(audio_service_handle_t handle, dueros_wifi_st_t *st);

#ifdef __cplusplus
}
#endif

#endif
