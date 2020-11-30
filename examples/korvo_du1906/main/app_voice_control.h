/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#ifndef __APP_VOICE_CONTROL_H__
#define __APP_VOICE_CONTROL_H__

#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <string.h>
#include "bdsc_json.h"
#include "bdsc_engine.h"

#define DEVICE_ACTION_KEY  "DEV_ACTION"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_VOICE_CTL_CMD_NONE,
    APP_VOICE_CTL_CMD_OPEN_BT,
    APP_VOICE_CTL_CMD_CLOSE_BT,
    APP_VOICE_CTL_CMD_MUSIC_CONTINUE,
    APP_VOICE_CTL_CMD_MUSIC_PAUSE,
    APP_VOICE_CTL_CMD_MUSIC_STOP,
} app_voice_ctr_cmd_t;

void app_voice_control_feed_data(BdsJson *data_j, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
