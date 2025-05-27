/* AEC Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "board.h"
#include "es8311.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "aec_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "audio_mem.h"
#if defined(CONFIG_ESP32_S3_KORVO2_V3_BOARD) || defined(CONFIG_ESP32_S3_KORVO2L_V1_BOARD)
#include "es7210.h"
#endif  /* CONFIG_ESP32_S3_KORVO2_V3_BOARD */

static const char *TAG = "AEC_EXAMPLE";

/* Debug original input data for AEC feature*/
// #define DEBUG_AEC_INPUT

#define I2S_SAMPLE_RATE     8000
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2L_V1_BOARD
#define I2S_CHANNELS        I2S_CHANNEL_FMT_RIGHT_LEFT
#else
#define I2S_CHANNELS        I2S_CHANNEL_FMT_ONLY_LEFT
#endif
#define I2S_BITS            CODEC_ADC_BITS_PER_SAMPLE

extern const uint8_t adf_music_mp3_start[] asm("_binary_test_mp3_start");
extern const uint8_t adf_music_mp3_end[]   asm("_binary_test_mp3_end");

static audio_element_handle_t i2s_stream_reader;
static audio_element_handle_t i2s_stream_writter;

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

static int i2s_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_read = audio_element_input(i2s_stream_reader, buf, len);
    if (bytes_read <= 0) {
        ESP_LOGE(TAG, "I2S read failed, %d", len);
    } 
    return bytes_read;
}

static int i2s_write_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int bytes_write = 0;
    bytes_write = audio_element_output(i2s_stream_writter, buf, len);
    if (bytes_write < 0) {
        ESP_LOGE(TAG, "I2S write failed, %d", len);
    }
    return bytes_write;
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[1.0] Mount sdcard");
    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[2.0] Start codec chip");
    i2s_stream_cfg_t i2s_w_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(I2S_NUM_0, I2S_SAMPLE_RATE, I2S_BITS, AUDIO_STREAM_WRITER);
    i2s_w_cfg.task_stack = -1;
    i2s_w_cfg.need_expand = (16 != I2S_BITS);
    i2s_stream_set_channel_type(&i2s_w_cfg, I2S_CHANNELS);
    i2s_stream_writter = i2s_stream_init(&i2s_w_cfg);

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 80);
#if CONFIG_ESP32_S3_KORVO2_V3_BOARD
    es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_30DB);
#elif CONFIG_ESP32_S3_KORVO2L_V1_BOARD
    es8311_set_mic_gain(ES8311_MIC_GAIN_24DB);
#endif
    i2s_stream_cfg_t i2s_r_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, I2S_SAMPLE_RATE, I2S_BITS, AUDIO_STREAM_READER);
    i2s_r_cfg.task_stack = -1;
    i2s_stream_set_channel_type(&i2s_r_cfg, I2S_CHANNELS);
    i2s_stream_reader = i2s_stream_init(&i2s_r_cfg);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline_rec for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_rec);
    
    ESP_LOGI(TAG, "[3.1] Create algorithm stream for aec");
    aec_stream_cfg_t aec_config = AEC_STREAM_CFG_DEFAULT();
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD
    aec_config.input_format = "RM";
#else
    aec_config.input_format = "MR";
#endif
    audio_element_handle_t element_aec = aec_stream_init(&aec_config);
    mem_assert(element_aec);
    audio_element_set_read_cb(element_aec, i2s_read_cb, NULL);
    audio_element_set_input_timeout(element_aec, portMAX_DELAY);

    ESP_LOGI(TAG, "[3.2] Create wav encoder to encode wav format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);

    ESP_LOGI(TAG, "[3.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline_rec");
    audio_pipeline_register(pipeline_rec, element_aec, "aec");
    audio_pipeline_register(pipeline_rec, wav_encoder, "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_stream_writer, "fatfs_stream");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->aec-->wav_encoder-->fatfs_stream-->[sdcard]");
    const char *link_rec[3] = {"aec", "wav_encoder", "fatfs_stream"};
    audio_pipeline_link(pipeline_rec, &link_rec[0], 3);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)");
#ifdef DEBUG_AEC_INPUT
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec_in.wav");
#else
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec_out.wav");
#endif

    ESP_LOGI(TAG, "[4.0] Create audio pipeline_play for playing");
    audio_pipeline_cfg_t pipeline_play_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_play = audio_pipeline_init(&pipeline_play_cfg);

    ESP_LOGI(TAG, "[4.1] Create mp3 decoder to decode mp3 file");
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
    audio_element_handle_t mp3_decoder = mp3_decoder_init(&mp3_decoder_cfg);
    audio_element_set_read_cb(mp3_decoder, mp3_music_read_cb, NULL);

    ESP_LOGI(TAG, "[4.1] Create resample filter to resample mp3");
    rsp_filter_cfg_t rsp_cfg_w = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg_w.src_rate = 16000;
    rsp_cfg_w.src_ch = 1;
    rsp_cfg_w.dest_rate = I2S_SAMPLE_RATE;
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2L_V1_BOARD
    rsp_cfg_w.dest_ch = 2;
#else
    rsp_cfg_w.dest_ch = 1;
#endif
    rsp_cfg_w.complexity = 5;
    audio_element_handle_t filter_w = rsp_filter_init(&rsp_cfg_w);
    audio_element_set_write_cb(filter_w, i2s_write_cb, NULL);
    audio_element_set_output_timeout(filter_w, portMAX_DELAY);

    ESP_LOGI(TAG, "[4.4] Register all elements to audio pipeline_play");
    audio_pipeline_register(pipeline_play, mp3_decoder, "mp3_decoder");
    audio_pipeline_register(pipeline_play, filter_w, "filter_w");

    ESP_LOGI(TAG, "[4.5] Link it together [flash]-->mp3_decoder-->filter-->[codec_chip]");
    const char *link_tag[2] = {"mp3_decoder", "filter_w"};
    audio_pipeline_link(pipeline_play, &link_tag[0], 2);

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
    fat_info.sample_rates = I2S_SAMPLE_RATE;
    fat_info.bits = 16;
#ifdef DEBUG_AEC_INPUT
    fat_info.channels = 2;
#else
    fat_info.channels = 1;
#endif

    audio_element_setinfo(fatfs_stream_writer, &fat_info);

    audio_pipeline_run(pipeline_play);
    audio_pipeline_run(pipeline_rec);

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)mp3_decoder
            && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = {0};
            audio_element_getinfo(mp3_decoder, &music_info);

            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
                     music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* Stop when the last pipeline element receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)filter_w
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    ESP_LOGI(TAG, "[7.0] Stop audio_pipeline");

    audio_pipeline_stop(pipeline_rec);
    audio_pipeline_wait_for_stop(pipeline_rec);
    audio_pipeline_deinit(pipeline_rec);

    audio_pipeline_stop(pipeline_play);
    audio_pipeline_wait_for_stop(pipeline_play);
    audio_pipeline_deinit(pipeline_play);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(i2s_stream_writter);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    esp_periph_set_destroy(set);
}
