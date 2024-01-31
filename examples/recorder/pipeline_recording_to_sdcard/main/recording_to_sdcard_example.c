/* Record file to SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_sys.h"
#include "board.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#if defined (CONFIG_CHOICE_WAV_ENCODER)
#include "wav_encoder.h"
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
#include "opus_encoder.h"
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
#include "aac_encoder.h"
#if defined (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
#include "filter_resample.h"
#endif
#elif defined (CONFIG_CHOICE_AMR_WB_ENCODER)
#include "amrwb_encoder.h"
#elif defined (CONFIG_CHOICE_AMR_NB_ENCODER)
#include "amrnb_encoder.h"
#endif

#include "audio_idf_version.h"

#define RECORD_TIME_SECONDS (10)
static const char *TAG = "RECORD_TO_SDCARD";

#if defined (CONFIG_CHOICE_OPUS_ENCODER)
#define SAMPLE_RATE         16000
#define CHANNEL             1
#define BIT_RATE            64000
#define COMPLEXITY          10
#endif

#if defined (CONFIG_CHOICE_AAC_ENCODER)
#define SAMPLE_RATE         16000
#define CHANNEL             2
#define BIT_RATE            80000
#endif

typedef enum {
    AUDIO_DATA_FORMNAT_ONLY_RIGHT,
    AUDIO_DATA_FORMNAT_ONLY_LEFT,
    AUDIO_DATA_FORMNAT_RIGHT_LEFT,
} audio_channel_format_t;

static esp_err_t audio_data_format_set(i2s_stream_cfg_t *i2s_cfg, audio_channel_format_t fmt)
{
    AUDIO_UNUSED(i2s_cfg);   // remove unused warning
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    switch (fmt) {
        case AUDIO_DATA_FORMNAT_ONLY_RIGHT:
            i2s_cfg->std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
            i2s_cfg->std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
            break;
        case AUDIO_DATA_FORMNAT_ONLY_LEFT:
            i2s_cfg->std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
            i2s_cfg->std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
            break;
        case AUDIO_DATA_FORMNAT_RIGHT_LEFT:
            i2s_cfg->std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
            i2s_cfg->std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
            break;
    }
#else
    switch (fmt) {
        case AUDIO_DATA_FORMNAT_ONLY_RIGHT:
            i2s_cfg->i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
            break;
        case AUDIO_DATA_FORMNAT_ONLY_LEFT:
            i2s_cfg->i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
            break;
        case AUDIO_DATA_FORMNAT_RIGHT_LEFT:
            i2s_cfg->i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
            break;
    }
#endif
    return ESP_OK;
}

void app_main(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t fatfs_stream_writer, i2s_stream_reader, audio_encoder;
    int channel_format = AUDIO_DATA_FORMNAT_RIGHT_LEFT;
    int sample_rate = 16000;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "[ 1 ] Mount sdcard");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    // Initialize SD Card peripheral
    audio_board_sdcard_init(set, SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_ENCODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[3.0] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[3.1] Create fatfs stream to write data to sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_stream_writer = fatfs_stream_init(&fatfs_cfg);

    ESP_LOGI(TAG, "[3.2] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
#if defined (CONFIG_CHOICE_WAV_ENCODER)
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
    sample_rate = SAMPLE_RATE;
    if (CHANNEL == 1) {
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
        channel_format = AUDIO_DATA_FORMNAT_ONLY_RIGHT;
#else
        channel_format = AUDIO_DATA_FORMNAT_ONLY_LEFT;
#endif
    } else {
        channel_format = AUDIO_DATA_FORMNAT_RIGHT_LEFT;
    }
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
    sample_rate = SAMPLE_RATE;
    if (CHANNEL == 1) {
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
        channel_format = AUDIO_DATA_FORMNAT_ONLY_RIGHT;
#else
        channel_format = AUDIO_DATA_FORMNAT_ONLY_LEFT;
#endif
    } else {
        channel_format = AUDIO_DATA_FORMNAT_RIGHT_LEFT;
    }
#elif defined (CONFIG_CHOICE_AMR_NB_ENCODER)
    sample_rate = 8000;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    channel_format = AUDIO_DATA_FORMNAT_ONLY_RIGHT;
#else
    channel_format = AUDIO_DATA_FORMNAT_ONLY_LEFT;
#endif
#elif defined (CONFIG_CHOICE_AMR_WB_ENCODER)
    sample_rate = 16000;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    channel_format = AUDIO_DATA_FORMNAT_ONLY_RIGHT;
#else
    channel_format = AUDIO_DATA_FORMNAT_ONLY_LEFT;
#endif
#endif

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_port = 1;
#if (ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(4, 0, 0))
    channel_format = AUDIO_DATA_FORMNAT_ONLY_RIGHT;
#endif

#endif

    audio_data_format_set(&i2s_cfg, channel_format);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    i2s_cfg.std_cfg.clk_cfg.sample_rate_hz = sample_rate;
#else
    i2s_cfg.i2s_config.sample_rate = sample_rate;
#endif
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.3] Create audio encoder to handle data");
#if defined CONFIG_CHOICE_WAV_ENCODER
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    audio_encoder = wav_encoder_init(&wav_cfg);
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
    opus_encoder_cfg_t opus_cfg = DEFAULT_OPUS_ENCODER_CONFIG();
    opus_cfg.sample_rate        = SAMPLE_RATE;
    opus_cfg.channel            = CHANNEL;
    opus_cfg.bitrate            = BIT_RATE;
    opus_cfg.complexity         = COMPLEXITY;
#if defined (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    rsp_filter_cfg_t rsp_file_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_file_cfg.src_rate = SAMPLE_RATE;
    rsp_file_cfg.src_ch = 2;
    rsp_file_cfg.dest_rate = SAMPLE_RATE;
    rsp_file_cfg.dest_ch = 1;
    rsp_file_cfg.complexity = 0;
    rsp_file_cfg.down_ch_idx = 1;
    audio_element_handle_t resample = rsp_filter_init(&rsp_file_cfg);
    if (opus_cfg.channel == 2) {
        ESP_LOGE(TAG, "esp_lyrat_mini only support one channel");
        return;
    }
#endif
    audio_encoder = encoder_opus_init(&opus_cfg);
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
    aac_encoder_cfg_t aac_cfg = DEFAULT_AAC_ENCODER_CONFIG();
    aac_cfg.sample_rate        = SAMPLE_RATE;
    aac_cfg.channel            = CHANNEL;
    aac_cfg.bitrate            = BIT_RATE;
#if defined (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    rsp_filter_cfg_t rsp_file_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_file_cfg.src_rate = SAMPLE_RATE;
    rsp_file_cfg.src_ch = 2;
    rsp_file_cfg.dest_rate = SAMPLE_RATE;
    rsp_file_cfg.dest_ch = 1;
    rsp_file_cfg.complexity = 0;
    rsp_file_cfg.down_ch_idx = 1;
    audio_element_handle_t resample = rsp_filter_init(&rsp_file_cfg);
    if (aac_cfg.channel == 2) {
        ESP_LOGE(TAG, "esp_lyrat_mini only support one channel");
        return;
    }
#endif
    audio_encoder = aac_encoder_init(&aac_cfg);
#elif defined CONFIG_CHOICE_AMR_WB_ENCODER
    amrwb_encoder_cfg_t amrwb_enc_cfg = DEFAULT_AMRWB_ENCODER_CONFIG();
    audio_encoder = amrwb_encoder_init(&amrwb_enc_cfg);
#elif defined CONFIG_CHOICE_AMR_NB_ENCODER
    amrnb_encoder_cfg_t amrnb_enc_cfg = DEFAULT_AMRNB_ENCODER_CONFIG();
    audio_encoder = amrnb_encoder_init(&amrnb_enc_cfg);
#endif

    ESP_LOGI(TAG, "[3.4] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");

#if defined (CONFIG_CHOICE_WAV_ENCODER)
    audio_pipeline_register(pipeline, audio_encoder, "wav");
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
    audio_pipeline_register(pipeline, audio_encoder, "opus");
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
    audio_pipeline_register(pipeline, audio_encoder, "aac");
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    audio_pipeline_register(pipeline, resample, "res");
#endif
#elif defined (CONFIG_CHOICE_AMR_WB_ENCODER)
    audio_pipeline_register(pipeline, audio_encoder, "Wamr");
#elif defined (CONFIG_CHOICE_AMR_NB_ENCODER)
    audio_pipeline_register(pipeline, audio_encoder, "amr");
#endif
    audio_pipeline_register(pipeline, fatfs_stream_writer, "file");

    ESP_LOGI(TAG, "[3.5] Link it together [codec_chip]-->i2s_stream-->audio_encoder-->fatfs_stream-->[sdcard]");
#if defined (CONFIG_CHOICE_WAV_ENCODER)
    const char *link_tag[3] = {"i2s", "wav", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    const char *link_tag[4] = {"i2s", "res", "opus", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 4);
#else
    const char *link_tag[3] = {"i2s", "opus", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    const char *link_tag[4] = {"i2s", "res", "aac", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 4);
#else
    const char *link_tag[3] = {"i2s", "aac", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif
#elif defined (CONFIG_CHOICE_AMR_WB_ENCODER)
    const char *link_tag[3] = {"i2s", "Wamr", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#elif defined (CONFIG_CHOICE_AMR_NB_ENCODER)
    const char *link_tag[3] = {"i2s", "amr", "file"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif
    ESP_LOGI(TAG, "[3.6] Set music info to fatfs");
    audio_element_info_t music_info = {0};
    audio_element_getinfo(i2s_stream_reader, &music_info);
    ESP_LOGI(TAG, "[ * ] Save the recording info to the fatfs stream writer, sample_rates=%d, bits=%d, ch=%d",
                music_info.sample_rates, music_info.bits, music_info.channels);
    audio_element_setinfo(fatfs_stream_writer, &music_info);

    ESP_LOGI(TAG, "[3.7] Set up  uri");
#if defined (CONFIG_CHOICE_WAV_ENCODER)
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.wav");
#elif defined (CONFIG_CHOICE_OPUS_ENCODER)
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.opus");
#elif defined (CONFIG_CHOICE_AAC_ENCODER)
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.aac");
#elif defined (CONFIG_CHOICE_AMR_WB_ENCODER)
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.Wamr");
#elif defined (CONFIG_CHOICE_AMR_NB_ENCODER)
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/rec.amr");
#endif
    
    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[4.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events, record for %d Seconds", RECORD_TIME_SECONDS);
    int second_recorded = 0;
    while (1) {
        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 1000 / portTICK_RATE_MS) != ESP_OK) {
            second_recorded ++;
            ESP_LOGI(TAG, "[ * ] Recording ... %d", second_recorded);
            if (second_recorded >= RECORD_TIME_SECONDS) {
                audio_element_set_ringbuf_done(i2s_stream_reader);
            }
            continue;
        }
        /* Stop when the last pipeline element (fatfs_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) fatfs_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED)
                || ((int)msg.data == AEL_STATUS_ERROR_OPEN))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }
    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, audio_encoder);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, fatfs_stream_writer);

#if defined (CONFIG_CHOICE_OPUS_ENCODER) && defined (CONFIG_CHOICE_AAC_ENCODER) && defined (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    audio_pipeline_unregister(pipeline, resample);
#endif

    /* Terminal the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_writer);
    audio_element_deinit(i2s_stream_reader);
#if defined (CONFIG_CHOICE_OPUS_ENCODER) && defined (CONFIG_CHOICE_AAC_ENCODER) && defined (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD)
    audio_element_deinit(resample);
#endif
    audio_element_deinit(audio_encoder);
    esp_periph_set_destroy(set);
}
