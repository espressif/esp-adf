/* Examples of speech recognition with multiple keywords.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board.h"
#include "audio_pipeline.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "esp_audio.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "mp3_decoder.h"
#include "filter_resample.h"
#include "rec_eng_helper.h"

static const char *TAG = "example_asr_keywords";

typedef enum {
    WAKE_UP = 1,
} asr_wakenet_event_t;

typedef enum {
    ID0_DAKAIKONGTIAO       = 0,
    ID1_GUANBIKONGTIAO      = 1,
    ID2_ZENGDAFENGSU        = 2,
    ID3_JIANXIOAFENGSU      = 3,
    ID4_SHENGGAOYIDU        = 4,
    ID5_JIANGDIYIDU         = 5,
    ID6_ZHIREMOSHI          = 6,
    ID7_ZHILENGMOSHI        = 7,
    ID8_SONGFENGMOSHI       = 8,
    ID9_JIENENGMOSHI        = 9,
    ID10_GUANBIJIENENGMOSHI = 10,
    ID11_CHUSHIMOSHI        = 11,
    ID12_GUANBICHUSHIMOSHI  = 12,
    ID13_DAKAILANYA         = 13,
    ID14_GUANBILANYA        = 14,
    ID15_BOFANGGEQU         = 15,
    ID16_ZANTINGBOFANG      = 16,
    ID17_DINGSHIYIXIAOSHI   = 17,
    ID18_DAKAIDIANDENG      = 18,
    ID19_GUANBIDIANDENG     = 19,
    ID_MAX,
} asr_multinet_event_t;

static esp_err_t asr_multinet_control(int commit_id);

static esp_audio_handle_t setup_player()
{
    esp_audio_handle_t player = NULL;
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    cfg.resample_rate = 16000;
#else
    cfg.resample_rate = 48000;
#endif
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    raw_stream_cfg_t raw_reader = RAW_STREAM_CFG_DEFAULT();
    raw_reader.type = AUDIO_STREAM_READER;
    esp_audio_input_stream_add(player, raw_stream_init(&raw_reader));
    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    mp3_decoder_cfg_t  mp3_dec_cfg  = DEFAULT_MP3_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_writer.i2s_config.sample_rate = 16000;
#else
    i2s_writer.i2s_config.sample_rate = 48000;
#endif

    i2s_writer.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_writer);
    esp_audio_output_stream_add(player, i2s_stream_writer);
    esp_audio_vol_set(player, 60);

    ESP_LOGI(TAG, "esp_audio instance is:%p\r\n", player);
    return player;
}

void app_main()
{

#if defined CONFIG_ESP_LYRAT_V4_3_BOARD
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1UL << get_green_led_gpio(),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = 0
    };
    gpio_config(&gpio_conf);
#endif

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Initialize SR wn handle");
    esp_wn_iface_t *wakenet;
    model_coeff_getter_t *model_coeff_getter;
    model_iface_data_t *model_wn_data;
    const esp_mn_iface_t *multinet = &MULTINET_MODEL;

    get_wakenet_iface(&wakenet);
    get_wakenet_coeff(&model_coeff_getter);
    model_wn_data = wakenet->create(model_coeff_getter, DET_MODE_90);
    int wn_num = wakenet->get_word_num(model_wn_data);
    for (int i = 1; i <= wn_num; i++) {
        char *name = wakenet->get_word_name(model_wn_data, i);
        ESP_LOGI(TAG, "keywords: %s (index = %d)", name, i);
    }
    float wn_threshold = wakenet->get_det_threshold(model_wn_data, 1);
    int wn_sample_rate = wakenet->get_samp_rate(model_wn_data);
    int audio_wn_chunksize = wakenet->get_samp_chunksize(model_wn_data);
    ESP_LOGI(TAG, "keywords_num = %d, threshold = %f, sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", wn_num, wn_threshold, wn_sample_rate, audio_wn_chunksize, sizeof(int16_t));

    model_iface_data_t *model_mn_data = multinet->create(&MULTINET_COEFF, 6000);
    int audio_mn_chunksize = multinet->get_samp_chunksize(model_mn_data);
    int mn_num = multinet->get_samp_chunknum(model_mn_data);
    int mn_sample_rate = multinet->get_samp_rate(model_mn_data);
    ESP_LOGI(TAG, "keywords_num = %d , sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", mn_num,  mn_sample_rate, audio_mn_chunksize, sizeof(int16_t));

    int size = audio_wn_chunksize;
    if (audio_mn_chunksize > audio_wn_chunksize) {
        size = audio_mn_chunksize;
    }
    int16_t *buffer = (int16_t *)malloc(size * sizeof(short));

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader, filter, raw_read;
    esp_audio_handle_t player;
    bool enable_wn = true;
    uint32_t mn_count = 0;

    ESP_LOGI(TAG, "[ 1 ] Start codec chip");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    audio_board_sdcard_init(esp_periph_set_init(&periph_cfg), SD_MODE_1_LINE);

    ESP_LOGI(TAG, "[ 2.0 ] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "[ 2.1 ] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_cfg.type = AUDIO_STREAM_READER;

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_config.sample_rate = 16000;
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
#else
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    ESP_LOGI(TAG, "[ 2.2 ] Create filter to resample audio data");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 1;
    filter = rsp_filter_init(&rsp_cfg);
#endif

    ESP_LOGI(TAG, "[ 2.3 ] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = {
        .out_rb_size = 8 * 1024,
        .type = AUDIO_STREAM_READER,
    };
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    ESP_LOGI(TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->raw-->[SR]");
    const char *link_tag[2] = {"i2s", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
#else
    audio_pipeline_register(pipeline, filter, "filter");
    ESP_LOGI(TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[SR]");
    const char *link_tag[3] = {"i2s", "filter", "raw"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);
#endif

    player = setup_player();

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    while (1) {
        raw_stream_read(raw_read, (char *)buffer, size * sizeof(short));
        if (enable_wn) {
            if (wakenet->detect(model_wn_data, (int16_t *)buffer) ==  WAKE_UP) {
                esp_audio_sync_play(player, "file://sdcard/dingdong.mp3", 0);
                ESP_LOGI(TAG, "wake up");
                enable_wn = false;
            }
        } else {
            mn_count++;
            int commit_id = multinet->detect(model_mn_data, buffer);
            if (asr_multinet_control(commit_id) == ESP_OK ) {
                esp_audio_sync_play(player, "file://sdcard/haode.mp3", 0);
                enable_wn = true;
                mn_count = 0;
            }
            if (mn_count == mn_num) {
                ESP_LOGI(TAG, "stop multinet");
                enable_wn = true;
                mn_count = 0;
            }
        }
    }

    ESP_LOGI(TAG, "[ 6 ] Stop audio_pipeline");

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_unregister(pipeline, raw_read);
    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, filter);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(raw_read);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(filter);

    ESP_LOGI(TAG, "[ 7 ] Destroy model");
    wakenet->destroy(model_wn_data);
    model_wn_data = NULL;
    free(buffer);
    buffer = NULL;
}

static esp_err_t asr_multinet_control(int commit_id)
{
    if (commit_id >=0 && commit_id < ID_MAX) {
        switch (commit_id) {
            case ID0_DAKAIKONGTIAO:
                ESP_LOGI(TAG, "turn on air-condition");
                break;
            case ID1_GUANBIKONGTIAO:
                ESP_LOGI(TAG, "turn off air-condition");
                break;
            case ID2_ZENGDAFENGSU:
                ESP_LOGI(TAG, "increase in speed");
                break;
            case ID3_JIANXIOAFENGSU:
                ESP_LOGI(TAG, "decrease in speed");
                break;
            case ID4_SHENGGAOYIDU:
                ESP_LOGI(TAG, "increase in temperature");
                break;
            case ID5_JIANGDIYIDU:
                ESP_LOGI(TAG, "decrease in temperature");
                break;
            case ID6_ZHIREMOSHI:
                ESP_LOGI(TAG, "hot mode");
                break;
            case ID7_ZHILENGMOSHI:
                ESP_LOGI(TAG, "slow mode");
                break;
            case ID8_SONGFENGMOSHI:
                ESP_LOGI(TAG, "blower mode");
                break;
            case ID9_JIENENGMOSHI:
                ESP_LOGI(TAG, "save power mode");
                break;
            case ID10_GUANBIJIENENGMOSHI:
                ESP_LOGI(TAG, "turn off save power mode");
                break;
            case ID11_CHUSHIMOSHI:
                ESP_LOGI(TAG, "dampness mode");
                break;
            case ID12_GUANBICHUSHIMOSHI:
                ESP_LOGI(TAG, "turn off dampness mode");
                break;
            case ID13_DAKAILANYA:
                ESP_LOGI(TAG, "turn on bt");
                break;
            case ID14_GUANBILANYA:
                ESP_LOGI(TAG, "turn off bt");
                break;
            case ID15_BOFANGGEQU:
                ESP_LOGI(TAG, "turn on");
                break;
            case ID16_ZANTINGBOFANG:
                ESP_LOGI(TAG, "turn off");
                break;
            case ID17_DINGSHIYIXIAOSHI:
                ESP_LOGI(TAG, "timer one hour");
                break;
            case ID18_DAKAIDIANDENG:
                ESP_LOGI(TAG, "turn on lignt");
                break;
            case ID19_GUANBIDIANDENG:
                ESP_LOGI(TAG, "turn off lignt");
                break;
            default:
                ESP_LOGI(TAG, "not supportint mode");
                break;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}
