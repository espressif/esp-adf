/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "unity.h"
#include "esp_coze_chat.h"

#define CONFIG_COZE_BOT_ID        "7480******************"
#define CONFIG_COZE_ACCESS_TOKEN  "pat_******************"

static const char *TAG = "COZE_TEST";

//Reference the binary-included jpeg file
extern const uint8_t nihao_pcm_start[] asm("_binary_nihao_pcm_start");
extern const uint8_t nihao_pcm_end[] asm("_binary_nihao_pcm_end");

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
    // output raw opus data
}

TEST_CASE("esp coze init_deinit test", "[esp_coze]")
{
    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.bot_id = CONFIG_COZE_BOT_ID;
    chat_config.access_token = CONFIG_COZE_ACCESS_TOKEN;
    chat_config.audio_callback = NULL;
    chat_config.event_callback = NULL;
    esp_coze_chat_handle_t chat = NULL;

    esp_err_t ret = ESP_OK;
    ret = esp_coze_chat_init(&chat_config, &chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_coze_chat_init failed, err: %d", ret);
        return;
    }
    esp_coze_chat_deinit(chat);
}

TEST_CASE("esp coze test", "[esp_coze]")
{
    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.bot_id = CONFIG_COZE_BOT_ID;
    chat_config.access_token = CONFIG_COZE_ACCESS_TOKEN;
    chat_config.audio_callback = audio_data_callback;
    chat_config.event_callback = audio_event_callback;
    esp_coze_chat_handle_t chat = NULL;

    esp_err_t ret = ESP_OK;
    ret = esp_coze_chat_init(&chat_config, &chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_coze_chat_init failed, err: %d", ret);
        return;
    }
    esp_coze_chat_start(chat);
    
    esp_coze_chat_send_audio_data(chat, nihao_pcm_start, (int)(nihao_pcm_end - nihao_pcm_end));
    // test 30s then exit
    vTaskDelay(pdMS_TO_TICKS(30000));

    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);
}

TEST_CASE("esp coze test subscribe event", "[esp_coze]")
{
    // const char **sub_event = (const char *[]) {"conversation.message.create", "conversation.chat.submit_tool_outputs", NULL};
    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.bot_id = CONFIG_COZE_BOT_ID;
    chat_config.access_token = CONFIG_COZE_ACCESS_TOKEN;
    // chat_config.subscribe_event = sub_event;
    chat_config.enable_subtitle = true;
    chat_config.audio_callback = audio_data_callback;
    chat_config.event_callback = audio_event_callback;
    esp_coze_chat_handle_t chat = NULL;

    esp_err_t ret = ESP_OK;
    ret = esp_coze_chat_init(&chat_config, &chat);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_coze_chat_init failed, err: %d", ret);
        return;
    }
    esp_coze_chat_start(chat);
    
    vTaskDelay(pdMS_TO_TICKS(10000));

    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);
}

