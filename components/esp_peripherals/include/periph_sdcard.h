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

#ifndef _SDCARD_DEV_H_
#define _SDCARD_DEV_H_

#include "rom/queue.h"
#include "audio_error.h"
#include "audio_common.h"
#include "esp_peripherals.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Peripheral sdcard event id
 */
typedef enum  {
    SDCARD_STATUS_UNKNOWN,              /*!< No event */
    SDCARD_STATUS_CARD_DETECT_CHANGE,   /*!< Detect changes in the card_detect pin */
    SDCARD_STATUS_MOUNTED,              /*!< SDCARD mounted successfully */
    SDCARD_STATUS_UNMOUNTED,            /*!< SDCARD unmounted successfully */
    SDCARD_STATUS_MOUNT_ERROR,          /*!< SDCARD mount error */
    SDCARD_STATUS_UNMOUNT_ERROR,        /*!< SDCARD unmount error */
} periph_sdcard_event_id_t;

/**
 * @brief   The SD Card Peripheral configuration
 */
typedef struct {
    int card_detect_pin;    /*!< Card detect gpio number */
    const char* root;       /*!< Base path for vfs */
} periph_sdcard_cfg_t;


/**
 * @brief      Create the sdcard peripheral handle for esp_peripherals
 *
 * @note       The handle was created by this function automatically destroy when `esp_periph_destroy` is called
 *
 * @param      sdcard_config  The sdcard configuration
 *
 * @return     The esp peripheral handle
 */
esp_periph_handle_t periph_sdcard_init(periph_sdcard_cfg_t* sdcard_config);

/**
 * @brief      Check the sdcard is mounted or not.
 *
 * @param[in]  periph  The periph
 *
 * @return     SDCARD mounted state
 */
bool periph_sdcard_is_mounted(esp_periph_handle_t periph);

#ifdef __cplusplus
}
#endif

#endif
