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

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <freertos/semphr.h>

#include "duer_audio_wrapper.h"
#include "lightduer_voice.h"

#include "esp_audio.h"
#include "esp_log.h"

#include "audio_element.h"
#include "audio_mem.h"
#include "board.h"
#include "audio_common.h"
#include "audio_recorder.h"
#include "audio_pipeline.h"
#include "recorder_sr.h"

#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "http_stream.h"
#include "filter_resample.h"

#include "model_path.h"

#ifndef CODEC_ADC_SAMPLE_RATE
#warning "Please define CODEC_ADC_SAMPLE_RATE first, default value is 48kHz may not correctly"
#define CODEC_ADC_SAMPLE_RATE    48000
#endif

#ifndef CODEC_ADC_BITS_PER_SAMPLE
#warning "Please define CODEC_ADC_BITS_PER_SAMPLE first, default value 16 bits may not correctly"
#define CODEC_ADC_BITS_PER_SAMPLE  16
#endif

#ifndef CODEC_ADC_I2S_PORT
#define CODEC_ADC_I2S_PORT  (0)
#endif

static const char *TAG = "AUDIO_WRAPPER";

static SemaphoreHandle_t        s_mutex     = NULL;
static esp_audio_handle_t       player      = NULL;
static audio_rec_handle_t       recorder    = NULL;
static audio_element_handle_t   raw_read    = NULL;

static int player_pause         = 0;
static int audio_pos            = 0;
static int duer_playing_type    = DUER_AUDIO_TYPE_UNKOWN;

static void esp_audio_state_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    esp_audio_state_t esp_state = {0};
    while (1) {
        xQueueReceive(que, &esp_state, portMAX_DELAY);
        ESP_LOGI(TAG, "esp_audio status:%x,err:%x,state:%d", esp_state.status, esp_state.err_msg, duer_playing_type);
        if ((esp_state.status == AUDIO_STATUS_STOPPED)
            || (esp_state.status == AUDIO_STATUS_FINISHED)
            || (esp_state.status == AUDIO_STATUS_ERROR)) {
            if (duer_playing_type == DUER_AUDIO_TYPE_SPEECH) {
                duer_playing_type = DUER_AUDIO_TYPE_UNKOWN;
                bool wakeup = audio_recorder_get_wakeup_state(recorder);
                if (wakeup == false) {
                    duer_dcs_speech_on_finished();
                }
                ESP_LOGE(TAG, "duer_dcs_speech_on_finished,%d, wakeup:%d", duer_playing_type, wakeup);
            } else if ((duer_playing_type == DUER_AUDIO_TYPE_MUSIC) && (player_pause == 0)) {
                bool wakeup = audio_recorder_get_wakeup_state(recorder);
                if (wakeup == false) {
                    duer_dcs_audio_on_finished();
                }
                ESP_LOGE(TAG, "duer_dcs_audio_on_finished,%d, wakeup:%d", duer_playing_type, wakeup);
            }
        }
    }
    vTaskDelete(NULL);
}

int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

void *duer_audio_setup_player(void)
{
    if (player) {
        return player;
    }

    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 48000;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    xTaskCreate(esp_audio_state_task, "player_task", 3 * 1024, cfg.evt_que, 1, NULL);

    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.need_expand = (CODEC_ADC_BITS_PER_SAMPLE != 16);
    i2s_writer.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_writer_h = i2s_stream_init(&i2s_writer);
    i2s_stream_set_clk(i2s_writer_h, 48000, CODEC_ADC_BITS_PER_SAMPLE, 16);

    // Add decoders and encoders to esp_audio
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_cfg.task_core = 1;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_writer));

    // Set default volume
    esp_audio_vol_set(player, 60);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", player);

    return player;
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}

void *duer_audio_start_recorder(rec_event_cb_t cb)
{
    if (recorder) {
        return recorder;
    }

    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (NULL == pipeline) {
        return NULL;
    }

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, CODEC_ADC_SAMPLE_RATE, 16, AUDIO_STREAM_READER);
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_stream_reader, CODEC_ADC_SAMPLE_RATE, 16, 2);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);

    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");
    const char *link_tag[3] = {"i2s", "raw"};
    uint8_t linked_num = 2;

#if (CODEC_ADC_SAMPLE_RATE != (16000))
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = CODEC_ADC_SAMPLE_RATE;
    rsp_cfg.dest_rate = 16000;
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    rsp_cfg.mode = RESAMPLE_UNCROSS_MODE;
    rsp_cfg.src_ch = 4;
    rsp_cfg.dest_ch = 4;
    rsp_cfg.max_indata_bytes = 1024;
#endif
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);
    audio_pipeline_register(pipeline, filter, "filter");
    link_tag[1] = "filter";
    link_tag[2] = "raw";
    linked_num = 3;
#endif
    audio_pipeline_link(pipeline, &link_tag[0], linked_num);
    audio_pipeline_run(pipeline);
    ESP_LOGI(TAG, "Recorder has been created");

    recorder_sr_cfg_t recorder_sr_cfg = DEFAULT_RECORDER_SR_CFG();
    recorder_sr_cfg.afe_cfg.aec_init = false;
    recorder_sr_cfg.afe_cfg.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    recorder_sr_cfg.afe_cfg.agc_mode = AFE_MN_PEAK_NO_AGC;

    audio_rec_cfg_t cfg = AUDIO_RECORDER_DEFAULT_CFG();
    cfg.read = (recorder_data_read_t)&input_cb_for_afe;
    cfg.sr_handle = recorder_sr_create(&recorder_sr_cfg, &cfg.sr_iface);
    cfg.event_cb = cb;
    cfg.vad_off = 1000;
    recorder = audio_recorder_create(&cfg);

    return recorder;
}

void duer_audio_wrapper_init(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
        } else {
            ESP_LOGD(TAG, "Create mutex success");
        }
    }
}

void duer_audio_wrapper_pause()
{
    esp_audio_stop(player, 0);
    ESP_LOGW(TAG, "duer_audio_wrapper_pause, playing type:%d", duer_playing_type);
}

int duer_audio_wrapper_get_state()
{
    esp_audio_state_t st = {0};
    esp_audio_state_get(player, &st);
    return st.status;
}

void duer_dcs_listen_handler(void)
{
    ESP_LOGI(TAG, "enable_listen_handler, open mic");
    audio_recorder_trigger_start(recorder);
}

void duer_dcs_stop_listen_handler(void)
{
    ESP_LOGI(TAG, "stop_listen, close mic");
    audio_recorder_trigger_stop(recorder);
}

void duer_dcs_volume_set_handler(int volume)
{
    ESP_LOGI(TAG, "set_volume to %d", volume);
    int ret = esp_audio_vol_set(player, volume);
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_volume_changed");
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_volume_adjust_handler(int volume)
{
    ESP_LOGI(TAG, "adj_volume by %d", volume);
    int vol = 0;
    esp_audio_vol_get(player, &vol);
    vol += volume;
    int ret = esp_audio_vol_set(player, vol);
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_volume_changed");
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_mute_handler(duer_bool is_mute)
{
    ESP_LOGI(TAG, "set_mute to  %d", (int)is_mute);
    int ret = 0;
    if (is_mute) {
        ret = esp_audio_vol_set(player, 0);
    }
    if (ret == 0) {
        ESP_LOGI(TAG, "report on_mute");
        duer_dcs_on_mute();
    }
}

void duer_dcs_get_speaker_state(int *volume, duer_bool *is_mute)
{
    ESP_LOGI(TAG, "duer_dcs_get_speaker_state");
    *volume = 60;
    *is_mute = false;
    int ret = 0;
    int vol = 0;
    ret = esp_audio_vol_get(player, &vol);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to get volume");
    } else {
        *volume = vol;
        if (vol != 0) {
            *is_mute = false;
        } else {
            *is_mute = true;
        }
    }
}

void duer_dcs_speak_handler(const char *url)
{
    ESP_LOGI(TAG, "Playing speak: %s", url);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duer_playing_type = DUER_AUDIO_TYPE_SPEECH;
    xSemaphoreGive(s_mutex);
}

void duer_dcs_audio_play_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Playing audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, audio_info->url, audio_info->offset);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    duer_playing_type = DUER_AUDIO_TYPE_MUSIC;
    player_pause = 0;
    xSemaphoreGive(s_mutex);
}

void duer_dcs_audio_stop_handler()
{
    ESP_LOGI(TAG, "Stop audio play");
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int status = duer_playing_type;
    xSemaphoreGive(s_mutex);
    if (status == 1) {
        ESP_LOGI(TAG, "Is playing speech, no need to stop");
    } else {
        esp_audio_stop(player, TERMINATION_TYPE_NOW);
    }
}

void duer_dcs_audio_pause_handler()
{
    ESP_LOGI(TAG, "DCS pause audio play");
    esp_audio_pos_get(player, &audio_pos);
    player_pause = 1;
    esp_audio_stop(player, 0);
}

void duer_dcs_audio_resume_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Resume audio, offset:%d url:%s", audio_info->offset, audio_info->url);
    player_pause = 0;
    duer_playing_type = DUER_AUDIO_TYPE_MUSIC;
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, audio_info->url, audio_pos);
}

int duer_dcs_audio_get_play_progress()
{
    int ret = 0;
    uint32_t total_size = 0;
    int position = 0;
    ret = esp_audio_pos_get(player, &position);
    if (ret == 0) {
        ESP_LOGI(TAG, "Get play position %d of %" PRIu32, position, total_size);
        return position;
    } else {
        ESP_LOGE(TAG, "Failed to get play progress.");
        return -1;
    }
}

duer_audio_type_t duer_dcs_get_player_type()
{
    return duer_playing_type;
}

int duer_dcs_set_player_type(duer_audio_type_t num)
{
    duer_playing_type = num;
    return 0;
}

int duer_dcs_audio_sync_play_tone(const char *uri)
{
    esp_audio_sync_play(player, uri, 0);
    return 0;
}

void duer_dcs_audio_active_paused()
{
    if (duer_dcs_send_play_control_cmd(DCS_PAUSE_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PAUSE_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_paused");
}

void duer_dcs_audio_active_play()
{
    if (duer_dcs_send_play_control_cmd(DCS_PLAY_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PLAY_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_play");
}

void duer_dcs_audio_active_previous()
{
    if (duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PREVIOUS_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "Fduer_dcs_audio_active_previous");
}

void duer_dcs_audio_active_next()
{
    if (duer_dcs_send_play_control_cmd(DCS_NEXT_CMD)) {
        ESP_LOGE(TAG, "Send DCS_NEXT_CMD to DCS failed");
    }
    ESP_LOGD(TAG, "duer_dcs_audio_active_next");
}
