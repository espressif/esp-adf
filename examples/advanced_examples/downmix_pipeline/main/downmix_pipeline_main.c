/* Multiple pipelines playback with downmix.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "mp3_decoder.h"
#include "fatfs_stream.h"
#include "downmix.h"
#include "filter_resample.h"
#include "raw_stream.h"
#include "board.h"
#include "periph_sdcard.h"
#include "periph_button.h"

static const char *TAG = "DOWNMIX_PIPELINE_EXAMPLE";

#define INDEX_BASE_STREAM 0
#define INDEX_NEWCOME_STREAM 1
#define SAMPLERATE 48000
#define NUM_INPUT_CHANNEL 1
#define TRANSMITTIME 500
#define MUSIC_GAIN_DB 0
#define PLAY_STATUS ESP_DOWNMIX_OUTPUT_TYPE_ONE_CHANNEL
#define NUMBER_SOURCE_FILE 2

void app_main(void)
{
    audio_element_handle_t base_fatfs_reader_el = NULL;
    audio_element_handle_t base_mp3_decoder_el = NULL;
    audio_element_handle_t base_rsp_filter_el = NULL;
    audio_element_handle_t base_raw_write_el = NULL;

    audio_element_handle_t newcome_fatfs_reader_el = NULL;
    audio_element_handle_t newcome_mp3_decoder_el = NULL;
    audio_element_handle_t newcome_rsp_filter_el = NULL;
    audio_element_handle_t newcome_raw_write_el = NULL;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2.0] Start and wait for SDCARD to mount");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[3.0] Create pipeline_mix pipeline");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_mix = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create down-mixer element");
    downmix_cfg_t downmix_cfg = DEFAULT_DOWNMIX_CONFIG();
    downmix_cfg.downmix_info.source_num = NUMBER_SOURCE_FILE;
    audio_element_handle_t downmixer = downmix_init(&downmix_cfg);
    downmix_set_input_rb_timeout(downmixer, 0, INDEX_BASE_STREAM);
    downmix_set_input_rb_timeout(downmixer, 0, INDEX_NEWCOME_STREAM);

    esp_downmix_input_info_t source_information[NUMBER_SOURCE_FILE] = {0};
    esp_downmix_input_info_t source_info_base = {
        .samplerate = SAMPLERATE,
        .channel = NUM_INPUT_CHANNEL,
        .bits_num = 16,
        /* base music depress form 0dB to -10dB */
        .gain = {0, -10},
        .transit_time = TRANSMITTIME,
    };
    source_information[0] = source_info_base;

    esp_downmix_input_info_t source_info_newcome = {
        .samplerate = SAMPLERATE,
        .channel = NUM_INPUT_CHANNEL,
        .bits_num = 16,
        /* newcome music rise form -10dB to 0dB */
        .gain = {-10, 0},
        .transit_time = TRANSMITTIME,
    };
    source_information[1] = source_info_newcome;
    source_info_init(downmixer, source_information);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Link elements together downmixer-->i2s_writer");
    audio_pipeline_register(pipeline_mix, downmixer, "mixer");
    audio_pipeline_register(pipeline_mix, i2s_writer, "i2s");

    ESP_LOGI(TAG, "[3.4] Link elements together downmixer-->i2s_stream-->[codec_chip]");
    const char *link_mix[2] = {"mixer", "i2s"};
    audio_pipeline_link(pipeline_mix, &link_mix[0], 2);

    ESP_LOGI(TAG, "[4.0] Create Fatfs stream to read input data");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    base_fatfs_reader_el = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(base_fatfs_reader_el, "/sdcard/music.mp3");
    newcome_fatfs_reader_el = fatfs_stream_init(&fatfs_cfg);
    audio_element_set_uri(newcome_fatfs_reader_el, "/sdcard/tone.mp3");

    ESP_LOGI(TAG, "[4.1] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    base_mp3_decoder_el = mp3_decoder_init(&mp3_cfg);
    newcome_mp3_decoder_el = mp3_decoder_init(&mp3_cfg);

    ESP_LOGI(TAG, "[4.1] Create resample element");
    rsp_filter_cfg_t rsp_sdcard_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_sdcard_cfg.src_rate = 44100,
    rsp_sdcard_cfg.src_ch = 2,
    rsp_sdcard_cfg.dest_rate = 48000,
    rsp_sdcard_cfg.dest_ch = 1,
    base_rsp_filter_el = rsp_filter_init(&rsp_sdcard_cfg);

    rsp_sdcard_cfg.src_rate = 16000,
    rsp_sdcard_cfg.src_ch = 1,
    rsp_sdcard_cfg.dest_rate = 48000,
    rsp_sdcard_cfg.dest_ch = 1,
    newcome_rsp_filter_el = rsp_filter_init(&rsp_sdcard_cfg);

    ESP_LOGI(TAG, "[4.2] Create raw stream of base mp3 to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    base_raw_write_el = raw_stream_init(&raw_cfg);
    newcome_raw_write_el = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[5.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_handle_t base_stream_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(base_stream_pipeline);
    audio_pipeline_register(base_stream_pipeline, base_fatfs_reader_el, "base_file");
    audio_pipeline_register(base_stream_pipeline, base_mp3_decoder_el, "base_mp3");
    audio_pipeline_register(base_stream_pipeline, base_rsp_filter_el, "base_filter");
    audio_pipeline_register(base_stream_pipeline, base_raw_write_el, "base_raw");

    const char *link_tag_base[4] = {"base_file", "base_mp3", "base_filter", "base_raw"};
    audio_pipeline_link(base_stream_pipeline, &link_tag_base[0], 4);
    ringbuf_handle_t rb_base = audio_element_get_input_ringbuf(base_raw_write_el);
    downmix_set_input_rb(downmixer, rb_base, 0);
    audio_pipeline_set_listener(base_stream_pipeline, evt);

    audio_pipeline_handle_t newcome_stream_pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(newcome_stream_pipeline);
    audio_pipeline_register(newcome_stream_pipeline, newcome_fatfs_reader_el, "newcome_file");
    audio_pipeline_register(newcome_stream_pipeline, newcome_mp3_decoder_el, "newcome_mp3");
    audio_pipeline_register(newcome_stream_pipeline, newcome_rsp_filter_el, "newcome_filter");
    audio_pipeline_register(newcome_stream_pipeline, newcome_raw_write_el, "newcome_raw");

    const char *link_tag_newcome[4] = {"newcome_file", "newcome_mp3", "newcome_filter", "newcome_raw"};
    audio_pipeline_link(newcome_stream_pipeline, &link_tag_newcome[0], 4);
    ringbuf_handle_t rb_newcome = audio_element_get_input_ringbuf(newcome_raw_write_el);
    downmix_set_input_rb(downmixer, rb_newcome, 1);
    audio_pipeline_set_listener(newcome_stream_pipeline, evt);

    ESP_LOGI(TAG, "[5.1] Listening event from peripherals");
    audio_pipeline_set_listener(pipeline_mix, evt);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    downmix_set_output_type(downmixer, PLAY_STATUS);
    i2s_stream_set_clk(i2s_writer, SAMPLERATE, 16, PLAY_STATUS);

    audio_pipeline_run(base_stream_pipeline);
    audio_pipeline_run(pipeline_mix);
    downmix_set_work_mode(downmixer, ESP_DOWNMIX_WORK_MODE_BYPASS);
    ESP_LOGI(TAG, "[6.0] Base stream pipeline running");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* When the mode button pressed, the downmix pipeline switch on */
        if (((int)msg.data == get_input_mode_id()) && (msg.cmd == PERIPH_BUTTON_PRESSED)) {
            audio_pipeline_run(newcome_stream_pipeline);
            downmix_set_work_mode(downmixer, ESP_DOWNMIX_WORK_MODE_SWITCH_ON);
            downmix_set_input_rb_timeout(downmixer, 50, INDEX_BASE_STREAM);
            downmix_set_input_rb_timeout(downmixer, 50, INDEX_NEWCOME_STREAM);
            ESP_LOGI(TAG, "New come music running...");
        }

        /* Stop when the last pipeline element i2s_writer receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED)
                    || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "Player finsihed, break loop");
            break;
        }

        /* Stop when the last pipeline element receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)newcome_rsp_filter_el
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED)
                    || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            downmix_set_work_mode(downmixer, ESP_DOWNMIX_WORK_MODE_SWITCH_OFF);
            downmix_set_input_rb_timeout(downmixer, 0, INDEX_BASE_STREAM);
            downmix_set_input_rb_timeout(downmixer, 0, INDEX_NEWCOME_STREAM);
            audio_pipeline_stop(newcome_stream_pipeline);
            audio_pipeline_wait_for_stop(newcome_stream_pipeline);
            audio_pipeline_terminate(newcome_stream_pipeline);
            audio_pipeline_reset_ringbuffer(newcome_stream_pipeline);
            audio_pipeline_reset_elements(newcome_stream_pipeline);
            ESP_LOGI(TAG, "New come music stoped or finsihed");
        }
    }

    ESP_LOGI(TAG, "[7.0] Stop all pipelines");
    /* Stop base stream pipeline, Release resources */
    audio_pipeline_stop(base_stream_pipeline);
    audio_pipeline_wait_for_stop(base_stream_pipeline);
    audio_pipeline_terminate(base_stream_pipeline);
    audio_pipeline_unregister_more(base_stream_pipeline, base_fatfs_reader_el,
                                    base_mp3_decoder_el, base_rsp_filter_el, base_raw_write_el, NULL);
    audio_pipeline_remove_listener(base_stream_pipeline);
    audio_pipeline_deinit(base_stream_pipeline);
    audio_element_deinit(base_fatfs_reader_el);
    audio_element_deinit(base_mp3_decoder_el);
    audio_element_deinit(base_rsp_filter_el);
    audio_element_deinit(base_raw_write_el);

    /* Stop newcome stream pipeline, Release resources */
    audio_pipeline_stop(newcome_stream_pipeline);
    audio_pipeline_wait_for_stop(newcome_stream_pipeline);
    audio_pipeline_terminate(newcome_stream_pipeline);
    audio_pipeline_unregister_more(newcome_stream_pipeline, newcome_fatfs_reader_el,
                                    newcome_mp3_decoder_el, newcome_rsp_filter_el, newcome_raw_write_el, NULL);
    audio_pipeline_remove_listener(newcome_stream_pipeline);
    audio_pipeline_deinit(newcome_stream_pipeline);
    audio_element_deinit(newcome_fatfs_reader_el);
    audio_element_deinit(newcome_mp3_decoder_el);
    audio_element_deinit(newcome_rsp_filter_el);
    audio_element_deinit(newcome_raw_write_el);

    /* Stop mixer stream pipeline, Release resources */
    audio_pipeline_stop(pipeline_mix);
    audio_pipeline_wait_for_stop(pipeline_mix);
    audio_pipeline_terminate(pipeline_mix);
    audio_pipeline_unregister_more(pipeline_mix, downmixer, i2s_writer, NULL);
    audio_pipeline_remove_listener(pipeline_mix);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release resources */
    audio_pipeline_deinit(pipeline_mix);
    audio_element_deinit(downmixer);
    audio_element_deinit(i2s_writer);
    esp_periph_set_destroy(set);
}
