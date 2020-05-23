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

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_mem.h"
#include "audio_player.h"

#include "board.h"
#include "audio_player_setup_common.h"
#include "audio_player_common.h"
#include "audio_player_helper.h"

static const char *TAG = "ESP_PLAYER_UT";

static void test_setup(void)
{
    ESP_LOGI(TAG, "[✓] setup Wi-Fi and SDcard");
    setup_wifi();
    setup_sdcard();
}

static void test_teardown(void)
{
    teardown_wifi();
    teardown_sdcard();
}

TEST_CASE("esp_palyer_init", "Player-IF-TEST")
{
    ESP_LOGI(TAG, "[✓] audio_element_init element");
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_INVALID_PARAMETER, audio_player_init(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NOT_READY, audio_player_deinit());

    AUDIO_MEM_SHOW(TAG);
    audio_player_cfg_t cfg = DEFAULT_AUDIO_PLAYER_CONFIG();

    for (int i = 0; i < 5; ++i) {
        cfg.handle = setup_esp_audio_instance(NULL);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_init(&cfg));
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_deinit());
        vTaskDelay(10 / portTICK_RATE_MS);
        AUDIO_MEM_SHOW(TAG);
        ESP_LOGW(TAG, "[✓] player %d \r\n", i);
    }

    ESP_LOGE(TAG, "[✓] external_ram_stack \r\n");
    cfg.external_ram_stack = true;
    for (int i = 0; i < 5; ++i) {
        cfg.handle = setup_esp_audio_instance(NULL);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_init(&cfg));
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_deinit());
        vTaskDelay(10 / portTICK_RATE_MS);
        AUDIO_MEM_SHOW(TAG);

    }
    ESP_LOGW(TAG, "[✓] player  Done\r\n");
    vTaskDelay(500 / portTICK_RATE_MS);
    AUDIO_MEM_SHOW(TAG);
}


TEST_CASE("audio_player_music_play and stop", "ADF-IF-TEST")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_MANAGER", ESP_LOG_DEBUG);
    esp_log_level_set("A2DP_STREAM", ESP_LOG_DEBUG);
    // esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_DEBUG);
    test_setup();

    AUDIO_MEM_SHOW(TAG);

    QueueHandle_t player_que = xQueueCreate(3, sizeof(audio_player_state_t));
    esp_audio_handle_t player = init_audio_player(player_que, ESP_AUDIO_PREFER_MEM);
    opt_para_t para = {
        .cb = ut_audio_player_stop,
        .ctx = NULL,
        .ticks_to_wait_ms = 4000,
    };
    ESP_LOGW(TAG, "Bluetooth music play test once:%d \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_RAW_A2DP_PCM_I2S], 0, MEDIA_SRC_TYPE_MUSIC_A2DP));
    para.cb = ut_audio_player_raw_stop;
    para.ticks_to_wait_ms = 30000;
    request_opt(&para);
    ESP_LOGW(TAG, "RAW music play test once:%d \r\n\r\n", __LINE__);

    audio_player_status_check(player_que, 30000);
    ESP_LOGW(TAG, "RAW music play test once:%d \r\n\r\n", __LINE__);

    TEST_ASSERT_EQUAL(AUDIO_STATUS_STOPPED, audio_player_status_check(player_que, portMAX_DELAY));
    ESP_LOGW(TAG, "RAW music play test once:%d \r\n\r\n", __LINE__);



    ESP_LOGW(TAG, "RAW music play test once:%d \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_RAW_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_RAW));
    raw_write(dec_uri[TEST_DEC_URI_RAW_MP3_I2S]);
    TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
    para.cb = ut_audio_player_raw_stop;
    para.ticks_to_wait_ms = 7000;
    request_opt(&para);
    TEST_ASSERT_EQUAL(AUDIO_STATUS_STOPPED, audio_player_status_check(player_que, portMAX_DELAY));

    ESP_LOGW(TAG, "HTTP music play test once:%d \r\n\r\n", __LINE__);

    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_HTTP_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_HTTP));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, portMAX_DELAY));

    ESP_LOGW(TAG, "File music play test once:%d \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_FILE_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_SD));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, portMAX_DELAY));


    ESP_LOGW(TAG, "File music play and stop:%d \r\n\r\n", __LINE__);
    para.ticks_to_wait_ms = 2000;
    AUDIO_MEM_SHOW(TAG);
    for (int i = TEST_DEC_URI_FILE_MP3_I2S; i <= TEST_DEC_URI_FILE_MP3_I2S + 1; ++i) {
        ESP_LOGI(TAG, "File Play, indx:%d, :%s\r\n\r\n", __LINE__, dec_uri[i] );
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        request_opt(&para);
        TEST_ASSERT_EQUAL(AUDIO_STATUS_STOPPED, audio_player_status_check(player_que, portMAX_DELAY));
    }
    ESP_LOGW(TAG, "Raw music play and stop :%d \r\n\r\n", __LINE__);
    opt_para_t raw_play_para = {
        .cb = ut_audio_player_raw_play,
        .ctx = dec_uri[TEST_DEC_URI_RAW_MP3_I2S],
    };
    for (int i = 0; i < 50; ++i) {
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_RAW_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_RAW));
        raw_play_para.ticks_to_wait_ms = esp_random() % 1000;
        request_opt(&raw_play_para);

        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        para.cb = ut_audio_player_raw_stop;
        para.ticks_to_wait_ms = esp_random() % 2500;
        request_opt(&para);
        TEST_ASSERT_EQUAL(AUDIO_STATUS_STOPPED, audio_player_status_check(player_que, portMAX_DELAY));
    }

    ESP_LOGW(TAG, "HTTP play and stop, %d \r\n\r\n", __LINE__);
    para.ticks_to_wait_ms = 4000;
    for (int i = TEST_DEC_URI_HTTP_MP3_I2S + 1; i <= TEST_DEC_URI_HTTPS_MP3_I2S; ++i) {
        ESP_LOGW(TAG, "HTTP Play, indx:%d, :%s\r\n\r\n", __LINE__, dec_uri[i] );
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        request_opt(&para);
        TEST_ASSERT_EQUAL(AUDIO_STATUS_STOPPED, audio_player_status_check(player_que, portMAX_DELAY));

    }
    vQueueDelete(player_que);
    deinit_audio_player();
    test_teardown();
    vTaskDelay(1000 / portTICK_RATE_MS);
    AUDIO_MEM_SHOW(TAG);
}

TEST_CASE("music and tone play", "ADF-IF-TEST")
{
    AUDIO_MEM_SHOW(TAG);
    /**
     * Flash tone and music testing note.
     *
     *  1. Add "flash_tone,data, 0xff,  0x110000 , 500K," in partition CSV file
     *  2. Download the audio tone file "examples/player/pipeline_flash_tone/tools/audio-esp.bin" to sepcific address.
     *  3. Copy the "examples/player/pipeline_flash_tone/components/audio_flash_tone" to "esp-idf/tools/unit-test-app/components/audio_flash_tone"
     *
     */

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_MANAGER", ESP_LOG_DEBUG);
    esp_log_level_set("A2DP_STREAM", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_DEBUG);
    test_setup();

    AUDIO_MEM_SHOW(TAG);

    QueueHandle_t player_que = xQueueCreate(3, sizeof(audio_player_state_t));
    esp_audio_handle_t player = init_audio_player(player_que, ESP_AUDIO_PREFER_MEM);
    opt_para_t para = {
        .cb = ut_audio_player_stop,
        .ctx = NULL,
        .ticks_to_wait_ms = 4000,
    };

    ESP_LOGE(TAG, "Case: blocking play tone, without auto resume");
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/3_New_Version_Available.mp3", true, false, MEDIA_SRC_TYPE_TONE_FLASH));


    ESP_LOGE(TAG, "Case: unblock play tone, without auto resume");
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/4_Bt_Success.mp3", false, false, MEDIA_SRC_TYPE_TONE_FLASH));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, 6000));

    for (int i = 1; i <= TEST_DEC_URI_FILE_OPUS_I2S; ++i) {

        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume, %d, %s ", i, dec_uri[i]);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        vTaskDelay(2000 / portTICK_RATE_MS);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/5_Bt_Success.mp3", true, true, MEDIA_SRC_TYPE_TONE_FLASH));
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone finished music running event
        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        vTaskDelay(1000 / portTICK_RATE_MS);
        clear_queue_events(player_que);
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 3000;
        request_opt(&para);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));

        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        vTaskDelay(2000 / portTICK_RATE_MS);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/5_Bt_Success.mp3", true, false, MEDIA_SRC_TYPE_TONE_FLASH));
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Resume music playing
        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_resume());
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));

        vTaskDelay(1000 / portTICK_RATE_MS);
        clear_queue_events(player_que);
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 2000;
        request_opt(&para);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));


        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        para.cb = ut_tone_unblock_resume_play;
        para.ctx = NULL;
        para.ticks_to_wait_ms = esp_random() % 1000;
        request_opt(&para);
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone running events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        // Tone finished events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_FINISHED, audio_player_status_check(player_que, 10000));
        // Resume music running event
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 3000;
        request_opt(&para);
        vTaskDelay(5000 / portTICK_RATE_MS);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));


        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        para.cb = ut_tone_unblock_no_resume_play;
        para.ctx = NULL;
        para.ticks_to_wait_ms = esp_random() % 1000;
        request_opt(&para);
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone running events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        // Tone finished events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_FINISHED, audio_player_status_check(player_que, 10000));
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_resume());
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 3000;
        request_opt(&para);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));
    }

    for (int i = TEST_DEC_URI_HTTP_MP3_I2S; i <= TEST_DEC_URI_LIVE_AAC_I2S; ++i) {
        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume, %d, %s ", i, dec_uri[i]);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        vTaskDelay(1500 / portTICK_RATE_MS);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/5_Bt_Success.mp3", true, true, MEDIA_SRC_TYPE_TONE_FLASH));
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone finished music running event
        ESP_LOGE(TAG, "Case: Music playing, block play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        vTaskDelay(1000 / portTICK_RATE_MS);
        clear_queue_events(player_que);
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 1000;
        request_opt(&para);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));

        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        vTaskDelay(1500 / portTICK_RATE_MS);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_tone_play("flash://tone/5_Bt_Success.mp3", true, false, MEDIA_SRC_TYPE_TONE_FLASH));
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Resume music playing
        ESP_LOGE(TAG, "Case: Music playing, block play tone without auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_resume());
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));

        vTaskDelay(1000 / portTICK_RATE_MS);
        clear_queue_events(player_que);
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 1000;
        request_opt(&para);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));


        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        para.cb = ut_tone_unblock_resume_play;
        para.ctx = NULL;
        para.ticks_to_wait_ms = esp_random() % 1000;
        request_opt(&para);
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone running events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        // Tone finished events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_FINISHED, audio_player_status_check(player_que, 10000));
        // Resume music running event
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone with auto resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 1000;
        request_opt(&para);
        vTaskDelay(1500 / portTICK_RATE_MS);
        // waiting for music stopped
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));


        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume");
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[i], 0, MEDIA_SRC_TYPE_MUSIC_SD));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 2000));
        para.cb = ut_tone_unblock_no_resume_play;
        para.ctx = NULL;
        para.ticks_to_wait_ms = esp_random() % 1000;
        request_opt(&para);
        // Receive the music stopped event due to tone interrput play
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 2000));
        // Tone running events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        // Tone finished events
        ESP_LOGE(TAG, "Case: Music playing, unblock play tone without resume %d", __LINE__);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_FINISHED, audio_player_status_check(player_que, 10000));
        TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_resume());
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_RUNNING, audio_player_status_check(player_que, 10000));
        para.cb = ut_audio_player_stop;
        para.ticks_to_wait_ms = 1000;
        request_opt(&para);
        TEST_ASSERT_EQUAL(AUDIO_PLAYER_STATUS_STOPPED, audio_player_status_check(player_que, 10000));
    }


    vQueueDelete(player_que);
    deinit_audio_player();
    AUDIO_MEM_SHOW(TAG);
    test_teardown();
    vTaskDelay(1000 / portTICK_RATE_MS);
}

TEST_CASE("music play with play mode", "ADF-IF-TEST")
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("ESP_AUDIO_CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_MANAGER", ESP_LOG_DEBUG);
    esp_log_level_set("A2DP_STREAM", ESP_LOG_DEBUG);
    // esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("ESP_AUDIO_TASK", ESP_LOG_DEBUG);
    setup_sdcard();

    AUDIO_MEM_SHOW(TAG);

    QueueHandle_t player_que = xQueueCreate(3, sizeof(audio_player_state_t));
    esp_audio_handle_t player = init_audio_player(player_que, ESP_AUDIO_PREFER_MEM);
    opt_para_t para = {
        .cb = ut_audio_player_stop,
        .ctx = NULL,
        .ticks_to_wait_ms = 4000,
    };

    ESP_LOGE(TAG, "File music play with AUDIO_PLAYER_MODE_ONE_SONG mode(line:%d) \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_mode_set(AUDIO_PLAYER_MODE_ONE_SONG));
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_FILE_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_SD));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 5000));
    TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, portMAX_DELAY));

    ESP_LOGE(TAG, "File music play with AUDIO_PLAYER_MODE_REPEAT mode(line:%d) \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_mode_set(AUDIO_PLAYER_MODE_REPEAT));
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_next());
    for (int i = 0; i < 2; ++i) {
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 5000));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, portMAX_DELAY));
    }

    ESP_LOGE(TAG, "File music play with AUDIO_PLAYER_MODE_SEQUENCE mode(line:%d) \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_mode_set(AUDIO_PLAYER_MODE_SEQUENCE));
    while (1) {
        int st = audio_player_status_check(player_que, portMAX_DELAY);
        ESP_LOGW(TAG, "Got Status is %x \r\n", st);
        if ( AUDIO_PLAYER_STATUS_PLAYLIST_FINISHED == st) {
            ESP_LOGW(TAG, "Got AUDIO_PLAYER_STATUS_PLAYLIST_FINISHED (line:%d) \r\n\r\n", __LINE__);
            break;
        }
    }

    ESP_LOGE(TAG, "File music play with AUDIO_PLAYER_MODE_SHUFFLE mode(line:%d) \r\n\r\n", __LINE__);
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_mode_set(AUDIO_PLAYER_MODE_SHUFFLE));
    TEST_ASSERT_EQUAL(ESP_ERR_AUDIO_NO_ERROR, audio_player_music_play(dec_uri[TEST_DEC_URI_FILE_MP3_I2S], 0, MEDIA_SRC_TYPE_MUSIC_SD));
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT_EQUAL(AUDIO_STATUS_RUNNING, audio_player_status_check(player_que, 5000));
        TEST_ASSERT_EQUAL(AUDIO_STATUS_FINISHED, audio_player_status_check(player_que, portMAX_DELAY));
    }

    vQueueDelete(player_que);
    deinit_audio_player();
    AUDIO_MEM_SHOW(TAG);
    teardown_sdcard();
    vTaskDelay(1000 / portTICK_RATE_MS);
}