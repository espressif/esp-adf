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

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_peripherals.h"
#include "esp_wifi_setting.h"
#include "esp_audio.h"
#include "audio_element.h"
#include "audio_mem.h"
#include "http_stream.h"
#include "a2dp_stream.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "mp3_decoder.h"
#include "wav_decoder.h"
#include "pcm_decoder.h"
#include "periph_wifi.h"
#include "periph_touch.h"
#include "wifi_service.h"

#include "blufi_config.h"
#include "input_key_service.h"
#include "board.h"
#include "sdkconfig.h"
#include "ble_gatts_module.h"

#if __has_include("esp_idf_version.h")
#include "esp_idf_version.h"
#else
#define ESP_IDF_VERSION_VAL(major, minor, patch) 1
#endif

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

#define SAMPLE_DEVICE_NAME "ESP_COEX_EXAMPLE"

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
    periph_service_handle_t wifi_serv = wifi_service_create(&cfg);

    int reg_idx = 0;
    esp_wifi_setting_handle_t wifi_setting_handle = blufi_config_create(NULL);
    esp_wifi_setting_regitster_notify_handle(wifi_setting_handle, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, wifi_setting_handle, &reg_idx);
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);
    wifi_service_setting_start(wifi_serv, 0);
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

    // Add decoders and encoders to esp_audio
    mp3_decoder_cfg_t  mp3_dec_cfg  = DEFAULT_MP3_DECODER_CONFIG();
    wav_decoder_cfg_t  wav_dec_cfg  = DEFAULT_WAV_DECODER_CONFIG();
    pcm_decoder_cfg_t  pcm_dec_cfg  = DEFAULT_PCM_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, pcm_decoder_init(&pcm_dec_cfg));

    // Create writers and add to esp_audio
    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.i2s_config.sample_rate = 44100;
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
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BTDM);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    ble_gatts_module_init();
    wifi_server_init();

    esp_err_t set_dev_name_ret = esp_bt_dev_set_device_name(SAMPLE_DEVICE_NAME);
    if (set_dev_name_ret) {
        ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
    }
    ESP_LOGI(TAG, "Create Bluetooth peripheral");
    handle->bt_periph = bt_create_periph();
    ESP_LOGI(TAG, "Start peripherals");
    esp_periph_start(handle->set, handle->bt_periph);

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

    coex_handle_t *handle = (coex_handle_t *)audio_malloc(sizeof(coex_handle_t));
    handle->work_mode = NONE_MODE;

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    handle->set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "create and start input key service");
    audio_board_key_init(handle->set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = handle->set;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);

    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, &input_key_service_cb, (void *)handle);

    a2dp_sink_blufi_start(handle);
}
