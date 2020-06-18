/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef AUDIO_PLAYER_COMMON
#define AUDIO_PLAYER_COMMON

typedef enum {
    TEST_DEC_URI_UTF8_FILE_MP3_I2S      = 0,
    TEST_DEC_URI_FILE_MP3_I2S           = 1,
    TEST_DEC_URI_FILE_WAV_I2S           = 2,
    TEST_DEC_URI_FILE_AAC_I2S           = 3,
    TEST_DEC_URI_FILE_M4A_I2S           = 4,
    TEST_DEC_URI_FILE_TS_I2S            = 5,
    TEST_DEC_URI_FILE_AMR_I2S           = 6,
    TEST_DEC_URI_FILE_FLAC_I2S          = 7,
    TEST_DEC_URI_FILE_OGG_I2S           = 8,
    TEST_DEC_URI_FILE_OPUS_I2S          = 9,
    TEST_DEC_URI_HTTP_MP3_I2S           = 10,
    TEST_DEC_URI_LIVE_AAC_I2S           = 11,
    TEST_DEC_URI_HTTPS_MP3_I2S          = 12,
    TEST_DEC_URI_HTTP_M4A_I2S           = 13,
    TEST_DEC_URI_HTTP_AAC_I2S           = 14,
    TEST_DEC_URI_FILE_MP3_RAW           = 15,
    TEST_DEC_URI_RAW_MP3_I2S            = 16,
    TEST_DEC_URI_RAW_A2DP_PCM_I2S       = 17,
    TEST_DEC_URI_MAX,
} test_dec_uri_t;

extern const char *dec_uri[];

typedef void (*async_opt_cb)(void *ctx);

typedef struct {
    async_opt_cb        cb;
    void                *ctx;
    int                 ticks_to_wait_ms;
} opt_para_t;

typedef struct {
    void *handle;
    const char         *uri;
    int                pos;
} async_play_para_t;

void request_opt(opt_para_t *p);

void uri_list_play_stop_opt(void *handle, QueueHandle_t que, async_opt_cb cb, const char **uri);

void ut_audio_player_stop (void *ctx);
void ut_audio_player_raw_stop (void *ctx);
void ut_audio_player_raw_play (void *ctx);

void ut_tone_unblock_no_resume_play (void *ctx);
void ut_tone_block_no_resume_play (void *ctx);
void ut_tone_unblock_resume_play (void *ctx);
void ut_tone_block_resume_play (void *ctx);



void async_sync_play (void *ctx);
void uri_list_sync_play_stop_opt(void *handle, QueueHandle_t que, const char **uri);
void play_insert_play(void *player, QueueHandle_t player_que, test_dec_uri_t first_url, test_dec_uri_t insert_url);
void play_insert_play_speed(void *player, QueueHandle_t player_que, test_dec_uri_t first_url, test_dec_uri_t insert_url);
void play_seek(void *player, QueueHandle_t player_que, test_dec_uri_t url_id);
void check_seek_time(void *player, int seek_time_ms, int duration);

#endif
