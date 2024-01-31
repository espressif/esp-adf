/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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
#include <string.h>

#include "esp_log.h"
#include "audio_error.h"
#include "audio_sys.h"
#include "esp_audio.h"
#include "tone_stream.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "algorithm_stream.h"
#include "audio_player_int_tone.h"

static const char *TAG = "PLAYER_INT_TONE";

static esp_audio_handle_t player;
static audio_element_handle_t i2s_writer;

static int i2s_write_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_write = 0, size = len;
    AUDIO_UNUSED(size);
#ifndef CONFIG_ESP32_S3_KORVO2L_V1_BOARD
    /* Drop right channel */
    int16_t *tmp = (int16_t *)buf;
    for (int i = 0; i < len / 4; i++) {
        tmp[i] = tmp[i << 1];
    }
    size = len / 2;
#endif

#if CONFIG_IDF_TARGET_ESP32
    algorithm_mono_fix((uint8_t *)buf, size);
#endif
    int ret = audio_element_output(el, buf, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed");
    }

    return bytes_write;
}

audio_err_t audio_player_int_tone_init(int sample_rate, int channel_format, int bits_per_sample)
{
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = sample_rate;
    player = esp_audio_create(&cfg);
    AUDIO_MEM_CHECK(TAG, player, return ESP_FAIL;);

    // Create readers and add to esp_audio
    tone_stream_cfg_t tn_reader = TONE_STREAM_CFG_DEFAULT();
    tn_reader.type = AUDIO_STREAM_READER;
    tn_reader.task_core = 1;
    esp_audio_input_stream_add(player, tone_stream_init(&tn_reader));

    // Add decoders and encoders to esp_audio
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    mp3_dec_cfg.task_prio = 20;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));

    i2s_stream_cfg_t i2s_writer_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_writer_cfg.type = AUDIO_STREAM_WRITER;
    i2s_writer_cfg.stack_in_ext = true;
    i2s_writer_cfg.task_core = 1;
    i2s_writer_cfg.need_expand = true;
    i2s_writer_cfg.expand_src_bits = 16;
    i2s_writer = i2s_stream_init(&i2s_writer_cfg);
    i2s_stream_set_clk(i2s_writer, sample_rate, bits_per_sample, channel_format);
    esp_audio_output_stream_add(player, i2s_writer);
    audio_element_set_write_cb(i2s_writer, i2s_write_cb, NULL);
    audio_element_set_output_timeout(i2s_writer, portMAX_DELAY);

    return ESP_OK;
}

audio_err_t audio_player_int_tone_stop()
{
    esp_audio_stop(player, TERMINATION_TYPE_NOW);
    return ESP_OK;
}

audio_err_t audio_player_int_tone_play(const char *url)
{
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
    return ESP_OK;
}
