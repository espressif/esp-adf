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
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "spiffs_stream.h"
#include "i2s_stream.h"
#include "amrnb_encoder.h"
#include "amr_decoder.h"
#include "audio_hal.h"
#include "filter_resample.h"
#include "esp_peripherals.h"
#include "periph_button.h"
#include "periph_spiffs.h"

static const char *TAG = "SPIFFS_AMR_RESAMPLE_EXAMPLE";

#define RECORD_RATE         48000
#define RECORD_CHANNEL      2
#define RECORD_BITS         16

#define SAVE_FILE_RATE      8000
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

static audio_element_handle_t create_spiffs_stream(int sample_rates, int bits, int channels, audio_stream_type_t type)
{
    spiffs_stream_cfg_t spiffs_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    spiffs_cfg.type = type;
    audio_element_handle_t spiffs_stream = spiffs_stream_init(&spiffs_cfg);
    mem_assert(spiffs_stream);
    audio_element_info_t writer_info = {0};
    audio_element_getinfo(spiffs_stream, &writer_info);
    writer_info.bits = bits;
    writer_info.channels = channels;
    writer_info.sample_rates = sample_rates;
    audio_element_setinfo(spiffs_stream, &writer_info);
    return spiffs_stream;
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

static audio_element_handle_t create_amrnb_encoder()
{
    amrnb_encoder_cfg_t amrnb_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    return amrnb_encoder_init(&amrnb_cfg);
}

static audio_element_handle_t create_amr_decoder()
{
    amr_decoder_cfg_t amr_cfg = DEFAULT_AMR_DECODER_CONFIG();
    return amr_decoder_init(&amr_cfg);
}

void record_playback_task()
{
    audio_pipeline_handle_t pipeline_rec = NULL;
    audio_pipeline_handle_t pipeline_play = NULL;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

    /**
     * For the Recorder:
     * We will setup I2S and get audio at sample rates 48000Hz, 16-bits, 2 channel.
     * And using resample-filter to convert to 8000Hz, 16-bits, 1 channel. Encode with amrnb encoder
     * Then write to Spiffs
     */
    ESP_LOGI(TAG, "[1.1] Initialize recorder pipeline");
    pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    pipeline_play = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[1.2] Create audio elements for recorder pipeline");
    audio_element_handle_t i2s_reader_el = create_i2s_stream(RECORD_RATE, RECORD_BITS, RECORD_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t filter_downsample_el = create_filter(RECORD_RATE, RECORD_CHANNEL, SAVE_FILE_RATE, SAVE_FILE_CHANNEL, AUDIO_CODEC_TYPE_ENCODER);
    audio_element_handle_t amrnb_encoder_el = create_amrnb_encoder();
    audio_element_handle_t spiffs_writer_el = create_spiffs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[1.3] Register audio elements to recorder pipeline");
    audio_pipeline_register(pipeline_rec, i2s_reader_el,        "i2s_reader");
    audio_pipeline_register(pipeline_rec, filter_downsample_el, "filter_downsample");
    audio_pipeline_register(pipeline_rec, amrnb_encoder_el,     "amrnb_encoder");
    audio_pipeline_register(pipeline_rec, spiffs_writer_el,     "file_writer");

    /**
     * For the Playback:
     * We will read the recorded file, with sameple rate 8000hz, 16-bits, 1 channel,
     * Decode with AMR decoder
     * And using resample-filter to convert to 48000Hz, 16-bits, 2 channel.
     * Then send the audio to I2S
     */
    ESP_LOGI(TAG, "[2.2] Create audio elements for playback pipeline");
    audio_element_handle_t spiffs_reader_el = create_spiffs_stream(SAVE_FILE_RATE, SAVE_FILE_BITS, SAVE_FILE_CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t amr_decoder_el = create_amr_decoder();
    audio_element_handle_t filter_upsample_el = create_filter(SAVE_FILE_RATE, SAVE_FILE_CHANNEL, PLAYBACK_RATE, PLAYBACK_CHANNEL, AUDIO_CODEC_TYPE_DECODER);
    audio_element_handle_t i2s_writer_el = create_i2s_stream(PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[2.3] Register audio elements to playback pipeline");
    audio_pipeline_register(pipeline_play, spiffs_reader_el,     "file_reader");
    audio_pipeline_register(pipeline_play, amr_decoder_el,       "amr_decoder");
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
        if ((int)msg.data == GPIO_MODE) {
            ESP_LOGI(TAG, "STOP");
            break;
        }

        if (msg.cmd == PERIPH_BUTTON_PRESSED) {
            ESP_LOGE(TAG, "STOP playback and START recording");
            audio_pipeline_stop(pipeline_play);
            audio_pipeline_wait_for_stop(pipeline_play);

            /**
             * Audio Recording Flow:
             * [codec_chip]-->i2s_stream--->filter-->amrnb_encoder-->spiffs_stream-->[flash]
             */
            ESP_LOGI(TAG, "Link audio elements to make recorder pipeline ready");
            audio_pipeline_link(pipeline_rec, (const char *[]) {"i2s_reader", "filter_downsample", "amrnb_encoder", "file_writer"}, 4);

            ESP_LOGI(TAG, "Setup file path to save recorded audio");
            i2s_stream_set_clk(i2s_writer_el, RECORD_RATE, RECORD_BITS, RECORD_CHANNEL);
            audio_element_set_uri(spiffs_writer_el, "/spiffs/rec.amr");
            audio_pipeline_run(pipeline_rec);
        } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
            ESP_LOGI(TAG, "STOP recording and START playback");
            audio_pipeline_stop(pipeline_rec);
            audio_pipeline_wait_for_stop(pipeline_rec);

            /**
             * Audio Playback Flow:
             * [flash]-->spiffs_stream-->amr_decoder-->filter-->i2s_stream-->[codec_chip]
             */
            ESP_LOGI(TAG, "Link audio elements to make playback pipeline ready");
            audio_pipeline_link(pipeline_play, (const char *[]) {"file_reader", "amr_decoder", "filter_upsample", "i2s_writer"}, 4);

            ESP_LOGI(TAG, "Setup file path to read the amr audio to play");
            i2s_stream_set_clk(i2s_writer_el, PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL);
            audio_element_set_uri(spiffs_reader_el, "/spiffs/rec.amr");
            audio_pipeline_run(pipeline_play);
        }
    }

    ESP_LOGI(TAG, "[ 4 ] Stop audio_pipeline");
    audio_pipeline_terminate(pipeline_rec);
    audio_pipeline_terminate(pipeline_play);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_rec);
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all periph before removing the listener */
    esp_periph_stop_all();
    audio_event_iface_remove_listener(esp_periph_get_event_iface(), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_unregister(pipeline_play, spiffs_reader_el);
    audio_pipeline_unregister(pipeline_play, amr_decoder_el);
    audio_pipeline_unregister(pipeline_play, filter_upsample_el);
    audio_pipeline_unregister(pipeline_play, i2s_writer_el);
    audio_element_deinit(spiffs_reader_el);
    audio_element_deinit(amr_decoder_el);
    audio_element_deinit(filter_upsample_el);
    audio_element_deinit(i2s_writer_el);

    audio_pipeline_unregister(pipeline_rec, i2s_reader_el);
    audio_pipeline_unregister(pipeline_rec, filter_downsample_el);
    audio_pipeline_unregister(pipeline_rec, amrnb_encoder_el);
    audio_pipeline_unregister(pipeline_rec, spiffs_writer_el);
    audio_element_deinit(i2s_reader_el);
    audio_element_deinit(filter_downsample_el);
    audio_element_deinit(amrnb_encoder_el);
    audio_element_deinit(spiffs_writer_el);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("PERIPH_SPIFFS", ESP_LOG_INFO);

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    // Initialize Spiffs peripheral
    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);

    // Initialize Button peripheral
    periph_button_cfg_t btn_cfg = {
        .gpio_mask = GPIO_SEL_REC | GPIO_SEL_MODE, //REC BTN & MODE BTN
    };
    esp_periph_handle_t button_handle = periph_button_init(&btn_cfg);

    // Start spiffs & button peripheral
    esp_periph_start(spiffs_handle);
    esp_periph_start(button_handle);

    // Wait until spiffs was mounted
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
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
