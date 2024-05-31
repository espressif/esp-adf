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
 * File: lightduer_dlp_dcs_adapter.h
 * Auth: Wang Ning (wangning23@baidu.com)
 * Desc: lightduer dlp dcs adapter.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_DCS_ADAPTER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_DCS_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "baidu_json.h"
#include "lightduer_types.h"

extern const char *DLP_DIRECTIVE_KEY;
extern const char *DLP_EVENT_KEY;
extern const char *DLP_TO_SERVER_KEY;
extern const char *DLP_TO_CLIENT_KEY;
extern const char *DLP_HEADER_KEY;
extern const char *DLP_NAMESPACE_KEY;
extern const char *DLP_NAME_KEY;
extern const char *DLP_PAYLOAD_KEY;
extern const char *DLP_MESSAGE_ID_KEY;
extern const char *DLP_DIALOG_REQUEST_ID_KEY;

extern const char *DLP_ALERTS_VER;
extern const char *DLP_ALERTS;
extern const char *DLP_ALERTS_GET_STATUS;
extern const char *DLP_ALERTS_GET_HOURLY_CHIME;
extern const char *DLP_ALERTS_HOURLY_CHIME_STATUS;

extern const char *DLP_SYSTEM_INFORMATION;
extern const char *DLP_SYSTEM_INFORMATION_GET_STATUS;
extern const char *DLP_SYSTEM_INFORMATION_HARD_RESET;
extern const char *DLP_SYSTEM_INFORMATION_SET_ALARM_VOLUME;
extern const char *DLP_SYSTEM_INFORMATION_SET_VOLUME_STEP;
extern const char *DLP_SYSTEM_INFORMATION_SET_LIGHT_THEME;
extern const char *DLP_SYSTEM_INFORMATION_SET_CHILD_MODE;
extern const char *DLP_SYSTEM_INFORMATION_STATUS;

extern const char *DLP_SCREEN;
extern const char *DLP_SCREEN_STATUS;
extern const char *DLP_SCREEN_GET_STATUS;
extern const char *DLP_SCREEN_GET_RENDER_PLAYER_INFO;
extern const char *DLP_SCREEN_GET_RENDER_AUDIO_LIST;
extern const char *DLP_SCREEN_BUTTON_CLICKED;
extern const char *DLP_SCREEN_RADIO_BUTTON_CLICKED;
extern const char *DLP_SCREEN_LINK_CLICKED;
extern const char *DLP_SCREEN_RENDER_PLAY_INFO;
extern const char *DLP_SCREEN_RENDER_AUDIO_LIST;
extern const char *DLP_SCREEN_RENDER_ALARM_LIST;
extern const char *DLP_SCREEN_RENDER_ALUM_LIST;

extern const char *DLP_AUDIO_PLAYER;
extern const char *DLP_AUDIO_PLAYER_GET_STATUS;
extern const char *DLP_AUDIO_PLAYER_SEEK_TO;
extern const char *DLP_AUDIO_PLAYER_STATUS;

extern const char *DLP_SPEAKER_CONTROLLER;
extern const char *DLP_SPEAKER_CONTROLLER_GET_STATUS;

extern const char *DLP_SYSTEM_MODE;
extern const char *DLP_SYSTEM_MODE_GET_STATUS;
extern const char *DLP_SYSTEM_MODE_SWITCH_MODE;
extern const char *DLP_SYSTEM_MODE_STATUS;

extern const char *DLP_LOCATION;
extern const char *DLP_LOCATION_GET_STATUS;
extern const char *DLP_LOCATION_SET_LOCATION;
extern const char *DLP_LOCATION_STATUS;

extern const char *DLP_PLAYBACK_CONTROLLER;
extern const char *DLP_PLAYBACK_CONTROLLER_PLAY;
extern const char *DLP_PLAYBACK_CONTROLLER_PAUSE;
extern const char *DLP_PLAYBACK_CONTROLLER_NEXT;
extern const char *DLP_PLAYBACK_CONTROLLER_PREVIOUS;
extern const char *DLP_PLAYBACK_CONTROLLER_TOGGLE;

extern const char *DLP_SETTINGS_VER;
extern const char *DLP_SETTINGS;
extern const char *DLP_SETTINGS_GET_STATUS;
extern const char *DLP_SETTINGS_GET_STATUS_ACK;
extern const char *DLP_SET_ChILD_FRIENDLY_MODE_ACK;
extern const char *DLP_SET_SLEEP_PROTETCT_MODE;
extern const char *DLP_SET_SLEEP_PROTETCT_MODE_ACK;

extern const char *DLP_PROTOCOL;
extern const char *DLP_PROTOCOL_SUPPORT_NAMESPACES;

extern const char *DLP_BLUETOOTH;
extern const char *DLP_BLUETOOTH_GET_STATUS;
extern const char *DLP_BLUETOOTH_SET_PARAM;
extern const char *DLP_BLUETOOTH_CONNECT;
extern const char *DLP_BLUETOOTH_DISCONNECT;
extern const char *DLP_BLUETOOTH_UNBIND;
extern const char *DLP_BLUETOOTH_STATUS;

extern const char *DLP_DEVICE_CONTROL;
extern const char *DLP_DEVICE_CONTROL_REPLACE_ACCOUNT_INFO;
extern const char *DLP_DEVICE_CONTROL_UPLOAD_DEBUG_LOG;
extern const char *DLP_DEVICE_CONTROL_RESTART;
extern const char *DLP_DEVICE_CONTROL_SET_PING_INTERVAL;

extern const char *DLP_DCI;
extern const char *DLP_DCI_SET_MODE;
extern const char *DLP_DCI_GET_STATUS;

/*dlp message type*/
typedef enum {
    TYPE_DCS_DIRECTIVE_FROM_CLIENT, // to_server msg, the instruction has a corresponding dcs command.
    TYPE_DCS_EVENT_FROM_CLIENT,     // to_server msg from xiaodu-app, But it is executed as an event of dcs
    TYPE_DLP_DIRECTIVE_FROM_CLIENT, // to_server msg, and the instruction does not have a corresponding dcs command.
    TYPE_DCS_EVENT_FROM_SERVER,     // to_client msg, speaker as a client
    TYPE_INVALID_MESSAGE,           // message parsing exception
    TYPE_UNSUPPORTED_MESSAGE,       // this message is not registered
    TYPE_DUPLICATE_MESSAGE          // duplicate message
} dlp_message_type_t;

/**
 * DESC:Get type by dlp directive
 * @PARAM[in] dlp directive.
 * @RETURN:   dlp type.
 */
dlp_message_type_t duer_get_dlp_msg_type(baidu_json *root);

/**
 * DESC:Convert dlp directive to dcs directive
 * @PARAM[in] dlp directive
 * @PARAM[in] dlp type
 * @PARAM[in] is use dialog id
 * @RETURN:   dcs directive.
 */
baidu_json *duer_dlp_trans_to_dcs(baidu_json *dlp_root,
                                 dlp_message_type_t dlp_msg_type,
                                 char is_use_dialog_request_id);

/**
 * DESC:Convert dcs&dlp directive to dlp directive
 * @PARAM[in] dcs&dlp directive.
 * @RETURN:   dlp directive.
 */
baidu_json *duer_dcs_trans_to_dlp(baidu_json *root);

/**
 * DESC:Convert dcs/dlp event to dcs&dlp event
 * @PARAM[in] dcs/dlp event
 * @PARAM[in] same_name, whether dcs/dlp event must have same name
 * @PARAM[out] more_event, whether this dcs event should map other dlp event
 * @RETURN:   dcs&dlp event.
 */
baidu_json *duer_event_to_dcs_dlp(baidu_json *root, duer_bool same_name, duer_bool *more_event);

/**
 * DESC:generat uuid
 * @PARAM[in] uuid
 * @RETURN:   uuid length
 */
int dlp_generate_uuid(char *uuid);

/**
 * DESC:get audio_player duration
 * @PARAM[in] none
 * @RETURN:   duration
 */
uint32_t duer_audio_player_duration(void);

/**
 * DESC:get audio_player type
 * @PARAM[in] mode_code
 * @RETURN:   mode_name
 */
const char *duer_audio_player_type(int *mode_code);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DLP_DCS_ADAPTER_H
