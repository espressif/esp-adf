/**
 * Copyright (2019) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_dlp.h
 * Auth: Wang Ning (wangning23@baidu.com)
 * Desc: light duer dlp apis.
 */
#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_H

#include <stdint.h>

typedef struct duer_device_info_s {
    const char  *client_id;
    const char  *device_id;
    char        version[32];
    char        ssid[33];
    uint8_t     mac[6];
    uint32_t    ip;
    int         volume;
    int         volume_step;
} duer_device_info_t;

enum duer_playback_control_type_e {
    PLAYBACK_CONTROL_PAUSE,
    PLAYBACK_CONTROL_RESUME,
    PLAYBACK_CONTROL_TOGGLE,
    PLAYBACK_CONTROL_PREVIOUS,
    PLAYBACK_CONTROL_NEXT,
};

/**
 * Initialize the dlp.
 *
 * @return none.
 */
void duer_dlp_init(void);

void duer_dlp_get_device_info(duer_device_info_t *info);

void duer_dlp_reset_system(void);

void duer_dlp_handle_playback_control(int control_type);

uint32_t duer_dlp_get_audio_duration(void);

void duer_dlp_handle_playback_seek(uint32_t offset);

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_H
