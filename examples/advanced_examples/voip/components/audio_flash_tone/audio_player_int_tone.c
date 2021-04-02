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
#include "esp_audio.h"
#include "tone_stream.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "audio_player_int_tone.h"

static const char *TAG = "PLAYER_INT_TONE";

static esp_audio_handle_t player;

audio_err_t audio_player_int_tone_init()
{
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = 16000;
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

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_writer.stack_in_ext = true;
    i2s_writer.i2s_config.sample_rate = 16000;
    i2s_writer.task_core = 1;
    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_writer));

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
