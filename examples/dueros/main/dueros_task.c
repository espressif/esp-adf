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

#include "esp_audio_device_info.h"
#include "duer_audio_wrapper.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_voice.h"
#include "lightduer_connagent.h"
#include "lightduer_dcs.h"

#include "periph_touch.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "board.h"

#include "sdkconfig.h"
#include "audio_mem.h"
#include "dueros_task.h"
#include "recorder_engine.h"
#include "esp_audio.h"
#include "esp_log.h"
#include "led.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"

#define DUEROS_TASK_PRIORITY        5
#define DUEROS_TASK_SIZE            6*1024
#define RECORD_SAMPLE_RATE          (16000)

#define FRACTION_BITS               (8)
#define CLAMPS_MAX_SHORT            (32767)
#define CLAMPS_MIN_SHORT            (-32768)

#define RECORD_DEBUG                0

static const char *TAG              = "DUEROS";
static TimerHandle_t                retry_login_timer;
extern esp_audio_handle_t           player;
static esp_periph_handle_t          wifi_periph_handle;
static xQueueHandle                 duer_que;
static TaskHandle_t                 wifi_set_tsk_handle;
static bool                         duer_login_success;

static EventGroupHandle_t           duer_task_evts;
const static int WIFI_CONNECT_BIT   = BIT0;

extern const uint8_t _duer_profile_start[] asm("_binary_duer_profile_start");
extern const uint8_t _duer_profile_end[]   asm("_binary_duer_profile_end");

typedef enum {
    DUER_CMD_UNKNOWN,
    DUER_CMD_LOGIN,
    DUER_CMD_CONNECTED,
    DUER_CMD_START,
    DUER_CMD_STOP,
    DUER_CMD_QUIT,
} duer_task_cmd_t;

typedef enum {
    DUER_STATE_IDLE,
    DUER_STATE_CONNECTING,
    DUER_STATE_CONNECTED,
    DUER_STATE_START,
    DUER_STATE_STOP,
} duer_task_state_t;

typedef struct {
    duer_task_cmd_t     type;
    uint32_t            *pdata;
    int                 index;
    int                 len;
} duer_task_msg_t;

static duer_task_state_t duer_state = DUER_STATE_IDLE;

static void duer_que_send(void *que, duer_task_cmd_t type, void *data, int index, int len, int dir)
{
    duer_task_msg_t evt = {0};
    evt.type = type;
    evt.pdata = data;
    evt.index = index;
    evt.len = len;
    if (dir) {
        xQueueSendToFront(que, &evt, 0) ;
    } else {
        xQueueSend(que, &evt, 0);
    }
}

void rec_engine_cb(rec_event_type_t type, void *user_data)
{
    if (REC_EVENT_WAKEUP_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        if (duer_state == DUER_STATE_START) {
            return;
        }
        if (duer_audio_wrapper_get_state() == AUDIO_STATUS_RUNNING) {
            duer_audio_wrapper_pause();
        }
        led_indicator_set(0, led_work_mode_turn_on);
    } else if (REC_EVENT_VAD_START == type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
        duer_que_send(duer_que, DUER_CMD_START, NULL, 0, 0, 0);
    } else if (REC_EVENT_VAD_STOP == type) {
        if (duer_state == DUER_STATE_START) {
            duer_que_send(duer_que, DUER_CMD_STOP, NULL, 0, 0, 0);
            led_indicator_set(0, led_work_mode_turn_off);
        }
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP, state:%d", duer_state);
    } else if (REC_EVENT_WAKEUP_END == type) {
        if (duer_state == DUER_STATE_START) {
            duer_que_send(duer_que, DUER_CMD_STOP, NULL, 0, 0, 0);
        }
        led_indicator_set(0, led_work_mode_turn_off);
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
    } else {

    }
}

void retry_login_timer_cb(xTimerHandle tmr)
{
    ESP_LOGE(TAG, "Func:%s", __func__);
    duer_que_send(duer_que, DUER_CMD_LOGIN, NULL, 0, 0, 0);
    xTimerStop(tmr, 0);
}

static void report_info_task(void *pvParameters)
{
    int ret;
    ret = duer_report_device_info();
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "Report device info failed ret:%d", ret);
    }
    vTaskDelete(NULL);
}

static void duer_event_hook(duer_event_t *event)
{
    if (!event) {
        ESP_LOGE(TAG, "NULL event!!!");
    }
    ESP_LOGE(TAG, "event: %d", event->_event);
    switch (event->_event) {
        case DUER_EVENT_STARTED:
            // Initialize the DCS API
            duer_dcs_init();
            duer_login_success = true;
            duer_que_send(duer_que, DUER_CMD_CONNECTED, NULL, 0, 0, 0);
            ESP_LOGE(TAG, "event: DUER_EVENT_STARTED");
            xTaskCreate(&report_info_task, "report_info_task", 1024 * 2, NULL, 5, NULL);
            break;
        case DUER_EVENT_STOPPED:
            ESP_LOGE(TAG, "event: DUER_EVENT_STOPPED");
            duer_login_success = false;
            duer_que_send(duer_que, DUER_CMD_QUIT, NULL, 0, 0, 0);
            break;
    }
}

static void duer_login(void)
{
    int sz = _duer_profile_end - _duer_profile_start;
    char *data = audio_calloc_inner(1, sz);
    if (NULL == data) {
        ESP_LOGE(TAG, "audio_malloc failed");
    }
    memcpy(data, _duer_profile_start, sz);
    ESP_LOGI(TAG, "duer_start, len:%d\n%s", sz, data);
    duer_start(data, sz);
    audio_free((void *)data);
}

static esp_err_t recorder_pipeline_open(void **handle)
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t recorder;
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

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

static void dueros_task(void *pvParameters)
{
    duer_initialize();
    duer_set_event_callback(duer_event_hook);
    duer_init_device_info();

    uint8_t *voiceData = audio_calloc(1, REC_ONE_BLOCK_SIZE);
    if (NULL == voiceData) {
        ESP_LOGE(TAG, "Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
        return;
    }
    static duer_task_msg_t duer_msg;
    // xEventGroupWaitBits(duer_task_evts, WIFI_CONNECT_BIT, false, true, 5000 / portTICK_PERIOD_MS);

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

    int retry_time = 1000 / portTICK_PERIOD_MS;
    int retry_num = 1;
    retry_login_timer = xTimerCreate("tm_duer_login", retry_time,
                                     pdFALSE, NULL, retry_login_timer_cb);
    FILE *file = NULL;
#if RECORD_DEBUG
    file = fopen("/sdcard/rec_adf_1.wav", "w+");
    if (NULL == file) {
        ESP_LOGW(TAG, "open rec_adf_1.wav failed,[%d]", __LINE__);
    }
#endif
    int task_run = 1;
    while (task_run) {
        if (xQueueReceive(duer_que, &duer_msg, portMAX_DELAY)) {
            if (duer_msg.type == DUER_CMD_LOGIN) {
                ESP_LOGE(TAG, "Recv Que DUER_CMD_LOGIN");
                if (duer_state == DUER_STATE_IDLE) {
                    duer_login();
                    duer_state = DUER_STATE_CONNECTING;
                } else {
                    ESP_LOGW(TAG, "DUER_CMD_LOGIN connecting,duer_state = %d", duer_state);
                }
            } else if (duer_msg.type == DUER_CMD_CONNECTED) {
                ESP_LOGI(TAG, "Dueros DUER_CMD_CONNECTED, duer_state:%d", duer_state);
                duer_state = DUER_STATE_CONNECTED;
                retry_num = 1;
            } else if (duer_msg.type == DUER_CMD_START) {
                if (duer_state < DUER_STATE_CONNECTED) {
                    ESP_LOGW(TAG, "Dueros has not connected, state:%d", duer_state);
                    continue;
                }
                ESP_LOGI(TAG, "Recv Que DUER_CMD_START");
                duer_voice_start(RECORD_SAMPLE_RATE);
                duer_dcs_on_listen_started();
                duer_state = DUER_STATE_START;
                while (1) {
                    int ret = rec_engine_data_read(voiceData, REC_ONE_BLOCK_SIZE, 110 / portTICK_PERIOD_MS);
                    ESP_LOGD(TAG, "index = %d", ret);
                    if ((ret == 0) || (ret == -1)) {
                        break;
                    }
                    if (file) {
                        fwrite(voiceData, 1, REC_ONE_BLOCK_SIZE, file);
                    }
                    ret  = duer_voice_send(voiceData, REC_ONE_BLOCK_SIZE);
                    if (ret < 0) {
                        ESP_LOGE(TAG, "duer_voice_send failed ret:%d", ret);
                        break;
                    }
                }
            } else if (duer_msg.type == DUER_CMD_STOP)  {
                ESP_LOGI(TAG, "Dueros DUER_CMD_STOP");
                if (file) {
                    fclose(file);
                }
                duer_voice_stop();
                duer_state = DUER_STATE_STOP;
            } else if (duer_msg.type == DUER_CMD_QUIT && (duer_state != DUER_STATE_IDLE))  {
                if (duer_login_success) {
                    duer_stop();
                }
                duer_state = DUER_STATE_IDLE;
                if (PERIPH_WIFI_SETTING == periph_wifi_is_connected(wifi_periph_handle)) {
                    continue;
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
                xTimerChangePeriod(retry_login_timer, retry_time * retry_num, portMAX_DELAY);
                xTimerStart(retry_login_timer, portMAX_DELAY);
            }
        }
    }
    free(voiceData);
    vTaskDelete(NULL);
}

void wifi_set_task (void *para)
{
    periph_wifi_config_start(wifi_periph_handle, WIFI_CONFIG_ESPTOUCH);
    if (ESP_OK == periph_wifi_config_wait_done(wifi_periph_handle, 30000 / portTICK_PERIOD_MS)) {
        ESP_LOGI(TAG, "Wi-Fi setting successfully");
    } else {
        ESP_LOGE(TAG, "Wi-Fi setting timeout");
    }
    wifi_set_tsk_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    ESP_LOGD(TAG, "Periph Event received: src_type:%x, source:%p cmd:%d, data:%p, data_len:%d",
             event->source_type, event->source, event->cmd, event->data, event->data_len);
    switch (event->source_type) {
        case PERIPH_ID_BUTTON: {
                if ((int)event->data == GPIO_REC && event->cmd == PERIPH_BUTTON_PRESSED) {
                    ESP_LOGI(TAG, "PERIPH_NOTIFY_KEY_REC");
                    rec_engine_trigger_start();
                } else if ((int)event->data == GPIO_MODE &&
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
                    ESP_LOGI(TAG, "AUDIO_USER_KEY_WIFI_SET [%d]", __LINE__);
                    if (NULL == wifi_set_tsk_handle) {
                        if (xTaskCreate(wifi_set_task, "WifiSetTask", 3 * 1024, duer_que,
                                        DUEROS_TASK_PRIORITY, &wifi_set_tsk_handle) != pdPASS) {
                            ESP_LOGE(TAG, "Error create WifiSetTask");
                        }
                        led_indicator_set(0, led_work_mode_setting);
                    } else {
                        ESP_LOGW(TAG, "WifiSetTask has already running");
                    }
                } else if ((int)event->data == TOUCH_PAD_NUM9 && (event->cmd == PERIPH_BUTTON_RELEASE)) {

                }
                break;
            }
        case PERIPH_ID_WIFI: {
                if (event->cmd == PERIPH_WIFI_CONNECTED) {
                    ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
                    duer_que_send(duer_que, DUER_CMD_LOGIN, NULL, 0, 0, 0);
                    led_indicator_set(0, led_work_mode_connectok);
                    // xEventGroupSetBits(duer_task_evts, WIFI_CONNECT_BIT);
                } else if (event->cmd == PERIPH_WIFI_DISCONNECTED) {
                    ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
                    led_indicator_set(0, led_work_mode_disconnect);
                }
                break;
            }
        default:
            break;
    }
    return ESP_OK;
}

void duer_service_create(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "ADF version is %s", ADF_VER);


    duer_que = xQueueCreate(3, sizeof(duer_task_msg_t));
    configASSERT(duer_que);
    esp_periph_config_t periph_cfg = {
        .user_context = NULL,
        .event_handle = periph_callback,
    };
    if (ESP_EXISTS == esp_periph_init(&periph_cfg)) {
        esp_periph_set_callback(periph_callback);
    }
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_36 | GPIO_SEL_39, //REC BTN & MODE BTN
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);
    esp_periph_start(button_handle);

    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    esp_periph_start(touch_periph);

    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_periph_start(sdcard_handle);
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    wifi_periph_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(wifi_periph_handle);

    duer_task_evts = xEventGroupCreate();

    if (xTaskCreatePinnedToCore(dueros_task, "duerosTask", DUEROS_TASK_SIZE, duer_que,
                                DUEROS_TASK_PRIORITY, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "Error create duerosTask");
        return;
    }
}
