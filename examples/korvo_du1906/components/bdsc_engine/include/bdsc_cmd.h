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
#ifndef __ESP32_BDSC_CMD_H__
#define __ESP32_BDSC_CMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bds_client.h"

/**
 * @brief send start asr cmd to DSP
 */
void bdsc_start_asr();

/**
 * @brief send stop asr cmd to DSP
 */
void bdsc_stop_asr();

/**
 * @brief send start wakeup cmd to DSP
 */
void bdsc_start_wakeup();

/**
 * @brief send stop wakeup cmd to DSP
 */
void bdsc_stop_wakeup();

/**
 * @brief send start event cmd to DSP
 */
void bdsc_start_event();

/**
 * @brief send cancel event cmd to DSP
 */
void bdsc_event_cancel();

/**
 * @brief send start recorder cmd to DSP
 */
void bdsc_start_recorder();

/**
 * @brief send stop recorder cmd to DSP
 */
void bdsc_stop_recorder();

/**
 * @brief send start link cmd to DSP
 */
void bdsc_link_start();

/**
 * @brief send stop link cmd to DSP
 */
void bdsc_link_stop();

/**
 * @brief send dynamic config cmd to DSP
 *
 * @param[in] config    DSP configuration
 */
void bdsc_dynamic_config(char *config);

void ws_connect_config();

#ifdef __cplusplus
}
#endif

#endif
