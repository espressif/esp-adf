/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_console.h"

#include "esp_check.h"
#include "esp_gmf_afe.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"
#include "esp_codec_dev.h"

#include "audio_processor.h"
#include "basic_board.h"
#include "baidu_chat_agents_engine.h"


#define STATS_TASK_PRIO        (5)
#define JOIN_EVENT_BIT         (1 << 0)
#define WAIT_DESTORY_EVENT_BIT (1 << 0)
#define WAKEUP_REC_READING     (1 << 0)

#define AUDIO_BUFFER_SIZE 640
#define AUDIO_QUEUE_SIZE  2
#define THREAD_STACK_SIZE 4096
#define THREAD_PRIORITY   12

static const char *TAG = "BRTC_APP";
typedef struct {
    char  *appid;
    char  *userId;
    char  *workflow;
    char  *license_key;
    int    instance_id;
} baidu_rtc_config_t;

struct baidu_rtc_t {
    baidu_rtc_config_t     config;
    esp_gmf_oal_thread_t   read_thread;
    esp_gmf_oal_thread_t   play_ctrl_thread;
    BaiduChatAgentEngine  *engine;
    EventGroupHandle_t     join_event;
    EventGroupHandle_t     wait_destory_event;
    EventGroupHandle_t     wakeup_event;
    QueueHandle_t          audio_player_queue;
    bool                   byte_rtc_running;
    bool                   data_proc_running;
    bool                   is_connected;
};

typedef struct {
    char  *url;
    bool   start;
} audio_player_ctrl_event_t;

static struct baidu_rtc_t s_bdrtc;

static void onErrorCallback(int errCode, const char *errMsg)
{
    ESP_LOGE(TAG, "Error occurred. Code: %d, Message: %s", errCode, errMsg);
}

static void onCallStateChangeCallback(AGentCallState state)
{
    ESP_LOGI(TAG, "Call state changed: %d", state);
    if (state == AGENT_LOGIN_SUCCESS) {
        ESP_LOGI(TAG, "Login success");
        s_bdrtc.is_connected = true;
        xEventGroupSetBits(s_bdrtc.join_event, JOIN_EVENT_BIT);
    } else if (state == AGENT_LOGIN_FAIL) {
        ESP_LOGE(TAG, "Login failed");
        s_bdrtc.is_connected = false;
    }
}

static void onConnectionStateChangeCallback(AGentConnectState state)
{
    ESP_LOGI(TAG, "Connection state changed: %d", state);
    switch (state) {
        case AGENT_CONNECTION_STATE_CONNECTED:
            ESP_LOGI(TAG, "Connection state changed: connected");
            s_bdrtc.is_connected = true;
            break;
        case AGENT_CONNECTION_STATE_DISCONNECTED:
            ESP_LOGE(TAG, "Connection state changed: disconnected");
            s_bdrtc.is_connected = false;
            break;
        default:
            break;
    }
}

static void onUserAsrSubtitleCallback(const char *text, bool isFinal)
{
    ESP_LOGI(TAG, "User ASR subtitle: %s (isFinal: %d)", text, isFinal);
}

static void onFunctionCall(const char *id, const char *params)
{
    ESP_LOGI(TAG, "onFunctionCall, id: %s, params: %s", id, params);
}

static void onMediaSetup(void)
{
    ESP_LOGI(TAG, "Media setup completed.");
    baidu_chat_agent_engine_send_text_to_TTS(s_bdrtc.engine, "你一出现，气氛就变得好软萌呀，快来和我说说话");
}

static void onAIAgentSubtitle(const char *text, bool isFinal)
{
    ESP_LOGI(TAG, "onAIAgentSubtitle %s", text);
}

static void onAIAgentSpeaking(bool speeking)
{
    ESP_LOGI(TAG, "onAIAgentSpeaking is %s", speeking ? "Start" : "Stop");
    if (speeking) {
        audio_mixer_switch_priority(AUDIO_MIXER_FEEDER_BOOST);
    } else {
        audio_mixer_switch_priority(AUDIO_MIXER_PLAYBACK_BOOST);
    }
}

static void onAudioPlayerOp(const char *path, bool start)
{
    ESP_LOGI(TAG, "onAudioPlayerOp. path:%s, start:%d", path, start);
    audio_player_ctrl_event_t event;
    event.url = esp_gmf_oal_strdup(path);
    event.start = start;

    if (xQueueSend(s_bdrtc.audio_player_queue, &event, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send audio player event");
        esp_gmf_oal_free(event.url);
    }
}

static void onAudioData(const uint8_t *data, size_t len)
{
    ESP_LOGD(TAG, "onAudioData, len: %d", len);
    audio_feeder_feed_data((uint8_t *)data, len);
}

static void onVideoData(const uint8_t *data, size_t len, int width, int height)
{
    ESP_LOGD(TAG, "Received video data of length: %zu, width: %d, height: %d", len, width, height);
}

static void onLicenseResult(bool result)
{
    ESP_LOGI(TAG, "onLicenseResult: %d", result);
    if (!result) {
        ESP_LOGE(TAG, "onLicenseResult: failed");
    }
}

static void byte_rtc_engine_destroy()
{
    if (s_bdrtc.engine) {
        baidu_chat_agent_engine_destroy(s_bdrtc.engine);
        s_bdrtc.engine = NULL;
    }
}

const char *config = "{\"app_id\": \"%s\", \"config\" : \"{\\\"llm\\\" : \\\"%s\\\", \\\"llm_token\\\" : \\\"no\\\", \\\"rtc_ac\\\": \\\"%s\\\", \\\"lang\\\" : \\\"%s\\\",  \\\"enable_visual\\\" : \\\"%s\\\", \\\"dfda\\\" : \\\"%s\\\"}\", \"quick_start\": true}";

static void setUserParameters(AgentEngineParams *params)
{
    memset(params, 0, sizeof(AgentEngineParams));

    snprintf(params->agent_platform_url, sizeof(params->agent_platform_url), "%s", CONFIG_SERVER_URL);
    snprintf(params->appid, sizeof(params->appid), "%s", s_bdrtc.config.appid);
    snprintf(params->userId, sizeof(params->userId), "%s", s_bdrtc.config.userId);
    snprintf(params->cer, sizeof(params->cer), "%s", "./a.cer");
    snprintf(params->workflow, sizeof(params->workflow), "%s", s_bdrtc.config.workflow);
    snprintf(params->license_key, sizeof(params->license_key), "%s", s_bdrtc.config.license_key);
    snprintf(params->config, sizeof(params->config), "%s", config);
    params->instance_id = s_bdrtc.config.instance_id;
    params->verbose = false;
    params->enable_local_agent = true;
    params->enable_voice_interrupt = true;
    params->level_voice_interrupt = 80;
    params->AudioInChannel = 1;
    params->AudioInFrequency = 16000;
}

static esp_err_t brtc_create()
{
    s_bdrtc.config.appid = CONFIG_BAIDU_APP_ID;
    s_bdrtc.config.userId = CONFIG_BAIDU_USER_ID;
    s_bdrtc.config.workflow = CONFIG_BAIDU_WORKFLOW;
    s_bdrtc.config.license_key = CONFIG_BAIDU_LICENSE_KEY;
    s_bdrtc.config.instance_id = CONFIG_BAIDU_INSTANCE_ID;

    BaiduChatAgentEvent events = {
        .onError = onErrorCallback,
        .onCallStateChange = onCallStateChangeCallback,
        .onConnectionStateChange = onConnectionStateChangeCallback,
        .onUserAsrSubtitle = onUserAsrSubtitleCallback,
        .onAIAgentSubtitle = onAIAgentSubtitle,
        .onAIAgentSpeaking = onAIAgentSpeaking,
        .onFunctionCall = onFunctionCall,
        .onAudioPlayerOp = onAudioPlayerOp,
        .onMediaSetup = onMediaSetup,
        .onAudioData = onAudioData,
        .onVideoData = onVideoData,
        .onLicenseResult = onLicenseResult,
    };

    s_bdrtc.engine = baidu_create_chat_agent_engine(&events);
    if (s_bdrtc.engine == NULL) {
        ESP_LOGE(TAG, "Engine initialization failed.");
        return ESP_FAIL;
    }

    AgentEngineParams agentParams;
    setUserParameters(&agentParams);

    ESP_LOGI(TAG, "Logging in...");
    int ret = baidu_chat_agent_engine_init(s_bdrtc.engine, &agentParams);
    if (ret != 200) {
        ESP_LOGE(TAG, "Failed to log in. Error code: %d", ret);
        baidu_chat_agent_engine_destroy(s_bdrtc.engine);
        return ESP_FAIL;
    }
    baidu_chat_agent_engine_call(s_bdrtc.engine);
    xEventGroupWaitBits(s_bdrtc.join_event, JOIN_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "join room success");

    return ESP_OK;
}

static void recorder_event_callback_fn(void *event, void *ctx)
{
    esp_gmf_afe_evt_t *afe_evt = (esp_gmf_afe_evt_t *)event;
    switch (afe_evt->type) {
        case ESP_GMF_AFE_EVT_WAKEUP_START:
            ESP_LOGI(TAG, "wakeup start");
            break;
        case ESP_GMF_AFE_EVT_WAKEUP_END:
            ESP_LOGI(TAG, "wakeup end");
            break;
        case ESP_GMF_AFE_EVT_VAD_START:
            ESP_LOGD(TAG, "vad start");
            break;
        case ESP_GMF_AFE_EVT_VAD_END:
            ESP_LOGD(TAG, "vad end");
            break;
        case ESP_GMF_AFE_EVT_VCMD_DECT_TIMEOUT:
            ESP_LOGI(TAG, "vcmd detect timeout");
            break;
        default:
            break;
    }
}

static void audio_pipe_open()
{
    basic_board_periph_t periph = {0};
    audio_manager_config_t config = {0};

    basic_board_init(&periph);

    config.play_dev = periph.play_dev;
    config.rec_dev = periph.rec_dev;
    strcpy(config.mic_layout, periph.mic_layout);
    config.board_sample_rate = periph.sample_rate;
    config.board_sample_bits = periph.sample_bits;
    config.board_channels = periph.channels;
    audio_manager_init(&config);

    esp_codec_dev_set_out_vol(config.play_dev, 70);
    esp_codec_dev_set_in_gain(config.rec_dev, 32.0);
    esp_codec_dev_set_in_channel_gain(config.rec_dev, BIT(2), 20.0);

    audio_mixer_open();
    audio_playback_open();
    audio_recorder_open(recorder_event_callback_fn, NULL);
    audio_feeder_open();
    audio_feeder_run();
}

static void audio_data_read_task(void *pv)
{
    uint8_t *data = esp_gmf_oal_calloc(1, AUDIO_BUFFER_SIZE);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    int ret = 0;
    while (s_bdrtc.byte_rtc_running) {
        ret = audio_recorder_read_data(data, AUDIO_BUFFER_SIZE);
        ESP_LOGD(TAG, "audio_data_read_task, ret: %d, is_connected: %d", ret, s_bdrtc.is_connected);
        if (ret > 0 && s_bdrtc.is_connected) {
            baidu_chat_agent_engine_send_audio(s_bdrtc.engine, data, ret);
        } else {
            ESP_LOGE(TAG, "audio_data_read_task, ret: %d, connecte status: %s", ret, s_bdrtc.is_connected ? "connected" : "disconnected");
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    esp_gmf_oal_free(data);
    vTaskDelete(NULL);
}

static void audio_player_ctrl_task(void *pv)
{
    audio_player_ctrl_event_t event;
    while (s_bdrtc.data_proc_running) {
        if (xQueueReceive(s_bdrtc.audio_player_queue, &event, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "audio_player_ctrl_task, url: %s, start: %d", event.url, event.start);
            if (event.start) {
                audio_playback_play(event.url);
                esp_gmf_oal_free(event.url);
            } else {
                audio_playback_stop();
            }
        }
    }
    vTaskDelete(NULL);
}

esp_err_t brtc_init(void)
{
    esp_err_t ret = ESP_OK;
    s_bdrtc.join_event = xEventGroupCreate();
    s_bdrtc.wait_destory_event = xEventGroupCreate();
    s_bdrtc.wakeup_event = xEventGroupCreate();
    s_bdrtc.is_connected = false;
    s_bdrtc.audio_player_queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(audio_player_ctrl_event_t));

    if (!s_bdrtc.join_event || !s_bdrtc.wait_destory_event || !s_bdrtc.wakeup_event || !s_bdrtc.audio_player_queue) {
        ESP_LOGE(TAG, "Failed to create events or queue");
        ret = ESP_FAIL;
        goto cleanup;
    }
    ret = brtc_create();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create brtc");
        ret = ESP_FAIL;
        goto cleanup;
    }
    audio_pipe_open();
    s_bdrtc.byte_rtc_running = true;
    s_bdrtc.data_proc_running = true;
    esp_gmf_oal_thread_create(&s_bdrtc.read_thread, "audio_data_read_task",
                              audio_data_read_task, NULL, THREAD_STACK_SIZE,
                              THREAD_PRIORITY, true, 0);
    esp_gmf_oal_thread_create(&s_bdrtc.play_ctrl_thread, "audio_player_ctrl_task",
                              audio_player_ctrl_task, NULL, THREAD_STACK_SIZE,
                              THREAD_PRIORITY, true, 0);

    return ESP_OK;

cleanup:
    if (s_bdrtc.join_event) {
        vEventGroupDelete(s_bdrtc.join_event);
        s_bdrtc.join_event = NULL;
    }
    if (s_bdrtc.wait_destory_event) {
        vEventGroupDelete(s_bdrtc.wait_destory_event);
        s_bdrtc.wait_destory_event = NULL;
    }
    if (s_bdrtc.wakeup_event) {
        vEventGroupDelete(s_bdrtc.wakeup_event);
        s_bdrtc.wakeup_event = NULL;
    }
    if (s_bdrtc.audio_player_queue) {
        vQueueDelete(s_bdrtc.audio_player_queue);
        s_bdrtc.audio_player_queue = NULL;
    }
    return ret;
}

esp_err_t brtc_deinit(void)
{
    s_bdrtc.byte_rtc_running = false;
    s_bdrtc.data_proc_running = false;

    if (s_bdrtc.audio_player_queue) {
        vQueueDelete(s_bdrtc.audio_player_queue);
        s_bdrtc.audio_player_queue = NULL;
    }
    if (s_bdrtc.join_event) {
        vEventGroupDelete(s_bdrtc.join_event);
        s_bdrtc.join_event = NULL;
    }
    if (s_bdrtc.wait_destory_event) {
        vEventGroupDelete(s_bdrtc.wait_destory_event);
        s_bdrtc.wait_destory_event = NULL;
    }
    if (s_bdrtc.wakeup_event) {
        vEventGroupDelete(s_bdrtc.wakeup_event);
        s_bdrtc.wakeup_event = NULL;
    }
    byte_rtc_engine_destroy();

    return ESP_OK;
}
