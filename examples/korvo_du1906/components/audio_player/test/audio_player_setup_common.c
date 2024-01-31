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
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "esp_audio.h"
#include "audio_player.h"
#include "audio_hal.h"

#include "raw_stream.h"
#include "http_stream.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "tone_stream.h"
#include "a2dp_stream.h"

#include "wav_encoder.h"
#include "amrwb_encoder.h"
#include "amrnb_encoder.h"
#include "opus_encoder.h"

#include "wav_decoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"
#include "amr_decoder.h"
#include "flac_decoder.h"
#include "ogg_decoder.h"
#include "opus_decoder.h"
#include "pcm_decoder.h"

#include "filter_resample.h"
#include "audio_forge.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "board.h"
#include "audio_player_helper.h"
#include "esp_audio_helper.h"

static const char *TAG = "ESP_AUDIO_COMMON";
static esp_periph_set_handle_t set;
static audio_board_handle_t board_handle ;
static esp_periph_handle_t wifi_handle;
static esp_periph_handle_t sdcard_handle;
static audio_element_handle_t raw_read_h;
static audio_element_handle_t raw_write_h;
static bool raw_task_run_flag = 0;

#include "audio_idf_version.h"

void setup_wifi()
{
    if (wifi_handle == NULL) {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
            // NVS partition was truncated and needs to be erased
            // Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        tcpip_adapter_init();

        ESP_LOGI(TAG, "Start Wi-Fi");
        if (set == NULL) {
            esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
            set = esp_periph_set_init(&periph_cfg);
        }
        periph_wifi_cfg_t wifi_cfg = {
            .disable_auto_reconnect = false,
            .wifi_config.sta.ssid = "YOUR_SSID",
            .wifi_config.sta.password = "YOUR_PASSWORD",
        };
        wifi_handle = periph_wifi_init(&wifi_cfg);
        esp_periph_start(set, wifi_handle);
        ESP_LOGI(TAG, "Waiting for Wi-Fi to connect");
        periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    }
}
void teardown_wifi()
{
    esp_periph_set_stop_all(set);
    if (set) {
        esp_periph_set_destroy(set);
        set = NULL;
    }
    wifi_handle = NULL;
}

void setup_sdcard()
{
    ESP_LOGI(TAG, "Start SdCard");
    if (sdcard_handle == NULL) {
        if (set == NULL) {
            esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
            set = esp_periph_set_init(&periph_cfg);
        }
        audio_board_sdcard_init(set, SD_MODE_1_LINE);
        sdcard_handle = esp_periph_set_get_by_id(set, PERIPH_ID_SDCARD);
    }
}

void teardown_sdcard()
{
    esp_periph_set_stop_all(set);
    if (set) {
        esp_periph_set_destroy(set);
        set = NULL;
    }
    sdcard_handle = NULL;
}

static void raw_read_task(void *para)
{
    char *uri = (char *)para;
    char *buf = audio_calloc(1, 4096);
    AUDIO_MEM_CHECK(TAG, buf, return;);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.task_stack = 0;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s = i2s_stream_init(&i2s_cfg);
    audio_element_run(i2s);
    i2s_stream_set_clk(i2s, 44100, 16, 2);

    ESP_LOGI(TAG, "Raw reading..., URI:%s", uri);
    while (raw_task_run_flag) {
        int ret  = raw_stream_read(raw_read_h, buf, 4096);
        if (AEL_IO_DONE == ret) {
            ESP_LOGE(TAG, "Raw read done");
            break;
        }
        audio_element_output(i2s, buf, 4096);
    }
    /// uninstall i2s is a dangerous operation
    audio_element_deinit(i2s);
    free(buf);
    ESP_LOGI(TAG, "Raw read stop");
    vTaskDelete(NULL);
}

void raw_read(const char *p)
{
    raw_task_run_flag = true;
    if (xTaskCreate(raw_read_task, "RawReadTask", 3072, (void *)p, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ERROR creating raw_read_task task! Out of memory?");
    }
}
extern audio_element_handle_t raw_play_handle;
static void raw_write_task(void *para)
{
    char *uri = (char *)para;
    char *buf = audio_calloc(1, 4096);
    AUDIO_MEM_CHECK(TAG, buf, return;);

    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.task_stack = 0;
    fatfs_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t fs = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(fs, uri);
    audio_element_process_init(fs);
    audio_element_run(fs);

    ESP_LOGI(TAG, "Raw writing..., URI:%s", uri);
    while (raw_task_run_flag) {
        int ret = audio_element_input(fs, buf, 4096);
        printf(".");
        if (AEL_IO_OK == ret) {
            ESP_LOGE(TAG, "Raw write done");
            audio_player_raw_feed_finish();
            break;
        }
        audio_player_raw_feed_data((uint8_t *)buf, 4096);
    }
    audio_element_process_deinit(fs);
    audio_element_deinit(fs);
    free(buf);
    ESP_LOGI(TAG, "Raw write stop");
    vTaskDelete(NULL);
}

void raw_write(const char *p)
{
    raw_task_run_flag = true;
    if (xTaskCreate(raw_write_task, "RawWriteTask", 3072, (void *)p, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ERROR creating raw_write_task task! Out of memory?");
    }
}

static void raw_read_save(void *para)
{
    char *uri = (char *)para;
    char *buf = audio_calloc(1, 4096);
    AUDIO_MEM_CHECK(TAG, buf, return;);

    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.task_stack = 0;
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t fs = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(fs, "/sdcard/ut_record.wav");
    audio_element_process_init(fs);
    audio_element_run(fs);

    ESP_LOGI(TAG, "Raw reading..., URI:%s", uri);
    while (raw_task_run_flag) {
        int ret  = raw_stream_read(raw_read_h, buf, 4096);
        printf(".");
        if (AEL_IO_DONE == ret) {
            ESP_LOGE(TAG, "Raw read done");
            break;
        } else if (ret == AEL_IO_ABORT) {
            ESP_LOGE(TAG, "Raw read stop");
            break;
        }
        audio_element_output(fs, buf, 4096);
    }
    audio_element_process_deinit(fs);
    audio_element_deinit(fs);
    free(buf);
    ESP_LOGI(TAG, "Raw read stop");
    vTaskDelete(NULL);
}

void raw_read_save_file(const char *p)
{
    raw_task_run_flag = true;
    if (xTaskCreate(raw_read_save, "RawReadTask", 3072, (void *)p, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ERROR creating raw_read_save task! Out of memory?");
    }
}

void raw_stop(void)
{
    raw_task_run_flag = false;
}

audio_element_handle_t create_raw_read_stream()
{
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read_h = raw_stream_init(&raw_cfg);
    return raw_read_h;
}
audio_element_handle_t create_raw_write_stream()
{
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_write_h = raw_stream_init(&raw_cfg);
    return raw_write_h;
}

audio_element_handle_t create_fatfs_read_stream()
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    return fatfs_stream_init(&fatfs_cfg);
}

audio_element_handle_t create_fatfs_write_stream()
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    return fatfs_stream_init(&fatfs_cfg);
}

audio_element_handle_t create_i2s_read_stream()
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t i2s_read_h = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_read_h, 48000, 16, 2);
    return i2s_read_h;
}

audio_element_handle_t create_i2s_write_stream()
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_write_h = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_write_h, 48000, 16, 2);
    return i2s_write_h;
}

audio_element_handle_t create_http_read_stream()
{
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    return http_stream_reader;
}

audio_element_handle_t create_wav_enc()
{
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    // wav_cfg.task_core = 1;
    wav_cfg.task_prio = 12;
    return wav_encoder_init(&wav_cfg);
}

audio_element_handle_t create_wav_dec()
{
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    // wav_cfg.task_core = 1;
    wav_cfg.task_prio = 12;
    return wav_decoder_init(&wav_cfg);
}

audio_element_handle_t create_mp3_dec()
{
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_dec_cfg.task_core = 1;
    mp3_dec_cfg.task_prio = 12;
    return mp3_decoder_init(&mp3_dec_cfg);
}

audio_element_handle_t create_aac_dec()
{
    aac_decoder_cfg_t cfg = DEFAULT_AAC_DECODER_CONFIG();
    cfg.task_core = 1;
    cfg.task_prio = 12;
    return aac_decoder_init(&cfg);
}

audio_element_handle_t create_amr_dec()
{
    amr_decoder_cfg_t cfg = DEFAULT_AMR_DECODER_CONFIG();
    cfg.task_core = 1;
    cfg.task_prio = 12;
    return amr_decoder_init(&cfg);
}

audio_element_handle_t create_amrnb_enc()
{
    amrnb_encoder_cfg_t cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return amrnb_encoder_init(&cfg);
}

audio_element_handle_t create_amrwb_enc()
{
    amrwb_encoder_cfg_t cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return amrwb_encoder_init(&cfg);
}

audio_element_handle_t create_opus_dec()
{
    opus_decoder_cfg_t cfg = DEFAULT_OPUS_DECODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return decoder_opus_init(&cfg);
}

audio_element_handle_t create_opus_enc()
{
    opus_encoder_cfg_t cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return encoder_opus_init(&cfg);
}

audio_element_handle_t create_flac_dec()
{
    flac_decoder_cfg_t cfg = DEFAULT_FLAC_DECODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return flac_decoder_init(&cfg);
}

audio_element_handle_t create_ogg_dec()
{
    ogg_decoder_cfg_t cfg = DEFAULT_OGG_DECODER_CONFIG();
    // cfg.task_core = 1;
    cfg.task_prio = 12;
    return ogg_decoder_init(&cfg);
}

audio_element_handle_t create_enc_filter(int source_rate, int source_channel, int dest_rate, int dest_channel)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    rsp_cfg.type = RESAMPLE_ENCODE_MODE;
    return rsp_filter_init(&rsp_cfg);
}

audio_element_handle_t create_dec_filter(int source_rate, int source_channel, int dest_rate, int dest_channel)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    rsp_cfg.type = RESAMPLE_DECODE_MODE;
    return rsp_filter_init(&rsp_cfg);
}

static int audio_forge_wr_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    audio_element_handle_t i2s_wr = (audio_element_handle_t)ctx;
    int ret = audio_element_output(i2s_wr, buf, len);
    return ret;
}

#define INPUT_SOURCE_NUM 1
audio_element_handle_t create_audio_forge(void)
{
    audio_forge_cfg_t audio_forge_cfg = AUDIO_FORGE_CFG_DEFAULT();
    audio_forge_cfg.audio_forge.component_select = AUDIO_FORGE_SELECT_RESAMPLE | AUDIO_FORGE_SELECT_SONIC;
    audio_forge_cfg.audio_forge.dest_samplerate = 48000;
    audio_forge_cfg.audio_forge.dest_channel = 2;
    audio_forge_cfg.audio_forge.sonic_pitch = 1.0;
    audio_forge_cfg.audio_forge.sonic_speed = 1.0;
    audio_forge_cfg.audio_forge.source_num = INPUT_SOURCE_NUM;
    audio_element_handle_t audio_forge = audio_forge_init(&audio_forge_cfg);
    audio_forge_src_info_t source_information = {
        .samplerate = 48000,
        .channel = 2,
    };

    audio_forge_downmix_t downmix_information[INPUT_SOURCE_NUM] = {
        {
            .gain = {0, -5},
            .transit_time = 0,
        },
        {
            .gain = { -5, 0},
            .transit_time = 0,
        }
    };
    audio_forge_src_info_t source_info[INPUT_SOURCE_NUM] = {NULL};
    audio_forge_downmix_t downmix_info[INPUT_SOURCE_NUM] = {0};
    for (int i = 0; i < INPUT_SOURCE_NUM; i++) {
        source_info[i] = source_information;
        downmix_info[i] = downmix_information[i];
    }
    audio_forge_source_info_init(audio_forge, source_info, downmix_info);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.task_stack = 0;
    i2s_cfg.out_rb_size = 0;
    audio_element_handle_t i2s_writer_el = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_writer_el, 48000, 16, 2);
    audio_element_set_write_cb(audio_forge, audio_forge_wr_cb, i2s_writer_el);
    audio_forge_downmix_set_input_rb_timeout(audio_forge, 0, 1);
    audio_element_set_input_timeout(audio_forge, 1);

    return audio_forge;
}

void audio_player_callback(audio_player_state_t *audio, void *ctx)
{
    if (xQueueSend((QueueHandle_t)ctx, audio, 5000 / portTICK_RATE_MS) != pdPASS) {
        ESP_LOGE(TAG, "AUDIO_PLAYER_CALLBACK send failure, status:%d, err_msg:%x, media_src:%x, ctx:%p",
                 audio->status, audio->err_msg, audio->media_src, ctx);
    } else {
        ESP_LOGE(TAG, "AUDIO_PLAYER_CALLBACK send OK, status:%d, err_msg:%x, media_src:%x, ctx:%p",
                 audio->status, audio->err_msg, audio->media_src, ctx);
    }
}
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

static void user_a2dp_sink_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    ESP_LOGI(TAG, "A2DP sink user cb");
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "A2DP disconnected");
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "A2DP connected");
            }
            break;
        default:
            ESP_LOGI(TAG, "User cb A2DP event: %d", event);
            break;
    }
}

esp_audio_handle_t ut_setup_esp_audio_instance(esp_audio_cfg_t *cfg)
{
    if (cfg == NULL) {
        esp_audio_cfg_t default_cfg = DEFAULT_ESP_AUDIO_CONFIG();
        default_cfg.vol_handle = NULL;
        default_cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
        default_cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
        default_cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
        default_cfg.evt_que = NULL;
        ESP_LOGI(TAG, "stack:%d", default_cfg.task_stack);
        cfg = &default_cfg;
    }
    esp_audio_handle_t handle = esp_audio_create(cfg);
    AUDIO_MEM_CHECK(TAG, handle, return NULL);
    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    fs_reader.task_prio = 12;

    tone_stream_cfg_t tn_reader = TONE_STREAM_CFG_DEFAULT();
    tn_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(handle, tone_stream_init(&tn_reader));
    esp_audio_input_stream_add(handle, fatfs_stream_init(&fs_reader));

    a2dp_stream_config_t a2dp_config = {
        .type = AUDIO_STREAM_READER,
        .user_callback.user_a2d_cb = user_a2dp_sink_cb,
    };
    audio_element_handle_t bt_stream_reader = a2dp_stream_init(&a2dp_config);
    esp_audio_input_stream_add(handle, bt_stream_reader);
    ap_helper_a2dp_handle_set(bt_stream_reader);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t esp_raw_handle = raw_stream_init(&raw_cfg);
    esp_audio_input_stream_add(handle, esp_raw_handle);
    // Set the raw handle to raw player helper
    ap_helper_raw_handle_set(esp_raw_handle);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    http_cfg.task_prio = 12;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);
    http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(handle, http_stream_reader);


#ifdef ESP_AUDIO_AUTO_PLAY
    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, esp_decoder_init(&auto_dec_cfg, auto_decode, 10)), ESP_ERR_AUDIO_NO_ERROR);
#else

    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_wav_dec()), ESP_ERR_AUDIO_NO_ERROR);
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_mp3_dec()), ESP_ERR_AUDIO_NO_ERROR);
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_aac_dec()), ESP_ERR_AUDIO_NO_ERROR);

    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    aac_cfg.task_core = 1;
    aac_cfg.task_prio = 12;
    audio_element_handle_t m4a_el = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(m4a_el, "m4a");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, m4a_el);

    audio_element_handle_t ts_el = aac_decoder_init(&aac_cfg);
    audio_element_set_tag(ts_el, "ts");
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, ts_el);

    pcm_decoder_cfg_t pcm_dec_cfg = DEFAULT_PCM_DECODER_CONFIG();
    esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, pcm_decoder_init(&pcm_dec_cfg));


    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_amr_dec()), ESP_ERR_AUDIO_NO_ERROR);
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_flac_dec()), ESP_ERR_AUDIO_NO_ERROR);
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_ogg_dec()), ESP_ERR_AUDIO_NO_ERROR);
    TEST_ASSERT_EQUAL(esp_audio_codec_lib_add(handle, AUDIO_CODEC_TYPE_DECODER, create_opus_dec()), ESP_ERR_AUDIO_NO_ERROR);
#endif

    // Create writers and add to esp_audio
    TEST_ASSERT_EQUAL(esp_audio_output_stream_add(handle, create_i2s_write_stream()), ESP_ERR_AUDIO_NO_ERROR);

    // Set default volume
    esp_audio_vol_set(handle, 60);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p", handle);
    esp_audio_helper_set_instance(handle);
    return handle;
}


#include <string.h>
#include "esp_log.h"
#include "esp_peripherals.h"
#include "audio_mem.h"
#include "a2dp_stream.h"
#include "ble_gatts_module.h"

#define BT_DEVICE_NAME  "ESP_BT_COEX_DEV"


esp_periph_handle_t a2dp_init()
{
    ESP_LOGI(TAG, "Init Bluetooth module");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ble_gatts_module_init();

    esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name(BT_DEVICE_NAME);
    if (set_dev_name_ret) {
        ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
    }
    esp_periph_handle_t bt_periph = bt_create_periph();

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    esp_periph_start(set, bt_periph);
    ESP_LOGI(TAG, "Start Bluetooth peripherals");

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
    esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif

    return bt_periph;
}

esp_err_t init_audio_player(QueueHandle_t que, esp_audio_prefer_t type)
{
    board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    AUDIO_MEM_SHOW(TAG);
    esp_audio_cfg_t default_cfg = DEFAULT_ESP_AUDIO_CONFIG();
    default_cfg.vol_handle = board_handle->audio_hal;
    default_cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    default_cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    default_cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    default_cfg.evt_que = NULL;

    // a2dp init before the ut_setup_esp_audio_instance
    esp_periph_handle_t bt_periph = a2dp_init();
    audio_player_cfg_t cfg = DEFAULT_AUDIO_PLAYER_CONFIG();
    cfg.external_ram_stack = false;
    cfg.task_prio = 6;
    cfg.task_stack = 6 * 1024;
    cfg.evt_cb = audio_player_callback;
    cfg.evt_ctx = que;
    cfg.music_list = NULL;
    cfg.handle = ut_setup_esp_audio_instance(&default_cfg);

    audio_err_t ret = audio_player_init(&cfg);
    ret |= default_http_player_init();

    ret |= default_sdcard_player_init("/sdcard/ut");

    // ret |= default_flash_music_player_init();

    ret |= default_flash_tone_player_init();

    ret |= default_a2dp_player_init(bt_periph);

    ret |= default_raw_player_init();
    return ESP_OK;
}

esp_err_t deinit_audio_player()
{
    audio_board_deinit(board_handle);
    board_handle = NULL;
    audio_player_deinit();
    return ESP_OK;
}

void clear_queue_events(QueueHandle_t que)
{
    audio_player_state_t tmp = {0};
    xQueueReceive(que, &tmp, 0);
}

audio_player_status_t audio_player_status_check(QueueHandle_t que, int ticks_ms)
{
    audio_player_state_t st = {0};
    if (xQueueReceive(que, &st, ticks_ms / portTICK_RATE_MS) != pdPASS) {
        return ESP_FAIL;
    }

    return st.status;
}

void show_task_list()
{
    static char buf[1024];
    vTaskList(buf);
    printf("\n\nTask List:\nTask Name    Status   Prio    HWM    Task Number\n%s\n", buf);
}
