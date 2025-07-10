/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
#include "esp_gmf_afe.h"
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"

#include "iot_button.h"
#include "button_adc.h"
#include "esp_coze_chat.h"
#include "audio_processor.h"

#define BUTTON_REC_READING    (1 << 0)
#define DEFAULT_REC_READ_SIZE 4096

#if defined CONFIG_UPLOAD_FORMAT_OPUS
#define UPLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_OPUS
#elif defined CONFIG_UPLOAD_FORMAT_G711A
#define UPLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_G711A
#elif defined CONFIG_UPLOAD_FORMAT_G711U
#define UPLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_G711U
#elif defined CONFIG_UPLOAD_FORMAT_PCM
#define UPLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_PCM
#endif  /* CONFIG_UPLOAD_FORMAT_OPUS */

#if defined CONFIG_DOWNLOAD_FORMAT_OPUS
#define DOWNLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_OPUS
#elif defined CONFIG_DOWNLOAD_FORMAT_G711A
#define DOWNLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_G711A
#elif defined CONFIG_DOWNLOAD_FORMAT_G711U
#define DOWNLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_G711U
#elif defined CONFIG_DOWNLOAD_FORMAT_PCM
#define DOWNLINK_AUDIO_ENCODE_TYPE ESP_COZE_CHAT_AUDIO_TYPE_PCM
#endif  /* CONFIG_DOWNLOAD_FORMAT_OPUS */

static char *TAG = "COZE_CHAT_APP";

struct coze_chat_t {
    esp_coze_chat_handle_t chat;
    bool                   wakeuped;
    esp_gmf_oal_thread_t   read_thread;
    esp_gmf_oal_thread_t   btn_thread;
    EventGroupHandle_t     data_evt_group;
    QueueHandle_t          btn_evt_q;
};

static struct coze_chat_t coze_chat;

static void audio_event_callback(esp_coze_chat_event_t event, char *data, void *ctx)
{
    if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED) {
        ESP_LOGI(TAG, "chat start");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED) {
        ESP_LOGI(TAG, "chat stop");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA) {
        // cjson format data
        ESP_LOGI(TAG, "Customer data: %s", data);
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT) {
        ESP_LOGI(TAG, "Subtitle data: %s", data);
    }
}

static void audio_data_callback(char *data, int len, void *ctx)
{
    audio_playback_feed_data((uint8_t *)data, len);
}

static esp_err_t init_coze_chat()
{
    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.enable_subtitle = true;
    chat_config.bot_id = CONFIG_COZE_BOT_ID;
    chat_config.access_token = CONFIG_COZE_ACCESS_TOKEN;
    chat_config.uplink_audio_type = UPLINK_AUDIO_ENCODE_TYPE;
    chat_config.downlink_audio_type = DOWNLINK_AUDIO_ENCODE_TYPE;
    chat_config.audio_callback = audio_data_callback;
    chat_config.event_callback = audio_event_callback;
#ifdef CONFIG_KEY_PRESS_DIALOG_MODE
    chat_config.websocket_buffer_size = 4096;
    chat_config.mode = ESP_COZE_CHAT_NORMAL_MODE;
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */

    esp_err_t ret = ESP_OK;
    ret = esp_coze_chat_init(&chat_config, &coze_chat.chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_coze_chat_init failed, err: %d", ret);
        return ESP_FAIL;
    }
    if (esp_coze_chat_start(coze_chat.chat) != ESP_OK) {
        ESP_LOGE(TAG, "esp_coze_chat_start failed");
        esp_coze_chat_deinit(coze_chat.chat);
        return ESP_FAIL;
    }

    return ESP_OK;
}

#ifndef CONFIG_KEY_PRESS_DIALOG_MODE
static void recorder_event_callback_fn(void *event, void *ctx)
{
    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START:
            ESP_LOGI(TAG, "wakeup start");
            if (coze_chat.wakeuped) {
                esp_coze_chat_send_audio_cancel(coze_chat.chat);
            }
            audio_prompt_play("file://spiffs/dingding.wav");
            coze_chat.wakeuped = true;
            break;
        case ESP_GMF_AFE_EVT_WAKEUP_END:
            ESP_LOGI(TAG, "wakeup end");
            coze_chat.wakeuped = false;
            break;
        case ESP_GMF_AFE_EVT_VAD_START:
            ESP_LOGI(TAG, "vad start");
            break;
        case ESP_GMF_AFE_EVT_VAD_END:
            ESP_LOGI(TAG, "vad end");
            break;
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
            ESP_LOGI(TAG, "vcmd detect timeout");
            break;
        default: {
            // TODO: vcmd detected
            // esp_gmf_afe_vcmd_info_t *info = event->event_data;
            // ESP_LOGW(TAG, "Command %d, phrase_id %d, prob %f, str: %s", sevent->type, info->phrase_id, info->prob, info->str);
        }
    }
}
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */

#if CONFIG_KEY_PRESS_DIALOG_MODE
static void button_event_cb(void *arg, void *data)
{
    button_event_t button_event = iot_button_get_event(arg);
    xQueueSend(coze_chat.btn_evt_q, &button_event, 0);
}

static void btn_event_task(void *pv)
{
    button_event_t btn_evt;
    while (1) {
        if (xQueueReceive(coze_chat.btn_evt_q, &btn_evt, portMAX_DELAY) == pdTRUE) {
            switch (btn_evt) {
                case BUTTON_PRESS_DOWN:
                    esp_coze_chat_send_audio_cancel(coze_chat.chat);
                    xEventGroupSetBits(coze_chat.data_evt_group, BUTTON_REC_READING);
                    break;
                case BUTTON_PRESS_UP:
                    xEventGroupClearBits(coze_chat.data_evt_group, BUTTON_REC_READING);
                    esp_coze_chat_send_audio_complete(coze_chat.chat);
                    break;
                default:
                    break;
            }
        }
    }
}
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */

static void audio_data_read_task(void *pv)
{
    uint8_t *data = esp_gmf_oal_calloc(1, DEFAULT_REC_READ_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        vTaskDelete(NULL);
    }

    int ret = 0;
    while (true) {
#if defined CONFIG_KEY_PRESS_DIALOG_MODE
        xEventGroupWaitBits(coze_chat.data_evt_group, BUTTON_REC_READING, pdFALSE, pdFALSE, portMAX_DELAY);
        ret = audio_recorder_read_data(data, DEFAULT_REC_READ_SIZE);
        if (ret > 0) {
            esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
        }

#elif defined CONFIG_VOICE_WAKEUP_MODE
        ret = audio_recorder_read_data(data, DEFAULT_REC_READ_SIZE);
        if (coze_chat.wakeuped) {
            esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
        }
#else
        ret = audio_recorder_read_data(data, DEFAULT_REC_READ_SIZE);
        esp_coze_chat_send_audio_data(coze_chat.chat, (char *)data, ret);
#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */
    }
}

static void audio_pipe_open()
{
    audio_manager_init();

#if CONFIG_KEY_PRESS_DIALOG_MODE
    audio_recorder_open(NULL, NULL);
#else
    audio_prompt_open();
    audio_recorder_open(recorder_event_callback_fn, NULL);
#endif /* CONFIG_KEY_PRESS_DIALOG_MODE */
    audio_playback_open();
    audio_playback_run();
}

esp_err_t coze_chat_app_init(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    coze_chat.wakeuped = false;

#if CONFIG_KEY_PRESS_DIALOG_MODE
    /** ESP32-S3-Korvo2 board */
    button_handle_t btn = NULL;
    const button_config_t btn_cfg = {0};
    button_adc_config_t btn_adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = 4,
        .button_index = 0,
        .min = 2310,
        .max = 2510
    };
    iot_button_new_adc_device(&btn_cfg, &btn_adc_cfg, &btn);
    ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL));
    ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_PRESS_UP, NULL, button_event_cb, NULL));
    coze_chat.data_evt_group = xEventGroupCreate();
    coze_chat.btn_evt_q = xQueueCreate(2, sizeof(button_event_t));
    esp_gmf_oal_thread_create(&coze_chat.btn_thread, "btn_event_task", btn_event_task, (void *)NULL, 3096, 12, true, 1);

#endif  /* CONFIG_KEY_PRESS_DIALOG_MODE */

    if (init_coze_chat() != ESP_OK) {
        ESP_LOGE(TAG, "init_coze_chat failed");
        return ESP_FAIL;
    }

    audio_pipe_open();

    esp_gmf_oal_thread_create(&coze_chat.read_thread, "audio_data_read_task", audio_data_read_task, (void *)NULL, 4096, 12, true, 1);
    
    return ESP_OK;
}
