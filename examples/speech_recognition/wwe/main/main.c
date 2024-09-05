/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "amrnb_encoder.h"
#include "amrwb_encoder.h"
#include "audio_element.h"
#include "audio_idf_version.h"
#include "audio_mem.h"
#include "audio_pipeline.h"
#include "audio_recorder.h"
#include "audio_thread.h"
#include "audio_tone_uri.h"
#include "board.h"
#include "esp_audio.h"
#include "filter_resample.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "periph_adc_button.h"
#include "raw_stream.h"
#include "recorder_encoder.h"
#include "recorder_sr.h"
#include "tone_stream.h"
#include "es7210.h"

#include "model_path.h"

#define NO_ENCODER  (0)
#define ENC_2_AMRNB (1)
#define ENC_2_AMRWB (2)

#define RECORDER_ENC_ENABLE (NO_ENCODER)
#define VOICE2FILE          (false)
#define WAKENET_ENABLE      (true)
#define MULTINET_ENABLE     (true)
#define SPEECH_CMDS_RESET   (false)

#define SPEECH_COMMANDS     ("da kai dian deng,kai dian deng;guan bi dian deng,guan dian deng;guan deng;")

#ifndef CODEC_ADC_SAMPLE_RATE
#warning "Please define CODEC_ADC_SAMPLE_RATE first, default value is 48kHz may not correctly"
#define CODEC_ADC_SAMPLE_RATE    48000
#endif

#ifndef CODEC_ADC_BITS_PER_SAMPLE
#warning "Please define CODEC_ADC_BITS_PER_SAMPLE first, default value 16 bits may not correctly"
#define CODEC_ADC_BITS_PER_SAMPLE  16
#endif

#ifndef RECORD_HARDWARE_AEC
#warning "The hardware AEC is disabled!"
#define RECORD_HARDWARE_AEC  (false)
#endif

#ifndef CODEC_ADC_I2S_PORT
#define CODEC_ADC_I2S_PORT  (0)
#endif

enum _rec_msg_id {
    REC_START = 1,
    REC_STOP,
    REC_CANCEL,
};

static char *TAG = "wwe_example";

static esp_audio_handle_t     player        = NULL;
static audio_rec_handle_t     recorder      = NULL;
static audio_element_handle_t raw_read      = NULL;
static QueueHandle_t          rec_q         = NULL;
static bool                   voice_reading = false;

static esp_audio_handle_t setup_player()
{
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();

    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 48000;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;

    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    // Create readers and add to esp_audio
    tone_stream_cfg_t tone_cfg = TONE_STREAM_CFG_DEFAULT();
    tone_cfg.type = AUDIO_STREAM_READER;
    esp_audio_input_stream_add(player, tone_stream_init(&tone_cfg));

    // Add decoders and encoders to esp_audio
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    // Create writers and add to esp_audio
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD == 1) && (CONFIG_AFE_MIC_NUM == 1)
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, 48000, I2S_DATA_BIT_WIDTH_16BIT, AUDIO_STREAM_WRITER);
#else
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, 48000, CODEC_ADC_BITS_PER_SAMPLE, AUDIO_STREAM_WRITER);
    i2s_writer.need_expand = (CODEC_ADC_BITS_PER_SAMPLE != 16);
#endif

    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_writer);
    esp_audio_output_stream_add(player, i2s_stream_writer);

    // Set default volume
    esp_audio_vol_set(player, 60);
    AUDIO_MEM_SHOW(TAG);

    ESP_LOGI(TAG, "esp_audio instance is:%p\r\n", player);
    return player;
}

#if VOICE2FILE == (true)
static void voice_2_file(uint8_t *buffer, int len)
{
#define MAX_FNAME_LEN (50)

    static FILE *fp = NULL;
    static int fcnt = 0;

    if (voice_reading) {
        if (!fp) {
            if (fp == NULL) {
                char fname[MAX_FNAME_LEN] = { 0 };

                if (RECORDER_ENC_ENABLE) {
                    snprintf(fname, MAX_FNAME_LEN - 1, "/sdcard/f%d.amr", fcnt++);
                } else {
                    snprintf(fname, MAX_FNAME_LEN - 1, "/sdcard/f%d.pcm", fcnt++);
                }
                fp = fopen(fname, "wb");
                if (!fp) {
                    ESP_LOGE(TAG, "File open failed");
                }
            }
        }
        if (len) {
            fwrite(buffer, len, 1, fp);
        }
    } else {
        if (fp) {
            ESP_LOGI(TAG, "File closed");
            fclose(fp);
            fp = NULL;
        }
    }
}
#endif /* VOICE2FILE == (true) */

static void voice_read_task(void *args)
{
    const int buf_len = 2 * 1024;
    uint8_t *voiceData = audio_calloc(1, buf_len);
    int msg = 0;
    TickType_t delay = portMAX_DELAY;

    while (true) {
        if (xQueueReceive(rec_q, &msg, delay) == pdTRUE) {
            switch (msg) {
                case REC_START: {
                    ESP_LOGW(TAG, "voice read begin");
                    delay = 0;
                    voice_reading = true;
                    break;
                }
                case REC_STOP: {
                    ESP_LOGW(TAG, "voice read stopped");
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
                default:
                    break;
            }
        }
        int ret = 0;
        if (voice_reading) {
            ret = audio_recorder_data_read(recorder, voiceData, buf_len, portMAX_DELAY);
            if (ret <= 0) {
                ESP_LOGW(TAG, "audio recorder read finished %d", ret);
                delay = portMAX_DELAY;
                voice_reading = false;
            }
        }
#if VOICE2FILE == (true)
        voice_2_file(voiceData, ret);
#endif /* VOICE2FILE == (true) */
    }

    free(voiceData);
    vTaskDelete(NULL);
}

static esp_err_t rec_engine_cb(audio_rec_evt_t *event, void *user_data)
{
    if (AUDIO_REC_WAKEUP_START == event->type) {
        recorder_sr_wakeup_result_t *wakeup_result = event->event_data;

        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_START");
        ESP_LOGI(TAG, "wakeup: vol %f, mod idx %d, word idx %d", wakeup_result->data_volume, wakeup_result->wakenet_model_index, wakeup_result->wake_word_index);
        esp_audio_sync_play(player, tone_uri[TONE_TYPE_DINGDONG], 0);
        if (voice_reading) {
            int msg = REC_CANCEL;
            if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
                ESP_LOGE(TAG, "rec cancel send failed");
            }
        }
    } else if (AUDIO_REC_VAD_START == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_START");
        if (!voice_reading) {
            int msg = REC_START;
            if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
                ESP_LOGE(TAG, "rec start send failed");
            }
        }
    } else if (AUDIO_REC_VAD_END == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_VAD_STOP");
        if (voice_reading) {
            int msg = REC_STOP;
            if (xQueueSend(rec_q, &msg, 0) != pdPASS) {
                ESP_LOGE(TAG, "rec stop send failed");
            }
        }

    } else if (AUDIO_REC_WAKEUP_END == event->type) {
        ESP_LOGI(TAG, "rec_engine_cb - REC_EVENT_WAKEUP_END");
        AUDIO_MEM_SHOW(TAG);
    } else if (AUDIO_REC_COMMAND_DECT <= event->type) {
        recorder_sr_mn_result_t *mn_result = event->event_data;

        ESP_LOGI(TAG, "rec_engine_cb - AUDIO_REC_COMMAND_DECT");
        ESP_LOGW(TAG, "command %d, phrase_id %d, prob %f, str: %s"
            , event->type, mn_result->phrase_id, mn_result->prob, mn_result->str);
        esp_audio_sync_play(player, tone_uri[TONE_TYPE_HAODE], 0);
    } else {
        ESP_LOGE(TAG, "Unkown event");
    }
    return ESP_OK;
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}

static void start_recorder()
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    int bits_per_sample = 0;
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (NULL == pipeline) {
        return;
    }

#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD == 1) && (CONFIG_AFE_MIC_NUM == 1)
    bits_per_sample = 16;
#else
    bits_per_sample = CODEC_ADC_BITS_PER_SAMPLE;
#endif
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 48000, bits_per_sample, AUDIO_STREAM_READER);
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    audio_element_handle_t filter = NULL;
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.dest_rate = 16000;
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD == 1) && (CONFIG_AFE_MIC_NUM == 2)
    rsp_cfg.mode = RESAMPLE_UNCROSS_MODE;
    rsp_cfg.src_ch = 4;
    rsp_cfg.dest_ch = 4;
    rsp_cfg.max_indata_bytes = 1024;
#endif
    filter = rsp_filter_init(&rsp_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");

    if (filter) {
        audio_pipeline_register(pipeline, filter, "filter");
        const char *link_tag[3] = {"i2s", "filter", "raw"};
        audio_pipeline_link(pipeline, &link_tag[0], 3);
    } else {
        const char *link_tag[2] = {"i2s", "raw"};
        audio_pipeline_link(pipeline, &link_tag[0], 2);
    }

    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "Recorder has been created");

    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.wakenet_init = WAKENET_ENABLE;
    recorder_sr_cfg.afe_cfg.vad_mode = VAD_MODE_4;
    recorder_sr_cfg.multinet_init = MULTINET_ENABLE;
#if !defined(CONFIG_SR_MN_CN_NONE)
    recorder_sr_cfg.mn_language = ESP_MN_CHINESE;
#elif !defined(CONFIG_SR_MN_EN_NONE)
    recorder_sr_cfg.mn_language = ESP_MN_ENGLISH;
#else
    recorder_sr_cfg.mn_language = "";
#endif
    recorder_sr_cfg.afe_cfg.aec_init = RECORD_HARDWARE_AEC;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;
#if (CONFIG_ESP32_S3_KORVO2_V3_BOARD == 1 || CONFIG_ESP32_P4_FUNCTION_EV_BOARD == 1) && (CONFIG_AFE_MIC_NUM == 1)
    recorder_sr_cfg.afe_cfg.pcm_config.mic_num = 1;
    recorder_sr_cfg.afe_cfg.pcm_config.ref_num = 1;
    recorder_sr_cfg.afe_cfg.pcm_config.total_ch_num = 2;
    recorder_sr_cfg.afe_cfg.wakenet_mode = DET_MODE_90;
    recorder_sr_cfg.input_order[0] = DAT_CH_0;
    recorder_sr_cfg.input_order[1] = DAT_CH_1;
#if defined(CONFIG_ESP32_S3_KORVO2_V3_BOARD)
    es7210_mic_select(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC3);
#endif
#endif

#if RECORDER_ENC_ENABLE
    recorder_encoder_cfg_t recorder_encoder_cfg = { 0 };
#if RECORDER_ENC_ENABLE == ENC_2_AMRNB
    rsp_filter_cfg_t filter_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    filter_cfg.src_ch = 1;
    filter_cfg.src_rate = 16000;
    filter_cfg.dest_ch = 1;
    filter_cfg.dest_rate = 8000;
    filter_cfg.stack_in_ext = true;
    filter_cfg.max_indata_bytes = 1024;

    amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amrnb_cfg.contain_amrnb_header = true;
    amrnb_cfg.stack_in_ext = true;

    recorder_encoder_cfg.resample = rsp_filter_init(&filter_cfg);
    recorder_encoder_cfg.encoder = amrnb_encoder_init(&amrnb_cfg);
#elif RECORDER_ENC_ENABLE == ENC_2_AMRWB
    amrwb_encoder_cfg_t amrwb_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amrwb_cfg.contain_amrwb_header = true;
    amrwb_cfg.stack_in_ext = true;
    amrwb_cfg.out_rb_size = 4 * 1024;

    recorder_encoder_cfg.encoder = amrwb_encoder_init(&amrwb_cfg);
#endif
#endif

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_cb_for_afe;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
#if SPEECH_CMDS_RESET
    char err[200];
    recorder_sr_reset_speech_cmd(cfg.sr_handle, SPEECH_COMMANDS, err);
#endif
#if RECORDER_ENC_ENABLE
    cfg.encoder_handle = recorder_encoder_create(&recorder_encoder_cfg, &cfg.encoder_iface);
#endif
    cfg.event_cb = rec_engine_cb;
    cfg.vad_off = 1000;
    recorder = audio_recorder_create(&cfg);
}

esp_err_t periph_callback(audio_event_iface_msg_t *event, void *context)
{
    ESP_LOGD(TAG, "Periph Event received: src_type:%x, source:%p cmd:%d, data:%p, data_len:%d",
        event->source_type, event->source, event->cmd, event->data, event->data_len);
    switch (event->source_type) {
        case PERIPH_ID_ADC_BTN:
            if (((int)event->data == get_input_rec_id()) && (event->cmd == PERIPH_ADC_BUTTON_PRESSED)) {
                audio_recorder_trigger_start(recorder);
                ESP_LOGI(TAG, "REC KEY PRESSED");
            } else if (((int)event->data == get_input_rec_id()) &&
                        (event->cmd == PERIPH_ADC_BUTTON_RELEASE || event->cmd == PERIPH_ADC_BUTTON_LONG_RELEASE)) {
                audio_recorder_trigger_stop(recorder);
                ESP_LOGI(TAG, "REC KEY RELEASE");
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void log_clear(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_log_level_set("AUDIO_THREAD", ESP_LOG_ERROR);
    esp_log_level_set("I2C_BUS", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_HAL", ESP_LOG_ERROR);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_ERROR);
    esp_log_level_set("ESP_DECODER", ESP_LOG_ERROR);
    esp_log_level_set("I2S", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_FORGE", ESP_LOG_ERROR);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    esp_log_level_set("TONE_PARTITION", ESP_LOG_ERROR);
    esp_log_level_set("TONE_STREAM", ESP_LOG_ERROR);
    esp_log_level_set("MP3_DECODER", ESP_LOG_ERROR);
    esp_log_level_set("I2S_STREAM", ESP_LOG_ERROR);
    esp_log_level_set("RSP_FILTER", ESP_LOG_ERROR);
    esp_log_level_set("AUDIO_EVT", ESP_LOG_ERROR);
}

void app_main(void)
{
    log_clear();

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.extern_stack = true;
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    if (set != NULL) {
        esp_periph_set_register_callback(set, periph_callback, NULL);
    }
#if VOICE2FILE
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
#endif
    audio_board_key_init(set);
    audio_board_init();
    setup_player();
    start_recorder();

    rec_q = xQueueCreate(3, sizeof(int));
    audio_thread_create(NULL, "read_task", voice_read_task, NULL, 4 * 1024, 5, true, 0);
}
