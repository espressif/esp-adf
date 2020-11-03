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
#ifndef __BDSC_EVENT_DISPATCHER_H__
#define __BDSC_EVENT_DISPATCHER_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief hint type enum
 */
typedef enum {
    BDSC_HINT_WAKEUP,
    BDSC_HINT_CONNECTED,
    BDSC_HINT_DISCONNECTED,
    BDSC_HINT_SC,
    BDSC_HINT_NOT_FIND,
    BDSC_HINT_OTA_START,
    BDSC_HINT_OTA_COMPLETE,
    BDSC_HINT_OTA_FAIL,
    BDSC_HINT_BT_CONNECTED,
    BDSC_HINT_BT_DISCONNECTED,
    BDSC_HINT_OTA_ALREADY_NEWEST,
} bdsc_hint_type_t;

/**
 * @brief Play customized hint tone
 *
 * @param[in] type    hint type
 *
 */
void bdsc_play_hint(bdsc_hint_type_t type);

/**
 * @brief init bdsc asr tts engine
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int bdsc_asr_tts_engine_init();

/**
 * @brief EnQueque a message to dispatcher
 *
 * @param[in] event    event type
 * @param[in] buffer   event payload
 * @param[in] len      event payload length
 *
 */
void event_engine_elem_EnQueque(int event, uint8_t *buffer, size_t len);

/**
 * @brief DeQueque a message from dispatcher
 */
void event_engine_elem_FlushQueque();

/**
 * @brief Get flag judge whether the player need be resume
 * @return
 *      - true:  need to be resumed
 *      - false: no need to be resumed
 */
bool bdsc_get_pre_player_flag();

/**
 * @brief Set flag judge whether the player need be resume
 * @param[in] state  Set flag
 */
void bdsc_set_pre_player_flag(bool flag);

#ifdef __cplusplus
}
#endif

#endif
