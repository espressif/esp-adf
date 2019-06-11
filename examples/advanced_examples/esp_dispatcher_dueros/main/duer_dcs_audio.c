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

#include "lightduer_dcs.h"
#include "duer_audio_action.h"
#include "lightduer_voice.h"
#include "esp_log.h"
#include "audio_mem.h"
#include "esp_dispatcher.h"
#include "esp_dispatcher_dueros_app.h"
#include "esp_action_exe_type.h"
#include "esp_player_wrapper.h"

static const char *TAG = "DUER_AUDIO";
extern esp_dispatcher_dueros_speaker_t *dueros_speaker;

void duer_dcs_listen_handler(void)
{
    ESP_LOGI(TAG, "enable_listen_handler, open mic");
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_REC_WAV_TURN_ON, NULL, NULL);
}

void duer_dcs_stop_listen_handler(void)
{
    ESP_LOGI(TAG, "stop_listen, close mic");
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_REC_WAV_TURN_OFF, NULL, NULL);
}

void duer_dcs_volume_set_handler(int volume)
{
    ESP_LOGI(TAG, "set_volume to %d", volume);
    action_arg_t arg = {
        .data = (void *)volume,
        .len = sizeof(int)
    };
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_VOLUME_SET, &arg, NULL);
}

void duer_dcs_volume_adjust_handler(int volume)
{
    ESP_LOGI(TAG, "adj_volume by %d", volume);
    action_arg_t arg = {
        .data = (void *)volume,
        .len = sizeof(int)
    };
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_DUER_VOLUME_ADJ, &arg, NULL);
}

void duer_dcs_mute_handler(duer_bool is_mute)
{
    ESP_LOGI(TAG, "set_mute to  %d", (int)is_mute);
    int ret = 0;
    if (is_mute) {
        esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_MUTE_ON, NULL, NULL);
    } else {
        esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_MUTE_OFF, NULL, NULL);
    }
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_mute");
        duer_dcs_on_mute();
    }
}

void duer_dcs_get_speaker_state(int *volume, duer_bool *is_mute)
{
    ESP_LOGE(TAG, "duer_dcs_get_speaker_state");
    *volume = 60;
    *is_mute = false;
    action_result_t ret = {0};
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_VOLUME_GET, NULL, &ret);
    if (ret.err == ESP_FAIL) {
        return;
    }
    int vol = *(int *)ret.data;
    audio_free(ret.data);
    if (ret.err != 0) {
        ESP_LOGE(TAG, "Failed to get volume");
    } else {
        *volume = vol;
        if (vol != 0) {
            *is_mute = false;
        } else {
            *is_mute = true;
        }
    }
}

void duer_dcs_speak_handler(const char *url)
{
    ESP_LOGI(TAG, "Playing speak: %p, %s", url, url);
    action_arg_t arg = {
        .data = (void *)url,
        .len = strlen(url),
    };
    int ret = esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_DUER_SPEAK, &arg, NULL);
    ESP_LOGE(TAG, "Speak ret:%x", ret);
}

void duer_dcs_audio_play_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Playing audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    action_arg_t arg = {
        .data = (void *)audio_info,
        .len = sizeof(*audio_info),
    };
    int ret = esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_DUER_AUDIO, &arg, NULL);
    ESP_LOGW(TAG, "Audio ret:%x", ret);
}

void duer_dcs_audio_stop_handler()
{
    int status = 0;
    ESP_LOGI(TAG, "Stop audio play");
    if (status == 1) {
        ESP_LOGI(TAG, "Is playing speech, no need to stop");
    } else {
        esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_STOP, NULL, NULL);
    }
}

void duer_dcs_audio_pause_handler()
{
    ESP_LOGI(TAG, "DCS pause audio play");
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_PAUSE, NULL, NULL);
}

void duer_dcs_audio_resume_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Resume audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    action_arg_t arg = {
        .data = (void *)audio_info,
        .len = sizeof(*audio_info),
    };
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_RESUME, &arg, NULL);
}

int duer_dcs_audio_get_play_progress()
{
    action_result_t ret = {0};
    esp_dispatcher_execute(dueros_speaker->dispatcher, ACTION_EXE_TYPE_AUDIO_GET_PROGRESS_BYTE, NULL, &ret);
    if (ret.err == ESP_FAIL) {
        return -1;
    }
    int position = *(int *)ret.data;
    audio_free(ret.data);
    if (ret.err == 0) {
        ESP_LOGI(TAG, "Get play position %d ", position);
        return position;
    } else {
        ESP_LOGE(TAG, "Failed to get play progress.");
        return -1;
    }
}

void duer_dcs_audio_speech_finished()
{
    duer_dcs_speech_on_finished();
}

void duer_dcs_audio_music_finished()
{
    duer_dcs_audio_on_finished();
}

void duer_dcs_audio_active_paused()
{
    if (duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PAUSE_CMD to DCS failed");
        return;
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_paused");
}

void duer_dcs_audio_active_play()
{
    if (duer_dcs_send_play_control_cmd(DCS_PLAY_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PLAY_CMD to DCS failed");
        return;
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_play");
}

void duer_dcs_audio_active_previous()
{
    if (duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PREVIOUS_CMD to DCS failed");
        return;
    }
    ESP_LOGD(TAG, "Fduer_dcs_audio_active_previous");
}

void duer_dcs_audio_active_next()
{
    if (duer_dcs_send_play_control_cmd(DCS_NEXT_CMD)) {
        ESP_LOGE(TAG, "Send DCS_NEXT_CMD to DCS failed");
        return;
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_next");
}
