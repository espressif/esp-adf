/* Play alternate loop

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#ifdef CONFIG_AUDIO_SUPPORT_MP3_DECODER
#include "mp3_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_WAV_DECODER
#include "wav_decoder.h"
#elif CONFIG_AUDIO_SUPPORT_AAC_DECODER
#include "aac_decoder.h"
#endif
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"

typedef enum {
    AUDIO_FIRST = 0,
    AUDIO_SECOND,
    AUDIO_NUM
} audio_index_t;

#define LOG_SPACE "       "
#define RINGBUF_INPUT_SIZE (1024 * 6)
#define AUDIO_INDEX_ITERATE(audio_index)  for (size_t audio_index = 0; audio_index < AUDIO_NUM; audio_index ++)

static const char *TAG = "LOOP_PLAYBACK";

void app_main(void)
{
    // Example of linking elements into an audio pipeline -- START
    audio_pipeline_handle_t pipeline_input[AUDIO_NUM];
    audio_element_handle_t fatfs_stream_reader[AUDIO_NUM], music_decoder[AUDIO_NUM], i2s_stream_writer;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    audio_hal_enable_pa(board_handle->audio_hal, false);

    ESP_LOGI(TAG, "[ 3 ] Create audio pipeline for playback");
    AUDIO_INDEX_ITERATE(audio_index) {
        audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
        pipeline_input[audio_index] = audio_pipeline_init(&pipeline_cfg);
        mem_assert(pipeline_input[audio_index]);

        ESP_LOGI(TAG, "%s- Create fatfs_stream_reader[%d] to read data from sdcard", LOG_SPACE, audio_index);
        fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
        fatfs_cfg.type = AUDIO_STREAM_READER;
        fatfs_stream_reader[audio_index] = fatfs_stream_init(&fatfs_cfg);

        ESP_LOGI(TAG, "%s- Create music_decoder[%d]", LOG_SPACE, audio_index);
#ifdef CONFIG_AUDIO_SUPPORT_MP3_DECODER
        mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
        music_decoder[audio_index] = mp3_decoder_init(&mp3_cfg);
        audio_element_set_uri(fatfs_stream_reader[audio_index], "/sdcard/test.mp3");
#elif CONFIG_AUDIO_SUPPORT_WAV_DECODER
        wav_decoder_cfg_t  wav_dec_cfg  = DEFAULT_WAV_DECODER_CONFIG();
        music_decoder[audio_index] = wav_decoder_init(&wav_dec_cfg);
        audio_element_set_uri(fatfs_stream_reader[audio_index], "/sdcard/test.wav");
#elif CONFIG_AUDIO_SUPPORT_AAC_DECODER
        aac_decoder_cfg_t  aac_dec_cfg  = DEFAULT_AAC_DECODER_CONFIG();
        music_decoder[audio_index] = aac_decoder_init(&aac_dec_cfg);
        audio_element_set_uri(fatfs_stream_reader[audio_index], "/sdcard/test.aac");
#endif

        audio_pipeline_register(pipeline_input[audio_index], fatfs_stream_reader[audio_index], "file");
        audio_pipeline_register(pipeline_input[audio_index], music_decoder[audio_index], "dec");
        audio_pipeline_link(pipeline_input[audio_index], (const char *[]) {"file", "dec"}, 2);
    }

    ESP_LOGI(TAG, "%s- Create i2s_stream_writer to write data to codec chip", LOG_SPACE);
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[ 4 ] Set up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "%s- Listening event from all elements of pipeline", LOG_SPACE);
    audio_pipeline_set_listener(pipeline_input[AUDIO_FIRST], evt);
    audio_pipeline_set_listener(pipeline_input[AUDIO_SECOND], evt);
    audio_element_msg_set_listener(i2s_stream_writer, evt);

    ESP_LOGI(TAG, "%s- Listening event from peripherals", LOG_SPACE);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Create the ringbuf and connect decoder to i2s");
    ringbuf_handle_t ringbuf = rb_create(RINGBUF_INPUT_SIZE, 1);
    audio_element_set_output_ringbuf(music_decoder[AUDIO_FIRST], ringbuf);
    audio_element_set_input_ringbuf(i2s_stream_writer, ringbuf);

    ESP_LOGI(TAG, "[ 6 ] Start pipeline_input and i2s_stream_writer");
    audio_pipeline_run(pipeline_input[AUDIO_FIRST]);
    audio_element_run(i2s_stream_writer);
    audio_element_resume(i2s_stream_writer, 0, 2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "[ 7 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            static audio_element_info_t prev_music_info = {0};
            AUDIO_INDEX_ITERATE(audio_index) {
                if (msg.source != (void *) music_decoder[audio_index]) {
                    continue;
                }
                audio_element_getinfo(music_decoder[audio_index], &music_info);
                if ((prev_music_info.bits != music_info.bits) || (prev_music_info.sample_rates != music_info.sample_rates)
                    || (prev_music_info.channels != music_info.channels)) {
                    ESP_LOGI(TAG, "[ * ] Receive music info from music_decoder[%d], sample_rates=%d, bits=%d, ch=%d", \
                            audio_index, music_info.sample_rates, music_info.bits, music_info.channels);
                    i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
                    memcpy(&prev_music_info, &music_info, sizeof(audio_element_info_t));
                    audio_hal_enable_pa(board_handle->audio_hal, true);
                }
                break;
            }
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.cmd == AEL_MSG_CMD_REPORT_STATUS \
            && (((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            AUDIO_INDEX_ITERATE(audio_index) {
                if (msg.source == (void *) music_decoder[audio_index]) {
                    ESP_LOGI(TAG, "[ * ] The Pipeline[%d] has been finished. Switch to another one", audio_index);
                    int next_index = audio_index;
                    if (audio_index == (AUDIO_NUM - 1)) {
                        next_index = 0;
                    } else {
                        next_index ++;
                    }
                    rb_reset_is_done_write(ringbuf);
                    audio_element_set_output_ringbuf(music_decoder[audio_index], NULL);
                    audio_element_set_output_ringbuf(music_decoder[next_index], ringbuf);
                    audio_pipeline_run(pipeline_input[next_index]);

                    audio_pipeline_stop(pipeline_input[audio_index]);
                    audio_pipeline_wait_for_stop(pipeline_input[audio_index]);
                    audio_pipeline_reset_ringbuffer(pipeline_input[audio_index]);
                    audio_pipeline_reset_elements(pipeline_input[audio_index]);
                    audio_pipeline_reset_items_state(pipeline_input[audio_index]);
                    break;
                }
            }
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    AUDIO_INDEX_ITERATE(audio_index) {
        audio_pipeline_stop(pipeline_input[audio_index]);
        audio_pipeline_wait_for_stop(pipeline_input[audio_index]);
        audio_pipeline_terminate(pipeline_input[audio_index]);
        audio_pipeline_unregister(pipeline_input[audio_index], fatfs_stream_reader[audio_index]);
        audio_pipeline_unregister(pipeline_input[audio_index], music_decoder[audio_index]);
        audio_pipeline_remove_listener(pipeline_input[audio_index]);
    }

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    AUDIO_INDEX_ITERATE(audio_index) {
        audio_pipeline_deinit(pipeline_input[audio_index]);
        audio_element_deinit(fatfs_stream_reader[audio_index]);
        audio_element_deinit(music_decoder[audio_index]);
    }
    audio_element_deinit(i2s_stream_writer);
    esp_periph_set_destroy(set);
}
