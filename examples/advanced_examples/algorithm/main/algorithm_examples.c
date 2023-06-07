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
#include "driver/i2s.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "board.h"
#include "es8311.h"
#include "fatfs_stream.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "algorithm_stream.h"
#include "audio_mem.h"
#include "audio_idf_version.h"

static const char *TAG = "ALGORITHM_EXAMPLES";

/* Debug original input data for AEC feature*/
// #define DEBUG_ALGO_INPUT

#define I2S_SAMPLE_RATE     8000
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
#define I2S_CHANNELS        I2S_CHANNEL_FMT_RIGHT_LEFT
#else
#define I2S_CHANNELS        I2S_CHANNEL_FMT_ONLY_LEFT
#endif
#define I2S_BITS            CODEC_ADC_BITS_PER_SAMPLE

/* The AEC internal buffering mechanism requires that the recording signal
   is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal. */
#define DEFAULT_REF_DELAY_MS    0
#define ESP_RING_BUFFER_SIZE    256

extern const uint8_t adf_music_mp3_start[] asm("_binary_test_mp3_start");
extern const uint8_t adf_music_mp3_end[]   asm("_binary_test_mp3_end");

#if !RECORD_HARDWARE_AEC
static ringbuf_handle_t ringbuf_ref;
#endif

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

static esp_err_t i2s_driver_init(i2s_port_t port, i2s_channel_fmt_t channels, i2s_bits_per_sample_t bits)
{
    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = bits,
        .channel_format = channels,
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0))
        .communication_format = I2S_COMM_FORMAT_I2S,
#else
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#endif
        .tx_desc_auto_clear = true,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
    };

    i2s_driver_install(port, &i2s_cfg, 0, NULL);
    board_i2s_pin_t board_i2s_pin = {0};
    i2s_pin_config_t i2s_pin_cfg;
    get_i2s_pins(port, &board_i2s_pin);
    i2s_pin_cfg.bck_io_num = board_i2s_pin.bck_io_num;
    i2s_pin_cfg.ws_io_num = board_i2s_pin.ws_io_num;
    i2s_pin_cfg.data_out_num = board_i2s_pin.data_out_num;
    i2s_pin_cfg.data_in_num = board_i2s_pin.data_in_num;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
    i2s_pin_cfg.mck_io_num = board_i2s_pin.mck_io_num;
#endif
    i2s_set_pin(port, &i2s_pin_cfg);

    return ESP_OK;
}

static int i2s_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_read = 0;

    int ret = i2s_read(CODEC_ADC_I2S_PORT, buf, len, &bytes_read, wait_time);
    if (ret == ESP_OK) {
#if (CONFIG_IDF_TARGET_ESP32 && !RECORD_HARDWARE_AEC)
        algorithm_mono_fix((uint8_t *)buf, bytes_read);
#endif
    } else {
        ESP_LOGE(TAG, "i2s read failed");
    }

    return bytes_read;
}

static int i2s_write_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_write = 0;

#if !RECORD_HARDWARE_AEC
    if (rb_write(ringbuf_ref, buf, len, wait_time) <= 0) {
        ESP_LOGW(TAG, "ringbuf write timeout");
    }
#endif

#if (CONFIG_IDF_TARGET_ESP32 && !RECORD_HARDWARE_AEC)
    algorithm_mono_fix((uint8_t *)buf, len);
#endif
    int ret = i2s_write_expand(I2S_NUM_0, buf, len, 16, I2S_BITS, &bytes_write, wait_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed");
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
    i2s_driver_init(I2S_NUM_0, I2S_CHANNELS, I2S_BITS);

#if (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD)
    audio_board_handle_t board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_codec_cfg.i2s_iface.samples = AUDIO_HAL_08K_SAMPLES;
    board_handle->audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    board_handle->adc_hal = audio_board_adc_init();
#else
    audio_board_handle_t board_handle = audio_board_init();
#endif
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 60);

#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_driver_init(I2S_NUM_1, I2S_CHANNELS, I2S_BITS);
#endif

    ESP_LOGI(TAG, "[3.0] Create audio pipeline_rec for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_rec);

    ESP_LOGI(TAG, "[3.1] Create algorithm stream for aec");
    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
#if !RECORD_HARDWARE_AEC
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
#endif
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    algo_config.ref_linear_factor = 3;
#endif
#ifdef DEBUG_ALGO_INPUT
    algo_config.debug_input = true;
#endif
#if (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD)
    algo_config.swap_ch = true;
#endif
    algo_config.sample_rate = I2S_SAMPLE_RATE;
    algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
    audio_element_handle_t element_algo = algo_stream_init(&algo_config);
    audio_element_set_music_info(element_algo, I2S_SAMPLE_RATE, 1, ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT);
    audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
    audio_element_set_input_timeout(element_algo, portMAX_DELAY);

    ESP_LOGI(TAG, "[3.2] Create wav encoder to encode wav format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);

    ESP_LOGI(TAG, "[3.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline_rec");
    audio_pipeline_register(pipeline_rec, element_algo, "algo");
    audio_pipeline_register(pipeline_rec, wav_encoder, "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_stream_writer, "fatfs_stream");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->algorithm-->wav_encoder-->fatfs_stream-->[sdcard]");
    const char *link_rec[3] = {"algo", "wav_encoder", "fatfs_stream"};
    audio_pipeline_link(pipeline_rec, &link_rec[0], 3);

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
    rsp_cfg_w.src_rate = 16000;
    rsp_cfg_w.src_ch = 1;
    rsp_cfg_w.dest_rate = I2S_SAMPLE_RATE;
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
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

#if !RECORD_HARDWARE_AEC
    //Please reference the way of ALGORITHM_STREAM_INPUT_TYPE2 in "algorithm_stream.h"
    ringbuf_ref = rb_create(ALGORITHM_STREAM_RINGBUFFER_SIZE, 1);
    audio_element_set_multi_input_ringbuf(element_algo, ringbuf_ref, 0);

    /* When the playback signal far ahead of the recording signal,
        the playback signal needs to be delayed */
    algo_stream_set_delay(element_algo, ringbuf_ref, DEFAULT_REF_DELAY_MS);
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
    fat_info.sample_rates = I2S_SAMPLE_RATE;
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

        /* Stop when the last pipeline element receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) filter_w
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
