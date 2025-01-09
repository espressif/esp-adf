/*
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "sdkconfig.h"

#include "audio_recorder.h"
#include "recorder_sr.h"
#include "audio_pipeline.h"
#include "audio_thread.h"
#include "raw_stream.h"
#include "audio_mem.h"
#include "esp_delegate.h"
#include "esp_dispatcher.h"

#include "VolcEngineRTCLite.h"
#include "audio_processor.h"
#include "config.h"

static const char* TAG = "volc_rtc";

#define STATS_TASK_PRIO        (5)
#define JOIN_EVENT_BIT         (1 << 0)
#define WAIT_DESTORY_EVENT_BIT (1 << 0)
#define WAKEUP_REC_READING     (1 << 0)

#define volc_rtc_safe_free(x, fn) do {   \
    if (x) {                             \
        fn(x);                           \
        x = NULL;                        \
    }                                    \
} while (0)

typedef struct {
    char *frame_ptr;
    int frame_len;
} frame_package_t;

struct volc_rtc_t {
    recorder_pipeline_handle_t record_pipeline;
    player_pipeline_handle_t   player_pipeline;
    void                      *recorder_engine;
    byte_rtc_engine_t          engine;
    EventGroupHandle_t         join_event;
    EventGroupHandle_t         wait_destory_event;
    EventGroupHandle_t         wakeup_event;
    QueueHandle_t              frame_q;
    esp_dispatcher_handle_t    esp_dispatcher;
    bool                       byte_rtc_running;
    bool                       data_proc_running;
#if defined(CONFIG_AUDIO_SUPPORT_OPUS_DECODER)
#define DEALULT_OPUS_DATA_CHACHE_SIZE (512)
    char                    *opus_data_cache;
    int                      opus_data_cache_len;
#endif  // CONFIF_AUDIO_SUPPORT_OPUS_DECODER
};

static struct volc_rtc_t s_volc_rtc;

static void audio_pull_data_process(char *ptr, int len);

// byte rtc lite callbacks
static void byte_rtc_on_join_room_success(byte_rtc_engine_t engine, const char* channel, int elapsed_ms, bool ms)
{
    ESP_LOGI(TAG, "join channel success %s elapsed %d ms now %d ms\n", channel, elapsed_ms, elapsed_ms);
    xEventGroupSetBits(s_volc_rtc.join_event, JOIN_EVENT_BIT);
};

static __attribute__((unused)) void byte_rtc_on_rejoin_room_success(byte_rtc_engine_t engine, const char* channel, int elapsed_ms)
{
    ESP_LOGI(TAG, "rejoin channel success %s\n", channel);
};

static void byte_rtc_on_user_joined(byte_rtc_engine_t engine, const char* channel, const char* user_name, int elapsed_ms)
{
    ESP_LOGI(TAG, "remote user joined  %s:%s", channel, user_name);
};

static void byte_rtc_on_user_offline(byte_rtc_engine_t engine, const char* channel, const char* user_name, int reason)
{
    ESP_LOGI(TAG, "remote user offline  %s:%s", channel, user_name);
};

static void byte_rtc_on_user_mute_audio(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted)
{
    ESP_LOGI(TAG, "remote user mute audio  %s:%s %d", channel, user_name, muted);
};

static void byte_rtc_on_user_mute_video(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted)
{
    ESP_LOGI(TAG, "remote user mute video  %s:%s %d", channel, user_name, muted);
};

static __attribute__((unused)) void byte_rtc_on_connection_lost(byte_rtc_engine_t engine, const char* channel)
{
    ESP_LOGI(TAG, "connection Lost  %s", channel);
};

static void byte_rtc_on_room_error(byte_rtc_engine_t engine, const char* channel, int code, const char* msg)
{
    ESP_LOGI(TAG, "error occur %s %d %s", channel, code, msg ? msg : "UnKown");
};

// remote audio
static void byte_rtc_on_audio_data(byte_rtc_engine_t engine, const char* channel, const char*  uid , uint16_t sent_ts,
                                   audio_codec_type_e codec, const void* data_ptr, size_t data_len)
{
    ESP_LOGD(TAG, "remote audio data %s %s %d %d %p %zu", channel, uid, sent_ts, codec, data_ptr, data_len);

    pipe_player_state_e state;
    player_pipeline_get_state(s_volc_rtc.player_pipeline, &state);
    if (state == PIPE_STATE_IDLE) {
        return;
    }
    frame_package_t frame = { 0 };
    frame.frame_ptr = audio_calloc(1, data_len);
    memcpy(frame.frame_ptr, data_ptr, data_len);
    frame.frame_len = data_len;
    if (xQueueSend(s_volc_rtc.frame_q, &frame, pdMS_TO_TICKS(10)) != pdPASS) {
        ESP_LOGW(TAG, "audio frame queue full");
    }
}

// remote video
static void byte_rtc_on_video_data(byte_rtc_engine_t engine, const char*  channel, const char* uid, uint16_t sent_ts,
                                    video_data_type_e codec, int is_key_frame,
                                    const void * data_ptr, size_t data_len){ }

// remote message
void on_message_received(byte_rtc_engine_t engine, const char*  room, const char* uid, const uint8_t* message, int size, bool binary)
{
    ESP_LOGD(TAG, "on_message_received uid: %s, message: %s, message size: %d", uid, message, size);
}

static void on_key_frame_gen_req(byte_rtc_engine_t engine, const char*  channel, const char*  uid) {}
// byte rtc lite callbacks end.

static void audio_pull_data_process(char *ptr, int len)
{
    char *data_ptr = ptr;
    int data_len = len;
    /* Since OPUS is in VBR mode, it needs to be packaged into a length + data format first then to decoder*/
#if defined (CONFIG_AUDIO_SUPPORT_OPUS_DECODER)

#define frame_length_prefix (2)
    if (s_volc_rtc.opus_data_cache_len + frame_length_prefix < len) {
        s_volc_rtc.opus_data_cache = audio_realloc(s_volc_rtc.opus_data_cache, len + frame_length_prefix);
        s_volc_rtc.opus_data_cache_len = len;
    }
    s_volc_rtc.opus_data_cache[0] = (len >> 8) & 0xFF;
    s_volc_rtc.opus_data_cache[1] = len & 0xFF;
    memcpy(s_volc_rtc.opus_data_cache + frame_length_prefix, ptr, len);
    data_ptr = s_volc_rtc.opus_data_cache;
    data_len += frame_length_prefix;
#else
    data_ptr = ptr;
    data_len = len;
#endif // CONFIG_AUDIO_SUPPORT_OPUS_DECODER
    raw_stream_write(player_pipeline_get_raw_write(s_volc_rtc.player_pipeline), data_ptr, data_len);
}

static esp_err_t dispatcher_audio_play(void *instance, action_arg_t *arg, action_result_t *result)
{
    audio_tone_play((char *)arg->data);
    return ESP_OK;
};

static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    if (AUDIO_REC_WAKEUP_START == event->type) {
#if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_START");
        player_pipeline_stop(s_volc_rtc.player_pipeline);
        action_arg_t action_arg = {0};
        action_arg.data = (void *)"spiffs://spiffs/dingding.wav";

        action_result_t result = {0};
        esp_dispatcher_execute_with_func(s_volc_rtc.esp_dispatcher, dispatcher_audio_play, NULL, &action_arg, &result);
        xEventGroupSetBits(s_volc_rtc.wakeup_event, WAKEUP_REC_READING);
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else if (AUDIO_REC_VAD_START == event->type) {
#if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_START");
        player_pipeline_stop(s_volc_rtc.player_pipeline);
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else if (AUDIO_REC_VAD_END == event->type) {
#if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_VAD_END");
        player_pipeline_run(s_volc_rtc.player_pipeline);
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else if (AUDIO_REC_WAKEUP_END == event->type) {
#if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
        xEventGroupClearBits(s_volc_rtc.wakeup_event, WAKEUP_REC_READING);
        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_WAKEUP_END");
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    } else {
    }

    return ESP_OK;
}

static esp_err_t open_audio_pipeline()
{
    s_volc_rtc.record_pipeline = recorder_pipeline_open();
    s_volc_rtc.player_pipeline = player_pipeline_open();
    recorder_pipeline_run(s_volc_rtc.record_pipeline);
    player_pipeline_run(s_volc_rtc.player_pipeline);
    return ESP_OK;
}

static void byte_rtc_engine_destroy() 
{
    if (s_volc_rtc.engine) {
        byte_rtc_fini(s_volc_rtc.engine);
        vTaskDelay(pdMS_TO_TICKS(1000));
        byte_rtc_destory(s_volc_rtc.engine);
        s_volc_rtc.engine = NULL;
    }
}

static esp_err_t byte_rtc_engine_create()
{
#if defined (CONFIG_AUDIO_SUPPORT_OPUS_DECODER)
    s_volc_rtc.opus_data_cache_len = DEALULT_OPUS_DATA_CHACHE_SIZE;
    s_volc_rtc.opus_data_cache = audio_calloc(1, s_volc_rtc.opus_data_cache_len);
#endif // CONFIG_AUDIO_SUPPORT_OPUS_DECODER

    byte_rtc_event_handler_t handler = {
        .on_join_room_success       =   byte_rtc_on_join_room_success,
        .on_room_error              =   byte_rtc_on_room_error,
        .on_user_joined             =   byte_rtc_on_user_joined,
        .on_user_offline            =   byte_rtc_on_user_offline,
        .on_user_mute_audio         =   byte_rtc_on_user_mute_audio,
        .on_user_mute_video         =   byte_rtc_on_user_mute_video,
        .on_audio_data              =   byte_rtc_on_audio_data,
        .on_video_data              =   byte_rtc_on_video_data,
        .on_key_frame_gen_req       =   on_key_frame_gen_req,
        .on_message_received        =   on_message_received,
    };

    s_volc_rtc.engine = byte_rtc_create(DEFAULT_APPID, &handler);

    byte_rtc_set_log_level(s_volc_rtc.engine, BYTE_RTC_LOG_LEVEL_ERROR);
#ifdef RTC_TEST_ENV
    byte_rtc_set_params(s_volc_rtc.engine, "{\"env\":2}"); // test env
#endif
    byte_rtc_set_params(s_volc_rtc.engine, "{\"debug\":{\"log_to_console\":1}}"); 
    byte_rtc_set_params(s_volc_rtc.engine,"{\"rtc\":{\"thread\":{\"pinned_to_core\":1}}}");
    byte_rtc_set_params(s_volc_rtc.engine,"{\"rtc\":{\"thread\":{\"priority\":5}}}");

    byte_rtc_init(s_volc_rtc.engine);
#if defined (CONFIG_AUDIO_SUPPORT_OPUS_DECODER)
    byte_rtc_set_audio_codec(s_volc_rtc.engine, AUDIO_CODEC_TYPE_OPUS);
#elif defined (CONFIG_AUDIO_SUPPORT_G711A_DECODER)
    byte_rtc_set_audio_codec(s_volc_rtc.engine, AUDIO_CODEC_TYPE_G711A);
#else // AACLC Encoder
    byte_rtc_set_audio_codec(s_volc_rtc.engine, AUDIO_CODEC_TYPE_AACLC);
#endif // CONFIG_AUDIO_SUPPORT_OPUS_DECODER
    open_audio_pipeline();

    ESP_LOGI(TAG, "start join room\n");
    byte_rtc_room_options_t options = { 0 };
    options.auto_subscribe_audio = 1;
    options.auto_subscribe_video = 0;
    byte_rtc_join_room(s_volc_rtc.engine, DEFAULT_ROOMID, DEFAULT_UID, DEFAULT_TOKEN, &options);

    // Default wait time is forever
    xEventGroupWaitBits(s_volc_rtc.join_event, JOIN_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "join room success\n");

    return ESP_OK;
}

static void audio_data_process_task(void *args)
{
    frame_package_t frame = { 0 };
    s_volc_rtc.data_proc_running = true;
    while (s_volc_rtc.data_proc_running) {
        xQueueReceive(s_volc_rtc.frame_q, &frame, portMAX_DELAY);
        if (frame.frame_ptr) {
            audio_pull_data_process(frame.frame_ptr, frame.frame_len);
            audio_free(frame.frame_ptr);
        }
    }
    vTaskDelete(NULL);
}

static void voice_read_task(void *args)
{
    const int voice_data_read_sz = recorder_pipeline_get_default_read_size(s_volc_rtc.record_pipeline);
    uint8_t *voice_data = audio_calloc(1, voice_data_read_sz);
    bool runing = true;

#if defined (CONFIG_AUDIO_SUPPORT_OPUS_DECODER)
    audio_frame_info_t audio_frame_info = {.data_type = AUDIO_DATA_TYPE_OPUS};
#elif defined (CONFIG_AUDIO_SUPPORT_G711A_DECODER)
    audio_frame_info_t audio_frame_info = {.data_type = AUDIO_DATA_TYPE_PCMA};
#else
    audio_frame_info_t audio_frame_info = {.data_type = AUDIO_DATA_TYPE_AACLC};
#endif // CONFIG_AUDIO_SUPPORT_OPUS_DECODER

#if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
    TickType_t wait_tm = portMAX_DELAY;
#else
    TickType_t wait_tm = 0;
#endif // CONFIG_LANGUAGE_WAKEUP_MODE
    while (runing) {
        EventBits_t bits = xEventGroupWaitBits(s_volc_rtc.wakeup_event, WAKEUP_REC_READING , false, true, wait_tm);
    #if defined (CONFIG_LANGUAGE_WAKEUP_MODE)
        if (bits & WAKEUP_REC_READING) {
            int ret = audio_recorder_data_read(s_volc_rtc.recorder_engine, voice_data, voice_data_read_sz, portMAX_DELAY);
            if (ret == 0 || ret == -1) {
                xEventGroupClearBits(s_volc_rtc.wakeup_event,  WAKEUP_REC_READING);
            } else {
                byte_rtc_send_audio_data(s_volc_rtc.engine, DEFAULT_ROOMID, voice_data, voice_data_read_sz, &audio_frame_info);
            }
        }
    #else
        audio_recorder_data_read(s_volc_rtc.recorder_engine, voice_data, voice_data_read_sz, portMAX_DELAY);
        byte_rtc_send_audio_data(s_volc_rtc.engine, DEFAULT_ROOMID, voice_data, voice_data_read_sz, &audio_frame_info);
    #endif
    }
    xEventGroupClearBits(s_volc_rtc.wakeup_event, WAKEUP_REC_READING);
    audio_free(voice_data);
    vTaskDelete(NULL);
}

static void log_clear(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_THREAD", ESP_LOG_ERROR);
    esp_log_level_set("i2c_bus_v2", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_HAL", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    esp_log_level_set("I2S_STREAM_IDF5.x", ESP_LOG_ERROR);
    esp_log_level_set("RSP_FILTER", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_EVT", ESP_LOG_ERROR);
}

esp_err_t volc_rtc_init(void)
{
    log_clear();
    audio_tone_init();

    s_volc_rtc.join_event = xEventGroupCreate();
    s_volc_rtc.wait_destory_event = xEventGroupCreate();
    s_volc_rtc.wakeup_event = xEventGroupCreate();
    s_volc_rtc.frame_q = xQueueCreate(30, sizeof(frame_package_t));
    s_volc_rtc.esp_dispatcher = esp_dispatcher_get_delegate_handle();

    byte_rtc_engine_create();
    s_volc_rtc.recorder_engine = audio_record_engine_init(s_volc_rtc.record_pipeline, rec_engine_cb);
    audio_thread_create(NULL, "voice_read_task", voice_read_task, (void *)NULL, 5 * 1024, 5, true, 1);
    audio_thread_create(NULL, "audio_data_process_task", audio_data_process_task, (void *)NULL, 5 * 1024, 10, true, 1);

    return ESP_OK;
}

esp_err_t volc_rtc_deinit(void)
{
    s_volc_rtc.byte_rtc_running = false;
    s_volc_rtc.data_proc_running = false;
    xEventGroupWaitBits(s_volc_rtc.wait_destory_event, WAIT_DESTORY_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    byte_rtc_engine_destroy();
    volc_rtc_safe_free(s_volc_rtc.join_event, vEventGroupDelete);
    volc_rtc_safe_free(s_volc_rtc.wait_destory_event, vEventGroupDelete);
    return ESP_OK;
}
