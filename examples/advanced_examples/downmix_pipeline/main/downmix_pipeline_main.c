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
#include "wav_decoder.h"
#include "fatfs_stream.h"
#include "downmix.h"
#include "raw_stream.h"
#include "board.h"
#include "periph_sdcard.h"
#include "periph_button.h"
static const char *TAG = "DOWNMIX_PIPELINE";

#define SAMPLERATE 48000
#define DEFAULT_CHANNEL 1
#define TRANSMITTIME 10
#define MUSIC_GAIN_DB 0
#define PLAY_STATUS ESP_DOWNMIX_OUTPUT_TYPE_ONE_CHANNEL
#define NUMBER_SOURCE_FILE 2

void app_main(void)
{
    audio_pipeline_handle_t pipeline[NUMBER_SOURCE_FILE] = {NULL};
    audio_element_handle_t fats_rd_el[NUMBER_SOURCE_FILE] = {NULL}, wav_decoder[NUMBER_SOURCE_FILE] = {NULL}, el_raw_write[NUMBER_SOURCE_FILE] = {NULL};

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Start audio codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[2.0] Start and wait for SDCARD to mount");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[3.0] Create pipeline_mix to mix");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_mix = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create downmixer");
    downmix_cfg_t downmix_cfg = DEFAULT_DOWNMIX_CONFIG();
    downmix_cfg.downmix_info.source_num = NUMBER_SOURCE_FILE;
    audio_element_handle_t downmixer = downmix_init(&downmix_cfg);
    esp_downmix_input_info_t source_info = {
        .samplerate = SAMPLERATE,
        .channel = DEFAULT_CHANNEL,
        .gain = {0, MUSIC_GAIN_DB},
        .transit_time = TRANSMITTIME,
    };
    esp_downmix_input_info_t source_information[NUMBER_SOURCE_FILE] = {0};
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        source_information[i] = source_info;
    }
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

    ESP_LOGI(TAG, "[4.1] Create wav decoder to decode wav file");
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_cfg.task_core = 1;

    ESP_LOGI(TAG, "[4.2] Create raw stream of base wav to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;

    ESP_LOGI(TAG, "[5.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    char str_name[18] = "/sdcard/test";
    char num = '1';
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        pipeline[i] = audio_pipeline_init(&pipeline_cfg);
        mem_assert(pipeline[i]);
        fats_rd_el[i] = fatfs_stream_init(&fatfs_cfg);
        str_name[12] = num + i;
        str_name[13] = '.';
        str_name[14] = 'w';
        str_name[15] = 'a';
        str_name[16] = 'v';
        audio_element_set_uri(fats_rd_el[i], str_name);
        wav_decoder[i] = wav_decoder_init(&wav_cfg);
        el_raw_write[i] = raw_stream_init(&raw_cfg);
        audio_pipeline_register(pipeline[i], fats_rd_el[i], "file");
        audio_pipeline_register(pipeline[i], wav_decoder[i], "wav");
        audio_pipeline_register(pipeline[i], el_raw_write[i], "raw");
        
        const char *link_tag[3] = {"file", "wav", "raw"};
        audio_pipeline_link(pipeline[i], &link_tag[0], 3);
        ringbuf_handle_t rb = audio_element_get_input_ringbuf(el_raw_write[i]);
        downmix_set_input_rb(downmixer, rb, i);
        audio_pipeline_set_listener(pipeline[i], evt);
    }

    ESP_LOGI(TAG, "[5.1] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    downmix_set_output_type(downmixer, PLAY_STATUS);
    i2s_stream_set_clk(i2s_writer, SAMPLERATE, 16, PLAY_STATUS);
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
            if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)wav_decoder[i]
                && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
                audio_element_info_t music_info = {0};
                audio_element_getinfo(wav_decoder[i], &music_info);
                ESP_LOGW(TAG, "[ * ] Receive music info from wav decoder, sample_rates=%d, bits=%d, ch=%d",
                         music_info.sample_rates, music_info.bits, music_info.channels);
                downmix_set_source_stream_info(downmixer, music_info.sample_rates, music_info.channels, i);
            }
        }
        if (((int)msg.data == get_input_mode_id()) && (msg.cmd == PERIPH_BUTTON_PRESSED)) {
            ESP_LOGE(TAG, "Open the downmix and enter downmixer mode");
            for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
                audio_pipeline_run(pipeline[i]);
            }
            audio_pipeline_run(pipeline_mix);
            downmix_set_work_mode(downmixer, ESP_DOWNMIX_WORK_MODE_SWITCH_ON);
        }
        /* Stop when the last pipeline element (fatfs_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (((int)msg.data == AEL_STATUS_STATE_STOPPED)
                    || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            break;
        }
    }

    ESP_LOGI(TAG, "[6.0] Stop pipelines");
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        audio_pipeline_stop(pipeline[i]);
        audio_pipeline_wait_for_stop(pipeline[i]);
        audio_pipeline_terminate(pipeline[i]);
        audio_pipeline_unregister_more(pipeline[i], fats_rd_el[i], wav_decoder[i], el_raw_write[i], NULL);
        audio_pipeline_remove_listener(pipeline[i]);
        /* Release resources */
        audio_pipeline_deinit(pipeline[i]);
        audio_element_deinit(fats_rd_el[i]);
        audio_element_deinit(wav_decoder[i]);
        audio_element_deinit(el_raw_write[i]);
    }
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
