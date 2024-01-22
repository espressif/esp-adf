/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "board.h"
#include "esp_peripherals.h"
#include "sdkconfig.h"
#include "audio_mem.h"
#include "esp_log.h"

#include "input_key_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "blufi_config.h"
#include "periph_adc_button.h"
#include "bdsc_event_dispatcher.h"
#include "app_player_init.h"
#include "app_ota_upgrade.h"
#include "app_bt_init.h"
#include "audio_player_helper.h"
#include "audio_player_type.h"
#include "audio_player.h"

#include "ble_gatts_module.h"
#include "bds_client_event.h"
#include "bdsc_engine.h"
#include "app_control.h"

#define WIFI_CONNECTED_BIT (BIT0)
#define WIFI_WAIT_CONNECT_TIME_MS  (15000 / portTICK_PERIOD_MS)

static const char *TAG = "DU1910_APP";

static EventGroupHandle_t WIFI_CONNECTED_FLAG;
static periph_service_handle_t wifi_serv = NULL;
static bool wifi_setting_flag;
display_service_handle_t disp_serv = NULL;
esp_wifi_setting_handle_t h = NULL;

static esp_err_t wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_CONNECTED, 0);
        if (WIFI_CONNECTED_FLAG) {
            xEventGroupSetBits(WIFI_CONNECTED_FLAG, WIFI_CONNECTED_BIT);
        }
        if (wifi_setting_flag) {
            wifi_setting_flag = false;
#ifdef CONFIG_ESP_BLUFI
            blufi_send_customized_data(h);
            blufi_set_sta_connected_flag(h, true);
            ble_gatts_module_stop_adv();
#endif
        }
        // BDSC Engine need net connected notifier to restart link
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
#ifdef CONFIG_ESP_BLUFI
        blufi_set_sta_connected_flag(h, false);
#endif
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0);
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        wifi_setting_flag = false;
    }

    return ESP_OK;
}
int bdsc_ota_start(char *url, int version);


static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "type=%d, source=%d, data=%d, len=%d", evt->type, (int)evt->source, (int)evt->data, evt->len);
    audio_player_state_t st = {0};
    switch ((int)evt->data) {
        case INPUT_KEY_USER_ID_MUTE:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
                app_bt_start();
                audio_player_state_get(&st);
                if (st.media_src == MEDIA_SRC_TYPE_MUSIC_A2DP) {
                    if (((int)st.status == AUDIO_STATUS_RUNNING)) {
                        audio_player_stop();
                        ESP_LOGE(TAG, "[ * ] [Exit BT mode]");
                        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_BT_DISCONNECTED, 0);
                    } else {
                        ESP_LOGE(TAG, "[ * ] [Enter BT mode by no running]");
                        if (audio_player_music_play("aadp://44100:2@bt/sink/stream.pcm", 0, MEDIA_SRC_TYPE_MUSIC_A2DP) == ESP_ERR_AUDIO_NO_ERROR) {
                            display_service_set_pattern(disp_serv, DISPLAY_PATTERN_BT_CONNECTED, 0);
                        } else {
                            display_service_set_pattern(disp_serv, DISPLAY_PATTERN_BT_DISCONNECTED, 0);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "[ * ] [Enter BT mode]");
                    char *a2dp_url = "aadp://44100:2@bt/sink/stream.pcm";
                    event_engine_elem_EnQueque(EVENT_RECV_A2DP_START_PLAY, (uint8_t *)a2dp_url, strlen(a2dp_url) + 1);
                }
            }
            break;
        case INPUT_KEY_USER_ID_SET:
            ESP_LOGI(TAG, "[ * ] [Set] Setting Wi-Fi");
#ifdef CONFIG_ESP_BLUFI
            ble_gatts_module_start_adv();
            blufi_set_sta_connected_flag(h, false);
#endif
            if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
                if (wifi_setting_flag == false) {
                    audio_player_state_get(&st);
                    if (((int)st.status == AUDIO_STATUS_RUNNING)) {
                        audio_player_stop();
                    }
                    bdsc_play_hint(BDSC_HINT_SC);
                    wifi_service_setting_start(wifi_serv, 0);
                    wifi_setting_flag = true;
                    ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting started.");
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_SETTING, 0);
                } else {
                    ESP_LOGW(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting will be stopped.");
                    wifi_service_setting_stop(wifi_serv, 0);
                    wifi_setting_flag = false;
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_SETTING_FINISHED, 0);
                }
            }
            break;
        case INPUT_KEY_USER_ID_VOLDOWN:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
                ESP_LOGI(TAG, "[ * ] [Vol-] input key event");
                int player_volume = 0;
                audio_player_vol_get(&player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_player_vol_set(player_volume);
                ESP_LOGI(TAG, "Now volume is %d", player_volume);
            }
            break;
        case INPUT_KEY_USER_ID_VOLUP:
            if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
                ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
                int player_volume = 0;
                audio_player_vol_get(&player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_player_vol_set(player_volume);
                ESP_LOGI(TAG, "Now volume is %d", player_volume);
            } else if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
                ESP_LOGI(TAG, "[ * ] [Vol+] press event");
            }
        default:
            break;
    }
    return ESP_OK;
}

void audio_player_callback(audio_player_state_t *audio, void *ctx)
{
    ESP_LOGW(TAG, "AUDIO_PLAYER_CALLBACK send OK, status:%d, err_msg:%x, media_src:%x, ctx:%p",
             audio->status, audio->err_msg, audio->media_src, ctx);
}

static const char *conn_state_str[] = { "Disconnected", "Connecting", "Connected", "Disconnecting" };

static void user_a2dp_sink_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    ESP_LOGI(TAG, "A2DP sink user cb");
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT: {
                esp_a2d_cb_param_t *a2d = param;
                uint8_t *bda = a2d->conn_stat.remote_bda;
                app_bt_set_addr(&a2d->conn_stat.remote_bda);
                ESP_LOGI(TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                         conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
                if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {

                    ESP_LOGI(TAG, "A2DP disconnected");
                } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                    ESP_LOGI(TAG, "A2DP connected");
                    display_service_set_pattern(disp_serv, DISPLAY_PATTERN_BT_CONNECTED, 0);
                } else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING) {
                }
                break;
            }
        case ESP_A2D_AUDIO_STATE_EVT: {

                // Some wrong actions occur if use this event to control
                // playing in bluetooth source. So we use avrc control.
                break;
            }
        default:
            ESP_LOGI(TAG, "User cb A2DP event: %d", event);
            break;
    }
}

void app_init(void)
{
    // Clear the debug message
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("A2DP_STREAM", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_ERROR);
    esp_log_level_set("spi_master", ESP_LOG_WARN);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_WARN);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_WARN);
    esp_log_level_set("AUDIO_MANAGER", ESP_LOG_WARN);

    WIFI_CONNECTED_FLAG = xEventGroupCreate();

    // step 1. init the peripherals set and peripherls
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.extern_stack = true;
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // step 2. initialize sdcard
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    // step 3. setup display service
    disp_serv = audio_board_led_init();

    // step 4. setup Wi-Fi service
    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.extern_stack = true;
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = NULL;
    cfg.task_prio = 11;
    cfg.setting_timeout_s = 3600;
    cfg.max_retry_time = -1;
    wifi_serv = wifi_service_create(&cfg);
    vTaskDelay(1000);
    int reg_idx = 0;

#ifdef CONFIG_AIRKISS_ENCRYPT
    airkiss_config_info_t air_info = AIRKISS_CONFIG_INFO_DEFAULT();
    air_info.lan_pack.appid = CONFIG_AIRKISS_APPID;
    air_info.lan_pack.deviceid = CONFIG_AIRKISS_DEVICEID;
    air_info.aes_key = CONFIG_AIRKISS_KEY;
    h = airkiss_config_create(&air_info);
    ESP_LOGI(TAG, "AIRKISS wifi setting module has been selected");
#elif (defined CONFIG_ESP_SMARTCONFIG)
    smart_config_info_t info = SMART_CONFIG_INFO_DEFAULT();
    h = smart_config_create(&info);
    ESP_LOGI(TAG, "SMARTCONFIG wifi setting module has been selected");
#elif (defined CONFIG_ESP_BLUFI)
    ESP_LOGI(TAG, "ESP_BLUFI wifi setting module has been selected");
    h = blufi_config_create(NULL);
    //blufi_set_customized_data(h, "hello world\n", strlen("hello world\n"));
#endif
    esp_wifi_setting_register_notify_handle(h, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);

    // step 5. wait for wifi connected, and start ota service
    EventBits_t bits = xEventGroupWaitBits(WIFI_CONNECTED_FLAG, WIFI_CONNECTED_BIT, true, false, WIFI_WAIT_CONNECT_TIME_MS);
    if (bits & WIFI_CONNECTED_BIT) {
        // app_ota_start();
    } else {
        ESP_LOGW(TAG, "WIFI  connection timeout(%dms), skipped OTA service", (int)WIFI_WAIT_CONNECT_TIME_MS);
    }
    vEventGroupDelete(WIFI_CONNECTED_FLAG);
    WIFI_CONNECTED_FLAG = NULL;

    // step 6. setup the input key service
    audio_board_key_init(set);
    input_key_service_info_t input_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t key_serv_info = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    key_serv_info.based_cfg.extern_stack = false;
    key_serv_info.handle = set;
    periph_service_handle_t input_key_handle = input_key_service_create(&key_serv_info);

    AUDIO_NULL_CHECK(TAG, input_key_handle, return);
    input_key_service_add_key(input_key_handle, input_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_key_handle, input_key_service_cb, NULL);

    // step 7. setup the esp_player
#if CONFIG_BT_ENABLED
    app_player_init(NULL, audio_player_callback, set, user_a2dp_sink_cb);
#else
    app_player_init(NULL, audio_player_callback, set, NULL);
#endif

    audio_player_vol_set(40);
}
