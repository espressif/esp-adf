/* Coex demo for wifi/bt/ble  mesh

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_peripherals.h"
#include "esp_wifi_setting.h"
#include "esp_audio.h"
#include "audio_pipeline.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "http_stream.h"
#include "a2dp_stream.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "hfp_stream.h"
#include "filter_resample.h"
#include "mp3_decoder.h"
#include "pcm_decoder.h"
#include "wifi_service.h"
#include "blufi_config.h"
#include "input_key_service.h"
#include "ble_gatts_module.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
#define HFP_RESAMPLE_RATE 16000
#else
#define HFP_RESAMPLE_RATE 8000
#endif

#define SAMPLE_DEVICE_NAME "ESP_ADF_COEX_EXAMPLE"

static const char *TAG = "COEX_EXAMPLE";

typedef enum {
    NONE_MODE = 0x0,
    BT_MODE = 0x01,
    WIFI_MODE = 0x02,
} work_mode_t;

typedef struct coex_handle {
    esp_periph_handle_t bt_periph;
    esp_periph_set_handle_t set;
    esp_audio_handle_t player;
    work_mode_t work_mode;
} coex_handle_t;

static bool g_wifi_connect_state = false;
static bool g_a2dp_connect_state = false;

static esp_wifi_setting_handle_t wifi_setting_handle = NULL;
static periph_service_handle_t wifi_serv = NULL;
static int play_pos = 0;
static coex_handle_t *g_coex_handle = NULL;

static void bt_app_hf_client_audio_open(hfp_data_enc_type_t type)
{
    ESP_LOGI(TAG, "bt_app_hf_client_audio_open type = %d", type);
    if (g_coex_handle) {
        esp_audio_state_t state = { 0 };
        esp_audio_state_get(g_coex_handle->player, &state);
        esp_audio_pos_get(g_coex_handle->player, &play_pos);
        if(state.status == AUDIO_STATUS_RUNNING || state.status == AUDIO_STATUS_PAUSED) {
            esp_audio_stop(g_coex_handle->player, TERMINATION_TYPE_NOW);
        }
          
        if (type == HF_DATA_CVSD) {
            esp_audio_play(g_coex_handle->player, AUDIO_CODEC_TYPE_DECODER, "hfp://8000:1@bt/hfp/stream.pcm", 0);
        } else if (type == HF_DATA_MSBC) {
            esp_audio_play(g_coex_handle->player, AUDIO_CODEC_TYPE_DECODER, "hfp://16000:1@bt/hfp/stream.pcm", 0);
        } else {
            ESP_LOGE(TAG, "error hfp enc type = %d", type);
        }
    }
}

static void bt_app_hf_client_audio_close(void)
{
    ESP_LOGI(TAG, "bt_app_hf_client_audio_close");
    if (g_coex_handle) {
        esp_audio_state_t state = { 0 };
        esp_audio_state_get(g_coex_handle->player, &state);
        if(state.status == AUDIO_STATUS_RUNNING || state.status == AUDIO_STATUS_PAUSED) {
            esp_audio_stop(g_coex_handle->player, TERMINATION_TYPE_NOW);
        }
        if (g_coex_handle->work_mode == BT_MODE) {
            if (g_a2dp_connect_state == true) {
                periph_bt_play(g_coex_handle->bt_periph);
                esp_audio_play(g_coex_handle->player, AUDIO_CODEC_TYPE_DECODER, "aadp://44100:2@bt/sink/stream.pcm", play_pos);
            }
        } else if (g_coex_handle->work_mode == WIFI_MODE) {
            if (g_wifi_connect_state == true) {
                esp_audio_play(g_coex_handle->player, AUDIO_CODEC_TYPE_DECODER, "https://dl.espressif.com/dl/audio/ff-16b-1c-44100hz.mp3", play_pos);
            }
        }
        play_pos = 0;
    }
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGI(TAG, "[ * ] input key id is %d, key type is %d", (int)evt->data, (int)evt->type);
    if (ctx == NULL) {
        return ESP_FAIL;
    }
    coex_handle_t *cx_handle = (coex_handle_t *)ctx;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_MODE:
                ESP_LOGI(TAG, "[ * ] [mode]");
                if (cx_handle->work_mode == NONE_MODE) {
                    ESP_LOGI(TAG, "[ * ] Enter BT mode");
                    cx_handle->work_mode = BT_MODE;
                    if (g_a2dp_connect_state == true) {
                        periph_bt_play(cx_handle->bt_periph);
                        esp_audio_play(cx_handle->player, AUDIO_CODEC_TYPE_DECODER, "aadp://44100:2@bt/sink/stream.pcm", 0);
                    }
                } else if (cx_handle->work_mode == BT_MODE) {
                    ESP_LOGI(TAG, "[ * ] Enter WIFI mode");
                    cx_handle->work_mode = WIFI_MODE;
                    if (g_a2dp_connect_state == true) {
                        periph_bt_pause(cx_handle->bt_periph);
                        vTaskDelay(300 / portTICK_RATE_MS);
                    }
                    esp_audio_stop(cx_handle->player, TERMINATION_TYPE_NOW);
                    if (g_wifi_connect_state == true) {
                        esp_audio_play(cx_handle->player, AUDIO_CODEC_TYPE_DECODER, "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3", 0);
                    }
                } else if (cx_handle->work_mode == WIFI_MODE) {
                    ESP_LOGI(TAG, "[ * ] Enter BT mode");
                    cx_handle->work_mode = BT_MODE;
                    esp_audio_stop(cx_handle->player, TERMINATION_TYPE_NOW);
                    if (g_a2dp_connect_state == true) {
                        periph_bt_play(cx_handle->bt_periph);
                        esp_audio_play(cx_handle->player, AUDIO_CODEC_TYPE_DECODER, "aadp://44100:2@bt/sink/stream.pcm", 0);
                    }
                }
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGI(TAG, "[ * ] [long Vol+] next");
                if (cx_handle->work_mode == BT_MODE) {
                    periph_bt_avrc_next(cx_handle->bt_periph);
                }
                break;

            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGI(TAG, "[ * ] [long Vol-] Previous");
                if (cx_handle->work_mode == BT_MODE) {
                    periph_bt_avrc_prev(cx_handle->bt_periph);
                }
                break;
            default:
                break;
        }

    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
        switch ((int) evt->data) {
            case INPUT_KEY_USER_ID_SET:
                ESP_LOGI(TAG, "[ * ] [Set] Setting Wi-Fi");
                ble_gatts_module_start_adv();
                blufi_set_sta_connected_flag(wifi_setting_handle, false);
                wifi_service_setting_start(wifi_serv, 0);
                break;
            default:
                break;
        }
    }
    return ESP_OK;
}

static esp_err_t wifi_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    ESP_LOGD(TAG, "event type:%d,source:%p, data:%p,len:%d,ctx:%p",
             evt->type, evt->source, evt->data, evt->len, ctx);
    if (evt->type == WIFI_SERV_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "WIFI_CONNECTED");
        g_wifi_connect_state = true;
        esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name(SAMPLE_DEVICE_NAME);
        if (set_dev_name_ret) {
            ESP_LOGE(TAG, "Set BT device name failed, error code = %x, line(%d)", set_dev_name_ret, __LINE__);
        }
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "WIFI_DISCONNECTED");
        g_wifi_connect_state = false;
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        ESP_LOGI(TAG, "WIFI_SETTING_TIMEOUT ");
    }
    return ESP_OK;
}

static void wifi_server_init(void)
{
    ESP_LOGI(TAG, "Blufi module init");
    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));

    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.extern_stack = true;
    cfg.evt_cb = wifi_service_cb;
    cfg.cb_ctx = NULL;
    cfg.setting_timeout_s = 60;
    wifi_serv = wifi_service_create(&cfg);

    int reg_idx = 0;
    wifi_setting_handle = blufi_config_create(NULL);
    esp_wifi_setting_register_notify_handle(wifi_setting_handle, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, wifi_setting_handle, &reg_idx);
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);
}

static void user_a2dp_sink_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    ESP_LOGI(TAG, "A2DP sink user cb");
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "A2DP disconnected");
                g_a2dp_connect_state = false;
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "A2DP connected");
                g_a2dp_connect_state = true;
            }
            break;
        default:
            ESP_LOGI(TAG, "User cb A2DP event: %d", event);
            break;
    }
}

static esp_audio_handle_t setup_player()
{
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = 44100;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    esp_audio_handle_t player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    // Create readers and add to esp_audio
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    a2dp_stream_config_t a2dp_config = {
        .type = AUDIO_STREAM_READER,
        .user_callback.user_a2d_cb = user_a2dp_sink_cb,
    };
    audio_element_handle_t bt_stream_reader = a2dp_stream_init(&a2dp_config);
    esp_audio_input_stream_add(player, bt_stream_reader);
    
    hfp_stream_config_t hfp_config;
    hfp_config.type = INCOMING_STREAM;
    audio_element_handle_t hfp_in_stream = hfp_stream_init(&hfp_config);
    esp_audio_input_stream_add(player, hfp_in_stream);

    // Add decoders and encoders to esp_audio
    mp3_decoder_cfg_t  mp3_dec_cfg  = DEFAULT_MP3_DECODER_CONFIG();
    pcm_decoder_cfg_t  pcm_dec_cfg  = DEFAULT_PCM_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, pcm_decoder_init(&pcm_dec_cfg));

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    raw_stream_cfg_t raw_writer = RAW_STREAM_CFG_DEFAULT();
    raw_writer.type = AUDIO_STREAM_WRITER;
    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_writer));
    esp_audio_output_stream_add(player, raw_stream_init(&raw_writer));

    esp_audio_vol_set(player, 60);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p\r\n", player);
    return player;
}

static void a2dp_sink_blufi_start(coex_handle_t *handle)
{
    if (handle == NULL) {
        return;
    }
    ESP_LOGI(TAG, "[4.1] Init Bluetooth");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    esp_bluedroid_init();
    esp_bluedroid_enable();
    
    ESP_LOGI(TAG, "[4.2] init gatts");
    ble_gatts_module_init();
    wifi_server_init();

    esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name(SAMPLE_DEVICE_NAME);
    if (set_dev_name_ret) {
        ESP_LOGE(TAG, "Set BT device name failed, error code = %x, line(%d)", set_dev_name_ret, __LINE__);
    }
    ESP_LOGI(TAG, "[4.3] Create Bluetooth peripheral");
    handle->bt_periph = bt_create_periph();

    ESP_LOGI(TAG, "[4.4] Start peripherals");
    esp_periph_start(handle->set, handle->bt_periph);
    
    ESP_LOGI(TAG, "[4.5] init hfp_stream");
    hfp_open_and_close_evt_cb_register(bt_app_hf_client_audio_open, bt_app_hf_client_audio_close);
    hfp_service_init();

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
#else
    esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
#endif

    handle->player = setup_player();
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
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    
    ESP_LOGI(TAG, "[ 1 ] Create coex handle for a2dp-gatt-wifi");
    g_coex_handle = (coex_handle_t *)audio_malloc(sizeof(coex_handle_t));
    AUDIO_MEM_CHECK(TAG, g_coex_handle, return);
    g_coex_handle->work_mode = NONE_MODE;
    
    ESP_LOGI(TAG, "[ 2 ] Initialize peripherals");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.extern_stack = true;
    g_coex_handle->set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 3 ] create and start input key service");
    audio_board_key_init(g_coex_handle->set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = g_coex_handle->set;
    input_cfg.based_cfg.task_stack = 2048;
    input_cfg.based_cfg.extern_stack = true;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, &input_key_service_cb, (void *)g_coex_handle);

    ESP_LOGI(TAG, "[ 4 ] Start a2dp and blufi network");
    a2dp_sink_blufi_start(g_coex_handle);
    
    ESP_LOGI(TAG, "[ 5 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline_out = audio_pipeline_init(&pipeline_cfg);
    
    ESP_LOGI(TAG, "[5.1] Create i2s stream to read data from codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT_WITH_PARA(CODEC_ADC_I2S_PORT, 44100, 16, AUDIO_STREAM_READER);
    audio_element_handle_t i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = 44100;
    rsp_cfg.src_ch = 2;
    rsp_cfg.dest_rate = HFP_RESAMPLE_RATE;
    rsp_cfg.dest_ch = 1;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

    ESP_LOGI(TAG, "[5.2] Create hfp stream");
    hfp_stream_config_t hfp_config;
    hfp_config.type = OUTGOING_STREAM;
    audio_element_handle_t hfp_out_stream = hfp_stream_init(&hfp_config);
    
    ESP_LOGI(TAG, "[5.3] Register i2s reader and hfp outgoing to audio pipeline");
    audio_pipeline_register(pipeline_out, i2s_stream_reader, "i2s_reader");
    audio_pipeline_register(pipeline_out, filter, "filter");
    audio_pipeline_register(pipeline_out, hfp_out_stream, "outgoing");
    
    ESP_LOGI(TAG, "[5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]");
    const char *link_out[3] = {"i2s_reader", "filter", "outgoing"};
    audio_pipeline_link(pipeline_out, &link_out[0], 3);

    ESP_LOGI(TAG, "[5.5] Start audio_pipeline out"); 
    audio_pipeline_run(pipeline_out);
}
