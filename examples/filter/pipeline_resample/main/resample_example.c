/*  Record and playback with re-sampling example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "wav_decoder.h"
#include "audio_hal.h"
#include "filter_resample.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_button.h"

static const char *TAG = "RESAMPLE_EXAMPLE";

#define RECORD_RATE         48000
#define RECORD_CHANNEL      2
#define RECORD_BITS         16

#define SAVE_FILE_RATE      16000
#define SAVE_FILE_CHANNEL   1
#define SAVE_FILE_BITS      16

#define PLAYBACK_RATE       48000
#define PLAYBACK_CHANNEL    2
#define PLAYBACK_BITS       16

static audio_element_handle_t create_filter(int source_rate, int source_channel, int dest_rate, int dest_channel, audio_codec_type_t type)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    rsp_cfg.type = type;
    return rsp_filter_init(&rsp_cfg);
}

static audio_element_handle_t create_fatfs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = type;
    audio_element_handle_t fatfs_stream = fatfs_stream_init(&fatfs_cfg);
    mem_assert(fatfs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(fatfs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(fatfs_stream, &writer_info);
    return fatfs_stream;
}

static audio_element_handle_t create_i2s_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = type;
    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    mem_assert(i2s_stream);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream, &i2s_info);
    i2s_info.bits = bits;
    i2s_info.channels = channels;
    i2s_info.sample_rates = sample_rates;
    audio_element_setinfo(i2s_stream, &i2s_info);
    return i2s_stream;
}

static audio_element_handle_t create_wav_encoder()
{
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    return wav_encoder_init(&wav_cfg);
}

static audio_element_handle_t create_wav_decoder()
{
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    return wav_decoder_init(&wav_cfg);
}

void record_playback_task()
{
    audio_pipeline_handle_t pipeline_rec = NULL;
    audio_pipeline_handle_t pipeline_play = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

    /**
     * For the Recorder:
     * We will setup I2S and get audio at sample rates 48000Hz, 16-bits, 2 channel.
     * And using resample-filter to convert to 16000Hz, 16-bits, 1 channel. Encode with Wav encoder
     * Then write to SDCARD
     */
    ESP_LOGI(TAG, "[1.1] Initialize recorder pipeline");
    pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    pipeline_play = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[1.2] Create audio elements for recorder pipeline");
    audio_element_handle_t i2s_reader_el = create_i2s_stream(RECORD_RATE, RECORD_BITS, RECORD_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t filter_downsample_el = create_filter(RECORD_RATE, RECORD_CHANNEL, SAVE_FILE_RATE, SAVE_FILE_CHANNEL, AUDIO_CODEC_TYPE_ENCODER);
    audio_element_handle_t wav_encoder_el = create_wav_encoder();
    audio_element_handle_t fatfs_writer_el = create_fatfs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[1.3] Register audio elements to recorder pipeline");
    audio_pipeline_register(pipeline_rec, i2s_reader_el,        "i2s_reader");
    audio_pipeline_register(pipeline_rec, filter_downsample_el, "filter_downsample");
    audio_pipeline_register(pipeline_rec, wav_encoder_el,       "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_writer_el,      "file_writer");

    /**
     * For the Playback:
     * We will read the recorded file, with sameple rates 16000hz, 16-bits, 1 channel,
     * Decode with WAV decoder
     * And using resample-filter to convert to 48000Hz, 16-bits, 2 channel.
     * Then send the audio to I2S
     */

    ESP_LOGI(TAG, "[2.2] Create audio elements for playback pipeline");
    audio_element_handle_t fatfs_reader_el = create_fatfs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t wav_decoder_el = create_wav_decoder();
    audio_element_handle_t filter_upsample_el = create_filter(SAVE_FILE_RATE, SAVE_FILE_CHANNEL, PLAYBACK_RATE, PLAYBACK_CHANNEL, AUDIO_CODEC_TYPE_DECODER);
    audio_element_handle_t i2s_writer_el = create_i2s_stream(PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[2.3] Register audio elements to playback pipeline");
    audio_pipeline_register(pipeline_play, fatfs_reader_el,      "file_reader");
    audio_pipeline_register(pipeline_play, wav_decoder_el,       "wav_decoder");
    audio_pipeline_register(pipeline_play, filter_upsample_el,   "filter_upsample");
    audio_pipeline_register(pipeline_play, i2s_writer_el,        "i2s_writer");

    ESP_LOGI(TAG, "[ 3 ] Setup event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_event_iface_set_listener(esp_periph_get_event_iface(), evt);
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type != PERIPH_ID_BUTTON) {
            continue;
        }
        if ((int)msg.data == GPIO_NUM_39) {
            ESP_LOGI(TAG, "STOP");
            break;
        }

        if (msg.cmd == PERIPH_BUTTON_PRESSED) {
            ESP_LOGE(TAG, "STOP Playback and START [Record]"); //using LOGE to make the log color different
            audio_pipeline_stop(pipeline_play);
            audio_pipeline_wait_for_stop(pipeline_play);

            /**
             * Audio Recording Flow:
             * [codec_chip]-->i2s_stream--->filter-->wav_encoder-->fatfs_stream-->[sdcard]
             */
            ESP_LOGI(TAG, "Link audio elements to make recorder pipeline ready");
            audio_pipeline_link(pipeline_rec, (const char *[]) {"i2s_reader", "filter_downsample", "wav_encoder", "file_writer"}, 4);

            ESP_LOGI(TAG, "Setup file path to save recorded audio");
            i2s_stream_set_clk(i2s_writer_el, RECORD_RATE, RECORD_BITS, RECORD_CHANNEL);
            audio_element_set_uri(fatfs_writer_el, "/sdcard/rec.wav");
            audio_pipeline_run(pipeline_rec);
        } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
            ESP_LOGI(TAG, "STOP [Record] and START Playback");
            audio_pipeline_stop(pipeline_rec);
            audio_pipeline_wait_for_stop(pipeline_rec);

            /**
             * Audio Playback Flow:
             * [sdcard]-->fatfs_stream-->wav_decoder-->filter-->i2s_stream-->[codec_chip]
             */
            ESP_LOGI(TAG, "Link audio elements to make playback pipeline ready");
            audio_pipeline_link(pipeline_play, (const char *[]) {"file_reader", "wav_decoder", "filter_upsample", "i2s_writer"}, 4);

            ESP_LOGI(TAG, "Setup file path to read the wav audio to play");
            i2s_stream_set_clk(i2s_writer_el, PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL);
            audio_element_set_uri(fatfs_reader_el, "/sdcard/rec.wav");
            audio_pipeline_run(pipeline_play);
        }
    }

    ESP_LOGI(TAG, "[ 4 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline_rec);
    audio_pipeline_terminate(pipeline_play);

    audio_pipeline_unregister(pipeline_play, fatfs_reader_el);
    audio_pipeline_unregister(pipeline_play, wav_decoder_el);
    audio_pipeline_unregister(pipeline_play, filter_upsample_el);
    audio_pipeline_unregister(pipeline_play, i2s_writer_el);

    audio_pipeline_unregister(pipeline_rec, i2s_reader_el);
    audio_pipeline_unregister(pipeline_rec, filter_downsample_el);
    audio_pipeline_unregister(pipeline_rec, wav_encoder_el);
    audio_pipeline_unregister(pipeline_rec, fatfs_writer_el);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_rec);
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all periph before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_element_deinit(fatfs_reader_el);
    audio_element_deinit(wav_decoder_el);
    audio_element_deinit(filter_upsample_el);
    audio_element_deinit(i2s_writer_el);

    audio_element_deinit(i2s_reader_el);
    audio_element_deinit(filter_downsample_el);
    audio_element_deinit(wav_encoder_el);
    audio_element_deinit(fatfs_writer_el);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    // Initialize SD Card peripheral
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO,   //GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);

    // Initialize Button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_36 | GPIO_SEL_39, //REC BTN & MODE BTN
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    // Start sdcard & button peripheral
    esp_periph_start(sdcard_handle);
    esp_periph_start(button_handle);

    // Wait until sdcard was mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // Setup audio codec
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_handle_t hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
    audio_hal_ctrl_codec(hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    // Start record/playback task
    record_playback_task();
    esp_periph_destroy();
}
