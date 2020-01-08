/* VoIP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "board.h"
#include "input_key_service.h"

#include "audio_mem.h"
#include "raw_stream.h"
#include "filter_resample.h"
#include "esp_sip.h"
#include "g711.h"

static const char *TAG = "VOIP_EXAMPLE";

#define RTP_HEADER_LEN 12
#define AUDIO_FRAME_SIZE (160)

#define I2S_SAMPLE_RATE     48000
#define I2S_CHANNEL         2
#define I2S_BITS            16

#define G711_SAMPLE_RATE    8000
#define G711_CHANNEL        1

static sip_handle_t sip;
static audio_element_handle_t raw_read;
static audio_element_handle_t raw_write;

static esp_err_t g711enc_pipeline_open()
{
    audio_element_handle_t i2s_stream_reader;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t recorder = audio_pipeline_init(&pipeline_cfg);
    if (NULL == recorder) {
        return ESP_FAIL;
    }
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
#if defined CONFIG_ESP_LYRAT_MINI_V1_1_BOARD
    i2s_cfg.i2s_port = 1;
#endif
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_reader, &i2s_info);
    i2s_info.bits = I2S_BITS;
    i2s_info.channels = I2S_CHANNEL;
    i2s_info.sample_rates = I2S_SAMPLE_RATE;
    audio_element_setinfo(i2s_stream_reader, &i2s_info);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = I2S_SAMPLE_RATE;
    rsp_cfg.src_ch = I2S_CHANNEL;
    rsp_cfg.dest_rate = G711_SAMPLE_RATE;
    rsp_cfg.dest_ch = G711_CHANNEL;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_READER;
    raw_read = raw_stream_init(&raw_cfg);
    audio_element_set_output_timeout(raw_read, portMAX_DELAY);

    audio_pipeline_register(recorder, i2s_stream_reader, "i2s");
    audio_pipeline_register(recorder, filter, "filter");
    audio_pipeline_register(recorder, raw_read, "raw");
    audio_pipeline_link(recorder, (const char *[]) {"i2s", "filter", "raw"}, 3);
    audio_pipeline_run(recorder);
    ESP_LOGI(TAG, "Recorder has been created");
    return ESP_OK;
}

static int _g711_encode(char *data, int len)
{
    int out_len_bytes;

    char *enc_buffer = (char *)audio_malloc(2 * AUDIO_FRAME_SIZE);
    out_len_bytes = raw_stream_read(raw_read, enc_buffer, 2 * AUDIO_FRAME_SIZE);
    if (out_len_bytes > 0) {
        int16_t *enc_buffer_16 = (int16_t *)(enc_buffer);
        for (int i = 0; i < AUDIO_FRAME_SIZE; i++) {
#ifdef CONFIG_SIP_CODEC_G711A
            data[i] = esp_g711a_encode(enc_buffer_16[i]);
#else
            data[i] = esp_g711u_encode(enc_buffer_16[i]);
#endif
        }
        free(enc_buffer);
        return AUDIO_FRAME_SIZE;
    } else {
        free(enc_buffer);
        return 0;
    }
}

static esp_err_t g711dec_pipeline_open()
{
    audio_element_handle_t i2s_stream_writer;
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t speaker = audio_pipeline_init(&pipeline_cfg);
    if (NULL == speaker) {
        return ESP_FAIL;
    }

    raw_stream_cfg_t raw_cfg = RAW_STREAM_CFG_DEFAULT();
    raw_cfg.type = AUDIO_STREAM_WRITER;
    raw_write = raw_stream_init(&raw_cfg);

    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = G711_SAMPLE_RATE;
    rsp_cfg.src_ch = G711_CHANNEL;
    rsp_cfg.dest_rate = I2S_SAMPLE_RATE;
    rsp_cfg.dest_ch = I2S_CHANNEL;
    rsp_cfg.complexity = 5;
    audio_element_handle_t filter = rsp_filter_init(&rsp_cfg);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    audio_element_info_t i2s_info = {0};
    audio_element_getinfo(i2s_stream_writer, &i2s_info);
    i2s_info.bits = I2S_BITS;
    i2s_info.channels = I2S_CHANNEL;
    i2s_info.sample_rates = I2S_SAMPLE_RATE;
    audio_element_setinfo(i2s_stream_writer, &i2s_info);

    audio_pipeline_register(speaker, raw_write, "raw");
    audio_pipeline_register(speaker, filter, "filter");
    audio_pipeline_register(speaker, i2s_stream_writer, "i2s");
    audio_pipeline_link(speaker, (const char *[]) {"raw", "filter", "i2s"}, 3);
    audio_pipeline_run(speaker);
    ESP_LOGI(TAG, "Speaker has been created");
    return ESP_OK;
}

static int _g711_decode(char *data, int len)
{
    int16_t *dec_buffer = (int16_t *)audio_malloc(2 * (len - RTP_HEADER_LEN));

    for (int i = 0; i < (len - RTP_HEADER_LEN); i++) {
#ifdef CONFIG_SIP_CODEC_G711A
        dec_buffer[i] = esp_g711a_decode((unsigned char)data[RTP_HEADER_LEN + i]);
#else
        dec_buffer[i] = esp_g711u_decode((unsigned char)data[RTP_HEADER_LEN + i]);
#endif
    }

    raw_stream_write(raw_write, (char *)dec_buffer, 2 * (len - RTP_HEADER_LEN));
    free(dec_buffer);
    return 2 * (len - RTP_HEADER_LEN);
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
            ESP_LOGI(TAG, "SIP_EVENT_REQUEST_NETWORK_STATUS");
            ip = _get_network_ip();
            if (ip.addr) {
                return true;
            }
            return ESP_OK;
        case SIP_EVENT_REQUEST_NETWORK_IP:
            ESP_LOGI(TAG, "SIP_EVENT_REQUEST_NETWORK_IP");
            ip = _get_network_ip();
            int ip_len = sprintf((char *)event->data, "%s", ip4addr_ntoa(&ip));
            return ip_len;
        case SIP_EVENT_REGISTERED:
            ESP_LOGI(TAG, "SIP_EVENT_REGISTERED");
            break;
        case SIP_EVENT_RINGING:
            ESP_LOGI(TAG, "ringing... RemotePhoneNum %s", (char *)event->data);
            break;
        case SIP_EVENT_INVITING:
            ESP_LOGI(TAG, "SIP_EVENT_INVITING Remote Ring...");
            break;
        case SIP_EVENT_BUSY:
            ESP_LOGI(TAG, "SIP_EVENT_BUSY");
            break;
        case SIP_EVENT_HANGUP:
            ESP_LOGI(TAG, "SIP_EVENT_HANGUP");
            break;
        case SIP_EVENT_AUDIO_SESSION_BEGIN:
            ESP_LOGI(TAG, "SIP_EVENT_AUDIO_SESSION_BEGIN");
            break;
        case SIP_EVENT_AUDIO_SESSION_END:
            ESP_LOGI(TAG, "SIP_EVENT_AUDIO_SESSION_END");
            break;
        case SIP_EVENT_READ_AUDIO_DATA:
            return _g711_encode(event->data, event->data_len);
        case SIP_EVENT_WRITE_AUDIO_DATA:
            return _g711_decode(event->data, event->data_len);
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
    audio_hal_get_volume(board_handle->audio_hal, &player_volume);
    if (evt->type == INPUT_KEY_SERVICE_ACTION_CLICK_RELEASE) {
        ESP_LOGI(TAG, "[ * ] input key id is %d", (int)evt->data);
        sip_state_t sip_state = esp_sip_get_state(sip);
        if (sip_state < SIP_STATE_REGISTERED) {
            return ESP_OK;
        }
        switch ((int)evt->data) {
            case INPUT_KEY_USER_ID_REC:
            case INPUT_KEY_USER_ID_PLAY:
                ESP_LOGI(TAG, "[ * ] [Play] input key event");
                if (sip_state & SIP_STATE_RINGING) {
                    esp_sip_uas_answer(sip, true);
                }
                if (sip_state & SIP_STATE_REGISTERED) {
                    esp_sip_uac_invite(sip, "101");
                }
                break;
            case INPUT_KEY_USER_ID_MODE:
            case INPUT_KEY_USER_ID_SET:
                ESP_LOGI(TAG, "[ * ] [Set] input key event");
                if (sip_state & SIP_STATE_RINGING) {
                    esp_sip_uas_answer(sip, false);
                } else if (sip_state & SIP_STATE_ON_CALL) {
                    esp_sip_uac_bye(sip);
                } else if ((sip_state & SIP_STATE_CALLING) || (sip_state & SIP_STATE_SESS_PROGRESS)) {
                    esp_sip_uac_cancel(sip);
                }
                break;
            case INPUT_KEY_USER_ID_VOLUP:
                ESP_LOGI(TAG, "[ * ] [Vol+] input key event");
                player_volume += 10;
                if (player_volume > 100) {
                    player_volume = 100;
                }
                audio_hal_set_volume(board_handle->audio_hal, player_volume);
                ESP_LOGI(TAG, "[ * ] Volume set to %d %%", player_volume);
                break;
            case INPUT_KEY_USER_ID_VOLDOWN:
                ESP_LOGI(TAG, "[ * ] [Vol-] input key event");
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

void app_main()
{
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set("VOIP_EXAMPLE", ESP_LOG_DEBUG);
    esp_log_level_set("SIP", ESP_LOG_DEBUG);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    tcpip_adapter_init();

    ESP_LOGI(TAG, "[1.0] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[1.2] Initialize and start peripherals");
    audio_board_key_init(set);

    ESP_LOGI(TAG, "[1.3] Start and wait for Wi-Fi network");
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Create and start input key service");
    input_key_service_info_t input_key_info[] = INPUT_KEY_DEFAULT_INFO();
    periph_service_handle_t input_ser = input_key_service_create(set);
    input_key_service_add_key(input_ser, input_key_info, INPUT_KEY_NUM);
    periph_service_set_callback(input_ser, input_key_service_cb, (void *)board_handle);

    ESP_LOGI(TAG, "[ 4 ] Create SIP Service");
    sip_config_t sip_cfg = {
        .uri = CONFIG_SIP_URI,
        .event_handler = _sip_event_handler,
#ifdef CONFIG_SIP_CODEC_G711A
        .acodec_type = SIP_ACODEC_G711A,
#else
        .acodec_type = SIP_ACODEC_G711U,
#endif
    };
    sip = esp_sip_init(&sip_cfg);
    esp_sip_start(sip);

    ESP_LOGI(TAG, "[ 5 ] Create decoder and encoder pipelines");
    g711enc_pipeline_open();
    g711dec_pipeline_open();
}

