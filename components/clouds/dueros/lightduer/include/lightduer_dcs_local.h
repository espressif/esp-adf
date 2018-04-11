/**
 * Copyright (2017) Baidu Inc. All rights reserved.
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
 * File: lightduer_dcs_local.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Provide some functions for dcs module locally.
 */
#ifndef BAIDU_DUER_LIGHTDUER_DCS_LOCAL_H
#define BAIDU_DUER_LIGHTDUER_DCS_LOCAL_H

#include <stdlib.h>
#include <stdbool.h>
#include "baidu_json.h"
#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DCS_DIALOG_FINISHED,
    DCS_RECORD_STARTED,
    DCS_SPEECH_NEED_PLAY,
    DCS_SPEECH_FINISHED,
    DCS_AUDIO_NEED_PLAY,
    DCS_AUDIO_FINISHED,
    DCS_AUDIO_STOPPED,
    DCS_NEED_OPEN_MIC,
} duer_dcs_channel_switch_event_t;

/**
 * DESC:
 * Get current dialog request id.
 *
 * PARAM: none
 *
 * @RETURN: current dialog request id
 */
int duer_get_request_id_internal(void);

/**
 * DESC:
 * Create a new dialog request id for each conversation.
 *
 * PARAM: none
 *
 * @RETURN: the new dialog request id
 */
int duer_create_request_id_internal(void);

/**
 * DESC:
 * Get the client context.
 *
 * PARAM: none
 *
 * @RETURN: NULL if failed, pointer point to the client context if success.
 */
baidu_json *duer_get_client_context_internal(void);

/**
 * DESC:
 * Check whether there is a speech waiting to play.
 *
 * PARAM: none
 *
 * @RETURN: true if a speech is waiting to play, false if not.
 */
bool duer_speech_need_play_internal(void);

/**
 * DESC:
 * Pause the audio player.
 *
 * PARAM is_breaking: breaking a audio or a new audio is pending
 *
 * @RETURN: none.
 */
void duer_pause_audio_internal(bool is_breaking);

/**
 * DESC:
 * Resume the audio player.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_resume_audio_internal(void);

/**
 * DESC:
 * Notify that speech is stopped.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_speech_on_stop_internal(void);

/**
 * DESC:
 * Report exception.
 *
 * @PARAM[in] directive: which directive cause to the exception
 * @PARAM[in] type: exception type
 * @PARAM[in] msg: excetpion content
 *
 * @RETURN: 0 if success, negative if failed.
 */
int duer_report_exception_internal(const char* directive, const char* type, const char* msg);

/**
 * DESC:
 * Declare the system interface.
 *
 * @PARAM: none
 *
 * @RETURN: none.
 */
void duer_declare_sys_interface_internal(void);

/**
 * DESC:
 * Used to reset user activety time.
 *
 * @PARAM: none
 *
 * @RETURN: none.
 */
void duer_user_activity_internal(void);

/**
 * DESC:
 * Returns a pointer to a new string which is a duplicate of the string 'str'.
 *
 * @PARAM[in] str: the string need to duplicated.
 *
 * @RETURN: a pointer to the duplicated string, or NULL if insufficient memory was available.
 */
char *duer_strdup_internal(const char *str);

/**
 * DESC:
 * Used to check whether there is a multiple rounds dialogue.
 *
 * @PARAM: none
 *
 * @RETURN: true if it is multiple round dialogue.
 */
bool duer_is_multiple_round_dialogue(void);

/**
 * DESC:
 * Checking whether a audio is playing.
 *
 * @RETURN: Reture true if audio is playing, else returen false.
 */
bool duer_audio_is_playing_internal(void);

/**
 * DESC:
 * Chosing the highest priority play channel.
 *
 * @RETURN: None.
 */
void duer_play_channel_control_internal(duer_dcs_channel_switch_event_t event);

/**
 * DESC:
 * Checking whether a audio is paused by higher play channel.
 *
 * @RETURN: Reture true if audio is paused, else returen false.
 */
bool duer_audio_is_paused_internal(void);

/**
 * DESC:
 * Initialize the local resource, such as lock.
 *
 * @RETURN: None.
 */
void duer_dcs_local_init_internal(void);

/**
 * DESC:
 * Checking whether the micphone is recording or not.
 *
 * @RETURN: Reture true if audio is paused, else returen false.
 */
bool duer_is_recording_internal(void);

/**
 * DESC:
 * Starting the audio player.
 *
 * @RETURN: None.
 */
void duer_start_audio_play_internal(void);

/**
 * DESC:
 * DCS mode use it to report data.
 *
 * @PARAM[in] data: the report data.
 * @PARAM[in] is_transparent: some items might be added to data if it is false,
 *                            such as the English to Chinese or Chinese to English translate flag.
 *
 * @RETURN: success return DUER_OK, failed return DUER_ERR_FAILED.
 */
int duer_dcs_data_report_internal(baidu_json *data, bool is_transparent);

/**
 * DESC:
 * Open micphone.
 *
 * @PARAM: none.
 *
 * @RETURN: none.
 */
void duer_open_mic_internal(void);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_DCS_LOCAL_H*/
