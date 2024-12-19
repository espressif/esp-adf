/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "board.h"
#include "board_pins_config.h"

#include "sdkconfig.h"
#include "audio_mem.h"
#include "dueros_app.h"
#include "esp_audio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "duer_audio_wrapper.h"
#include "dueros_service.h"
#include "audio_mem.h"
#include "audio_recorder.h"
#include "recorder_sr.h"
#include "audio_sys.h"
#include "audio_idf_version.h"
#include "audio_thread.h"

#include "display_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "periph_adc_button.h"
#include "periph_spiffs.h"
#include "algorithm_stream.h"
#include "duer_wifi_cfg.h"
#include "duer_profile.h"
#include "lightduer_dipb_data_handler.h"
#include "esp_delegate.h"
#include "esp_dispatcher.h"

#ifdef CONFIG_DUER_WIFI_CONFIG
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#endif  // CONFIG_DUER_WIFI_CONFIG

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
#include "driver/touch_pad.h"
#endif  /* (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)) */

#define DUER_REC_READING (BIT0)

static const char              *TAG               = "DUEROS";
static esp_audio_handle_t       player            = NULL;
static audio_rec_handle_t       recorder          = NULL;
static audio_service_handle_t   duer_serv_handle  = NULL;
static display_service_handle_t disp_serv         = NULL;
static periph_service_handle_t  wifi_serv         = NULL;
static bool                     wifi_setting_flag = false;
static EventGroupHandle_t       duer_evt          = NULL;
static esp_dispatcher_handle_t  esp_dispatcher    = NULL;

#ifdef CONFIG_DUER_WIFI_CONFIG
static esp_err_t wifi_cfg_start();
static void      wifi_cfg_stop();
#endif  /* CONFIG_DUER_WIFI_CONFIG */

extern int duer_dcs_audio_sync_play_tone(const char *uri);

static esp_err_t dispatcher_audio_play(void *instance, action_arg_t *arg, action_result_t *result)
{
    duer_dcs_audio_sync_play_tone((char *)arg->data);
    return ESP_OK;
};

static void voice_read_task(void *args)
{
    const int buf_len = 2 * 1024;
    uint8_t *voiceData = audio_calloc(1, buf_len);
    bool runing = true;

    while (runing) {
        EventBits_t bits = xEventGroupWaitBits(duer_evt, DUER_REC_READING, false, true, portMAX_DELAY);
        if (bits & DUER_REC_READING) {
            int ret = audio_recorder_data_read(recorder, voiceData, buf_len, portMAX_DELAY);
            if (ret == 0 || ret == -1) {
                xEventGroupClearBits(duer_evt, DUER_REC_READING);
                ESP_LOGE(TAG, "Read Finished");
            } else {
                dueros_voice_upload(duer_serv_handle, voiceData, ret);
            }
        }
    }

    xEventGroupClearBits(duer_evt, DUER_REC_READING);
    free(voiceData);
    vTaskDelete(NULL);
}

static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    if (AUDIO_REC_WAKEUP_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_START");
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            dueros_voice_cancel(duer_serv_handle);
        }
        if (duer_audio_wrapper_get_state() == AUDIO_STATUS_RUNNING) {
            duer_audio_wrapper_pause();
        }
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);
        ESP_LOGI(TAG, "rec_engine_cb - Play tone");
        action_arg_t action_arg = {0};
        action_arg.data = (void *)"spiffs://spiffs/dingding.wav";

        action_result_t result = {0};
        esp_dispatcher_execute_with_func(esp_dispatcher, dispatcher_audio_play, NULL, &action_arg, &result);
    } else if (AUDIO_REC_VAD_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_START");
        audio_service_start(duer_serv_handle);
        xEventGroupSetBits(duer_evt, DUER_REC_READING);
    } else if (AUDIO_REC_VAD_END == event->type) {
        xEventGroupClearBits(duer_evt, DUER_REC_READING);
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(duer_serv_handle);
        }
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_STOP, state:%d", dueros_service_state_get());
    } else if (AUDIO_REC_WAKEUP_END == event->type) {
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(duer_serv_handle);
        }
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_END");
    } else {
    }

    return ESP_OK;
}

static esp_err_t wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
        audio_service_connect(duer_serv_handle);
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_CONNECTED, 0);
#ifndef CONFIG_DUER_WIFI_CONFIG
        wifi_setting_flag = false;
#endif
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0);
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
#ifndef CONFIG_DUER_WIFI_CONFIG
        wifi_setting_flag = false;
#endif
    }
    dueros_wifi_st_t st = {
        .status = evt->type == WIFI_SERV_EVENT_CONNECTED ? DUER_DIPB_ST_CONNECT_ROUTER_SUC : DUER_DIPB_ST_CONNECT_ROUTER_FAILED,
        .err = wifi_service_disconnect_reason_get(handle) == WIFI_SERV_STA_AUTH_ERROR ? DUER_WIFI_ERR_PASSWORD_ERR : DUER_WIFI_ERR_UNKOWN,
    };
    dueros_wifi_status_report(duer_serv_handle, &st);
    return ESP_OK;
}

static void retry_login_timer_cb(xTimerHandle tmr)
{
    ESP_LOGE(TAG, "Func:%s", __func__);
    audio_service_connect(duer_serv_handle);
    xTimerStop(tmr, 0);
}

#ifdef CONFIG_DUER_WIFI_CONFIG
static void start_ble()
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
}

static void stop_ble()
{
    esp_err_t ret = esp_bluedroid_disable();
    if (ret) {
        ESP_LOGE(TAG, "%s disable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bluedroid_deinit();
    if (ret) {
        ESP_LOGE(TAG, "%s deinit bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_disable();
    if (ret) {
        ESP_LOGE(TAG, "%s disable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ret = esp_bt_controller_deinit();
    if (ret) {
        ESP_LOGE(TAG, "%s deinitialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
}

static void duer_wifi_cfg_user_cb(duer_wifi_cfg_event_t event, void *data)
{
    switch (event) {
        case DUER_WIFI_CFG_SSID_GET: {
            duer_wifi_ssid_get_t *wifi_info = data;
            ESP_LOGI(TAG, "wifi info get %s, %s, %s, %s",
                     wifi_info->ssid, wifi_info->pwd, wifi_info->bduss, wifi_info->device_bduss);

            duer_profile_update(wifi_info->bduss, CONFIG_DUER_CLIENT_ID);

            wifi_config_t sta_cfg = {0};
            memcpy((char *)&sta_cfg.sta.ssid, wifi_info->ssid, sizeof(sta_cfg.sta.ssid));
            memcpy((char *)&sta_cfg.sta.password, wifi_info->pwd, sizeof(sta_cfg.sta.password));
            wifi_service_set_sta_info(wifi_serv, &sta_cfg);
            wifi_service_connect(wifi_serv);
            break;
        }
        case DUER_WIFI_CFG_BLE_DISC:
            ESP_LOGI(TAG, "DUER_WIFI_CFG_BLE_DISC");
            if (wifi_setting_flag) {
                wifi_cfg_stop();
                wifi_setting_flag = false;
            }
            break;
        case DUER_WIFI_CFG_BLE_CONN:
            ESP_LOGI(TAG, "DUER_WIFI_CFG_BLE_CONN");
            break;
        default:
            break;
    }
}

static bool wifi_has_cfg_saved()
{
    wifi_config_t wifi_cfg = {0};
    if (wifi_service_get_last_ssid_cfg(wifi_serv, &wifi_cfg) == ESP_OK) {
        return true;
    } else {
        return false;
    }
}

static esp_err_t wifi_cfg_start()
{
    start_ble();

    duer_wifi_cfg_t duer_wifi = {
        .user_cb = duer_wifi_cfg_user_cb,
        .client_id = CONFIG_DUER_CLIENT_ID,
        .pub_key = CONFIG_DUER_ECC_PUB_KEY,
    };
    return dueros_start_wifi_cfg(duer_serv_handle, &duer_wifi);
}

static void wifi_cfg_stop()
{
    dueros_stop_wifi_cfg(duer_serv_handle);
    stop_ble();
}

#endif  // CONFIG_DUER_WIFI_CONFIG

static esp_err_t duer_callback(audio_service_handle_t handle, service_event_t *evt, void *ctx)
{
    static int retry_num = 1;
    int state = *((int *)evt->data);
    ESP_LOGW(TAG, "duer_callback: type:%x, source:%p data:%d, data_len:%d", evt->type, evt->source, state, evt->len);
    switch (state) {
        case SERVICE_STATE_IDLE: {
            xTimerHandle retry_login_timer = (xTimerHandle)ctx;
            ESP_LOGW(TAG, "reason:%d", wifi_service_disconnect_reason_get(wifi_serv));

            if (WIFI_SERV_STA_BY_USER == wifi_service_disconnect_reason_get(wifi_serv)) {
                break;
            }
            if (retry_num < 128) {
                retry_num *= 2;
                ESP_LOGI(TAG, "Dueros DUER_CMD_QUIT reconnect, retry_num:%d", retry_num);
            } else {
                ESP_LOGE(TAG, "Dueros reconnect failed,time num:%d ", retry_num);
                xTimerStop(retry_login_timer, portMAX_DELAY);
                break;
            }
            xTimerStop(retry_login_timer, portMAX_DELAY);
            xTimerChangePeriod(retry_login_timer, (1000 / portTICK_PERIOD_MS) * retry_num, portMAX_DELAY);
            xTimerStart(retry_login_timer, portMAX_DELAY);
            break;
        }
        case SERVICE_STATE_CONNECTING:
            break;
        case SERVICE_STATE_CONNECTED:
            retry_num = 1;
            break;
        case SERVICE_STATE_RUNNING:
            break;
        case SERVICE_STATE_STOPPED:
            break;
        default:
            break;
    }

    return ESP_OK;
}

esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
#if FUNC_BUTTON_EN
    ESP_LOGD(TAG, "Periph Event received: src_type:%x, source:%p cmd:%d, data:%p, data_len:%d",
             event->source_type, event->source, event->cmd, event->data, event->data_len);
    switch (event->source_type) {
        case PERIPH_ID_BUTTON: {
            if ((int)event->data == get_input_rec_id() && event->cmd == PERIPH_BUTTON_PRESSED) {
                ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC");
                audio_recorder_trigger_start(recorder);
            } else if ((int)event->data == get_input_mode_id() && ((event->cmd == PERIPH_BUTTON_RELEASE) || (event->cmd == PERIPH_BUTTON_LONG_RELEASE))) {
                ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC_QUIT");
            }
            break;
        }
        case PERIPH_ID_TOUCH: {
            if ((int)event->data == TOUCH_PAD_NUM4 && event->cmd == PERIPH_BUTTON_PRESSED) {

                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_DOWN [%d]", player_volume);
            } else if ((int)event->data == TOUCH_PAD_NUM4 && (event->cmd == PERIPH_BUTTON_RELEASE)) {

            } else if ((int)event->data == TOUCH_PAD_NUM7 && event->cmd == PERIPH_BUTTON_PRESSED) {
                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_UP [%d]", player_volume);
            } else if ((int)event->data == TOUCH_PAD_NUM7 && (event->cmd == PERIPH_BUTTON_RELEASE)) {

            } else if ((int)event->data == TOUCH_PAD_NUM8 && event->cmd == PERIPH_BUTTON_PRESSED) {
                ESP_LOGI(TAG, "AUDIO_USER_KEY_PLAY [%d]", __LINE__);

            } else if ((int)event->data == TOUCH_PAD_NUM8 && (event->cmd == PERIPH_BUTTON_RELEASE)) {

            } else if ((int)event->data == TOUCH_PAD_NUM9 && event->cmd == PERIPH_BUTTON_PRESSED) {
                if (wifi_setting_flag == false) {
#ifdef CONFIG_DUER_WIFI_CONFIG
                    wifi_cfg_start();
#else
                    wifi_service_setting_start(wifi_serv, 0);
#endif  // CONFIG_DUER_WIFI_CONFIG
                    wifi_setting_flag = true;
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_SETTING, 0);
                    ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting started.");
                } else {
                    ESP_LOGW(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting will be stopped.");
#ifdef CONFIG_DUER_WIFI_CONFIG
                    wifi_cfg_stop();
#else
                    wifi_service_setting_stop(wifi_serv, 0);
#endif  // CONFIG_DUER_WIFI_CONFIG
                    wifi_setting_flag = false;
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
                }
            } else if ((int)event->data == TOUCH_PAD_NUM9 && (event->cmd == PERIPH_BUTTON_RELEASE)) {
            }
            break;
        }
        case PERIPH_ID_ADC_BTN:
            if (((int)event->data == get_input_volup_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_UP [%d]", player_volume);
            } else if (((int)event->data == get_input_voldown_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                int player_volume = 0;
                esp_audio_vol_get(player, &player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                esp_audio_vol_set(player, player_volume);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_DOWN [%d]", player_volume);
            } else if (((int)event->data == get_input_play_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {

            } else if (((int)event->data == get_input_mode_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                esp_audio_vol_set(player, 0);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_MUTE [0]");
            } else if (((int)event->data == get_input_set_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                if (wifi_setting_flag == false) {
#ifdef CONFIG_DUER_WIFI_CONFIG
                    wifi_service_disconnect(wifi_serv);
                    wifi_cfg_start();
#else
                    wifi_service_setting_start(wifi_serv, 0);
#endif  // CONFIG_DUER_WIFI_CONFIG
                    wifi_setting_flag = true;
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_SETTING, 0);
                    ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting started.");
                } else {
                    ESP_LOGW(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting will be stopped.");
#ifdef CONFIG_DUER_WIFI_CONFIG
                    wifi_cfg_stop();
#else
                    wifi_service_setting_stop(wifi_serv, 0);
#endif  // CONFIG_DUER_WIFI_CONFIG
                    wifi_setting_flag = false;
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
                }
                ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET [0]");
            }
            break;
        default:
            break;
    }
#endif  // FUNC_BUTTON_EN
    return ESP_OK;
}

void sys_monitor_task(void *para)
{
    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        AUDIO_MEM_SHOW(TAG);
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
        audio_sys_get_real_time_stats();
#endif
    }
    vTaskDelete(NULL);
}

void start_sys_monitor(void)
{
    xTaskCreatePinnedToCore(sys_monitor_task, "sys_monitor_task", (4 * 1024), NULL, 1, NULL, 1);
}

esp_err_t duer_init_hal(void *instance, action_arg_t *arg, action_result_t *result)
{
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    if (set != NULL) {
        esp_periph_set_register_callback(set, periph_callback, NULL);
    }
    audio_board_init();
    audio_board_key_init(set);
    // Initialize Spiffs peripheral
    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",
        .partition_label = "spiffs_data",
        .max_files = 5,
        .format_if_mount_failed = true};
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);
    esp_periph_start(set, spiffs_handle);

    // Wait until spiffs is mounted
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
#ifdef FUNC_SYS_LEN_EN
    disp_serv = audio_board_led_init();
#endif
    result->err = ESP_OK;
    return result->err;
}

void duer_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    esp_dispatcher = esp_dispatcher_get_delegate_handle();
    action_result_t result = {0};
    esp_dispatcher_execute_with_func(esp_dispatcher, duer_init_hal, NULL, NULL, &result);

    xTimerHandle retry_login_timer = xTimerCreate("tm_duer_login", 1000 / portTICK_PERIOD_MS,
                                                  pdFALSE, NULL, retry_login_timer_cb);
    duer_serv_handle = dueros_service_create();
    audio_service_set_callback(duer_serv_handle, duer_callback, retry_login_timer);

    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = NULL;
    cfg.setting_timeout_s = 60;
    wifi_serv = wifi_service_create(&cfg);
    vTaskDelay(1000);
#if defined(CONFIG_AIRKISS_ENCRYPT) || defined(CONFIG_ESP_SMARTCONFIG)
    int reg_idx = 0;
    esp_wifi_setting_handle_t h = NULL;
#if defined(CONFIG_AIRKISS_ENCRYPT)
    airkiss_config_info_t air_info = AIRKISS_CONFIG_INFO_DEFAULT();
    air_info.lan_pack.appid = CONFIG_AIRKISS_APPID;
    air_info.lan_pack.deviceid = CONFIG_AIRKISS_DEVICEID;
    air_info.aes_key = CONFIG_DUER_AIRKISS_KEY;
    h = airkiss_config_create(&air_info);
#elif (defined CONFIG_ESP_SMARTCONFIG)
    smart_config_info_t info = SMART_CONFIG_INFO_DEFAULT();
    h = smart_config_create(&info);
#endif
    esp_wifi_setting_register_notify_handle(h, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);
#elif (defined CONFIG_DUER_WIFI_CONFIG)
    int32_t profile_state = duer_profile_certified();
    bool wifi_configured = wifi_has_cfg_saved();
    ESP_LOGI(TAG, "profile state %d, wifi_configured %d", (int)profile_state, (int)wifi_configured);
    if (profile_state == 1 || profile_state == 2) {
        ESP_LOGW(TAG, "\nPlease fill ${ADF_PATH}/components/dueros_service/duer_profile and enable CONFIG_DUEROS_GEN_PROFILE & CONFIG_DUEROS_FLASH_PROFILE in sdkconfig\n");
    }
    if (!wifi_configured || profile_state == 3) {
        wifi_setting_flag = wifi_cfg_start() == ESP_OK ? true : false;
    } else if (wifi_configured && profile_state == 0) {
        wifi_service_connect(wifi_serv);
    }
#elif (defined CONFIG_WIFI_STATIC_CONFIG)
    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);
#endif

    duer_audio_wrapper_init();
    duer_evt = xEventGroupCreate();
    player = duer_audio_setup_player();
    recorder = duer_audio_start_recorder(rec_engine_cb);
    audio_thread_create(NULL, "voice_read_task", voice_read_task, (void *)NULL, 2 * 1024, 5, true, 1);
    // start_sys_monitor();
}
