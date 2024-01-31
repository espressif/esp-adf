/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include "freertos/timers.h"
#include <freertos/semphr.h>

#include "esp_log.h"
#include "esp_audio.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "board.h"
#include "audio_common.h"
#include "audio_hal.h"
#include "filter_resample.h"
#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "http_stream.h"
#include "audio_idf_version.h"

#include "model_path.h"

#include "audio_recorder.h"
#include "recorder_sr.h"

static char *TAG = "AUDIO_SETUP";

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

static audio_element_handle_t raw_read;

static int _http_stream_event_handle(http_stream_event_msg_t *msg)
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


void *setup_player(void *cb, void *ctx)
{
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.resample_rate = 48000;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.cb_func = cb;
    cfg.cb_ctx = ctx;
    esp_audio_handle_t handle = esp_audio_create(&cfg);

    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(handle, fatfs_stream_init(&fs_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);
    http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);

    // Add decoders and encoders to esp_audio
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_cfg.task_core = 1;
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_cfg));
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_writer.need_expand = (CODEC_ADC_BITS_PER_SAMPLE != 16);
    audio_element_handle_t i2s_write_h = i2s_stream_init(&i2s_writer);
    i2s_stream_set_clk(i2s_write_h, 48000, CODEC_ADC_BITS_PER_SAMPLE, 2);
    esp_audio_output_stream_add(handle, i2s_write_h);

    // Set default volume
    esp_audio_vol_set(handle, 60);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", handle);
    return handle;
}

static int input_cb_for_afe(int16_t *buffer, int buf_sz, void *user_ctx, TickType_t ticks)
{
    return raw_stream_read(raw_read, (char *)buffer, buf_sz);
}

void *setup_recorder(rec_event_cb_t cb, void *ctx)
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_handle_t pipeline;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (NULL == pipeline) {
        return NULL;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 44100, 16, AUDIO_STREAM_READER);
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
#ifdef CONFIG_ESP32_S3_KORVO2_V3_BOARD
    i2s_stream_set_clk(i2s_stream_reader, CODEC_ADC_SAMPLE_RATE, 32, 2);
#else 
    i2s_stream_set_clk(i2s_stream_reader, CODEC_ADC_SAMPLE_RATE, CODEC_ADC_BITS_PER_SAMPLE, 2);
#endif

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
    cfg.user_data = ctx;
    return audio_recorder_create(&cfg);
}
