/* Algorithm Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "board.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "algorithm_stream.h"

static const char *TAG = "ALGORITHM_EXAMPLES";

/* Debug original input data for AEC feature*/
// #define DEBUG_ALGO_INPUT

#define I2S_SAMPLE_RATE     16000
#define I2S_CHANNELS        2
#define I2S_BITS            16

/* The AEC internal buffering mechanism requires that the recording signal
   is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal. */
#define DEFAULT_REF_DELAY_MS    35
#define DEFAULT_REC_DELAY_MS    0

extern const uint8_t adf_music_mp3_start[] asm("_binary_test_mp3_start");
extern const uint8_t adf_music_mp3_end[]   asm("_binary_test_mp3_end");

int mp3_music_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    static int mp3_pos;
    int read_size = adf_music_mp3_end - adf_music_mp3_start - mp3_pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    memcpy(buf, adf_music_mp3_start + mp3_pos, read_size);
    mp3_pos += read_size;
    return read_size;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[2.0] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 60);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline_rec for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_rec);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_rd_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_rd_cfg.type = AUDIO_STREAM_READER;
    i2s_rd_cfg.i2s_config.sample_rate = I2S_SAMPLE_RATE;
#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_rd_cfg.i2s_port = 1;
#endif
    i2s_rd_cfg.task_core = 1;
    audio_element_handle_t i2s_stream_reader = i2s_stream_init(&i2s_rd_cfg);

    rsp_filter_cfg_t rsp_cfg_r = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg_r.src_rate = I2S_SAMPLE_RATE;
    rsp_cfg_r.src_ch = I2S_CHANNELS;
    rsp_cfg_r.dest_rate = I2S_SAMPLE_RATE;
#ifndef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    rsp_cfg_r.dest_ch = 1;
#endif
    rsp_cfg_r.complexity = 5;
    rsp_cfg_r.task_core = 1;
    rsp_cfg_r.out_rb_size = 10 * 1024;
    audio_element_handle_t filter_r = rsp_filter_init(&rsp_cfg_r);

    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
#ifdef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE1;
#else
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
#endif
    algo_config.task_core = 1;
#ifdef DEBUG_ALGO_INPUT
    algo_config.debug_input = true;
#endif
    audio_element_handle_t element_algo = algo_stream_init(&algo_config);
    audio_element_set_music_info(element_algo, I2S_SAMPLE_RATE, 1, I2S_BITS);

    ESP_LOGI(TAG, "[3.2] Create wav encoder to encode wav format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);

    ESP_LOGI(TAG, "[3.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline_rec");
    audio_pipeline_register(pipeline_rec, i2s_stream_reader, "i2s_rd");
    audio_pipeline_register(pipeline_rec, filter_r, "filter_r");
    audio_pipeline_register(pipeline_rec, element_algo, "algo");
    audio_pipeline_register(pipeline_rec, wav_encoder, "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_stream_writer, "fatfs_stream");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->filter-->algorithm-->wav_encoder-->fatfs_stream-->[sdcard]");
    const char *link_rec[5] = {"i2s_rd", "filter_r", "algo", "wav_encoder", "fatfs_stream"};
    audio_pipeline_link(pipeline_rec, &link_rec[0], 5);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)");
#ifdef DEBUG_ALGO_INPUT
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec_in.wav");
#else
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec_out.wav");
#endif

    ESP_LOGI(TAG, "[4.0] Create audio pipeline_play for playing");
    audio_pipeline_cfg_t pipeline_play_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_play = audio_pipeline_init(&pipeline_play_cfg);

    ESP_LOGI(TAG, "[4.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_decoder = mp3_decoder_init(&mp3_decoder_cfg);
    audio_element_set_read_cb(mp3_decoder, mp3_music_read_cb, NULL);

    rsp_filter_cfg_t rsp_cfg_w = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg_w.src_rate = I2S_SAMPLE_RATE;
    rsp_cfg_w.src_ch = 1;
    rsp_cfg_w.dest_rate = I2S_SAMPLE_RATE;
    rsp_cfg_w.dest_ch = I2S_CHANNELS;
    rsp_cfg_w.complexity = 5;
    audio_element_handle_t filter_w = rsp_filter_init(&rsp_cfg_w);

    ESP_LOGI(TAG, "[4.3] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_wd_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_wd_cfg.type = AUDIO_STREAM_WRITER;
    i2s_wd_cfg.i2s_config.sample_rate = I2S_SAMPLE_RATE;
#ifndef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_wd_cfg.multi_out_num = 1;
#endif
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_wd_cfg);

    ESP_LOGI(TAG, "[4.4] Register all elements to audio pipeline_play");
    audio_pipeline_register(pipeline_play, mp3_decoder, "mp3_decoder");
    audio_pipeline_register(pipeline_play, filter_w, "filter_w");
    audio_pipeline_register(pipeline_play, i2s_stream_writer, "i2s_wd");

    ESP_LOGI(TAG, "[4.5] Link it together [flash]-->mp3_decoder-->filter-->i2s_stream--->[codec_chip]");
    const char *link_tag[3] = {"mp3_decoder", "filter_w", "i2s_wd"};
    audio_pipeline_link(pipeline_play, &link_tag[0], 3);

#ifndef CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    //Please reference the way of ALGORITHM_STREAM_INPUT_TYPE2 in "algorithm_stream.h"
    ringbuf_handle_t ringbuf_ref = rb_create(50 * 1024, 1);
    audio_element_set_multi_input_ringbuf(element_algo, ringbuf_ref, 0);
    audio_element_set_multi_output_ringbuf(i2s_stream_writer, ringbuf_ref, 0);

    /* When the playback signal far ahead of the recording signal,
        the playback signal needs to be delayed */
    algo_stream_set_delay(i2s_stream_writer, ringbuf_ref, DEFAULT_REF_DELAY_MS);

    /* When the playback signal after the recording signal,
        the recording signal needs to be delayed */
    algo_stream_set_delay(element_algo, audio_element_get_input_ringbuf(element_algo), DEFAULT_REC_DELAY_MS);
#endif

    ESP_LOGI(TAG, "[5.0] Set up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline_play, evt);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[6.0] Start audio_pipeline");

    audio_element_info_t fat_info = {0};
    audio_element_getinfo(fatfs_stream_writer, &fat_info);
    fat_info.sample_rates = ALGORITHM_STREAM_DEFAULT_SAMPLE_RATE_HZ;
    fat_info.bits = ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT;
#ifdef DEBUG_ALGO_INPUT
    fat_info.channels = 2;
#else
    fat_info.channels = 1;
#endif
    audio_element_setinfo(fatfs_stream_writer, &fat_info);

    audio_pipeline_run(pipeline_play);
    audio_pipeline_run(pipeline_rec);

    ESP_LOGI(TAG, "[7.0] Listen for all pipeline events");

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[8.0] Stop audio_pipeline");

    audio_pipeline_stop(pipeline_rec);
    audio_pipeline_wait_for_stop(pipeline_rec);
    audio_pipeline_deinit(pipeline_rec);

    audio_pipeline_stop(pipeline_play);
    audio_pipeline_wait_for_stop(pipeline_play);
    audio_pipeline_deinit(pipeline_play);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    esp_periph_set_destroy(set);
}
