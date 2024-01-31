/* Record wav and amr to SD card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_log.h"
#include "audio_pipeline.h"
#include "board.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "wav_encoder.h"
#include "amrwb_encoder.h"
#include "amrnb_encoder.h"

static const char *TAG = "PIPELINR_REC_WAV_AMR_SDCARD";

#define  RECORD_TIME_SECONDS (10)

void app_main()
{
    audio_pipeline_handle_t pipeline_wav, pipeline_amr;
    audio_element_handle_t wav_fatfs_stream_writer, i2s_stream_reader, wav_encoder, amr_fatfs_stream_writer, amr_encoder;
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

    ESP_LOGI(TAG, "[3.0] Create audio pipeline_wav for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_wav = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline_wav);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to read audio data from codec chip");
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

    ESP_LOGI(TAG, "[3.2] Create wav encoder to encode wav format");
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    wav_encoder = wav_encoder_init(&wav_cfg);

    ESP_LOGI(TAG, "[3.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    wav_fatfs_stream_writer = fatfs_stream_init(&fatfs_cfg);

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    audio_element_getinfo(i2s_stream_reader, &info);
    audio_element_setinfo(wav_fatfs_stream_writer, &info);

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_wav, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline_wav, wav_encoder, "wav");
    audio_pipeline_register(pipeline_wav, wav_fatfs_stream_writer, "wav_file");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]");
    const char *link_wav[3] = {"i2s", "wav", "wav_file"};
    audio_pipeline_link(pipeline_wav, &link_wav[0], 3);

    ESP_LOGI(TAG, "[3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)");
    audio_element_set_uri(wav_fatfs_stream_writer, "/sdcard/rec_out.wav");

    ESP_LOGI(TAG, "[4.0] Create audio amr_pipeline for recording");
    pipeline_amr = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[4.1] Create raw stream to write data");
    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    audio_element_handle_t el_raw_reader = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[4.2] Create amr encoder to encode wav format");
#ifdef CONFIG_CHOICE_AMR_WB
    amrwb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    amr_encoder = amrwb_encoder_init(&amr_enc_cfg);
#elif defined CONFIG_CHOICE_AMR_NB
    amrnb_encoder_cfg_t amr_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    amr_encoder = amrnb_encoder_init(&amr_enc_cfg);
#endif

    ESP_LOGI(TAG, "[4.3] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t amr_fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    amr_fatfs_cfg.type = AUDIO_STREAM_WRITER;
    amr_fatfs_cfg.task_core = 1;
    amr_fatfs_stream_writer = fatfs_stream_init(&amr_fatfs_cfg);

    ESP_LOGI(TAG, "[4.4] Register all elements to audio amr_pipeline");
    audio_pipeline_register(pipeline_amr, el_raw_reader, "amr_raw");
#ifdef CONFIG_CHOICE_AMR_WB
    audio_pipeline_register(pipeline_amr, amr_encoder, "Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_pipeline_register(pipeline_amr, amr_encoder, "amr");
#endif
    audio_pipeline_register(pipeline_amr, amr_fatfs_stream_writer, "amr_file");

    ESP_LOGI(TAG, "[4.5] Link it together raw_stream-->amr_encoder-->fatfs_stream-->[sdcard]");
#ifdef CONFIG_CHOICE_AMR_WB
    const char *link_amr[3] = {"amr_raw", "Wamr", "amr_file"};
    audio_pipeline_link(pipeline_amr, &link_amr[0], 3);
#elif defined CONFIG_CHOICE_AMR_NB
     const char *link_amr[3] = {"amr_raw", "amr", "amr_file"};
    audio_pipeline_link(pipeline_amr, &link_amr[0], 3);
#endif

    ESP_LOGI(TAG, "[4.6] Create ringbuf to link  i2s");
    ringbuf_handle_t rb = audio_element_get_output_ringbuf(el_raw_reader);
    audio_element_set_multi_output_ringbuf(i2s_stream_reader, rb, 0);

    ESP_LOGI(TAG, "[4.7] Set up  uri (file as fatfs_stream, wav as wav encoder)");
#ifdef CONFIG_CHOICE_AMR_WB
    audio_element_set_uri(amr_fatfs_stream_writer, "/sdcard/rec_out.Wamr");
#elif defined CONFIG_CHOICE_AMR_NB
    audio_element_set_uri(amr_fatfs_stream_writer, "/sdcard/rec_out.amr");
#endif

    ESP_LOGI(TAG, "[5.0] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_set_listener(pipeline_wav, evt);
    audio_pipeline_set_listener(pipeline_amr, evt);

    ESP_LOGI(TAG, "[5.1] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);
    
    ESP_LOGI(TAG, "[6.0] start audio_pipeline");
    audio_pipeline_run(pipeline_wav);
    audio_pipeline_run(pipeline_amr);

    ESP_LOGI(TAG, "[7.0] Listen for all pipeline events, record for %d seconds", RECORD_TIME_SECONDS);
    int second_recorded = 0;
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK) {
            second_recorded++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= RECORD_TIME_SECONDS) {
                ESP_LOGI(TAG, "Finishing recording");
                audio_element_set_ringbuf_done(i2s_stream_reader);
            }
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_reader in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)i2s_stream_reader
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    audio_pipeline_stop(pipeline_wav);
    audio_pipeline_wait_for_stop(pipeline_wav);
    audio_pipeline_terminate(pipeline_wav);
    audio_pipeline_unregister_more(pipeline_wav, i2s_stream_reader,
                                   wav_encoder, wav_fatfs_stream_writer, NULL);
    audio_pipeline_unregister_more(pipeline_amr, el_raw_reader,
                                   amr_encoder, amr_fatfs_stream_writer, NULL);
    ESP_LOGI(TAG, "[8.0] Stop audio_pipeline");

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_wav);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline_wav);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(wav_encoder);
    audio_element_deinit(wav_fatfs_stream_writer);
    esp_periph_set_destroy(set);
}
