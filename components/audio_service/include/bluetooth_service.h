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

#ifndef _BLUETOOTH_SERVICE_H_
#define _BLUETOOTH_SERVICE_H_

#include "freertos/event_groups.h"
#include "audio_error.h"
#include "audio_element.h"
#include "esp_peripherals.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * brief      Bluetooth service working mode
 */
typedef enum {
    BLUETOOTH_A2DP_SINK,    /*!< A2DP Bluetooth sink audio, ESP32 will receive audio data from another bluetooth devices */
    BLUETOOTH_A2DP_SOUCE,   /*!< A2DP Bluetooth source audio, ESP32 can send audio data to another bluetooth devices */
} bluetooth_service_mode_t;

/**
 * brief      Bluetooth peripheral event id
 */
typedef enum {
    PERIPH_BLUETOOTH_UNKNOWN = 0,       /*!< No event */
    PERIPH_BLUETOOTH_CONNECTED,         /*!< A bluetooth device was connected */
    PERIPH_BLUETOOTH_DISCONNECTED,      /*!< Last connection was disconnected */
    PERIPH_BLUETOOTH_AUDIO_STARTED,     /*!< The audio session has been started */
    PERIPH_BLUETOOTH_AUDIO_SUSPENDED,   /*!< The audio session has been suspended */
    PERIPH_BLUETOOTH_AUDIO_STOPPED,     /*!< The audio session has been stopped */
} periph_bluetooth_event_id_t;

/**
 *brief      Bluetooth service configuration
 */
typedef struct {
    const char                  *device_name;   /*!< Bluetooth device name */
    bluetooth_service_mode_t    mode;           /*!< Bluetooth working mode */
} bluetooth_service_cfg_t;

/**
 * @brief      Initialize and start the Bluetooth service. This function can called only 1 time,
 *             and `bluetooth_service_destroy` must be called after use.
 *
 * @param      config  The configuration
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t bluetooth_service_start(bluetooth_service_cfg_t *config);

/**
 * @brief      Create Bluetooth stream, it is valid when Bluetooth service has started.
 *             The returned audio stream compatible with existing audio streams and can be used with the Audio Pipeline
 *
 * @return     The Audio Element handle
 */
audio_element_handle_t bluetooth_service_create_stream();

/**
 * @brief      Create Bluetooth peripheral, it is valid when Bluetooth service has started.
 *             The returned bluetooth peripheral compatible with existing peripherals and can be used with the ESP Peripherals
 *
 * @return     The Peripheral handle
 */
esp_periph_handle_t bluetooth_service_create_periph();

/**
 * @brief      Send the AVRC passthrough command (PLAY) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_play(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (PAUSE) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_pause(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (STOP) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_stop(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (NEXT) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_next(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (PREV) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_prev(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (REWIND) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_rewind(esp_periph_handle_t periph);

/**
 * @brief      Send the AVRC passthrough command (FAST FORWARD) to the Bluetooth device
 *
 * @param[in]  periph  The periph
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t periph_bluetooth_fast_forward(esp_periph_handle_t periph);

/**
 * @brief      Destroy and cleanup bluetooth service, this function must be called after destroying
 *             the Bluetoth Stream and Bluetooth Peripheral created by `bluetooth_service_create_stream` and `bluetooth_service_create_periph`
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t bluetooth_service_destroy();

#ifdef __cplusplus
}
#endif


#endif
