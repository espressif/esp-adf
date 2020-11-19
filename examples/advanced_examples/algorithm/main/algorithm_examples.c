/* Record wav and amr to SD card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
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

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
#define MINI_I2S_READER_SAMPLES 16000
#define MINI_I2S_READER_BITS     16
#define MINI_I2S_READER_CHANNEL  1
#endif

void app_main()
{
    audio_pipeline_handle_t pipeline_rec, pipeline_play;
    audio_element_handle_t i2s_stream_writer, i2s_stream_reader, wav_encoder, mp3_decoder, fatfs_stream_writer, fatfs_stream_reader;
    audio_element_handle_t element_algo;

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
    audio_hal_set_volume(board_handle->audio_hal, 50);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline_rec for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_rec);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_rd_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_rd_cfg.type = AUDIO_STREAM_READER;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_rd_cfg.i2s_config.use_apll = false;
    i2s_rd_cfg.i2s_port = 1;
#endif
    i2s_stream_reader = i2s_stream_init(&i2s_rd_cfg);

    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE1;
#elif defined CONFIG_ESP_LYRAT_V4_3_BOARD
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
#else
    ESP_LOGE(TAG, "[ * ] Do not support current board");
    return;
#endif
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
    element_algo = algo_stream_init(&algo_config);

    ESP_LOGI(TAG, "[3.2] Create mp3 encoder to encode mp3 format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    wav_encoder = wav_encoder_init(&wav_cfg);

    ESP_LOGI(TAG, "[3.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_rec, i2s_stream_reader, "i2s_rd");
    audio_pipeline_register(pipeline_rec, element_algo, "algo");
    audio_pipeline_register(pipeline_rec, wav_encoder, "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_stream_writer, "mp3_wd_file");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->mp3_encoder-->fatfs_stream-->[sdcard]");
    const char *link_rec[4] = {"i2s_rd", "algo", "wav_encoder", "mp3_wd_file"};
    audio_pipeline_link(pipeline_rec, &link_rec[0], 4);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 encoder)");
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec_out.wav");

    ESP_LOGI(TAG, "[4.0] Create audio pipeline_rec for recording");
    audio_pipeline_cfg_t pipeline_play_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_play = audio_pipeline_init(&pipeline_play_cfg);

    ESP_LOGI(TAG, "[4.1] Create fatfs stream to read data from sdcard");
    fatfs_stream_cfg_t fatfs_rd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_rd_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_rd_cfg);
    
    ESP_LOGI(TAG, "[4.2] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_decoder_cfg);

    ESP_LOGI(TAG, "[4.3] Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_wd_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_wd_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_wd_cfg);

    ESP_LOGI(TAG, "[4.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_play, fatfs_stream_reader, "file_rd");
    audio_pipeline_register(pipeline_play, mp3_decoder, "mp3_decoder");
    audio_pipeline_register(pipeline_play, i2s_stream_writer, "i2s_wd");

    ESP_LOGI(TAG, "[4.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"file_rd", "mp3_decoder", "i2s_wd"};
    audio_pipeline_link(pipeline_play, &link_tag[0], 3);

    ESP_LOGI(TAG, "[4.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)");
    audio_element_set_uri(fatfs_stream_reader, "/sdcard/test.mp3");

#if defined CONFIG_ESP_LYRAT_V4_3_BOARD
    //Please reference the way of ALGORITHM_STREAM_INPUT_TYPE2 in "algorithm_stream.h"
    ringbuf_handle_t input_rb = algo_stream_get_multi_input_rb(element_algo);
    audio_element_set_multi_output_ringbuf(mp3_decoder, input_rb, 0);
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
    fat_info.channels = ALGORITHM_STREAM_DEFAULT_CHANNEL;
    fat_info.bits = ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT;
    audio_element_setinfo(fatfs_stream_writer, &fat_info);

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    //The purpose is to set a fixed frequency, when current board is "lyrat mini v1.1".
    i2s_stream_set_clk(i2s_stream_reader, MINI_I2S_READER_SAMPLES, MINI_I2S_READER_BITS, MINI_I2S_READER_CHANNEL);
    algo_stream_set_record_rate(element_algo, MINI_I2S_READER_CHANNEL, MINI_I2S_READER_SAMPLES);
#endif

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

            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);

#if defined CONFIG_ESP_LYRAT_V4_3_BOARD
            algo_stream_set_record_rate(element_algo, music_info.channels, music_info.sample_rates);
            algo_stream_set_reference_rate(element_algo, music_info.channels, music_info.sample_rates);
#endif
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

    audio_pipeline_stop(pipeline_rec);
    audio_pipeline_wait_for_stop(pipeline_rec);
    audio_pipeline_terminate(pipeline_rec);
    audio_pipeline_unregister_more(pipeline_rec, i2s_stream_reader,
                                   wav_encoder, fatfs_stream_writer);

    audio_pipeline_stop(pipeline_play);
    audio_pipeline_wait_for_stop(pipeline_play);
    audio_pipeline_terminate(pipeline_play);
    audio_pipeline_unregister_more(pipeline_play, fatfs_stream_reader,
                                   mp3_decoder, i2s_stream_writer);

    ESP_LOGI(TAG, "[8.0] Stop audio_pipeline");

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline_rec);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(wav_encoder);
    audio_element_deinit(fatfs_stream_writer);

    audio_pipeline_deinit(pipeline_play);
    audio_element_deinit(fatfs_stream_reader);
    audio_element_deinit(mp3_decoder);
    audio_element_deinit(i2s_stream_writer);

    esp_periph_set_destroy(set);
}
