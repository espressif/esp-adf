/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "string.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "sip_service.h"
#include "media_lib_adapter.h"
#include "media_lib_netif.h"

static const char *TAG = "SIP_SERVICE";

static char *_get_network_ip()
{
    media_lib_ipv4_info_t ip_info;
    media_lib_netif_get_ipv4_info(MEDIA_LIB_NET_TYPE_STA, &ip_info);
    return media_lib_ipv4_ntoa(&ip_info.ip);
}

static int _esp_sip_event_handler(esp_rtc_event_t event, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    ESP_LOGD(TAG, "_esp_sip_event_handler event %d", event);
    switch ((int)event) {
        case ESP_RTC_EVENT_REGISTERED:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_REGISTERED");
            audio_player_int_tone_play(tone_uri[TONE_TYPE_SERVER_CONNECT]);
            break;
        case ESP_RTC_EVENT_UNREGISTERED:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_UNREGISTERED");
            break;
        case ESP_RTC_EVENT_CALLING:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_CALLING Remote Ring...");
            break;
        case ESP_RTC_EVENT_INCOMING:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_INCOMING...");
            audio_player_int_tone_play(tone_uri[TONE_TYPE_ALARM]);
            break;
        case ESP_RTC_EVENT_AUDIO_SESSION_BEGIN:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_AUDIO_SESSION_BEGIN");
            av_audio_enc_start(av_stream);
            av_audio_dec_start(av_stream);
            break;
        case ESP_RTC_EVENT_AUDIO_SESSION_END:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_AUDIO_SESSION_END");
            av_audio_enc_stop(av_stream);
            av_audio_dec_stop(av_stream);
            break;
        case ESP_RTC_EVENT_HANGUP:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_HANGUP");
            break;
        case ESP_RTC_EVENT_ERROR:
            ESP_LOGI(TAG, "ESP_RTC_EVENT_ERROR");
            break;
    }

    return ESP_OK;
}

static int _send_audio(unsigned char *data, int len, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    av_stream_frame_t frame = {0};
    frame.data = data;
    frame.len = len;
    if (av_audio_enc_read(&frame, av_stream) < 0) {
        return 0;
    }
    return frame.len;
}

static int _receive_audio(unsigned char *data, int len, void *ctx)
{
    if ((len == 6) && !strncasecmp((char *)data, "DTMF-", 5)) {
        ESP_LOGI(TAG,"Receive DTMF Event ID : %d", data[5]);
        return 0;
    } else {
        av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
        av_stream_frame_t frame = {0};
        frame.data = data;
        frame.len = len;
        return av_audio_dec_write(&frame, av_stream);
    }
}

esp_rtc_handle_t sip_service_start(av_stream_handle_t av_stream, const char *uri)
{
    AUDIO_NULL_CHECK(TAG, uri, return NULL);
    AUDIO_NULL_CHECK(TAG, av_stream, return NULL);
    media_lib_add_default_adapter();

    esp_rtc_data_cb_t data_cb = {
        .send_audio = _send_audio,
        .receive_audio = _receive_audio,
    };
    esp_rtc_config_t sip_service_config = {
        .uri = uri,
        .ctx = av_stream,
        .local_addr = _get_network_ip(),
        .acodec_type = RTC_ACODEC_G711A,
        .data_cb = &data_cb,
        .event_handler = _esp_sip_event_handler,
    };

    return esp_rtc_init(&sip_service_config);
}

int sip_service_stop(esp_rtc_handle_t esp_sip)
{
    int ret = ESP_FAIL;
    if (esp_sip) {
        return esp_rtc_deinit(esp_sip);
    }
    return ret;
}
