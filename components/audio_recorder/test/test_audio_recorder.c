/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_spiffs.h"

#include "amrnb_encoder.h"
#include "amrwb_encoder.h"
#include "filter_resample.h"

#include "audio_mem.h"
#include "audio_recorder.h"
#include "recorder_encoder.h"
#include "recorder_sr.h"

#include "model_path.h"

#include "unity.h"

#define SR_WAKEUP         (BIT0)
#define SR_VAD_START      (BIT1)
#define SR_VAD_END        (BIT2)
#define SR_SLEEP          (BIT3)
#define SR_CMD            (BIT4)
#define RECORDER_GOT_DAT  (BIT5)
#define RECORDER_READ_FIN (BIT6)
#define RECORDER_TASK_FIN (BIT7)

enum _rec_msg_id {
    REC_START = 1,
    REC_STOP,
    REC_CANCEL,
    TASK_DESTROY,
};

static const char *TAG = "TEST_AUDIO_RECORDER";

/* [ch0:REF],[ch1: LT],[ch2: NULL][ch3: RT] */
extern const uint8_t _4ch16bit16k_pcm_start[] asm("_binary_4ch16bit16k_pcm_start");
extern const uint8_t _4ch16bit16k_pcm_end[] asm("_binary_4ch16bit16k_pcm_end");

/* [ch0:REF],[ch1: voice] */
extern const uint8_t _2ch16bit16k_pcm_start[] asm("_binary_2ch16bit16k_pcm_start");
extern const uint8_t _2ch16bit16k_pcm_end[] asm("_binary_2ch16bit16k_pcm_end");

/* [ch0: voice] */
extern const uint8_t _1ch16bit16k_pcm_start[] asm("_binary_1ch16bit16k_pcm_start");
extern const uint8_t _1ch16bit16k_pcm_end[] asm("_binary_1ch16bit16k_pcm_end");

static EventGroupHandle_t events;
static QueueHandle_t rec_q = NULL;
static int read_cnt = 0;

static void voice_read_task(void *args)
{
    const int buf_len = 2 * 1024;
    uint8_t *voiceData = audio_calloc(1, buf_len);
    int msg = 0;
    unsigned long delay = portMAX_DELAY;
    audio_rec_handle_t recorder = (audio_rec_handle_t)args;
    bool runing = true;
    bool voice_reading = false;

    while (runing) {
        if (xQueueReceive(rec_q, &msg, delay) == pdTRUE) {
            switch (msg) {
                case REC_START: {
                    ESP_LOGW(TAG, "voice read begin");
                    delay = 0;
                    voice_reading = true;
                    break;
                }
                case REC_STOP: {
                    ESP_LOGW(TAG, "voice read over");
                    delay = portMAX_DELAY;
                    voice_reading = false;
                    break;
                }
                case REC_CANCEL: {
                    ESP_LOGW(TAG, "voice read cancel");
                    delay = portMAX_DELAY;
                    voice_reading = false;
                    break;
                }
                case TASK_DESTROY:
                    ESP_LOGI(TAG, "task destroy");
                    delay = portMAX_DELAY;
                    voice_reading = false;
                    runing = false;
                    break;
                default:
                    break;
            }
        }

        if (voice_reading) {
            int ret = audio_recorder_data_read(recorder, voiceData, buf_len, portMAX_DELAY);
            if (ret == 0 || ret == -1) {
                delay = portMAX_DELAY;
                voice_reading = false;
                ESP_LOGE(TAG, "Read Finished");
            } else {
                xEventGroupSetBits(events, RECORDER_GOT_DAT);
            }
        }
        if (voice_reading == false) {
            xEventGroupSetBits(events, RECORDER_READ_FIN);
        }
    }

    xEventGroupSetBits(events, RECORDER_TASK_FIN);
    free(voiceData);
    vTaskDelete(NULL);
}

static int input_4ch_for_afe(int16_t *buffer, int buf_sz, void *user_ctx)
{
    const int sz = _4ch16bit16k_pcm_end - _4ch16bit16k_pcm_start;
    int remain_sz = sz - read_cnt;

    if (buf_sz < remain_sz) {
        memcpy(buffer, (char *)&_4ch16bit16k_pcm_start[read_cnt], buf_sz);
        read_cnt += buf_sz;
    } else if (remain_sz > 0) {
        memcpy(buffer, (char *)&_4ch16bit16k_pcm_start[read_cnt], remain_sz);
        memset(&buffer[remain_sz], 0, buf_sz - remain_sz);
        read_cnt += remain_sz;
    } else {
        memset(buffer, 0, buf_sz);
    }

    return buf_sz;
}

static int input_2ch_for_afe(int16_t *buffer, int buf_sz, void *user_ctx)
{
    const int sz = _2ch16bit16k_pcm_end - _2ch16bit16k_pcm_start;
    int remain_sz = sz - read_cnt;

    if (buf_sz < remain_sz) {
        memcpy(buffer, (char *)&_2ch16bit16k_pcm_start[read_cnt], buf_sz);
        read_cnt += buf_sz;
    } else if (remain_sz > 0) {
        memcpy(buffer, (char *)&_2ch16bit16k_pcm_start[read_cnt], remain_sz);
        memset(&buffer[remain_sz], 0, buf_sz - remain_sz);
        read_cnt += remain_sz;
    } else {
        memset(buffer, 0, buf_sz);
    }

    for (int i = 0; i < (buf_sz / 4); i++) {
        int16_t tmp = buffer[2 * i];
        buffer[2 * i] = buffer[2 * i + 1];
        buffer[2 * i + 1] = tmp;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return buf_sz;
}

static int input_1ch_for_encoder(int16_t *buffer, int buf_sz, void *user_ctx)
{
    const int sz = _1ch16bit16k_pcm_end - _1ch16bit16k_pcm_start;
    int remain_sz = sz - read_cnt;

    if (buf_sz < remain_sz) {
        memcpy(buffer, (char *)&_1ch16bit16k_pcm_start[read_cnt], buf_sz);
        read_cnt += buf_sz;
    } else if (remain_sz > 0) {
        memcpy(buffer, (char *)&_1ch16bit16k_pcm_start[read_cnt], remain_sz);
        memset(&buffer[remain_sz], 0, buf_sz - remain_sz);
        read_cnt += remain_sz;
    } else {
        memset(buffer, 0, buf_sz);
    }
    // ESP_LOGI(TAG, "read_cnt %d, remain %d, buf_sz %d", read_cnt, remain_sz, buf_sz);
    vTaskDelay(pdMS_TO_TICKS(10));
    return buf_sz;
}

static esp_err_t recorder_event_cb(audio_rec_evt_t type, void *user_data)
{
    switch (type) {
        case AUDIO_REC_WAKEUP_START: {
            ESP_LOGI(TAG, "recorder_event_cb - REC_EVENT_WAKEUP_START");
            xEventGroupSetBits(events, SR_WAKEUP);
            break;
        }
        case AUDIO_REC_VAD_START: {
            ESP_LOGI(TAG, "recorder_event_cb - REC_EVENT_VAD_START");
            int msg = REC_START;
            if (xQueueSend(rec_q, &msg, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(TAG, "rec start send failed");
            }
            xEventGroupSetBits(events, SR_VAD_START);
            break;
        }
        case AUDIO_REC_VAD_END: {
            ESP_LOGI(TAG, "recorder_event_cb - REC_EVENT_VAD_STOP");
            int msg = REC_STOP;
            if (xQueueSend(rec_q, &msg, portMAX_DELAY) != pdPASS) {
                ESP_LOGE(TAG, "rec stop send failed");
            }
            xEventGroupSetBits(events, SR_VAD_END);
            break;
        }
        case AUDIO_REC_WAKEUP_END: {
            ESP_LOGI(TAG, "recorder_event_cb - REC_EVENT_WAKEUP_END");
            xEventGroupSetBits(events, SR_SLEEP);
            break;
        }
        default: {
            if (type >= AUDIO_REC_COMMAND_DECT) {
                ESP_LOGW(TAG, "recorder_event_cb - Command %d", type);
                xEventGroupSetBits(events, SR_CMD);
            }
            break;
        }
    }

    return ESP_OK;
}

static void test_init(void *recorder)
{
    read_cnt = 0;
    TEST_ASSERT_NOT_NULL(events = xEventGroupCreate());
    TEST_ASSERT_NOT_NULL(rec_q = xQueueCreate(3, sizeof(int)));
    TEST_ASSERT(pdTRUE == xTaskCreate(voice_read_task, "rec_task", 4 * 1024, recorder, 6, NULL));
}

static void test_deinit()
{
    int msg = TASK_DESTROY;
    if (xQueueSend(rec_q, &msg, pdMS_TO_TICKS(2000)) != pdPASS) {
        ESP_LOGE(TAG, "rec destroy send failed");
    }
    EventBits_t bits = xEventGroupWaitBits(events, RECORDER_TASK_FIN, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & RECORDER_TASK_FIN) == RECORDER_TASK_FIN);
    vQueueDelete(rec_q);
    vEventGroupDelete(events);
    read_cnt = 0;
}

TEST_CASE("All function bypass, create and destroy", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
    cfg.event_cb = recorder_event_cb;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT(audio_recorder_destroy(recorder) == ESP_OK);
}

TEST_CASE("All function bypass, read only", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
    cfg.event_cb = recorder_event_cb;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);
    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(audio_recorder_destroy(recorder) == ESP_OK);

    test_deinit();
}

TEST_CASE("Use [resample]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_1ch_for_encoder;
    cfg.event_cb = recorder_event_cb;

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };

    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.stack_in_ext = true;

    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.resample);
    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
    TEST_ASSERT_NOT_NULL(cfg.encoder_handle);
    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_encoder_destroy(cfg.encoder_handle));

    test_deinit();
}

TEST_CASE("Use [amrwb]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_1ch_for_encoder;
    cfg.event_cb = recorder_event_cb;

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };

    amrwb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amr_enc_cfg.contain_amrwb_header = true;
    amr_enc_cfg.task_core = 1;
    recorder_encoder_cfg.encoder = amrwb_encoder_init(&amr_enc_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.encoder);

    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
    TEST_ASSERT_NOT_NULL(cfg.encoder_handle);
    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_encoder_destroy(cfg.encoder_handle));

    test_deinit();
}

TEST_CASE("Use [resample, amrnb]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_1ch_for_encoder;
    cfg.event_cb = recorder_event_cb;

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };

    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.stack_in_ext = true;

    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.resample);

    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_enc_cfg.contain_amrnb_header = true;
    amr_enc_cfg.stack_in_ext = true;
    recorder_encoder_cfg.encoder = amrnb_encoder_init(&amr_enc_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.encoder);

    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
    TEST_ASSERT_NOT_NULL(cfg.encoder_handle);

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(2000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_encoder_destroy(cfg.encoder_handle));

    test_deinit();
}

TEST_CASE("Use [sr - wwe don't init, vad don't init, mn don't init]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.wakenet_init = false;
    recorder_sr_cfg.afe_cfg.vad_init = false;
    recorder_sr_cfg.multinet_init = false;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);
    TEST_ASSERT(ESP_OK == audio_recorder_vad_check_enable(recorder, false));

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - wwe disabled, vad disabled, mn disabled]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);
    TEST_ASSERT(ESP_OK == audio_recorder_wakenet_enable(recorder, false));
    TEST_ASSERT(ESP_OK == audio_recorder_multinet_enable(recorder, false));
    TEST_ASSERT(ESP_OK == audio_recorder_vad_check_enable(recorder, false));

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - wwe enable, vad disabled, mn disabled]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);
    TEST_ASSERT(ESP_OK == audio_recorder_multinet_enable(recorder, false));
    TEST_ASSERT(ESP_OK == audio_recorder_vad_check_enable(recorder, false));

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - wwe disable, vad enabled, mn disabled]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;
    recorder_sr_cfg.afe_cfg.wakenet_init = false;
    recorder_sr_cfg.multinet_init = false;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    TEST_ASSERT(audio_recorder_trigger_start(recorder) == ESP_OK);
    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    vTaskDelay(pdMS_TO_TICKS(5000));

    TEST_ASSERT(audio_recorder_trigger_stop(recorder) == ESP_OK);
    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - wwe enable, vad enabled, mn disabled]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);
    TEST_ASSERT(ESP_OK == audio_recorder_multinet_enable(recorder, false));

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - all enable]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
#ifdef CONFIG_USE_MULTINET
    expect |= SR_CMD;
#endif
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}

TEST_CASE("Use [sr - all enable, resample, encoder]", "[audio][timeout=100][test_env=UT_T1_AUDIO]")
{
    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
#if CONFIG_AFE_MIC_NUM == (1)
    cfg.read = (recorder_data_read_t)&input_2ch_for_afe;
#else
    cfg.read = (recorder_data_read_t)&input_4ch_for_afe;
#endif
    cfg.event_cb = recorder_event_cb;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.vad_off = 1000;

    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };

    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.stack_in_ext = true;
    filter_cfg.max_indata_bytes = 1024; // IMPORTANT

    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.resample);

    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_enc_cfg.contain_amrnb_header = true;
    amr_enc_cfg.stack_in_ext = true;
    recorder_encoder_cfg.encoder = amrnb_encoder_init(&amr_enc_cfg);
    TEST_ASSERT_NOT_NULL(recorder_encoder_cfg.encoder);

    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
    TEST_ASSERT_NOT_NULL(cfg.encoder_handle);

    audio_rec_handle_t recorder = audio_recorder_create(&cfg);
    TEST_ASSERT_NOT_NULL(recorder);

    test_init(recorder);

    EventBits_t bits = 0;
    EventBits_t expect = 0;

    expect = SR_WAKEUP | SR_VAD_START | RECORDER_GOT_DAT;
#ifdef CONFIG_USE_MULTINET
    expect |= SR_CMD;
#endif
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    expect = SR_VAD_END | SR_SLEEP | RECORDER_READ_FIN;
    bits = xEventGroupWaitBits(events, expect, true, true, pdMS_TO_TICKS(10000));
    TEST_ASSERT((bits & expect) == expect);

    TEST_ASSERT(ESP_OK == audio_recorder_destroy(recorder));
    TEST_ASSERT(ESP_OK == recorder_encoder_destroy(cfg.encoder_handle));
    TEST_ASSERT(ESP_OK == recorder_sr_destroy(cfg.sr_handle));

    test_deinit();
}
