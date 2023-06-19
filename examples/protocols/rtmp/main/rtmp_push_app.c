/* RTMP push application to publish data to RTMP server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_timer.h"
#include "rtmp_src_app.h"
#include "esp_rtmp_push.h"
#include "av_record.h"
#include "esp_log.h"
#include "media_lib_os.h"
#include "rtmp_app_setting.h"

#define TAG                         "RTMP Push_App"
#define RTMP_PUSH_CHUNK_SIZE        (1024 * 4)

#ifdef CONFIG_USB_CAMERA_SUPPORT
#define RTMP_PUSH_VIDEO_SRC         RECORD_SRC_TYPE_USB_CAM
#else
#define RTMP_PUSH_VIDEO_SRC         RECORD_SRC_TYPE_SPI_CAM
#endif
#define RTMP_PUSH_AUDIO_SRC         RECORD_SRC_TYPE_I2S_AUD
#define RTMP_AAC_HEAD_SIZE          7

static esp_rtmp_audio_codec_t map_audio_codec(av_record_audio_fmt_t codec)
{
    switch (codec) {
        case AV_RECORD_AUDIO_FMT_PCM:
            return RTMP_AUDIO_CODEC_PCM;
        case AV_RECORD_AUDIO_FMT_G711A:
            return RTMP_AUDIO_CODEC_G711A;
        case AV_RECORD_AUDIO_FMT_G711U:
            return RTMP_AUDIO_CODEC_G711U;
        case AV_RECORD_AUDIO_FMT_AAC:
            return RTMP_AUDIO_CODEC_AAC;
        default:
            return RTMP_AUDIO_CODEC_NONE;
    }
}

static esp_rtmp_video_codec_t map_video_codec(av_record_video_fmt_t codec)
{
    switch (codec) {
        case AV_RECORD_VIDEO_FMT_MJPEG:
            return RTMP_VIDEO_CODEC_MJPEG;
        case AV_RECORD_VIDEO_FMT_H264:
            return RTMP_VIDEO_CODEC_H264;
        default:
            return RTMP_VIDEO_CODEC_NONE;
    }
}

int rtmp_push_data_received(av_record_data_t *frame, void *ctx)
{
    esp_media_err_t ret = ESP_MEDIA_ERR_OK;
    rtmp_push_handle_t rtmp_push = (rtmp_push_handle_t) ctx;
    switch (frame->stream_type) {
        case AV_RECORD_STREAM_TYPE_AUDIO: {
            esp_rtmp_audio_data_t audio_data = {
                .pts = frame->pts,
                .data = frame->data,
                .size = frame->size,
            };
            if (rtmp_setting_get_audio_fmt() == AV_RECORD_AUDIO_FMT_AAC && audio_data.size >= RTMP_AAC_HEAD_SIZE) {
                audio_data.data += RTMP_AAC_HEAD_SIZE;
                audio_data.size -= RTMP_AAC_HEAD_SIZE;
            }
            ret = esp_rtmp_push_audio(rtmp_push, &audio_data);
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Add audio packet return %d", ret);
            }
            break;
        }
        case AV_RECORD_STREAM_TYPE_VIDEO: {
            esp_rtmp_video_data_t video_data = {
                .pts = frame->pts,
                .key_frame = true,
                .data = frame->data,
                .size = frame->size,
            };
            ret = esp_rtmp_push_video(rtmp_push, &video_data);
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Add video packet return %d", ret);
            }
            break;
        }
        default:
            break;
    }
    return (int) ret;
}

int rtmp_push_app_run(char *uri, uint32_t duration)
{
    int ret;
    media_lib_tls_cfg_t ssl_cfg;
    rtmp_push_cfg_t cfg = {
        .url = uri,
        .chunk_size = RTMP_PUSH_CHUNK_SIZE,
        .thread_cfg = {.priority = 10, .stack_size = 10*1024},
    };
    if (strncmp(uri, "rtmps://", 8) == 0) {
        cfg.ssl_cfg = &ssl_cfg;
        rtmp_setting_get_client_ssl_cfg(uri, &ssl_cfg);
    }
    ESP_LOGI(TAG, "Start to push to %s", uri);
    rtmp_push_handle_t rtmp_push = esp_rtmp_push_open(&cfg);
    if (rtmp_push == NULL) {
        ESP_LOGE(TAG, "Fail to open rtmp Pusher\n");
        return -1;
    }
    ret = record_src_register_default();
    esp_rtmp_audio_info_t audio_info = {
        .bits_per_sample = 16,
        .sample_rate = rtmp_setting_get_audio_sample_rate(),
        .channel = rtmp_setting_get_audio_channel(),
        .codec = map_audio_codec(rtmp_setting_get_audio_fmt()),
    };
    // For g711 default use 8K sample rate
    if (audio_info.codec == RTMP_AUDIO_CODEC_G711A || audio_info.codec == RTMP_AUDIO_CODEC_G711U) {
        audio_info.sample_rate = 8000;
    }
    // Use 16K 1 channel to decrease cpu usage
    if (audio_info.codec == RTMP_AUDIO_CODEC_AAC) {
        audio_info.sample_rate = 16000;
        audio_info.channel = 1;
        ESP_LOGW(TAG, "AAC force to use 16k 1 channel");
    }
    ret = esp_rtmp_push_set_audio_info(rtmp_push, &audio_info);
    ESP_LOGI(TAG, "Add audio stream return %d", ret);

    esp_rtmp_video_info_t video_info = {
        .fps = rtmp_setting_get_video_fps(),
        .codec = map_video_codec(rtmp_setting_get_video_fmt()),
    };
    av_record_get_video_size(rtmp_setting_get_video_quality(), &video_info.width, &video_info.height);
    ret = esp_rtmp_push_set_video_info(rtmp_push, &video_info);
    ESP_LOGI(TAG, "Add video stream return %d", ret);

    ret = esp_rtmp_push_connect(rtmp_push);
    if (ret != 0) {
        ESP_LOGE(TAG, "Fail to connect ret %d\n", ret);
    } else {
        av_record_cfg_t record_cfg = {
            .audio_src = RTMP_PUSH_AUDIO_SRC,
            .video_src = RTMP_PUSH_VIDEO_SRC,
            .video_fmt = rtmp_setting_get_video_fmt(),
            .audio_fmt = rtmp_setting_get_audio_fmt(),
            .video_fps = rtmp_setting_get_video_fps(),
            .sw_jpeg_enc = rtmp_setting_get_sw_jpeg(),
            .video_quality = rtmp_setting_get_video_quality(),
            .audio_sample_rate = audio_info.sample_rate,
            .audio_channel = audio_info.channel,
            .data_cb = rtmp_push_data_received,
            .ctx = rtmp_push,
        };
        av_record_start(&record_cfg);
        uint32_t start_time = esp_timer_get_time() / 1000;
        while (av_record_running() && rtmp_setting_get_allow_run()) {
            media_lib_thread_sleep(1000);
            uint32_t cur_time = esp_timer_get_time() / 1000;
            if (cur_time > duration && cur_time > start_time + duration) {
                break;
            }
        }
        av_record_stop();
    }
    ret = esp_rtmp_push_close(rtmp_push);
    record_src_unregister_all();
    ESP_LOGI(TAG, "Close rtmp return %d\n", ret);
    return ret;
}
