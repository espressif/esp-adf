/* Record wav and amr to SD card 

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "board.h"
#include "esp_log.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "wav_encoder.h"
#include "amrwb_encoder.h"
#include "amrnb_encoder.h"

static const char *TAG = "ELEMENT_REC_WAV_AMR_SDCARD";

#define RING_BUFFER_SIZE (2048)
#define RECORD_TIME_SECONDS (10)

esp_err_t audio_element_event_handler(audio_element_handle_t self, audio_event_iface_msg_t *event, void *ctx)
{
    ESP_LOGI(TAG, "Audio event %d from %s element", event->cmd, audio_element_get_tag(self));
    if (event->cmd == AEL_MSG_CMD_REPORT_STATUS) {
        switch ((int) event->data) {
            case AEL_STATUS_STATE_RUNNING:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_RUNNING");
                break;
            case AEL_STATUS_STATE_STOPPED:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_STOPPED");
                break;
            case AEL_STATUS_STATE_FINISHED:
                ESP_LOGI(TAG, "AEL_STATUS_STATE_FINISHED");
                break;
            default:
                ESP_LOGI(TAG, "Some other event = %d", (int) event->data);
        }
    }
    return ESP_OK;
}

void app_main()
{
/*
    * structure:
    * i2s_stream |-> ringbuf01 -> wav_encoder -> ringbuf02 -> wav_fatfs_stream_writer
    *            |
    *            |-> ringbuf11 -> amr_encoder -> ringbuf12 -> amr_fatfs_stream_writer
    * 
    */

    audio_element_handle_t wav_fatfs_stream_writer, i2s_stream_reader, wav_encoder, amr_fatfs_stream_writer, amr_encoder;
    ringbuf_handle_t ringbuf01, ringbuf02, ringbuf11, ringbuf12;
    int sample_rate = 0;

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
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);
   
    ESP_LOGI(TAG, "[3.0] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.multi_out_num = 1;
    i2s_cfg.task_core = 1;

#ifdef CONFIG_CHOICE_AMR_WB
    sample_rate = 16000;
#elif defined CONFIG_CHOICE_AMR_NB
    sample_rate = 8000;
#endif
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
    i2s_cfg.chan_cfg.id = CODEC_ADC_I2S_PORT;
    i2s_cfg.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_cfg.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
#else
    i2s_cfg.i2s_port = CODEC_ADC_I2S_PORT;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.sample_rate = sample_rate;
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);    

    ESP_LOGI(TAG, "[3.1] Create wav encoder to encode wav format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    wav_encoder = wav_encoder_init(&wav_cfg);
  
    ESP_LOGI(TAG, "[3.2] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    wav_fatfs_stream_writer = fatfs_stream_init(&fatfs_cfg);

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();  
    audio_element_getinfo(i2s_stream_reader, &info);
    audio_element_setinfo(wav_fatfs_stream_writer, &info);

    ESP_LOGI(TAG, "[3.3] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder");
    ringbuf01 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_output_ringbuf(i2s_stream_reader, ringbuf01);
    audio_element_set_input_ringbuf(wav_encoder, ringbuf01);
    
    ESP_LOGI(TAG, "[3.4] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer");
    ringbuf02 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_output_ringbuf(wav_encoder, ringbuf02);
    audio_element_set_input_ringbuf(wav_fatfs_stream_writer, ringbuf02);

    ESP_LOGI(TAG, "[3.5] Set up uri (file as fatfs_stream, wav as wav encoder)");
    audio_element_set_uri(wav_fatfs_stream_writer, "/sdcard/rec_out.wav");

    ESP_LOGI(TAG, "[4.0] Create amr encoder to encode wav format");
#ifdef CONFIG_CHOICE_AMR_WB
    amrwb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amr_encoder = amrwb_encoder_init(&amr_enc_cfg);
#elif defined CONFIG_CHOICE_AMR_NB
    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_encoder = amrnb_encoder_init(&amr_enc_cfg);
#endif

    ESP_LOGI(TAG, "[4.1] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t amr_fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    amr_fatfs_cfg.type = AUDIO_STREAM_WRITER;
    amr_fatfs_cfg.task_core = 1;
    amr_fatfs_stream_writer = fatfs_stream_init(&amr_fatfs_cfg);

    ESP_LOGI(TAG, "[4.2] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder");
    ringbuf11 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_multi_output_ringbuf(i2s_stream_reader,ringbuf11,0);
    audio_element_set_input_ringbuf(amr_encoder,ringbuf11);
    
    ESP_LOGI(TAG, "[4.3] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer");
    ringbuf12 = rb_create(RING_BUFFER_SIZE, 1);
    audio_element_set_output_ringbuf(amr_encoder,ringbuf12);
    audio_element_set_input_ringbuf(amr_fatfs_stream_writer, ringbuf12);

    ESP_LOGI(TAG, "[4.4] Set up uri (file as fatfs_stream, wav as wav encoder)");
#ifdef CONFIG_CHOICE_AMR_WB
    audio_element_set_uri(amr_fatfs_stream_writer, "/sdcard/rec_out.Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_element_set_uri(amr_fatfs_stream_writer, "/sdcard/rec_out.amr");
#endif

    ESP_LOGI(TAG, "[5.0] Set callback function for audio_elements");
    /**
    * Event handler used here is quite generic and simple one.
    * It just reports state changes of different audio_elements
    * Note that, it is not mandatory to set event callbacks.
    * One may remove entire step [5.0]. This example could still run.
    */
    audio_element_set_event_callback(wav_encoder, audio_element_event_handler, NULL);
    audio_element_set_event_callback(i2s_stream_reader, audio_element_event_handler, NULL);
    audio_element_set_event_callback(wav_fatfs_stream_writer, audio_element_event_handler, NULL);
    audio_element_set_event_callback(amr_encoder, audio_element_event_handler, NULL);
    audio_element_set_event_callback(amr_fatfs_stream_writer, audio_element_event_handler, NULL);

    ESP_LOGI(TAG, "[6.0] Set up event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[7.0] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
   
    ESP_LOGI(TAG, "[8.0] Start audio elements");
    audio_element_run(i2s_stream_reader);
    audio_element_run(wav_encoder);
    audio_element_run(wav_fatfs_stream_writer);
    audio_element_run(amr_encoder);
    audio_element_run(amr_fatfs_stream_writer);

    audio_element_resume(i2s_stream_reader, 0, 0);
    audio_element_resume(wav_encoder, 0, 0);
    audio_element_resume(wav_fatfs_stream_writer, 0, 0);
    audio_element_resume(amr_encoder, 0, 0);
    audio_element_resume(amr_fatfs_stream_writer, 0, 0);
    int second_recorded = 0;
    while (1) {
        ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
        vTaskDelay(1000 / portTICK_RATE_MS);
        second_recorded++;
        if (second_recorded >= RECORD_TIME_SECONDS) {
            ESP_LOGI(TAG, "Finishing recording");
            break;
        }
    }
    ESP_LOGI(TAG, "[9.0] Stop elements and release resources");

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);

    /* Release all resources */
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(wav_encoder);
    audio_element_deinit(wav_fatfs_stream_writer);
    audio_element_deinit(amr_encoder);
    audio_element_deinit(amr_fatfs_stream_writer);

    esp_periph_set_destroy(set);
    rb_destroy(ringbuf01);
    rb_destroy(ringbuf02);
    rb_destroy(ringbuf11);
    rb_destroy(ringbuf12);
}
