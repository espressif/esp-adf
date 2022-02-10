/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _ESP_DISPATCHER_DUER_APP_H_
#define _ESP_DISPATCHER_DUER_APP_H_

#include "esp_action_def.h"
#include "audio_service.h"
#include "periph_service.h"
#include "display_service.h"
#include "esp_dispatcher.h"

typedef struct $ {
    audio_service_handle_t          audio_serv;         /*!< Clouds service handle */
    display_service_handle_t        disp_serv;          /*!< Display servce handle */
    periph_service_handle_t         wifi_serv;          /*!< WiFi manager service handle */
    periph_service_handle_t         input_serv;         /*!< Input event service handle */
    esp_dispatcher_handle_t         dispatcher;         /*!< ESP dispatcher handle */
    void                            *player;            /*!< The esp_audio handle */
    void                            *recorder;          /*!< The audio recorder handle */
    xTimerHandle                    retry_login_timer;
    bool                            wifi_setting_flag;
    bool                            is_palying;
} esp_dispatcher_dueros_speaker_t;

void duer_app_init(void);

#endif
