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

#include "app_voice_control.h"
#include "app_bt_init.h"
#include "audio_player.h"
#include "bdsc_event_dispatcher.h"

#define TAG "APP_VC"

static app_voice_ctr_cmd_t app_voice_control_cmd_filter(BdsJson *data_j, void *userdata)
{
    char *intent, *name, *value;
    char *user_action_key = NULL, *user_action_value = NULL;
    char *user_func_key = NULL, *user_func_value = NULL;
    char *user_att_key = NULL, *user_att_value = NULL;
    BdsJson *slots_j;
    int arr_len;
    int on_off = -1;

    if (!data_j) {
        return APP_VOICE_CTL_CMD_NONE;
    }
    if (!(intent = BdsJsonObjectGetString(data_j, "intent"))) {
        return APP_VOICE_CTL_CMD_NONE;
    }
    if (strcmp(intent, DEVICE_ACTION_KEY)) {
        return APP_VOICE_CTL_CMD_NONE;
    }
    if (!(slots_j = BdsJsonObjectGet(data_j, "slots"))) {
        return APP_VOICE_CTL_CMD_NONE;
    }
    arr_len = BdsJsonArrayLen(slots_j);
    if (arr_len <= 0) {
        return APP_VOICE_CTL_CMD_NONE;
    }
    BdsJsonArrayForeach(slots_j, elem_j) {
        if (!(name = BdsJsonObjectGetString(elem_j, "name")) ||
            !(value = BdsJsonObjectGetString(elem_j, "value"))) {
            ESP_LOGE(TAG, "format error");
            return APP_VOICE_CTL_CMD_NONE;
        }
        if (!strcmp(name, "user_action")) {
            user_action_key = name;
            user_action_value = value;
        }
        if (!strcmp(name, "user_func_bluetooth")) {
            user_func_key = name;
            user_func_value = value;
        }
        if (!strcmp(name, "user_attr_mode")) {
            user_func_key = name;
            user_func_value = value;
        }
        if (!strcmp(name, "user_attr")) {
            user_att_key = name;
            user_att_value = value;
        }
    }

    if (user_att_value && user_att_key) {
        ESP_LOGI(TAG, "get user att value and key");
    }

    if (!user_action_key || !user_action_value || strcmp(user_action_key, "user_action")) {
        ESP_LOGE(TAG, "format error");
        return APP_VOICE_CTL_CMD_NONE;
    }
    if (!strcmp(user_action_value, "CONTINUE")) {
        return APP_VOICE_CTL_CMD_MUSIC_CONTINUE;
    } else if (!strcmp(user_action_value, "PAUSE")) {
        return APP_VOICE_CTL_CMD_MUSIC_PAUSE;
    } else if (!strcmp(user_action_value, "STOP")) {
        return APP_VOICE_CTL_CMD_MUSIC_STOP;
    } else if (!strcmp(user_action_value, "ON")) {
        on_off = 1;
    } else if (!strcmp(user_action_value, "OFF")) {
        on_off = 0;
    } else {
        ESP_LOGE(TAG, "format error");
        return APP_VOICE_CTL_CMD_NONE;
    }

    if (!user_func_key || !user_func_value) {
        ESP_LOGE(TAG, "format error");
        return APP_VOICE_CTL_CMD_NONE;
    }
    if (!strcmp(user_func_key, "user_func_bluetooth")) {
        if (on_off == 1) {
            return APP_VOICE_CTL_CMD_OPEN_BT;
        } else {
            return APP_VOICE_CTL_CMD_CLOSE_BT;
        }
    } else {
        ESP_LOGE(TAG, "format error");
        return APP_VOICE_CTL_CMD_NONE;
    }
}


void app_voice_control_feed_data(BdsJson *data_j, void *userdata)
{
    switch (app_voice_control_cmd_filter(data_j, userdata)) {
        case APP_VOICE_CTL_CMD_NONE:
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_NONE");
            break;
        case APP_VOICE_CTL_CMD_OPEN_BT:
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_OPEN_BT");
            app_bt_start();
            break;
        case APP_VOICE_CTL_CMD_CLOSE_BT:
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_CLOSE_BT");
            bdsc_engine_close_bt();
            app_bt_stop();
            break;
        case APP_VOICE_CTL_CMD_MUSIC_CONTINUE:
            bdsc_set_pre_player_flag(true);
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_MUSIC_CONTINUE");
            break;
        case APP_VOICE_CTL_CMD_MUSIC_PAUSE:
            bdsc_set_pre_player_flag(false);
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_MUSIC_PAUSE");
            break;
        case APP_VOICE_CTL_CMD_MUSIC_STOP:
            bdsc_set_pre_player_flag(false);
            audio_player_clear_audio_info();
            ESP_LOGI(TAG, "APP_VOICE_CTL_CMD_MUSIC_STOP");
            break;
        default:
            break;
    }
}
