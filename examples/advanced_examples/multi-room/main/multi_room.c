/* Multi-Room Music Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"

#include "esp_peripherals.h"
#include "periph_is31fl3216.h"
#include "periph_wifi.h"
#include "input_key_service.h"

#include "esp_audio.h"
#include "esp_decoder.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "http_stream.h"
#include "audio_mem.h"
#include "audio_thread.h"
#include "media_lib_adapter.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0))

#include "esp_mrm_client.h"
#include "esp_netif.h"

#define DEFAULT_PLAY_URL "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3"
#define ESP_READ_BUFFER_SIZE    4096

static const char *TAG = "MRM_EXAMPLE";

static bool                             play_task_run;
static esp_audio_handle_t               player;
static esp_mrm_client_handle_t          mrm_client;
static audio_element_handle_t           player_raw_in_h, i2s_h, http_stream_reader;

static void setup_wifi(esp_periph_set_handle_t set)
{
    ESP_LOGI(TAG, "Start Wi-Fi");
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = true,
        .wifi_config.sta.ssid = CONFIG_WIFI_SSID,
        .wifi_config.sta.password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
}

static int _player_get_pts()
{
    int time;
    esp_audio_time_get(player, &time);
    return time;
}

static void _multi_room_play_task(void *para)
{
    char *buf = audio_calloc(1, ESP_READ_BUFFER_SIZE);
    AUDIO_MEM_CHECK(TAG, buf, vTaskDelete(NULL); return);

    while (play_task_run) {
        int ret = audio_element_input(http_stream_reader, buf, ESP_READ_BUFFER_SIZE);
        if (AEL_IO_OK == ret) {
            audio_element_set_ringbuf_done(player_raw_in_h);
            audio_element_finish_state(player_raw_in_h);
            break;
        }
        raw_stream_write(player_raw_in_h, buf, ESP_READ_BUFFER_SIZE);
    }

    audio_element_process_deinit(http_stream_reader);
    audio_element_stop(http_stream_reader);
    free(buf);

    esp_mrm_client_master_stop(mrm_client);
    esp_mrm_client_slave_stop(mrm_client);
    ESP_LOGI(TAG, "_multi_room_play_task stop");
    vTaskDelete(NULL);
}

static esp_err_t multi_room_play_start(const char *url)
{
    audio_element_set_uri(http_stream_reader, url);
    audio_element_process_init(http_stream_reader);
    audio_element_run(http_stream_reader);

    play_task_run = true;
    if (audio_thread_create(NULL,
                            "multi_room_play", _multi_room_play_task,
                            NULL,
                            DEFAULT_MRM_TASK_STACK,
                            DEFAULT_MRM_TASK_PRIO,
                            true,
                            0) != ESP_OK) {
        ESP_LOGE(TAG, "Can not start multi_room_play service");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    audio_board_handle_t board_handle = (audio_board_handle_t) ctx;
    int player_volume;
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGD(TAG, "[ * ] input key id is %d", (int)evt->data);
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_PLAY:
            case INPUT_KEY_USER_ID_REC:
                ESP_LOGI(TAG, "[ * ] [Play] input key event");
                esp_mrm_client_master_start(mrm_client, DEFAULT_PLAY_URL);
                multi_room_play_start(DEFAULT_PLAY_URL);
                break;
            case INPUT_KEY_USER_ID_MODE:
            case INPUT_KEY_USER_ID_SET:
                ESP_LOGI(TAG, "[ * ] [Set] input key event");
                play_task_run = false;
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGD(TAG, "[ * ] [Vol+] input key event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGD(TAG, "[ * ] [Vol-] input key event");
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
        }
    }

    return ESP_OK;
}

static void setup_player(esp_periph_set_handle_t set)
{
    if (player) {
        return ;
    }
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = 48000;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    audio_board_key_init(set);
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)board_handle);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.task_stack = 0;
    http_cfg.out_rb_size = 100 * 1024;
    http_stream_reader = http_stream_init(&http_cfg);

    raw_stream_cfg_t raw_reader = RAW_STREAM_CFG_DEFAULT();
    raw_reader.type = AUDIO_STREAM_READER;
    raw_reader.out_rb_size = 100 * 1024;
    player_raw_in_h = raw_stream_init(&raw_reader);
    esp_audio_input_stream_add(player, player_raw_in_h);

    // Add decoders and encoders to esp_audio
    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    auto_dec_cfg.out_rb_size = 50 * 1024;
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, esp_decoder_init(&auto_dec_cfg, auto_decode, 10));

    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;
    i2s_h = i2s_stream_init(&i2s_writer);
    i2s_stream_set_clk(i2s_h, 48000, 16, 2);
    esp_audio_output_stream_add(player, i2s_h);

    // Set default volume
    esp_audio_vol_set(player, 40);
}

static int _mrm_event_handler(mrm_event_msg_t *event, void *ctx)
{
    int64_t tsf_time = 0;
    int sync = 0;

    switch ((int)event->type) {
        case MRM_EVENT_SET_URL:
            ESP_LOGI(TAG, "slave set url %s", (char *)event->data);
            multi_room_play_start((char *)event->data);
            break;
        case MRM_EVENT_GET_PTS:
            *(int *)event->data = _player_get_pts();
            break;
        case MRM_EVENT_GET_TSF:
            tsf_time = esp_wifi_get_tsf_time(ESP_IF_WIFI_STA);
            *(int64_t *)event->data = tsf_time / 1000;
            break;
        case MRM_EVENT_SET_SYNC:
            sync = *(int *)event->data;
            ESP_LOGD(TAG, "slave got sync %d", sync);
            break;
        case MRM_EVENT_SYNC_FAST:
            sync = *(int *)event->data;
            if (sync < -200) {
                sync = -200;
            }
            i2s_stream_sync_delay(i2s_h, sync);
            break;
        case MRM_EVENT_SYNC_SLOW:
            sync = *(int *)event->data;
            if (sync > 200) {
                sync = 200;
            }
            i2s_stream_sync_delay(i2s_h, sync);
            break;
        case MRM_EVENT_PLAY_STOP:
            play_task_run = false;
            break;
    }

    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    media_lib_add_default_adapter();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

#if defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD
    periph_is31fl3216_cfg_t is31fl3216_cfg = { 0 };
    is31fl3216_cfg.state = IS31FL3216_STATE_BY_AUDIO;
    esp_periph_handle_t is31fl3216_periph = periph_is31fl3216_init(&is31fl3216_cfg);
    periph_is31fl3216_set_state(is31fl3216_periph, IS31FL3216_STATE_BY_AUDIO);
    esp_periph_start(set, is31fl3216_periph);
#endif

    setup_wifi(set);
    setup_player(set);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, "raw://http/audio", 0);

    esp_mrm_client_config_t config = {
        .event_handler = _mrm_event_handler,
        .group_addr = DEFAULT_MRM_GROUP_ADDR,
        .sync_sock_port = DEFAULT_MRM_SYNC_SOCK_PORT,
        .ctx = NULL,
    };
    mrm_client = esp_mrm_client_create(&config);

    esp_mrm_client_slave_start(mrm_client);
}
#else
void app_main(void)
{
    ESP_LOGE("MRM_EXAMPLE", "Not found right IDF version, please compile this routine with IDF version above v4.2");
}
#endif
