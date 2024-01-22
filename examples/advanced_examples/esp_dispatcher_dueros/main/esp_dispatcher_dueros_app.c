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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

#include "esp_peripherals.h"
#include "audio_mem.h"
#include "audio_setup.h"
#include "esp_dispatcher_dueros_app.h"
#include "esp_player_wrapper.h"
#include "duer_audio_action.h"

#include "display_service.h"
#include "dueros_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "blufi_config.h"

#include "input_key_service.h"
#include "input_key_com_user_id.h"
#include "esp_dispatcher.h"
#include "esp_action_exe_type.h"
#include "wifi_action.h"
#include "display_action.h"
#include "dueros_action.h"
#include "recorder_action.h"
#include "player_action.h"
#include "audio_recorder.h"
#include "esp_delegate.h"
#include "audio_thread.h"

#define DUER_REC_READING (BIT0)

static const char               *TAG            = "DISPATCHER_DUEROS";
esp_dispatcher_dueros_speaker_t *dueros_speaker = NULL;
static EventGroupHandle_t       duer_evt        = NULL;

static void voice_read_task(void *args)
{
    const int buf_len = 2 * 1024;
    uint8_t *voiceData = audio_calloc(1, buf_len);
    bool runing = true;
    esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)args;

    while (runing) {
        EventBits_t bits = xEventGroupWaitBits(duer_evt, DUER_REC_READING, false, true, portMAX_DELAY);
        if (bits & DUER_REC_READING) {
            int ret = audio_recorder_data_read(d->recorder, voiceData, buf_len, portMAX_DELAY);
            if (ret == 0 || ret == -1) {
                xEventGroupClearBits(duer_evt, DUER_REC_READING);
                ESP_LOGE(TAG, "Read Finished");
            } else {
                dueros_voice_upload(d->audio_serv, voiceData, ret);
            }
        }
    }

    xEventGroupClearBits(duer_evt, DUER_REC_READING);
    free(voiceData);
    vTaskDelete(NULL);
}

static void esp_audio_callback_func(esp_audio_state_t *audio, void *ctx)
{
    ESP_LOGE(TAG, "ESP_AUDIO_CALLBACK_FUNC, st:%d,err:%d,src:%x",
             audio->status, audio->err_msg, audio->media_src);
    if (audio->status == AUDIO_STATUS_FINISHED) {
        if (audio->media_src == MEDIA_SRC_TYPE_DUER_SPEAK) {
            duer_dcs_audio_speech_finished();
        } else if (audio->media_src == MEDIA_SRC_TYPE_DUER_MUSIC) {
            duer_dcs_audio_music_finished();
        }
    }
}

static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)user_data;
    if (AUDIO_REC_WAKEUP_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_START");
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            dueros_voice_cancel(d->audio_serv);
        }
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_AUDIO_PAUSE, NULL, NULL);
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DISPLAY_TURN_ON, NULL, NULL);
    } else if (AUDIO_REC_VAD_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_START");
        audio_service_start(d->audio_serv);
        xEventGroupSetBits(duer_evt, DUER_REC_READING);
    } else if (AUDIO_REC_VAD_END == event->type) {
        xEventGroupClearBits(duer_evt, DUER_REC_READING);
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(d->audio_serv);
        }
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_STOP, state:%d", dueros_service_state_get());
    } else if (AUDIO_REC_WAKEUP_END == event->type) {
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(d->audio_serv);
        }
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DISPLAY_TURN_OFF, NULL, NULL);
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_END");
    } else {

    }

    return ESP_OK;
}

static esp_err_t wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)ctx;
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {
        d->wifi_setting_flag = false;
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DUER_CONNECT, NULL, NULL);
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DISPLAY_WIFI_CONNECTED, NULL, NULL);
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DUER_DISCONNECT, NULL, NULL);
        esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DISPLAY_WIFI_CONNECTED, NULL, NULL);
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        d->wifi_setting_flag = false;
    }
    return ESP_OK;
}

static void retry_login_timer_cb(xTimerHandle tmr)
{
    ESP_LOGE(TAG, "Func:%s", __func__);
    esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)pvTimerGetTimerID(tmr);
    xTimerStop(tmr, 0);
    esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_DUER_CONNECT, NULL, NULL);
}

static esp_err_t duer_callback(audio_service_handle_t handle, service_event_t *evt, void *ctx)
{
    static int retry_num = 1;
    int state = *((int *)evt->data);
    ESP_LOGW(TAG, "duer_callback: type:%x, source:%p data:%d, data_len:%d", evt->type, evt->source, state, evt->len);
    switch (state) {
        case SERVICE_STATE_IDLE: {
                esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)ctx;
                ESP_LOGW(TAG, "reason:%d", wifi_service_disconnect_reason_get(d->wifi_serv));
                if (WIFI_SERV_STA_BY_USER == wifi_service_disconnect_reason_get(d->wifi_serv)) {
                    break;
                }
                if (retry_num < 128) {
                    retry_num *= 2;
                    ESP_LOGI(TAG, "Dueros DUER_CMD_QUIT reconnect, retry_num:%d", retry_num);
                } else {
                    ESP_LOGE(TAG, "Dueros reconnect failed,time num:%d ", retry_num);
                    xTimerStop(d->retry_login_timer, portMAX_DELAY);
                    break;
                }
                xTimerStop(d->retry_login_timer, portMAX_DELAY);
                xTimerChangePeriod(d->retry_login_timer, (1000 / portTICK_PERIOD_MS) * retry_num, portMAX_DELAY);
                xTimerStart(d->retry_login_timer, portMAX_DELAY);
                break;
            }
        case SERVICE_STATE_CONNECTING: break;
        case SERVICE_STATE_CONNECTED: break;
            retry_num = 1;
        case SERVICE_STATE_RUNNING: break;
        case SERVICE_STATE_STOPPED: break;
        default:
            break;
    }

    return ESP_OK;
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    esp_dispatcher_dueros_speaker_t *d = (esp_dispatcher_dueros_speaker_t *)ctx;
    switch ((int)evt->data) {
        case INPUT_KEY_USER_ID_REC:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
                ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC_QUIT");
            } else if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK) {
                ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC");
                esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_REC_WAV_TURN_ON, NULL, NULL);
            }
            break;
        case INPUT_KEY_USER_ID_MODE:
            ESP_LOGI(TAG, "[ * ] [Mode]");
            break;
        case INPUT_KEY_USER_ID_PLAY:
            if (d->is_palying) {
                esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_AUDIO_PAUSE, NULL, NULL);
                d->is_palying = false;
            } else {
                esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_AUDIO_PLAY, NULL, NULL);
                d->is_palying = true;
            }
            break;
        case INPUT_KEY_USER_ID_SET:
            ESP_LOGI(TAG, "[ * ] [Set] input key event,%d", evt->type);
            if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
                if (d->wifi_setting_flag == false) {
                    esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_WIFI_SETTING_START, NULL, NULL);
                    d->wifi_setting_flag = true;
                } else {
                    esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_WIFI_SETTING_STOP, NULL, NULL);
                    d->wifi_setting_flag = false;
                }
            }
            break;
        case INPUT_KEY_USER_ID_VOLDOWN:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
                ESP_LOGI(TAG, "[ * ] [Vol-] input key event");
                esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_AUDIO_VOLUME_DOWN, NULL, NULL);
            }
            break;
        case INPUT_KEY_USER_ID_VOLUP:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
                ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
                esp_dispatcher_execute(d->dispatcher, ACTION_EXE_TYPE_AUDIO_VOLUME_UP, NULL, NULL);
            }
            break;
    }
    return ESP_OK;
}

static esp_err_t initialize_ble_stack(void)
{
    esp_err_t ret = ESP_OK;
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable bt controller failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_init();
    if (ret) {
       ESP_LOGE(TAG, "%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}


void duer_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);

    ESP_LOGI(TAG, "Step 1. Create dueros_speaker instance");
    dueros_speaker = audio_calloc(1, sizeof(esp_dispatcher_dueros_speaker_t));
    AUDIO_MEM_CHECK(TAG, dueros_speaker, return);

    esp_dispatcher_handle_t dispatcher = esp_dispatcher_get_delegate_handle();
    dueros_speaker->dispatcher = dispatcher;

    ESP_LOGI(TAG, "[Step 2.0] Create esp_periph_set_handle_t instance and initialize Touch, Button, SDcard");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    audio_board_key_init(set);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[Step 2.1] Initialize input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    dueros_speaker->input_serv = input_key_service_create(&input_cfg);
    input_key_service_add_key(dueros_speaker->input_serv, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(dueros_speaker->input_serv, input_key_service_cb, (void *)dueros_speaker);

    ESP_LOGI(TAG, "[Step 3.0] Create display service instance");
#ifdef FUNC_SYS_LEN_EN
    dueros_speaker->disp_serv = audio_board_led_init();
#endif
    ESP_LOGI(TAG, "[Step 3.1] Register display service execution type");
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->disp_serv,
                                ACTION_EXE_TYPE_DISPLAY_TURN_OFF, display_action_turn_off);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->disp_serv,
                                ACTION_EXE_TYPE_DISPLAY_TURN_ON, display_action_turn_on);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->disp_serv,
                                ACTION_EXE_TYPE_DISPLAY_WIFI_SETTING, display_action_wifi_setting);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->disp_serv,
                                ACTION_EXE_TYPE_DISPLAY_WIFI_CONNECTED, display_action_wifi_connected);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->disp_serv,
                                ACTION_EXE_TYPE_DISPLAY_WIFI_DISCONNECTED, display_action_wifi_disconnected);


    ESP_LOGI(TAG, "[Step 4.0] Initialize recorder engine");
    dueros_speaker->recorder = setup_recorder(rec_engine_cb, dueros_speaker);
    ESP_LOGI(TAG, "[Step 4.1] Register wanted recorder execution type");
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->recorder,
                                ACTION_EXE_TYPE_REC_WAV_TURN_OFF, recorder_action_rec_wav_turn_off);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->recorder,
                                ACTION_EXE_TYPE_REC_WAV_TURN_ON, recorder_action_rec_wav_turn_on);

    ESP_LOGI(TAG, "[Step 5.0] Initialize esp player");
    dueros_speaker->player = setup_player(esp_audio_callback_func, NULL);
    esp_player_init(dueros_speaker->player);
    ESP_LOGI(TAG, "[Step 5.1] Register wanted player execution type");
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->player, ACTION_EXE_TYPE_AUDIO_PLAY, player_action_play);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->player, ACTION_EXE_TYPE_AUDIO_PAUSE, player_action_pause);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->player, ACTION_EXE_TYPE_AUDIO_VOLUME_UP, player_action_vol_up);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->player, ACTION_EXE_TYPE_AUDIO_VOLUME_DOWN, player_action_vol_down);

    ESP_LOGI(TAG, "[Step 6.0] Initialize dueros service");
    dueros_speaker->retry_login_timer = xTimerCreate("tm_duer_login", 1000 / portTICK_PERIOD_MS,
                                        pdFALSE, (void *)dueros_speaker, retry_login_timer_cb);
    dueros_speaker->audio_serv = dueros_service_create();

    ESP_LOGI(TAG, "[Step 6.1] Register dueros service execution type");
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_DUER_VOLUME_ADJ, duer_dcs_action_vol_adj);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_DUER_AUDIO, duer_dcs_action_audio_play);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_DUER_SPEAK, duer_dcs_action_speak);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_AUDIO_GET_PROGRESS_BYTE, duer_dcs_action_get_progress);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_AUDIO_PAUSE, duer_dcs_action_audio_pause);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_AUDIO_RESUME, duer_dcs_action_audio_resume);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->player,
                                ACTION_EXE_TYPE_AUDIO_VOLUME_GET, duer_dcs_action_get_state);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_AUDIO_VOLUME_SET, duer_dcs_action_vol_set);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_AUDIO_STOP, duer_dcs_action_audio_stop);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_DUER_CONNECT, dueros_action_connect);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->audio_serv,
                                ACTION_EXE_TYPE_DUER_DISCONNECT, dueros_action_disconnect);
    audio_service_set_callback(dueros_speaker->audio_serv, duer_callback, dueros_speaker);

    ESP_LOGI(TAG, "[Step 7.0] Create Wi-Fi service instance");
    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = dueros_speaker;
    cfg.setting_timeout_s = 60;
    dueros_speaker->wifi_serv = wifi_service_create(&cfg);

    ESP_LOGI(TAG, "[Step 7.1] Register wanted display service execution type");
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->wifi_serv,
                                ACTION_EXE_TYPE_WIFI_CONNECT, wifi_action_connect);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->wifi_serv,
                                ACTION_EXE_TYPE_WIFI_DISCONNECT, wifi_action_disconnect);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->wifi_serv,
                                ACTION_EXE_TYPE_WIFI_SETTING_STOP, wifi_action_setting_stop);
    esp_dispatcher_reg_exe_func(dispatcher, dueros_speaker->wifi_serv,
                                ACTION_EXE_TYPE_WIFI_SETTING_START, wifi_action_setting_start);
    ESP_LOGI(TAG, "[Step 7.2] Initialize Wi-Fi provisioning type(AIRKISS, SMARTCONFIG or ESP-BLUFI)");
    int reg_idx = 0;
    esp_wifi_setting_handle_t h = NULL;
#ifdef CONFIG_AIRKISS_ENCRYPT
    airkiss_config_info_t air_info = AIRKISS_CONFIG_INFO_DEFAULT();
    air_info.lan_pack.appid = CONFIG_AIRKISS_APPID;
    air_info.lan_pack.deviceid = CONFIG_AIRKISS_DEVICEID;
    air_info.aes_key = CONFIG_DUER_AIRKISS_KEY;
    h = airkiss_config_create(&air_info);
#elif (defined CONFIG_ESP_SMARTCONFIG)
    smart_config_info_t info = SMART_CONFIG_INFO_DEFAULT();
    h = smart_config_create(&info);
#elif (defined CONFIG_ESP_BLUFI_PROVISIONING)
    initialize_ble_stack();
    h = blufi_config_create(NULL);
#endif
    esp_wifi_setting_register_notify_handle(h, (void *)dueros_speaker->wifi_serv);
    wifi_service_register_setting_handle(dueros_speaker->wifi_serv, h, &reg_idx);
    wifi_service_set_sta_info(dueros_speaker->wifi_serv, &sta_cfg);
    wifi_service_connect(dueros_speaker->wifi_serv);

    duer_evt = xEventGroupCreate();
    audio_thread_create(NULL, "voice_read_task", voice_read_task, dueros_speaker, 2 * 1024, 5, true, 1);

    ESP_LOGI(TAG, "[Step 8.0] Initialize Done");
}
