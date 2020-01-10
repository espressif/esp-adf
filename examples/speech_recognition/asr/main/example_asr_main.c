/* Examples of speech recognition with multiple keywords.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "board.h"
#include "audio_common.h"
#include "audio_pipeline.h"
#include "mp3_decoder.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "rec_eng_helper.h"

static const char *TAG = "example_asr_keywords";
static const char *EVENT_TAG = "asr_event";

typedef enum {
    WAKE_UP = 1,
    OPEN_THE_LIGHT,
    CLOSE_THE_LIGHT,
    VOLUME_INCREASE,
    VOLUME_DOWN,
    PLAY,
    PAUSE,
    MUTE,
    PLAY_LOCAL_MUSIC,
} asr_event_t;

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
    esp_log_level_set(EVENT_TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Initialize SR handle");
    esp_wn_iface_t *wakenet;
    model_coeff_getter_t *model_coeff_getter;
    model_iface_data_t *model_data;

    get_wakenet_iface(&wakenet);
    get_wakenet_coeff(&model_coeff_getter);
    model_data = wakenet->create(model_coeff_getter, DET_MODE_90);
    int num = wakenet->get_word_num(model_data);
    for (int i = 1; i <= num; i++) {
        char *name = wakenet->get_word_name(model_data, i);
        ESP_LOGI(TAG, "keywords: %s (index = %d)", name, i);
    }
    float threshold = wakenet->get_det_threshold(model_data, 1);
    int sample_rate = wakenet->get_samp_rate(model_data);
    int audio_chunksize = wakenet->get_samp_chunksize(model_data);
    ESP_LOGI(EVENT_TAG, "keywords_num = %d, threshold = %f, sample_rate = %d, chunksize = %d, sizeof_uint16 = %d", num, threshold, sample_rate, audio_chunksize, sizeof(int16_t));
    int16_t *buff = (int16_t *)malloc(audio_chunksize * sizeof(short));
    if (NULL == buff) {
        ESP_LOGE(EVENT_TAG, "Memory allocation failed!");
        wakenet->destroy(model_data);
        model_data = NULL;
        return;
    }

    ESP_LOGI(EVENT_TAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader, filter, raw_read;

    ESP_LOGI(EVENT_TAG, "[ 2.0 ] Create audio pipeline for recording");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(EVENT_TAG, "[ 2.1 ] Create i2s stream to read audio data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 48000;
    i2s_cfg.type = AUDIO_STREAM_READER;

    // Mini board record by I2S1 and play music by I2S0, no need to add resample element.
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_config.sample_rate = 16000;
    i2s_cfg.i2s_port = 1;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
#else
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    ESP_LOGI(EVENT_TAG, "[ 2.2 ] Create filter to resample audio data");
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 48000;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = 16000;
    rsp_cfg.dest_ch = 1;
    filter = rsp_filter_init(&rsp_cfg);
#endif

    ESP_LOGI(EVENT_TAG, "[ 2.3 ] Create raw to receive data");
    raw_stream_cfg_t raw_cfg = {
        .out_rb_size = 8 * 1024,
        .type = AUDIO_STREAM_READER,
    };
    raw_read = raw_stream_init(&raw_cfg);

    ESP_LOGI(EVENT_TAG, "[ 3 ] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s");
    audio_pipeline_register(pipeline, raw_read, "raw");

#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    ESP_LOGI(EVENT_TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->raw-->[SR]");
    audio_pipeline_link(pipeline, (const char *[]) {"i2s",  "raw"}, 2);
#else
    audio_pipeline_register(pipeline, filter, "filter");
    ESP_LOGI(EVENT_TAG, "[ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[SR]");
    audio_pipeline_link(pipeline, (const char *[]) {"i2s", "filter", "raw"}, 3);
#endif

    ESP_LOGI(EVENT_TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);
    while (1) {
        raw_stream_read(raw_read, (char *)buff, audio_chunksize * sizeof(short));
        int keyword = wakenet->detect(model_data, (int16_t *)buff);
        switch (keyword) {
            case WAKE_UP:
                ESP_LOGI(TAG, "Wake up");
                break;
            case OPEN_THE_LIGHT:
                ESP_LOGI(TAG, "Turn on the light");
#if defined CONFIG_ESP_LYRAT_V4_3_BOARD
                gpio_set_level(get_green_led_gpio(), 1);
#endif
                break;
            case CLOSE_THE_LIGHT:
                ESP_LOGI(TAG, "Turn off the light");
#if defined CONFIG_ESP_LYRAT_V4_3_BOARD
                gpio_set_level(get_green_led_gpio(), 0);
#endif
                break;
            case VOLUME_INCREASE:
                ESP_LOGI(TAG, "volume increase");
                break;
            case VOLUME_DOWN:
                ESP_LOGI(TAG, "volume down");
                break;
            case PLAY:
                ESP_LOGI(TAG, "play");
                break;
            case PAUSE:
                ESP_LOGI(TAG, "pause");
                break;
            case MUTE:
                ESP_LOGI(TAG, "mute");
                break;
            case PLAY_LOCAL_MUSIC:
                ESP_LOGI(TAG, "play local music");
                break;
            default:
                ESP_LOGD(TAG, "Not supported keyword");
                break;
        }
    }

    ESP_LOGI(EVENT_TAG, "[ 6 ] Stop audio_pipeline");

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

    ESP_LOGI(EVENT_TAG, "[ 7 ] Destroy model");
    wakenet->destroy(model_data);
    model_data = NULL;
    free(buff);
    buff = NULL;
}
