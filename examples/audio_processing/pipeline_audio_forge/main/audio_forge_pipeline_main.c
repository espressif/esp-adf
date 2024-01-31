/* Multiple pipeline playback with audio processing.

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
#include "audio_forge.h"
#include "raw_stream.h"
#include "board.h"
#include "periph_button.h"

static const char *TAG = "AUDIO_FORGE_PIPELINE";

#define DEFAULT_SAMPLERATE 48000
#define DEFAULT_CHANNEL 1
#define DEST_SAMPLERATE 11025
#define DEST_CHANNEL 1
#define TRANSMITTIME 0
#define MUSIC_GAIN_DB 0
#define NUMBER_SOURCE_FILE 2

int audio_forge_wr_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    audio_element_handle_t i2s_wr = (audio_element_handle_t)ctx;
    int ret = audio_element_output(i2s_wr, buf, len);
    return ret;
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline[NUMBER_SOURCE_FILE] = {NULL};
    audio_element_handle_t fats_rd_el[NUMBER_SOURCE_FILE] = {NULL};
    audio_element_handle_t wav_decoder[NUMBER_SOURCE_FILE] = {NULL};
    audio_element_handle_t el_raw_write[NUMBER_SOURCE_FILE] = {NULL};

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

    ESP_LOGI(TAG, "[3.1] Create audio_forge");
    audio_forge_cfg_t audio_forge_cfg = AUDIO_FORGE_CFG_DEFAULT();
    audio_forge_cfg.audio_forge.component_select = AUDIO_FORGE_SELECT_RESAMPLE | AUDIO_FORGE_SELECT_DOWNMIX | AUDIO_FORGE_SELECT_ALC | AUDIO_FORGE_SELECT_EQUALIZER | AUDIO_FORGE_SELECT_SONIC;
    audio_forge_cfg.audio_forge.dest_samplerate = DEST_SAMPLERATE;
    audio_forge_cfg.audio_forge.dest_channel = DEST_CHANNEL;
    audio_forge_cfg.audio_forge.source_num = NUMBER_SOURCE_FILE;
    audio_element_handle_t audio_forge = audio_forge_init(&audio_forge_cfg);
    audio_forge_src_info_t source_information = {
        .samplerate = DEFAULT_SAMPLERATE,
        .channel = DEFAULT_CHANNEL,
        .bit_num = 16,
    };

    audio_forge_downmix_t downmix_information = {
        .gain = {0, MUSIC_GAIN_DB},
        .transit_time = TRANSMITTIME,
    };
    audio_forge_src_info_t source_info[NUMBER_SOURCE_FILE] = {0};
    audio_forge_downmix_t downmix_info[NUMBER_SOURCE_FILE];
    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        source_info[i] = source_information;
        downmix_info[i] = downmix_information;
    }
    audio_forge_source_info_init(audio_forge, source_info, downmix_info);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.task_stack = 0;
    i2s_cfg.out_rb_size = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_cfg.chan_cfg.auto_clear = true;
#else
    i2s_cfg.i2s_config.tx_desc_auto_clear = true;
#endif
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_writer, DEST_SAMPLERATE, 16, DEST_CHANNEL);

    ESP_LOGI(TAG, "[3.3] Link elements together audio_forge-->i2s_writer");
    audio_pipeline_register(pipeline_mix, audio_forge, "audio_forge");
    audio_element_set_write_cb(audio_forge, audio_forge_wr_cb, i2s_writer);
    audio_element_process_init(i2s_writer);


    ESP_LOGI(TAG, "[3.4] Link elements together audio_forge-->i2s_stream-->[codec_chip]");
    audio_pipeline_link(pipeline_mix, (const char *[]) {"audio_forge"}, 1);

    ESP_LOGI(TAG, "[4.0] Create Fatfs stream to read input data");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;

    ESP_LOGI(TAG, "[4.1] Create wav decoder to decode wav file");
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_cfg.task_core = 0;

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
        if (NUMBER_SOURCE_FILE != 1) {
            audio_element_set_multi_input_ringbuf(audio_forge, rb, i);
        } else {
            audio_element_set_input_ringbuf(audio_forge, rb);
        }
        audio_pipeline_set_listener(pipeline[i], evt);
    }
    audio_pipeline_set_listener(pipeline_mix, evt);
    ESP_LOGI(TAG, "[5.1] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

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
                audio_forge_src_info_t src_info = {
                    .samplerate = music_info.sample_rates,
                    .channel = music_info.channels,
                    .bit_num = music_info.bits,
                };
                audio_forge_set_src_info(audio_forge, src_info, i);
            }
        }
        if (((int)msg.data == get_input_mode_id()) && (msg.cmd == PERIPH_BUTTON_PRESSED)) {
            ESP_LOGE(TAG, "audio_forge start");
            for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
                ret = audio_pipeline_run(pipeline[i]);
            }
            audio_pipeline_run(pipeline_mix);
        }
        /* Stop when the last pipeline element (fatfs_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT
            && msg.source == (void *) audio_forge
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED)
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
    }
    audio_pipeline_stop(pipeline_mix);
    audio_pipeline_wait_for_stop(pipeline_mix);
    audio_pipeline_terminate(pipeline_mix);
    audio_pipeline_unregister_more(pipeline_mix, audio_forge, NULL);
    audio_pipeline_remove_listener(pipeline_mix);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    for (int i = 0; i < NUMBER_SOURCE_FILE; i++) {
        /* Release resources */
        audio_pipeline_deinit(pipeline[i]);
        audio_element_deinit(fats_rd_el[i]);
        audio_element_deinit(wav_decoder[i]);
        audio_element_deinit(el_raw_write[i]);
    }
    /* Release resources */
    audio_pipeline_deinit(pipeline_mix);
    audio_element_deinit(audio_forge);
    audio_element_deinit(i2s_writer);
    esp_periph_set_destroy(set);
}
