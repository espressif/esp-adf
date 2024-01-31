/* Voice Changer Example with Sonic

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
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "wav_decoder.h"
#include "board.h"
#include "audio_sonic.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_button.h"

static const char *TAG = "SONIC_EXAMPLE";
static esp_periph_set_handle_t set;

#define SAMPLE_RATE         16000
#define CHANNEL             1
#define BITS                16

#define SONIC_PITCH         1.4f
#define SONIC_SPEED         2.0f

static audio_element_handle_t create_sonic()
{
    sonic_cfg_t sonic_cfg = DEFAULT_SONIC_CONFIG();
    sonic_cfg.sonic_info.samplerate = SAMPLE_RATE;
    sonic_cfg.sonic_info.channel = CHANNEL;
    sonic_cfg.sonic_info.resample_linear_interpolate = 1;
    return sonic_init(&sonic_cfg);
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
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    if (i2s_cfg.type == AUDIO_STREAM_READER) {
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        i2s_cfg.chan_cfg.id = CODEC_ADC_I2S_PORT;
        i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
        i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#else
        i2s_cfg.i2s_port = CODEC_ADC_I2S_PORT;
        i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#endif
    }
#endif

    audio_element_handle_t i2s_stream = i2s_stream_init(&i2s_cfg);
    mem_assert(i2s_stream);
    audio_element_set_music_info(i2s_stream, sample_rates, channels, bits);
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
     * We will setup I2S and get audio at sample rates 16000Hz, 16-bits, 1 channel.
     * And the audio stream will be encoded with Wav encoder.
     * Then the audio stream will be written to SDCARD.
     */
    ESP_LOGI(TAG, "[1.1] Initialize recorder pipeline");
    pipeline_rec = audio_pipeline_init(&pipeline_cfg);
    pipeline_play = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[1.2] Create audio elements for recorder pipeline");
    audio_element_handle_t i2s_reader_el = create_i2s_stream(SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t wav_encoder_el = create_wav_encoder();
    audio_element_handle_t fatfs_writer_el = create_fatfs_stream(SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[1.3] Register audio elements to recorder pipeline");
    audio_pipeline_register(pipeline_rec, i2s_reader_el, "i2s_reader");
    audio_pipeline_register(pipeline_rec, wav_encoder_el, "wav_encoder");
    audio_pipeline_register(pipeline_rec, fatfs_writer_el, "file_writer");
    const char *link_rec[3] = {"i2s_reader", "wav_encoder", "file_writer"};
    audio_pipeline_link(pipeline_rec, &link_rec[0], 3);

    /**
     * For the Playback:
     * We will read the recorded file processed by sonic.
     */
    ESP_LOGI(TAG, "[2.2] Create audio elements for playback pipeline");
    audio_element_handle_t fatfs_reader_el = create_fatfs_stream(SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_READER);
    audio_element_handle_t wav_decoder_el = create_wav_decoder();
    audio_element_handle_t sonic_el = create_sonic();
    audio_element_handle_t i2s_writer_el = create_i2s_stream(SAMPLE_RATE, BITS, CHANNEL, AUDIO_STREAM_WRITER);

    ESP_LOGI(TAG, "[2.3] Register audio elements to playback pipeline");
    audio_pipeline_register(pipeline_play, fatfs_reader_el, "file_reader");
    audio_pipeline_register(pipeline_play, wav_decoder_el, "wav_decoder");
    audio_pipeline_register(pipeline_play, sonic_el, "sonic");
    audio_pipeline_register(pipeline_play, i2s_writer_el, "i2s_writer");
    
    const char *link_play[4] = {"file_reader", "wav_decoder", "sonic", "i2s_writer"};
    audio_pipeline_link(pipeline_play, &link_play[0], 4);

    ESP_LOGI(TAG, "[ 3 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    ESP_LOGW(TAG, "Press [Rec] to start recording");
    bool is_modify_speed = true;
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }
        if ((int)msg.data == get_input_mode_id()) {
            if ((msg.cmd == PERIPH_BUTTON_LONG_PRESSED)
                || (msg.cmd == PERIPH_BUTTON_PRESSED)) {
                is_modify_speed = !is_modify_speed;
                if (is_modify_speed) {
                    ESP_LOGI(TAG, "The speed of audio file is changed");
                } else {
                    ESP_LOGI(TAG, "The pitch of audio file is changed");
                }
            }
            continue;
        }
        if ((int)msg.data == get_input_rec_id()) {
            if (msg.cmd == PERIPH_BUTTON_PRESSED) {
                //using LOGE to make the log color different
                ESP_LOGE(TAG, "Now recording, release [Rec] to STOP");
                audio_pipeline_stop(pipeline_play);
                audio_pipeline_wait_for_stop(pipeline_play);
                audio_pipeline_terminate(pipeline_play);
                audio_pipeline_reset_ringbuffer(pipeline_play);
                audio_pipeline_reset_elements(pipeline_play);

                /**
                 * Audio Recording Flow:
                 * [codec_chip]-->i2s_stream--->wav_encoder-->fatfs_stream-->[sdcard]
                 */
                ESP_LOGI(TAG, "Setup file path to save recorded audio");
                i2s_stream_set_clk(i2s_reader_el, SAMPLE_RATE, BITS, CHANNEL);
                audio_element_set_uri(fatfs_writer_el, "/sdcard/rec.wav");
                audio_pipeline_run(pipeline_rec);
            } else if (msg.cmd == PERIPH_BUTTON_RELEASE || msg.cmd == PERIPH_BUTTON_LONG_RELEASE) {
                ESP_LOGI(TAG, "START Playback");
                audio_pipeline_stop(pipeline_rec);
                audio_pipeline_wait_for_stop(pipeline_rec);
                audio_pipeline_terminate(pipeline_rec);
                audio_pipeline_reset_ringbuffer(pipeline_rec);
                audio_pipeline_reset_elements(pipeline_rec);

                /**
                 * Audio Playback Flow:
                 * [sdcard]-->fatfs_stream-->wav_decoder-->sonic-->i2s_stream-->[codec_chip]
                 */
                ESP_LOGI(TAG, "Setup file path to read the wav audio to play");
                i2s_stream_set_clk(i2s_writer_el, SAMPLE_RATE, BITS, CHANNEL);
                audio_element_set_uri(fatfs_reader_el, "/sdcard/rec.wav");
                if (is_modify_speed) {
                    sonic_set_pitch_and_speed_info(sonic_el, 1.0f, SONIC_SPEED);
                } else {
                    sonic_set_pitch_and_speed_info(sonic_el, SONIC_PITCH, 1.0f);
                }
                audio_pipeline_run(pipeline_play);
            }
        }
    }

    ESP_LOGI(TAG, "[ 4 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline_rec);
    audio_pipeline_wait_for_stop(pipeline_rec);
    audio_pipeline_terminate(pipeline_rec);
    audio_pipeline_stop(pipeline_play);
    audio_pipeline_wait_for_stop(pipeline_play);
    audio_pipeline_terminate(pipeline_play);

    audio_pipeline_unregister(pipeline_play, fatfs_reader_el);
    audio_pipeline_unregister(pipeline_play, wav_decoder_el);
    audio_pipeline_unregister(pipeline_play, i2s_writer_el);

    audio_pipeline_unregister(pipeline_rec, i2s_reader_el);
    audio_pipeline_unregister(pipeline_rec, sonic_el);
    audio_pipeline_unregister(pipeline_rec, wav_encoder_el);
    audio_pipeline_unregister(pipeline_rec, fatfs_writer_el);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_rec);
    audio_pipeline_remove_listener(pipeline_play);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline_rec);
    audio_pipeline_deinit(pipeline_play);

    audio_element_deinit(fatfs_reader_el);
    audio_element_deinit(wav_decoder_el);
    audio_element_deinit(i2s_writer_el);

    audio_element_deinit(i2s_reader_el);
    audio_element_deinit(sonic_el);
    audio_element_deinit(wav_encoder_el);
    audio_element_deinit(fatfs_writer_el);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Initialize peripherals management
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    // Initialize Button peripheral
    audio_board_key_init(set);

    // Setup audio codec
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    // Start record/playback task
    record_playback_task();
    esp_periph_set_destroy(set);
}
