/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "periph_touch.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "board.h"

#include "sdkconfig.h"
#include "audio_mem.h"
#include "dueros_app.h"
#include "recorder_engine.h"
#include "esp_audio.h"
#include "esp_log.h"

#include "duer_audio_wrapper.h"
#include "dueros_service.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"

#include "display_service.h"
#include "led_indicator.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"

static const char *TAG              = "DUEROS";
extern esp_audio_handle_t           player;

static audio_service_handle_t duer_serv_handle = NULL;
static display_service_handle_t disp_serv = NULL;
static periph_service_handle_t wifi_serv = NULL;
static bool wifi_setting_flag;

void rec_engine_cb(rec_event_type_t type, void *user_data)
{
    if (REC_EVENT_WAKEUP_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            return;
        }
        if (duer_audio_wrapper_get_state() == AUDIO_STATUS_RUNNING) {
            duer_audio_wrapper_pause();
        }
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);
    } else if (REC_EVENT_VAD_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
        audio_service_start(duer_serv_handle);
    } else if (REC_EVENT_VAD_STOP == type) {
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(duer_serv_handle);
        }
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP, state:%d", dueros_service_state_get());
    } else if (REC_EVENT_WAKEUP_END == type) {
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(duer_serv_handle);
        }
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
    } else {

    }
}

static esp_err_t recorder_pipeline_open(void **handle)
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t recorder;
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return ESP_FAIL;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_reader, &i2s_info);
    i2s_info.bits = 16;
    i2s_info.channels = 2;
    i2s_info.sample_rates = 48000;
    audio_element_setinfo(i2s_stream_reader, &i2s_info);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 1;
    rsp_cfg.type = AUDIO_CODEC_TYPE_ENCODER;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
    audio_pipeline_register(recorder, filter, "filter");
    audio_pipeline_register(recorder, raw_read, "raw");
    audio_pipeline_link(recorder, (const char *[]) {"i2s", "filter", "raw"}, 3);
    audio_pipeline_run(recorder);
    ESP_LOGI(TAG, "Recorder has been created");
    *handle = recorder;
    return ESP_OK;
}

static esp_err_t recorder_pipeline_read(void *handle, char *data, int data_size)
{
    raw_stream_read(audio_pipeline_get_el_by_tag((audio_pipeline_handle_t)handle, "raw"), data, data_size);
    return ESP_OK;
}

static esp_err_t recorder_pipeline_close(void *handle)
{
    audio_pipeline_deinit(handle);
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
        wifi_setting_flag = false;
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_DISCONNECTED, 0);

    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        wifi_setting_flag = false;
    }

    return ESP_OK;
}
static void retry_login_timer_cb(xTimerHandle tmr)
{
    ESP_LOGE(TAG, "Func:%s", __func__);
    audio_service_connect(duer_serv_handle);
    xTimerStop(tmr, 0);
}

static esp_err_t duer_callback(audio_service_handle_t handle, service_event_t *evt, void *ctx)
{
    static int retry_num = 1;
    int state = *((int *)evt->data);
    ESP_LOGW(TAG, "duer_callback: type:%x, source:%p data:%d, data_len:%d", evt->type, evt->source, state, evt->len);
    switch (state) {
        case SERVICE_STATE_IDLE: {
                xTimerHandle retry_login_timer =  (xTimerHandle) ctx;
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
        case SERVICE_STATE_CONNECTING: break;
        case SERVICE_STATE_CONNECTED: break;
            retry_num = 1;
        case SERVICE_STATE_RUNNING: break;
        case SERVICE_STATE_STOPED: break;
        default:
            break;
    }

    return ESP_OK;
}


esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    ESP_LOGD(TAG, "Periph Event received: src_type:%x, source:%p cmd:%d, data:%p, data_len:%d",
             event->source_type, event->source, event->cmd, event->data, event->data_len);
    switch (event->source_type) {
        case PERIPH_ID_BUTTON: {
                if ((int)event->data == get_input_rec_id() && event->cmd == PERIPH_BUTTON_PRESSED) {
                    ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC");
                    rec_engine_trigger_start();
                } else if ((int)event->data == get_input_mode_id() &&
                           ((event->cmd == PERIPH_BUTTON_RELEASE) || (event->cmd == PERIPH_BUTTON_LONG_RELEASE))) {
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
                        wifi_service_setting_start(wifi_serv, 0);
                        wifi_setting_flag = true;
                        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_WIFI_SETTING, 0);
                        ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting started.");
                    } else {
                        ESP_LOGW(TAG, "AUDIO_USER_KEY_WIFI_SET, WiFi setting will be stopped.");
                        wifi_service_setting_stop(wifi_serv, 0);
                        wifi_setting_flag = false;
                        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_OFF, 0);
                    }
                } else if ((int)event->data == TOUCH_PAD_NUM9 && (event->cmd == PERIPH_BUTTON_RELEASE)) {

                }
                break;
            }
        default:
            break;
    }
    return ESP_OK;
}

void duer_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    if (set != NULL) {
        esp_periph_set_register_callback(set, periph_callback, NULL);
    }
    led_indicator_handle_t led = led_indicator_init((gpio_num_t)get_green_led_gpio());
    display_service_config_t display = {
        .based_cfg = {
            .task_stack = 0,
            .task_prio  = 0,
            .task_core  = 0,
            .task_func  = NULL,
            .service_start = NULL,
            .service_stop = NULL,
            .service_destroy = NULL,
            .service_ioctl = led_indicator_pattern,
            .service_name = "DISPLAY_serv",
            .user_data = NULL,
        },
        .instance = led,
    };
    disp_serv = display_service_create(&display);

    periph_button_cfg_t btn_cfg = {
        .gpio_mask = (1ULL << get_input_rec_id()) | (1ULL << get_input_mode_id()), //REC BTN & MODE BTN
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    esp_periph_start(set, button_handle);

    // If enabled, the touch will consume a lot of CPU.
    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    esp_periph_start(set, touch_periph);

    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = get_sdcard_intr_gpio(), //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_periph_start(set, sdcard_handle);
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }

    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, strlen(CONFIG_WIFI_SSID));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, strlen(CONFIG_WIFI_PASSWORD));

    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = NULL;
    cfg.setting_timeout_s = 60;
    wifi_serv = wifi_service_create(&cfg);

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
#endif
    esp_wifi_setting_regitster_notify_handle(h, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);

    rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
    eng.vad_off_delay_ms = 800;
    eng.wakeup_time_ms = 10 * 1000;
    eng.evt_cb = rec_engine_cb;
    eng.open = recorder_pipeline_open;
    eng.close = recorder_pipeline_close;
    eng.fetch = recorder_pipeline_read;
    eng.extension = NULL;
    eng.support_encoding = false;
    eng.user_data = NULL;
    rec_engine_create(&eng);

    xTimerHandle retry_login_timer = xTimerCreate("tm_duer_login", 1000 / portTICK_PERIOD_MS,
                                     pdFALSE, NULL, retry_login_timer_cb);
    duer_serv_handle = dueros_service_create();

    audio_service_set_callback(duer_serv_handle, duer_callback, retry_login_timer);

}
