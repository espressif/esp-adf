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
#ifndef __BDSC_PROFILE_H__
#define __BDSC_PROFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "freertos/event_groups.h"
// #include "esp_event_loop.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <string.h>

/**
 * @brief vendor_info_t structure to store user info
 */
typedef struct {
    char *fc;
    char *pk;
    char *ak;
    char *sk;

    char *mqtt_broker;
    char *mqtt_username;
    char *mqtt_password;
    char *mqtt_cid;

    int cur_version_num;
    char *tone_sub_ver;
    char *dsp_sub_ver;
    char *app_sub_ver;
} vendor_info_t;

/**
 * @brief load vendor_info from profile partition
 */
int profile_init();

/**
 * @brief save vendor_info to profile partition
 */
int profile_save();

#ifdef __cplusplus
}
#endif

#endif
