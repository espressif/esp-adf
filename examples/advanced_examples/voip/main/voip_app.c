/* VoIP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"
#include "es8311.h"
#include "input_key_service.h"

#include "raw_stream.h"
#include "filter_resample.h"
#include "algorithm_stream.h"
#include "fatfs_stream.h"
#include "wav_encoder.h"
#include "g711_decoder.h"
#include "g711_encoder.h"
#include "esp_sip.h"

#include "wifi_service.h"
#include "smart_config.h"

#include "audio_tone_uri.h"
#include "audio_player_int_tone.h"
#include "media_lib_adapter.h"
#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

static const char *TAG = "VOIP_EXAMPLE";

/* Debug original input/output data for AEC feature*/
#define DEBUG_AEC_INPUT     0
#define DEBUG_AEC_OUTPUT    0

#define I2S_SAMPLE_RATE     8000
#define I2S_BITS            CODEC_ADC_BITS_PER_SAMPLE
#define I2S0_CHANNELS       I2S_CHANNEL_FMT_ONLY_LEFT
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
#define I2S1_CHANNELS       I2S_CHANNEL_FMT_RIGHT_LEFT
#endif

/* The AEC internal buffering mechanism requires that the recording signal
   is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal. */
#define DEFAULT_REF_DELAY_MS    0
#define ESP_RING_BUFFER_SIZE    256

#define SIP_URI CONFIG_SIP_URI

#if !RECORD_HARDWARE_AEC
static ringbuf_handle_t ringbuf_ref;
#endif

static sip_handle_t sip;
static audio_element_handle_t raw_read, raw_write, element_algo;
static audio_pipeline_handle_t recorder, player;
static bool mute, is_smart_config;
static display_service_handle_t disp;
static periph_service_handle_t wifi_serv;

static esp_err_t i2s_driver_init(i2s_port_t port, i2s_channel_fmt_t channels, i2s_bits_per_sample_t bits)
{
    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = bits,
        .channel_format = channels,
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 3, 0))
        .communication_format = I2S_COMM_FORMAT_I2S,
#else
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#endif
        .tx_desc_auto_clear = true,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_IRAM,
    };

    i2s_driver_install(port, &i2s_cfg, 0, NULL);
    i2s_pin_config_t i2s_pin_cfg = {0};
    memset(&i2s_pin_cfg, -1, sizeof(i2s_pin_cfg));
    get_i2s_pins(port, &i2s_pin_cfg);
    i2s_set_pin(port, &i2s_pin_cfg);
    i2s_mclk_gpio_select(port, GPIO_NUM_0);

    return ESP_OK;
}

static int i2s_read_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_read = 0;

    int ret = i2s_read(CODEC_ADC_I2S_PORT, buf, len, &bytes_read, wait_time);
    if (ret == ESP_OK) {
#if (CONFIG_IDF_TARGET_ESP32 && !RECORD_HARDWARE_AEC)
        algorithm_mono_fix((uint8_t *)buf, bytes_read);
#endif
    } else {
        ESP_LOGE(TAG, "i2s read failed");
    }

    return bytes_read;
}

static int i2s_write_cb(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    size_t bytes_write = 0;

#if !RECORD_HARDWARE_AEC
    if (rb_write(ringbuf_ref, buf, len, wait_time) <= 0) {
        ESP_LOGW(TAG, "ringbuf write timeout");
    }
#endif

#if CONFIG_IDF_TARGET_ESP32
    algorithm_mono_fix((uint8_t *)buf, len);
#endif
    int ret = i2s_write_expand(I2S_NUM_0, buf, len, 16, I2S_BITS, &bytes_write, wait_time);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s write failed");
    }

    return bytes_write;
}

static esp_err_t recorder_pipeline_open()
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    recorder = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, recorder, return ESP_FAIL);

    algorithm_stream_cfg_t algo_config = ALGORITHM_STREAM_CFG_DEFAULT();
#if !RECORD_HARDWARE_AEC
    algo_config.input_type = ALGORITHM_STREAM_INPUT_TYPE2;
#endif
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    algo_config.ref_linear_factor = 3;
#endif
#if DEBUG_AEC_INPUT
    algo_config.debug_input = true;
#endif
#if (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD)
    algo_config.swap_ch = true;
#endif
    algo_config.task_core = 1;
    algo_config.sample_rate = I2S_SAMPLE_RATE;
    algo_config.out_rb_size = ESP_RING_BUFFER_SIZE;
    element_algo = algo_stream_init(&algo_config);
    audio_element_set_music_info(element_algo, I2S_SAMPLE_RATE, 1, ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT);
    audio_element_set_read_cb(element_algo, i2s_read_cb, NULL);
    audio_element_set_input_timeout(element_algo, portMAX_DELAY);

    audio_pipeline_register(recorder, element_algo, "algo");

#if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
    wav_encoder_cfg_t wav_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    wav_cfg.task_core = 1;
    audio_element_handle_t wav_encoder = wav_encoder_init(&wav_cfg);

    fatfs_stream_cfg_t fatfs_wd_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_wd_cfg.type = AUDIO_STREAM_WRITER;
    fatfs_wd_cfg.task_core = 1;
    audio_element_handle_t fatfs_stream_writer = fatfs_stream_init(&fatfs_wd_cfg);

    audio_pipeline_register(recorder, wav_encoder, "wav_enc");
    audio_pipeline_register(recorder, fatfs_stream_writer, "fatfs_stream");

    const char *link_tag[3] = {"algo", "wav_enc", "fatfs_stream"};
    audio_pipeline_link(recorder, &link_tag[0], 3);

    audio_element_info_t fat_info = {0};
    audio_element_getinfo(fatfs_stream_writer, &fat_info);
    fat_info.sample_rates = I2S_SAMPLE_RATE;
    fat_info.bits = ALGORITHM_STREAM_DEFAULT_SAMPLE_BIT;
#if DEBUG_AEC_INPUT
    fat_info.channels = 2;
#else
    fat_info.channels = 1;
#endif
    audio_element_setinfo(fatfs_stream_writer, &fat_info);
    audio_element_set_uri(fatfs_stream_writer, "/sdcard/aec_in.wav");
#else
    g711_encoder_cfg_t g711_cfg = DEFAULT_G711_ENCODER_CONFIG();
    g711_cfg.task_core = 1;
    g711_cfg.out_rb_size = ESP_RING_BUFFER_SIZE / 2;
    audio_element_handle_t sip_encoder = g711_encoder_init(&g711_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_cfg.out_rb_size = ESP_RING_BUFFER_SIZE / 2;
    raw_read = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(raw_read, portMAX_DELAY);

    audio_pipeline_register(recorder, sip_encoder, "sip_enc");
    audio_pipeline_register(recorder, raw_read, "raw");

    const char *link_tag[3] = {"algo", "sip_enc", "raw"};
    audio_pipeline_link(recorder, &link_tag[0], 3);
#endif

    ESP_LOGI(TAG, " SIP recorder has been created");
    return ESP_OK;
}

static esp_err_t player_pipeline_open()
{
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    player = audio_pipeline_init(&pipeline_cfg);
    AUDIO_NULL_CHECK(TAG, player, return ESP_FAIL);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_cfg.out_rb_size = 1024;
    raw_write = raw_stream_init(&raw_cfg);

    g711_decoder_cfg_t g711_cfg = DEFAULT_G711_DECODER_CONFIG();
    audio_element_handle_t sip_decoder = g711_decoder_init(&g711_cfg);
    audio_element_set_write_cb(sip_decoder, i2s_write_cb, NULL);
    audio_element_set_output_timeout(sip_decoder, portMAX_DELAY);

    audio_pipeline_register(player, raw_write, "raw");
    audio_pipeline_register(player, sip_decoder, "sip_dec");
    const char *link_tag[2] = {"raw", "sip_dec"};
    audio_pipeline_link(player, &link_tag[0], 2);

#if !RECORD_HARDWARE_AEC
    //Please reference the way of ALGORITHM_STREAM_INPUT_TYPE2 in "algorithm_stream.h"
    ringbuf_ref = rb_create(2 * ESP_RING_BUFFER_SIZE, 1);
    audio_element_set_multi_input_ringbuf(element_algo, ringbuf_ref, 0);

    /* When the playback signal far ahead of the recording signal,
        the playback signal needs to be delayed */
    algo_stream_set_delay(element_algo, ringbuf_ref, DEFAULT_REF_DELAY_MS);
#endif

    ESP_LOGI(TAG, "SIP player has been created");
    return ESP_OK;
}

static ip4_addr_t _get_network_ip()
{
    tcpip_adapter_ip_info_t ip;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    return ip.ip;
}

static int _sip_event_handler(sip_event_msg_t *event)
{
    ip4_addr_t ip;
    switch ((int)event->type) {
        case SIP_EVENT_REQUEST_NETWORK_STATUS:
            ESP_LOGD(TAG, "SIP_EVENT_REQUEST_NETWORK_STATUS");
            ip = _get_network_ip();
            if (ip.addr) {
                return true;
            }
            return ESP_OK;
        case SIP_EVENT_REQUEST_NETWORK_IP:
            ESP_LOGD(TAG, "SIP_EVENT_REQUEST_NETWORK_IP");
            ip = _get_network_ip();
            int ip_len = sprintf((char *)event->data, "%s", ip4addr_ntoa(&ip));
            return ip_len;
        case SIP_EVENT_REGISTERED:
            ESP_LOGI(TAG, "SIP_EVENT_REGISTERED");
            audio_player_int_tone_play(tone_uri[TONE_TYPE_SERVER_CONNECT]);
            break;
        case SIP_EVENT_RINGING:
            ESP_LOGI(TAG, "ringing... RemotePhoneNum %s", (char *)event->data);
            audio_player_int_tone_play(tone_uri[TONE_TYPE_ALARM]);
            break;
        case SIP_EVENT_INVITING:
            ESP_LOGI(TAG, "SIP_EVENT_INVITING Remote Ring...");
            break;
        case SIP_EVENT_ERROR:
            ESP_LOGI(TAG, "SIP_EVENT_ERROR");
            break;
        case SIP_EVENT_HANGUP:
            ESP_LOGI(TAG, "SIP_EVENT_HANGUP");
            break;
        case SIP_EVENT_AUDIO_SESSION_BEGIN:
            ESP_LOGI(TAG, "SIP_EVENT_AUDIO_SESSION_BEGIN");
            recorder_pipeline_open();
            player_pipeline_open();
            audio_pipeline_run(player);
            audio_pipeline_run(recorder);
            break;
        case SIP_EVENT_AUDIO_SESSION_END:
            ESP_LOGI(TAG, "SIP_EVENT_AUDIO_SESSION_END");
            audio_pipeline_stop(player);
            audio_pipeline_wait_for_stop(player);
            audio_pipeline_deinit(player);
            audio_pipeline_stop(recorder);
            audio_pipeline_wait_for_stop(recorder);
            audio_pipeline_deinit(recorder);
            break;
        case SIP_EVENT_READ_AUDIO_DATA:
#if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
            vTaskDelay(20 / portTICK_PERIOD_MS);
            memset((char *)event->data, 0, event->data_len);
            return event->data_len;
#else
            return raw_stream_read(raw_read, (char *)event->data, event->data_len);
#endif
        case SIP_EVENT_WRITE_AUDIO_DATA:
            return raw_stream_write(raw_write, (char *)event->data, event->data_len);
        case SIP_EVENT_READ_DTMF:
            ESP_LOGI(TAG, "SIP_EVENT_READ_DTMF ID : %d ", ((char *)event->data)[0]);
            break;
    }
    return 0;
}

static esp_err_t input_key_service_cb(periph_service_handle_t handle, periph_service_event_t *evt, void *ctx)
{
    audio_board_handle_t board_handle = (audio_board_handle_t) ctx;
    int player_volume;
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGD(TAG, "[ * ] input key id is %d", (int)evt->data);
        sip_state_t sip_state = esp_sip_get_state(sip);
        if (sip_state < SIP_STATE_REGISTERED) {
            return ESP_OK;
        }
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
            case INPUT_KEY_USER_ID_MUTE:
                ESP_LOGI(TAG, "[ * ] [Rec] Set MIC Mute or not");
                if (mute) {
                    mute = false;
                    display_service_set_pattern(disp, DISPLAY_PATTERN_TURN_OFF, 0);
                } else {
                    mute = true;
                    display_service_set_pattern(disp, DISPLAY_PATTERN_TURN_ON, 0);
                }
#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
                audio_hal_set_mute(board_handle->adc_hal, mute);
#endif
                break;
            case INPUT_KEY_USER_ID_PLAY:
                ESP_LOGI(TAG, "[ * ] [Play] input key event");
                if (sip_state == SIP_STATE_RINGING) {
                    audio_player_int_tone_stop();
                    esp_sip_uas_answer(sip, true);
                } else if (sip_state == SIP_STATE_REGISTERED) {
                    esp_sip_uac_invite(sip, "101");
                }
                break;
            case INPUT_KEY_USER_ID_MODE:
            case INPUT_KEY_USER_ID_SET:
                if (sip_state & SIP_STATE_RINGING) {
                    audio_player_int_tone_stop();
                    esp_sip_uas_answer(sip, false);
                } else if (sip_state & SIP_STATE_ON_CALL) {
                    esp_sip_uac_bye(sip);
                } else if ((sip_state & SIP_STATE_CALLING) || (sip_state & SIP_STATE_SESS_PROGRESS)) {
                    esp_sip_uac_cancel(sip);
                }
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGD(TAG, "[ * ] [Vol+] input key event");
                audio_hal_get_volume(board_handle->audio_hal, &player_volume);
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGD(TAG, "[ * ] [Vol-] input key event");
                audio_hal_get_volume(board_handle->audio_hal, &player_volume);
                player_volume -= 10;
                if (player_volume < 0) {
                    player_volume = 0;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
        }
    } else if (evt->type == INPUT_KEY_SERVICE_ACTION_PRESS) {
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_SET:
                is_smart_config = true;
                esp_sip_destroy(sip);
                wifi_service_setting_start(wifi_serv, 0);
                audio_player_int_tone_play(tone_uri[TONE_TYPE_UNDER_SMARTCONFIG]);
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
        ESP_LOGI(TAG, "PERIPH_WIFI_CONNECTED [%d]", __LINE__);
        is_smart_config = false;
        ESP_LOGI(TAG, "[ 5 ] Create SIP Service");
        sip_config_t sip_cfg = {
            .uri = SIP_URI,
            .event_handler = _sip_event_handler,
            .send_options = true,
    #ifdef CONFIG_SIP_CODEC_G711A
            .acodec_type = SIP_ACODEC_G711A,
    #else
            .acodec_type = SIP_ACODEC_G711U,
    #endif
        };
        sip = esp_sip_init(&sip_cfg);
        esp_sip_start(sip);
    } else if (evt->type == WIFI_SERV_EVENT_DISCONNECTED) {
        ESP_LOGI(TAG, "PERIPH_WIFI_DISCONNECTED [%d]", __LINE__);
        if (is_smart_config == false) {
            audio_player_int_tone_play(tone_uri[TONE_TYPE_PLEASE_SETTING_WIFI]);
        }
    } else if (evt->type == WIFI_SERV_EVENT_SETTING_TIMEOUT) {
        ESP_LOGW(TAG, "WIFI_SERV_EVENT_SETTING_TIMEOUT [%d]", __LINE__);
        audio_player_int_tone_play(tone_uri[TONE_TYPE_PLEASE_SETTING_WIFI]);
        is_smart_config = false;
    }

    return ESP_OK;
}

void setup_wifi()
{
    int reg_idx = 0;
    wifi_service_config_t cfg = WIFI_SERVICE_DEFAULT_CONFIG();
    cfg.evt_cb = wifi_service_cb;
    cfg.setting_timeout_s = 300;
    cfg.max_retry_time = 2;
    wifi_serv = wifi_service_create(&cfg);

    smart_config_info_t info = SMART_CONFIG_INFO_DEFAULT();
    esp_wifi_setting_handle_t h = smart_config_create(&info);
    esp_wifi_setting_regitster_notify_handle(h, (void *)wifi_serv);
    wifi_service_register_setting_handle(wifi_serv, h, &reg_idx);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)&sta_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)&sta_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    wifi_service_set_sta_info(wifi_serv, &sta_cfg);
    wifi_service_connect(wifi_serv);
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    media_lib_add_default_adapter();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.1] Initialize and start peripherals");
    audio_board_key_init(set);
#if (DEBUG_AEC_INPUT || DEBUG_AEC_OUTPUT)
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
#endif

    ESP_LOGI(TAG, "[1.2] Create and start input key service");
    input_key_service_cfg_t input_cfg = INPUT_KEY_SERVICE_DEFAULT_CONFIG();
    input_cfg.handle = set;
    input_cfg.based_cfg.task_stack = 4 * 1024;
    periph_service_handle_t input_ser = input_key_service_create(&input_cfg);

#ifdef FUNC_SYS_LEN_EN
    ESP_LOGI(TAG, "[ 1.3 ] Create display service instance");
    disp = audio_board_led_init();
#endif

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    i2s_driver_init(I2S_NUM_0, I2S0_CHANNELS, I2S_BITS);

#if (CONFIG_ESP_LYRAT_MINI_V1_1_BOARD || CONFIG_ESP32_S3_KORVO2_V3_BOARD)
    audio_board_handle_t board_handle = (audio_board_handle_t) audio_calloc(1, sizeof(struct audio_board_handle));
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_CODEC_DEFAULT_CONFIG();
    audio_codec_cfg.i2s_iface.samples = AUDIO_HAL_08K_SAMPLES;
    board_handle->audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_ES8311_DEFAULT_HANDLE);
    board_handle->adc_hal = audio_board_adc_init();
#else
    audio_board_handle_t board_handle = audio_board_init();
#endif
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(board_handle->audio_hal, 60);

#if CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_driver_init(I2S_NUM_1, I2S1_CHANNELS, I2S_BITS);
#endif

    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)board_handle);

    ESP_LOGI(TAG, "[ 3 ] Initialize tone player");
    audio_player_int_tone_init(I2S_SAMPLE_RATE, I2S0_CHANNELS, I2S_BITS);

    ESP_LOGI(TAG, "[ 4 ] Create Wi-Fi service instance");
    setup_wifi();
}
