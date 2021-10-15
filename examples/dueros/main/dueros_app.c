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

#include "sdkconfig.h"
#include "audio_mem.h"
#include "dueros_app.h"
#include "esp_audio.h"
#include "esp_log.h"

#include "duer_audio_wrapper.h"
#include "dueros_service.h"
#include "audio_mem.h"
#include "audio_recorder.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "recorder_sr.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "audio_sys.h"
#include "audio_idf_version.h"

#include "display_service.h"
#include "wifi_service.h"
#include "airkiss_config.h"
#include "smart_config.h"
#include "periph_adc_button.h"
#include "algorithm_stream.h"

#include "model_path.h"


#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
#include "driver/touch_pad.h"
#endif

static const char *TAG              = "DUEROS";
extern esp_audio_handle_t           player;

static audio_rec_handle_t recorder = NULL;
static audio_element_handle_t raw_read;
static audio_service_handle_t duer_serv_handle = NULL;
static display_service_handle_t disp_serv = NULL;
static periph_service_handle_t wifi_serv = NULL;
static bool wifi_setting_flag;
extern int duer_dcs_audio_sync_play_tone(const char *uri);

static esp_err_t rec_engine_cb(audio_rec_evt_t type, void *user_data)
{
    if (AUDIO_REC_WAKEUP_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_START");
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            return ESP_OK;
        }
        if (duer_audio_wrapper_get_state() == AUDIO_STATUS_RUNNING) {
            duer_audio_wrapper_pause();
        }
        display_service_set_pattern(disp_serv, DISPLAY_PATTERN_TURN_ON, 0);
        ESP_LOGI(TAG, "rec_engine_cb - Play tone");
        duer_dcs_audio_sync_play_tone("file://sdcard/dingding.wav");
    } else if (AUDIO_REC_VAD_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_START");
        audio_service_start(duer_serv_handle);
    } else if (AUDIO_REC_VAD_END == type) {
        if (dueros_service_state_get() == SERVICE_STATE_RUNNING) {
            audio_service_stop(duer_serv_handle);
        }
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_STOP, state:%d", dueros_service_state_get());
    } else if (AUDIO_REC_WAKEUP_END == type) {
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
        case SERVICE_STATE_CONNECTED:
            retry_num = 1;
            break;
        case SERVICE_STATE_RUNNING: break;
        case SERVICE_STATE_STOPPED: break;
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
                    audio_recorder_trigger_start(recorder);
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

            } else if (((int)event->data == get_input_set_id()) && (event->cmd == PERIPH_ADC_BUTTON_RELEASE)) {
                esp_audio_vol_set(player, 0);
                ESP_LOGI(TAG, "AUDIO_USER_KEY_VOL_MUTE [0]");
            }
            break;
        default:
            break;
    }
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
    xTaskCreatePinnedToCore(sys_monitor_task, "sys_monitor_task", (2 * 1024), NULL, 1, NULL, 1);
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}

static void start_recorder()
{
#ifdef CONFIG_MODEL_IN_SPIFFS
    srmodel_spiffs_init();
#endif
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (NULL == pipeline) {
        return;
    }

#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.use_apll = 0;
    i2s_cfg.i2s_config.sample_rate = 16000;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
#else
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    i2s_cfg.i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
#endif
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_reader, &i2s_info);
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    i2s_info.bits = 32;
#else
    i2s_info.bits = 16;
#endif
    i2s_info.channels = 2;
    i2s_info.sample_rates = 48000;
    audio_element_setinfo(i2s_stream_reader, &i2s_info);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.dest_rate = 16000;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
#endif

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");


#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    const char *link_tag[2] = {"i2s", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
#else
    audio_pipeline_register(pipeline, filter, "filter");
    const char *link_tag[3] = {"i2s", "filter", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif

    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "Recorder has been created");

    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.aec_init = false;
    recorder_sr_cfg.afe_cfg.alloc_from_psram = 3;
    recorder_sr_cfg.afe_cfg.agc_mode = 0;
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0))
    recorder_sr_cfg.input_order[0] = DAT_CH_REF0;
    recorder_sr_cfg.input_order[1] = DAT_CH_0;
#endif

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_cb_for_afe;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.event_cb = rec_engine_cb;
    cfg.vad_off = 1000;
    recorder = audio_recorder_create(&cfg);
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
    audio_board_init();
    audio_board_key_init(set);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
#ifndef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    disp_serv = audio_board_led_init();
#endif

    start_recorder();

    xTimerHandle retry_login_timer = xTimerCreate("tm_duer_login", 1000 / portTICK_PERIOD_MS,
                                     pdFALSE, NULL, retry_login_timer_cb);
    duer_serv_handle = dueros_service_create(recorder);
    audio_service_set_callback(duer_serv_handle, duer_callback, retry_login_timer);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = NULL;
    cfg.setting_timeout_s = 60;
    wifi_serv = wifi_service_create(&cfg);
    vTaskDelay(1000);
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

    duer_audio_wrapper_init();
    // start_sys_monitor();
}
