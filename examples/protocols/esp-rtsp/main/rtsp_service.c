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
#include "rtsp_service.h"
#include "media_lib_adapter.h"
#include "media_lib_netif.h"

static const char *TAG = "RTSP_SERVICE";

static esp_rtsp_mode_t rtsp_mode;

static char *_get_network_ip()
{
    media_lib_ipv4_info_t ip_info;
    media_lib_netif_get_ipv4_info(MEDIA_LIB_NET_TYPE_STA, &ip_info);
    return media_lib_ipv4_ntoa(&ip_info.ip);
}

static int _esp_rtsp_state_handler(esp_rtsp_state_t state, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    ESP_LOGD(TAG, "_esp_rtsp_state_handler state %d", state);
    switch ((int)state) {
        case RTSP_STATE_SETUP:
            ESP_LOGI(TAG, "RTSP_STATE_SETUP");
            break;
        case RTSP_STATE_PLAY:
            if (rtsp_mode == RTSP_CLIENT_PLAY) {
                av_audio_dec_start(av_stream);
            } else {
                av_audio_enc_start(av_stream);
                av_video_enc_start(av_stream);
            }
            ESP_LOGI(TAG, "RTSP_STATE_PLAY");
            break;
        case RTSP_STATE_TEARDOWN:
            if (rtsp_mode == RTSP_CLIENT_PLAY) {
                av_audio_dec_stop(av_stream);
            } else {
                av_audio_enc_stop(av_stream);
                av_video_enc_stop(av_stream);
            }
            ESP_LOGI(TAG, "RTSP_STATE_TEARDOWN");
            break;
    }
    return 0;
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
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    av_stream_frame_t frame = {0};
    frame.data = data;
    frame.len = len;
    return av_audio_dec_write(&frame, av_stream);
}

static int _send_video(unsigned char *data, unsigned int *len, void *ctx)
{
    av_stream_handle_t av_stream = (av_stream_handle_t) ctx;
    av_stream_frame_t frame = {0};
    frame.data = data;
    if (av_video_enc_read(&frame, av_stream) < 0) {
        *len = 0;
        return ESP_FAIL;
    }
    *len = frame.len;
    return ESP_OK;
}

esp_rtsp_handle_t rtsp_service_start(av_stream_handle_t av_stream, esp_rtsp_mode_t mode, const char *uri)
{
    AUDIO_NULL_CHECK(TAG, uri, return NULL);
    AUDIO_NULL_CHECK(TAG, av_stream, return NULL);
    media_lib_add_default_adapter();

    esp_rtsp_video_info_t vcodec_info = {
        .vcodec = RTSP_VCODEC_MJPEG,
        .width = av_resolution[RTSP_FRAME_SIZE].width,
        .height = av_resolution[RTSP_FRAME_SIZE].height,
        .fps = VIDEO_FPS,
        .len = VIDEO_MAX_SIZE,
    };
    esp_rtsp_data_cb_t data_cb = {
        .send_audio = _send_audio,
        .receive_audio = _receive_audio,
        .send_video = _send_video,
    };
    esp_rtsp_config_t rtsp_config = {
        .uri = uri,
        .mode = mode,
        .ctx = av_stream,
        .data_cb = &data_cb,
        .audio_enable = true,
        .video_enable = true,
        .acodec = RTSP_ACODEC_G711A,
        .video_info = &vcodec_info,
        .local_addr = _get_network_ip(),
        .stack_size = RTSP_STACK_SZIE,
        .task_prio = RTSP_TASK_PRIO,
        .state = _esp_rtsp_state_handler,
        .trans = RTSP_TRANSPORT_TCP,
    };

    rtsp_mode = mode;
    if (mode == RTSP_CLIENT_PLAY) {
        data_cb.receive_audio = _receive_audio;
        data_cb.send_audio = NULL;
        data_cb.send_video = NULL;
        rtsp_config.video_enable = false;
        rtsp_config.trans = RTSP_TRANSPORT_UDP;
    }

    if (mode == RTSP_SERVER) {
        rtsp_config.local_port = RTSP_SERVER_PORT;
        return esp_rtsp_server_start(&rtsp_config);
    } else {
        return esp_rtsp_client_start(&rtsp_config);
    }

    return ESP_OK;
}

int rtsp_service_stop(esp_rtsp_handle_t esp_rtsp)
{
    if (esp_rtsp) {
        if (rtsp_mode == RTSP_SERVER) {
            esp_rtsp_server_stop(esp_rtsp);
        } else {
            esp_rtsp_client_stop(esp_rtsp);
        }
    }
    return ESP_FAIL;
}
