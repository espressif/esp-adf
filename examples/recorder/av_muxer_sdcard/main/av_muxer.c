/*  AV muxer module to record video and audio data and mux into media file

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_log.h"
#include "media_lib_os.h"
#include "esp_muxer.h"
#include "mp4_muxer.h"
#include "flv_muxer.h"
#include "ts_muxer.h"
#include "av_muxer.h"
#include "av_record.h"

#define AV_MUXER_VIDEO_FMT      ESP_MUXER_VDEC_H264
#define AV_MUXER_USE_SW_JPEG    true
#define AV_MUXER_VIDEO_FPS      6
#define AV_MUXER_AUDIO_FMT      ESP_MUXER_ADEC_AAC
#define AV_MUXER_SLICE_DURATION 60000
#ifdef CONFIG_USB_CAMERA_SUPPORT
#define AV_MUXER_VIDEO_SRC         RECORD_SRC_TYPE_USB_CAM
#else
#define AV_MUXER_VIDEO_SRC         RECORD_SRC_TYPE_SPI_CAM
#endif
#define LOG_ON_ERR(ret, fmt, ...)        \
    if (ret != 0) {                      \
        ESP_LOGE(TAG, fmt, __VA_ARGS__); \
    }
typedef union {
    ts_muxer_config_t  ts_cfg;
    flv_muxer_config_t flv_cfg;
    mp4_muxer_config_t mp4_cfg;
} muxer_cfg_t;

static const char* TAG = "AV Muxer";

static esp_muxer_handle_t muxer = NULL;
static av_muxer_cfg_t av_muxer_cfg;
static esp_muxer_type_t muxer_type;
static uint32_t record_duration;
static int audio_stream_idx;
static int video_stream_idx;

static av_record_video_quality_t get_video_quality(muxer_video_quality_t quality)
{
    switch (quality) {
        case MUXER_VIDEO_QUALITY_QVGA:
        default:
            return AV_RECORD_VIDEO_QUALITY_QVGA;
        case MUXER_VIDEO_QUALITY_HVGA:
            return AV_RECORD_VIDEO_QUALITY_HVGA;
        case MUXER_VIDEO_QUALITY_VGA:
            return AV_RECORD_VIDEO_QUALITY_VGA;
        case MUXER_VIDEO_QUALITY_XVGA:
            return AV_RECORD_VIDEO_QUALITY_XVGA;
        case MUXER_VIDEO_QUALITY_HD:
            return AV_RECORD_VIDEO_QUALITY_HD;
        case MUXER_VIDEO_QUALITY_FHD:
            return AV_RECORD_VIDEO_QUALITY_FHD;
    }
}

static av_record_video_fmt_t get_video_codec(esp_muxer_video_codec_t video_codec)
{
    switch (video_codec) {
        case ESP_MUXER_VDEC_MJPEG:
            return AV_RECORD_VIDEO_FMT_MJPEG;
        case ESP_MUXER_VDEC_H264:
            return AV_RECORD_VIDEO_FMT_H264;
        default:
            return AV_RECORD_VIDEO_FMT_NONE;
    }
}

static av_record_audio_fmt_t get_audio_codec(esp_muxer_audio_codec_t audio_codec)
{
    switch (audio_codec) {
        case ESP_MUXER_ADEC_AAC:
            return AV_RECORD_AUDIO_FMT_AAC;
        case ESP_MUXER_ADEC_PCM:
            return AV_RECORD_AUDIO_FMT_PCM;
        default:
            return AV_RECORD_AUDIO_FMT_NONE;
    }
}

static esp_muxer_err_t muxer_register()
{
    esp_muxer_err_t ret = 0;
    ret = ts_muxer_register();
    LOG_ON_ERR(ret, "Register ts muxer fail %d", ret);
    ret |= mp4_muxer_register();
    LOG_ON_ERR(ret, "Register mp4 muxer fail %d", ret);
    ret |= flv_muxer_register();
    LOG_ON_ERR(ret, "Register flv muxer fail %d", ret);
    ret |= record_src_register_default();
    LOG_ON_ERR(ret, "Register source fail %d", ret);
    return ret;
}

static esp_muxer_err_t muxer_video(av_record_data_t *data)
{
    if (video_stream_idx < 0) {
        return 0;
    }
    esp_muxer_video_packet_t video_packet = {
        .data = data->data,
        .len = data->size,
        .pts = data->pts,
        .dts = 0,
    };
    return esp_muxer_add_video_packet(muxer, video_stream_idx, &video_packet);
}

static esp_muxer_err_t muxer_audio(av_record_data_t *data)
{
    if (audio_stream_idx < 0) {
        return 0;
    }
    esp_muxer_audio_packet_t audio_packet = {
        .data = data->data,
        .len = data->size,
        .pts = data->pts,
    };
    return esp_muxer_add_audio_packet(muxer, audio_stream_idx, &audio_packet);
}

static int _record_data_reached(av_record_data_t *data, void *ctx)
{
    switch (data->stream_type) {
        case AV_RECORD_STREAM_TYPE_AUDIO:
        return muxer_audio(data);
        case AV_RECORD_STREAM_TYPE_VIDEO:
        return muxer_video(data);;
        default:
        return 0;
    }
}

static int _muxer_data_reached(esp_muxer_data_info_t* data, void* ctx)
{
    if (av_muxer_cfg.data_cb) {
        av_muxer_cfg.data_cb(data->data, data->size, av_muxer_cfg.ctx);
    }
    return 0;
}

static int av_muxer_url_pattern(char* file_name, int len, int slice_idx)
{
    switch (muxer_type) {
        case ESP_MUXER_TYPE_TS:
            snprintf(file_name, len, "/sdcard/slice_%d.ts", slice_idx);
            break;
        case ESP_MUXER_TYPE_MP4:
            snprintf(file_name, len, "/sdcard/slice_%d.mp4", slice_idx);
            break;
        case ESP_MUXER_TYPE_FLV:
            snprintf(file_name, len, "/sdcard/slice_%d.flv", slice_idx);
            break;
        default:
            return -1;
    }
    return 0;
}

int av_muxer_start(av_muxer_cfg_t* cfg)
{
    muxer_cfg_t muxer_cfg = {0};
    esp_muxer_config_t* base_cfg = &muxer_cfg.ts_cfg.base_config;
    base_cfg->data_cb = _muxer_data_reached;
    if ((cfg->data_cb == NULL && cfg->save_to_file == false) || cfg->muxer_flag == 0) {
        ESP_LOGE(TAG, "Bad muxer configuration, filepath or data callback need set");
        return -1;
    }
    base_cfg->url_pattern = NULL;
    if (cfg->save_to_file) {
        base_cfg->url_pattern = av_muxer_url_pattern;
    }
    int cfg_size = 0;
    base_cfg->slice_duration = ESP_MUXER_MAX_SLICE_DURATION;
    record_duration = ESP_MUXER_MAX_SLICE_DURATION;
    if (strcmp(cfg->fmt, "ts") == 0) {
        muxer_type = ESP_MUXER_TYPE_TS;
        cfg_size = sizeof(ts_muxer_config_t);
    } else if (strcmp(cfg->fmt, "flv") == 0) {
        muxer_type = ESP_MUXER_TYPE_FLV;
        cfg_size = sizeof(flv_muxer_config_t);
    } else if (strcmp(cfg->fmt, "mp4") == 0) {
        muxer_type = ESP_MUXER_TYPE_MP4;
        base_cfg->slice_duration = AV_MUXER_SLICE_DURATION;
        cfg_size = sizeof(mp4_muxer_config_t);
        base_cfg->data_cb = NULL;
    } else {
        ESP_LOGE(TAG, "Not supported muxer format %s", cfg->fmt);
        return -1;
    }
    base_cfg->muxer_type = muxer_type;
    av_muxer_cfg = *cfg;
    cfg = &av_muxer_cfg;
    do {
        esp_muxer_err_t ret = muxer_register();
        LOG_ON_ERR(ret, "Register muxer fail ret %d\n", ret);
        if (ret != ESP_MUXER_ERR_OK) {
            break;
        }
        ESP_LOGI(TAG, "Start to open muxer");
        muxer = esp_muxer_open(base_cfg, cfg_size);
        if (muxer == NULL) {
            break;
        }
        if (cfg->muxer_flag & AV_MUXER_VIDEO_FLAG) {
            esp_muxer_video_stream_info_t video_info = {
                .codec = AV_MUXER_VIDEO_FMT,
                .codec_spec_info = NULL,
                .spec_info_len = 0,
                .fps = AV_MUXER_VIDEO_FPS,
                .min_packet_duration = 30,
            };
            av_record_get_video_size(get_video_quality(cfg->quality), &video_info.width, &video_info.height);
            video_stream_idx = -1;
            ret = esp_muxer_add_video_stream(muxer, &video_info, &video_stream_idx);
            if (ret != ESP_MUXER_ERR_OK) {
                ESP_LOGE(TAG, "Fail to add video stream %d", ret);
                cfg->muxer_flag &= ~AV_MUXER_VIDEO_FLAG;
            }
        }
        if (cfg->muxer_flag & AV_MUXER_AUDIO_FLAG) {
            esp_muxer_audio_stream_info_t audio_info = {
                .bits_per_sample = 16,
                .channel = 2,
                .sample_rate = cfg->audio_sample_rate,
                .codec = AV_MUXER_AUDIO_FMT,
                .min_packet_duration = 16,
            };
            audio_stream_idx = -1;
            ret = esp_muxer_add_audio_stream(muxer, &audio_info, &audio_stream_idx);
            if (ret != ESP_MUXER_ERR_OK) {
                ESP_LOGE(TAG, "Fail to add audio stream %d", ret);
                cfg->muxer_flag &= ~AV_MUXER_AUDIO_FLAG;
            }
        }
        if (audio_stream_idx < 0 && video_stream_idx < 0) {
            break;
        }
        av_record_cfg_t record_cfg = {
            .video_quality = get_video_quality(cfg->quality),
            .audio_src = RECORD_SRC_TYPE_I2S_AUD,
            .video_src = RECORD_SRC_TYPE_SPI_CAM,
            .video_fmt = get_video_codec(AV_MUXER_VIDEO_FMT),
            .sw_jpeg_enc = AV_MUXER_USE_SW_JPEG,
            .video_fps = AV_MUXER_VIDEO_FPS,
            .audio_fmt = get_audio_codec(AV_MUXER_AUDIO_FMT),
            .audio_channel = 2,
            .audio_sample_rate = cfg->audio_sample_rate,
            .data_cb = _record_data_reached,
        };
        ret = av_record_start(&record_cfg);
        if (ret != 0) {
            ESP_LOGE(TAG, "Start recorder fail return %d", ret);
            break;
        }
        return 0;
    } while (0);
    av_muxer_stop();
    return -1;
}

int av_muxer_stop()
{
    ESP_LOGI(TAG, "Stopping muxer process");
    av_record_stop();
    ESP_LOGI(TAG, "Waiting record exit done");
    if (muxer) {
        esp_muxer_close(muxer);
        muxer = NULL;
    }
    ESP_LOGI(TAG, "Stop muxer done");
    return 0;
}

bool av_muxer_running()
{
    return av_record_running();
}

uint32_t av_muxer_get_pts()
{
    return av_record_get_pts();
}
