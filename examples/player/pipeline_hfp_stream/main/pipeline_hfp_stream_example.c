/* Play music from Bluetooth device

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_peripherals.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "i2s_stream.h"
#include "board.h"
#include "hfp_stream.h"

#include "audio_idf_version.h"

static const char *TAG = "HFP_EXAMPLE";

static audio_element_handle_t hfp_in_stream, hfp_out_stream, i2s_stream_writer, i2s_stream_reader;
static audio_pipeline_handle_t pipeline_in, pipeline_out;
static int g_hfp_audio_rate = 16000;

static void bt_app_hf_client_audio_open(hfp_data_enc_type_t type)
{
    ESP_LOGI(TAG, "bt_app_hf_client_audio_open type = %d", type);
    if (type == HF_DATA_CVSD) {
        g_hfp_audio_rate = 8000;
    } else if (type == HF_DATA_MSBC) {
        g_hfp_audio_rate = 16000;
    } else {
        ESP_LOGE(TAG, "error hfp enc type = %d", type);
    }
    i2s_stream_set_clk(i2s_stream_reader, g_hfp_audio_rate, 16, 1);
    i2s_stream_set_clk(i2s_stream_writer, g_hfp_audio_rate, 16, 1);
    audio_pipeline_run(pipeline_in);
    audio_pipeline_run(pipeline_out);
}

static void bt_app_hf_client_audio_close(void)
{
    ESP_LOGI(TAG, "bt_app_hf_client_audio_close");
    audio_pipeline_pause(pipeline_out);
    audio_pipeline_pause(pipeline_in);
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] init Bluetooth");
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        AUDIO_ERROR(TAG, "initialize controller failed");
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        AUDIO_ERROR(TAG, "enable controller failed");
    }

    if (esp_bluedroid_init() != ESP_OK) {
        AUDIO_ERROR(TAG, "initialize bluedroid failed");
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        AUDIO_ERROR(TAG, "enable bluedroid failed");
    }

    esp_bt_dev_set_device_name("ESP-ADF-HFP");
    hfp_open_and_close_evt_cb_register(bt_app_hf_client_audio_open, bt_app_hf_client_audio_close);
    hfp_service_init();
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
    esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline_in = audio_pipeline_init(&pipeline_cfg);
    pipeline_out = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1] Create i2s stream to write data to codec chip and read data from codec chip");
    i2s_stream_cfg_t i2s_cfg1 = I2S_STREAM_CFG_DEFAULT();

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        i2s_cfg1.chan_cfg.id = CODEC_ADC_I2S_PORT;
        i2s_cfg1.std_cfg.clk_cfg.sample_rate_hz = g_hfp_audio_rate;
        i2s_cfg1.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
#if defined(CONFIG_ESP_LYRAT_MINI_V1_1_BOARD) || defined(CONFIG_ESP_LYRATD_MSC_V2_1_BOARD) || defined(CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
        i2s_cfg1.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
#else
        i2s_cfg1.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
#endif
#else
        i2s_cfg1.i2s_port = CODEC_ADC_I2S_PORT;
        i2s_cfg1.i2s_config.sample_rate = g_hfp_audio_rate;
#if defined(CONFIG_ESP_LYRAT_MINI_V1_1_BOARD) || defined(CONFIG_ESP_LYRATD_MSC_V2_1_BOARD) || defined(CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
        i2s_cfg1.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#else
        i2s_cfg1.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
#endif
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))

    i2s_stream_reader = i2s_stream_init(&i2s_cfg1);

    i2s_stream_cfg_t i2s_cfg2 = I2S_STREAM_CFG_DEFAULT();
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
    i2s_cfg2.std_cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
    i2s_cfg2.std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    i2s_cfg2.std_cfg.clk_cfg.sample_rate_hz = g_hfp_audio_rate;
#else
    i2s_cfg2.i2s_config.sample_rate = g_hfp_audio_rate;
    i2s_cfg2.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
#endif
    i2s_cfg2.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg2);

    ESP_LOGI(TAG, "[3.2] Get hfp stream");
    hfp_stream_config_t hfp_config;
    hfp_config.type = OUTGOING_STREAM;
    hfp_out_stream = hfp_stream_init(&hfp_config);
    hfp_config.type = INCOMING_STREAM;
    hfp_in_stream = hfp_stream_init(&hfp_config);

    ESP_LOGI(TAG, "[3.2] Register all elements to audio pipeline");
    audio_pipeline_register(pipeline_in, hfp_in_stream, "incoming");
    audio_pipeline_register(pipeline_in, i2s_stream_writer, "i2s_writer");

    audio_pipeline_register(pipeline_out, i2s_stream_reader, "i2s_reader");
    audio_pipeline_register(pipeline_out, hfp_out_stream, "outgoing");

    ESP_LOGI(TAG, "[3.3] Link it together [Bluetooth]-->hfp_in_stream-->i2s_stream_writer-->[codec_chip]");
    const char *link_in[2] = {"incoming", "i2s_writer"};
    audio_pipeline_link(pipeline_in, &link_in[0], 2);
    const char *link_out[2] = {"i2s_reader", "outgoing"};
    audio_pipeline_link(pipeline_out, &link_out[0], 2);

    ESP_LOGI(TAG, "[ 4 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 5 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[5.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline_in, evt);
    audio_pipeline_set_listener(pipeline_out, evt);

    ESP_LOGI(TAG, "[5.2] Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) i2s_stream_writer
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS && (int) msg.data == AEL_STATUS_STATE_STOPPED) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline_in);
    audio_pipeline_wait_for_stop(pipeline_in);
    audio_pipeline_terminate(pipeline_in);
    audio_pipeline_stop(pipeline_out);
    audio_pipeline_wait_for_stop(pipeline_out);
    audio_pipeline_terminate(pipeline_out);

    audio_pipeline_unregister(pipeline_in, hfp_in_stream);
    audio_pipeline_unregister(pipeline_in, i2s_stream_writer);

    audio_pipeline_unregister(pipeline_out, i2s_stream_reader);
    audio_pipeline_unregister(pipeline_out, hfp_out_stream);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline_in);
    audio_pipeline_remove_listener(pipeline_out);

    /* Stop all peripherals before removing the listener */
    esp_periph_set_stop_all(set);
    audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline_in);
    audio_pipeline_deinit(pipeline_out);
    audio_element_deinit(hfp_in_stream);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(hfp_out_stream);
    esp_periph_set_destroy(set);
}
