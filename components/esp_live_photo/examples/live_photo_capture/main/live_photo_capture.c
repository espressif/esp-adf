/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_capture.h"
#include "esp_capture_defaults.h"
#include "esp_capture_sink.h"
#include "esp_board_manager_includes.h"
#include "esp_cam_sensor_types.h"
#include "esp_video_isp_ioctl.h"
#include "esp_video_ioctl.h"
#include "esp_audio_enc_default.h"
#include "esp_live_photo_muxer.h"
#include "esp_muxer.h"
#include "esp_video_enc_default.h"
#include "settings.h"
#include "esp_log.h"

#define TAG  "LIVE_PHOTO_CAPTURE"

typedef enum {
    LIVE_PHOTO_CAPTURE_STAGE_VIDEO = 0,
    LIVE_PHOTO_CAPTURE_STAGE_COVER = 1,
} live_photo_capture_stage_t;

typedef struct {
    esp_capture_video_src_if_t  stage_vid_src;
    esp_capture_video_src_if_t *video_src;
    live_photo_capture_stage_t  stage;
    esp_capture_video_info_t    video_info;
    uint8_t                    *best_frame;
    int                         best_frame_size;
    uint32_t                    cur_pts;
    uint32_t                    best_frame_pts;
    uint32_t                    best_frame_score;
    int                         metadata_fd;
    int                         camera_fd;
    bool                        camera_stats_supported;
} live_photo_capture_stage_src_t;

typedef struct {
    live_photo_capture_stage_src_t  stage_src;
    esp_capture_audio_src_if_t     *audio_src;
    esp_capture_handle_t            capture;
    esp_muxer_handle_t              muxer;
} live_photo_capture_sys_t;

static live_photo_capture_sys_t live_capture;
static char *live_photo_path = NULL;

static esp_capture_video_src_if_t *create_video_source(void)
{
#ifdef CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT
    dev_camera_handle_t *camera_handle = NULL;
    esp_err_t ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&camera_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get camera device");
        return NULL;
    }
    esp_capture_video_v4l2_src_cfg_t v4l2_cfg = {
        .buf_count = 2,
    };
    strncpy(v4l2_cfg.dev_name, camera_handle->dev_path, sizeof(v4l2_cfg.dev_name) - 1);
    return esp_capture_new_video_v4l2_src(&v4l2_cfg);
#else
    return NULL;
#endif  /* CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT */
}

static esp_capture_err_t stage_src_open(esp_capture_video_src_if_t *src)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    stage_src->metadata_fd = -1;
    stage_src->camera_fd = open("/dev/video0", O_RDONLY);
    if (stage_src->camera_fd >= 0) {
        struct v4l2_query_ext_ctrl qctrl;
        qctrl.id = V4L2_CID_CAMERA_STATS;
        int ret = ioctl(stage_src->camera_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
        if (ret == 0) {
            stage_src->camera_stats_supported = true;
        } else {
            close(stage_src->camera_fd);
            stage_src->camera_fd = -1;
            ESP_LOGW(TAG, "Not support camera stats");
        }
    }
    if (stage_src->camera_stats_supported == false) {
        stage_src->metadata_fd = open("/dev/video20", O_RDONLY);
    }
    return stage_src->video_src->open(stage_src->video_src);
}

static esp_capture_err_t stage_src_get_support_codecs(esp_capture_video_src_if_t *src, const esp_capture_format_id_t **codecs,
                                                      uint8_t *num)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    return stage_src->video_src->get_support_codecs(stage_src->video_src, codecs, num);
}

static esp_capture_err_t stage_src_set_fixed_caps(esp_capture_video_src_if_t *src, const esp_capture_video_info_t *fixed_caps)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    return stage_src->video_src->set_fixed_caps(stage_src->video_src, fixed_caps);
}

static esp_capture_err_t stage_src_negotiate_caps(esp_capture_video_src_if_t *src, esp_capture_video_info_t *in_caps,
                                                  esp_capture_video_info_t *out_caps)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_COVER) {
        if (in_caps->format_id == ESP_CAPTURE_FMT_ID_ANY || in_caps->format_id == stage_src->video_info.format_id) {
            *out_caps = stage_src->video_info;
            return ESP_CAPTURE_ERR_OK;
        }
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    esp_capture_err_t ret = stage_src->video_src->negotiate_caps(stage_src->video_src, in_caps, out_caps);
    if (ret == ESP_CAPTURE_ERR_OK) {
        stage_src->video_info = *out_caps;
    }
    return ret;
}

static esp_capture_err_t stage_src_start(esp_capture_video_src_if_t *src)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_COVER) {
        return ESP_CAPTURE_ERR_OK;
    }
    stage_src->best_frame_pts = 0;
    stage_src->best_frame_score = 0;
    return stage_src->video_src->start(stage_src->video_src);
}

static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}

static uint32_t stage_src_calc_score(live_photo_capture_stage_src_t *stage_src)
{
    // Prefer real-time camera stats when available.
    if (stage_src->camera_stats_supported) {
        struct v4l2_ext_controls stats_ctrls = {0};
        struct v4l2_ext_control stats_ctrl = {0};
        esp_cam_sensor_stats_t sensor_stats = {0};

        stats_ctrls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        stats_ctrls.count = 1;
        stats_ctrls.controls = &stats_ctrl;
        stats_ctrl.id = V4L2_CID_CAMERA_STATS;
        stats_ctrl.p_u8 = (uint8_t *)&sensor_stats;
        stats_ctrl.size = sizeof(sensor_stats);
        if (ioctl(stage_src->camera_fd, VIDIOC_G_EXT_CTRLS, &stats_ctrls) == 0) {
            float r = sensor_stats.wb_avg.red_avg / 255.0f;
            float g = sensor_stats.wb_avg.green_avg / 255.0f;
            float b = sensor_stats.wb_avg.blue_avg / 255.0f;
            float luma = 0.299f * r + 0.587f * g + 0.114f * b;
            float exposure_score = 1.0f - clampf(fabsf(luma - 0.5f) * 2.2f, 0.0f, 1.0f);

            float rg = r / (g + 1e-3f);
            float bg = b / (g + 1e-3f);
            float wb_err = fabsf(log2f(clampf(rg, 0.125f, 8.0f))) + fabsf(log2f(clampf(bg, 0.125f, 8.0f)));
            float wb_score = 1.0f - clampf(wb_err * 0.5f, 0.0f, 1.0f);

            float gain = sensor_stats.agc_gain > 0.0f ? sensor_stats.agc_gain : 1.0f;
            float gain_penalty = clampf(log2f(gain) / 6.0f, 0.0f, 1.0f);
            float exposure_us = (float)sensor_stats.aec_exp;
            float exp_penalty = clampf((exposure_us - 6000.0f) / 26000.0f, 0.0f, 1.0f);
            float stability_score = 1.0f - clampf(0.6f * gain_penalty + 0.4f * exp_penalty, 0.0f, 1.0f);

            float final_score = 0.55f * exposure_score + 0.25f * wb_score + 0.20f * stability_score;
            final_score = clampf(final_score, 0.0f, 1.0f);
            return (uint32_t)(final_score * 1000.0f);
        }
    }

    if (stage_src->metadata_fd >= 0) {
        struct v4l2_query_ext_ctrl qctrl = {0};
        struct v4l2_ext_controls ext_ctrls = {0};
        struct v4l2_ext_control controls[5] = {0};
        esp_video_isp_af_t af_cfg = {0};
        esp_video_isp_awb_t awb_cfg = {0};

        // Use ISP extra controls only
        controls[0].id = V4L2_CID_BRIGHTNESS;
        controls[1].id = V4L2_CID_RED_BALANCE;
        controls[2].id = V4L2_CID_BLUE_BALANCE;
        controls[3].id = V4L2_CID_USER_ESP_ISP_AF;
        controls[3].p_u8 = (uint8_t *)&af_cfg;
        controls[3].size = sizeof(af_cfg);
        controls[4].id = V4L2_CID_USER_ESP_ISP_AWB;
        controls[4].p_u8 = (uint8_t *)&awb_cfg;
        controls[4].size = sizeof(awb_cfg);

        ext_ctrls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        ext_ctrls.count = 5;
        ext_ctrls.controls = controls;
        int ret = ioctl(stage_src->metadata_fd, VIDIOC_G_EXT_CTRLS, &ext_ctrls);
        if (ret != 0) {
            ESP_LOGW(TAG, "Failed to get ISP extra controls ret %d", ret);
            return 0;
        }

        // Exposure proxy from current brightness.
        int brightness = controls[0].value;
        int brightness_min = 0;
        int brightness_max = 255;
        qctrl.id = V4L2_CID_BRIGHTNESS;
        ret = ioctl(stage_src->metadata_fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
        if (ret == 0) {
            brightness_min = (int)qctrl.minimum;
            brightness_max = (int)qctrl.maximum;
        }
        float brightness_norm = 0.5f;
        if (brightness_max > brightness_min) {
            brightness_norm = ((float)brightness - (float)brightness_min) / ((float)brightness_max - (float)brightness_min);
            brightness_norm = clampf(brightness_norm, 0.0f, 1.0f);
        }
        float exposure_score = 1.0f - clampf(fabsf(brightness_norm - 0.5f) * 2.0f, 0.0f, 1.0f);

        // White balance proxy from current RGB balance gains.
        float red_gain = controls[1].value / (float)V4L2_CID_RED_BALANCE_DEN;
        float blue_gain = controls[2].value / (float)V4L2_CID_BLUE_BALANCE_DEN;
        red_gain = clampf(red_gain, 0.125f, 8.0f);
        blue_gain = clampf(blue_gain, 0.125f, 8.0f);
        float wb_err = fabsf(log2f(red_gain)) + fabsf(log2f(blue_gain));
        float wb_score = 1.0f - clampf(wb_err * 0.5f, 0.0f, 1.0f);

        // AF/AWB provide a lightweight stability hint.
        float af_score = af_cfg.enable ? clampf(0.4f + (float)af_cfg.edge_thresh / 2048.0f, 0.0f, 1.0f) : 0.4f;
        float awb_score = awb_cfg.enable ? 1.0f : 0.6f;
        float stability_score = clampf(0.7f * af_score + 0.3f * awb_score, 0.0f, 1.0f);

        float final_score = 0.50f * exposure_score + 0.30f * wb_score + 0.20f * stability_score;
        final_score = clampf(final_score, 0.0f, 1.0f);
        return (uint32_t)(final_score * 1000.0f);
    }
    return 0;
}

static esp_capture_err_t stage_src_store_best_frame(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->best_frame == NULL || frame->size > stage_src->best_frame_size) {
        uint8_t *new_buf = heap_caps_aligned_alloc(64, frame->size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (new_buf == NULL) {
            return ESP_CAPTURE_ERR_NO_MEM;
        }
        if (stage_src->best_frame) {
            free(stage_src->best_frame);
        }
        stage_src->best_frame = new_buf;
        stage_src->best_frame_size = frame->size;
    }
    uint32_t score = 0;
    if (stage_src->metadata_fd >= 0 || stage_src->camera_stats_supported) {
        score = stage_src_calc_score(stage_src);
        if (stage_src->best_frame_score == 0) {
            stage_src->best_frame_score = score;
        }
    }
    // Store half if all score same
    if (score > stage_src->best_frame_score ||
        (stage_src->best_frame_pts == 0 && stage_src->cur_pts >= LIVE_PHOTO_DURATION_MS / 2)) {
        memcpy(stage_src->best_frame, frame->data, frame->size);
        stage_src->best_frame_size = frame->size;
        stage_src->best_frame_pts = stage_src->cur_pts;
        stage_src->best_frame_score = score;
    }
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t stage_src_acquire_frame(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_COVER) {
        if (stage_src->best_frame == NULL || stage_src->best_frame_size <= 0) {
            return ESP_CAPTURE_ERR_NOT_FOUND;
        }
        frame->data = stage_src->best_frame;
        frame->size = stage_src->best_frame_size;
        frame->pts = stage_src->best_frame_pts;
        return ESP_CAPTURE_ERR_OK;
    }

    esp_capture_err_t ret = stage_src->video_src->acquire_frame(stage_src->video_src, frame);
    if (ret == ESP_CAPTURE_ERR_OK && stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_VIDEO) {
        (void)stage_src_store_best_frame(src, frame);
    }
    return ret;
}

static esp_capture_err_t stage_src_release_frame(esp_capture_video_src_if_t *src, esp_capture_stream_frame_t *frame)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_COVER) {
        return ESP_CAPTURE_ERR_OK;
    }
    return stage_src->video_src->release_frame(stage_src->video_src, frame);
}

static esp_capture_err_t stage_src_stop(esp_capture_video_src_if_t *src)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->stage == LIVE_PHOTO_CAPTURE_STAGE_COVER) {
        return ESP_CAPTURE_ERR_OK;
    }
    return stage_src->video_src->stop(stage_src->video_src);
}

static esp_capture_err_t stage_src_close(esp_capture_video_src_if_t *src)
{
    live_photo_capture_stage_src_t *stage_src = (live_photo_capture_stage_src_t *)src;
    if (stage_src->metadata_fd >= 0) {
        close(stage_src->metadata_fd);
        stage_src->metadata_fd = -1;
    }
    if (stage_src->camera_fd >= 0) {
        close(stage_src->camera_fd);
        stage_src->camera_fd = -1;
    }
    return stage_src->video_src->close(stage_src->video_src);
}

static int create_stage_source(void)
{
    live_photo_capture_stage_src_t *stage_src = &live_capture.stage_src;

    stage_src->video_src = create_video_source();
    if (stage_src->video_src == NULL) {
        ESP_LOGE(TAG, "Failed to create video source");
        return -1;
    }
    stage_src->metadata_fd = -1;
    stage_src->stage = LIVE_PHOTO_CAPTURE_STAGE_VIDEO;

    stage_src->stage_vid_src.open = stage_src_open;
    stage_src->stage_vid_src.get_support_codecs = stage_src_get_support_codecs;
    stage_src->stage_vid_src.set_fixed_caps = stage_src_set_fixed_caps;
    stage_src->stage_vid_src.negotiate_caps = stage_src_negotiate_caps;
    stage_src->stage_vid_src.start = stage_src_start;
    stage_src->stage_vid_src.acquire_frame = stage_src_acquire_frame;
    stage_src->stage_vid_src.release_frame = stage_src_release_frame;
    stage_src->stage_vid_src.stop = stage_src_stop;
    stage_src->stage_vid_src.close = stage_src_close;
    return 0;
}

static void live_photo_set_stage(live_photo_capture_stage_t stage)
{
    live_capture.stage_src.stage = stage;
}

static int slice_cb(char *file_path, int len, int slice_idx)
{
    (void)slice_idx;
    if (file_path == NULL || len <= 1 || live_photo_path == NULL) {
        return -1;
    }
    strncpy(file_path, live_photo_path, len - 1);
    file_path[len - 1] = '\0';
    ESP_LOGI(TAG, "Start to write to file %s", file_path);
    return 0;
}

int live_photo_capture_sys_create(void)
{
    if (create_stage_source() != 0) {
        ESP_LOGE(TAG, "Failed to create stage source");
        return -1;
    }

    esp_codec_dev_handle_t record_handle = NULL;
    dev_audio_codec_handles_t *codec_handle = NULL;
    esp_err_t ret = esp_board_device_get_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&codec_handle);
    if (ret == ESP_OK) {
        record_handle = codec_handle->codec_dev;
    }
    if (record_handle) {
        esp_codec_dev_set_in_gain(record_handle, 32);
        esp_capture_audio_dev_src_cfg_t codec_cfg = {
            .record_handle = record_handle,
        };
        live_capture.audio_src = esp_capture_new_audio_dev_src(&codec_cfg);
        if (live_capture.audio_src == NULL) {
            ESP_LOGE(TAG, "Fail to create audio source");
            return -1;
        }
    }

    esp_capture_cfg_t capture_cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = live_capture.audio_src,
        .video_src = &live_capture.stage_src.stage_vid_src,
    };
    esp_capture_open(&capture_cfg, &live_capture.capture);
    if (live_capture.capture == NULL) {
        ESP_LOGE(TAG, "Fail to create capture");
        return -1;
    }
    return 0;
}

int live_photo_capture_sys_destroy(void)
{
    if (live_capture.capture) {
        esp_capture_close(live_capture.capture);
        live_capture.capture = NULL;
    }
    if (live_capture.audio_src) {
        free(live_capture.audio_src);
        live_capture.audio_src = NULL;
    }
    if (live_capture.stage_src.video_src) {
        free(live_capture.stage_src.video_src);
        live_capture.stage_src.video_src = NULL;
    }
    if (live_capture.stage_src.metadata_fd >= 0) {
        close(live_capture.stage_src.metadata_fd);
        live_capture.stage_src.metadata_fd = -1;
    }
    memset(&live_capture, 0, sizeof(live_photo_capture_sys_t));
    return 0;
}

int live_photo_capture_start(void)
{
    esp_live_photo_muxer_config_t lp_cfg = {0};
    esp_muxer_config_t *base_cfg = &lp_cfg.mp4_config.base_config;
    base_cfg->muxer_type = ESP_MUXER_TYPE_LIVE_PHOTO;
    base_cfg->ram_cache_size = 16 * 1024;
    base_cfg->url_pattern = slice_cb;
    base_cfg->slice_duration = LIVE_PHOTO_DURATION_MS + 1000;
    lp_cfg.mp4_config.display_in_order = true;
    lp_cfg.mp4_config.moov_before_mdat = false;
    lp_cfg.reserve_cover_size = MAX_JPEG_SIZE;

    live_capture.muxer = esp_muxer_open(base_cfg, sizeof(esp_live_photo_muxer_config_t));
    if (live_capture.muxer == NULL) {
        ESP_LOGE(TAG, "Failed to create muxer");
        return -1;
    }

    int audio_index = -1;
    int video_index = -1;
    esp_muxer_audio_stream_info_t aud_info = {
        .codec = AUDIO_FORMAT,
        .channel = AUDIO_CHANNEL,
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = 16,
        .min_packet_duration = 20,
    };
    (void)esp_muxer_add_audio_stream(live_capture.muxer, &aud_info, &audio_index);

    esp_muxer_video_stream_info_t vid_info = {
        .codec = VIDEO_FORMAT,
        .width = VIDEO_WIDTH,
        .height = VIDEO_HEIGHT,
        .fps = VIDEO_FPS,
        .min_packet_duration = 1000 / VIDEO_FPS,
    };
    (void)esp_muxer_add_video_stream(live_capture.muxer, &vid_info, &video_index);

    live_photo_set_stage(LIVE_PHOTO_CAPTURE_STAGE_VIDEO);
    esp_capture_sink_cfg_t sink_cfg_video = {
        .audio_info = {
            .format_id = AUDIO_FORMAT,
            .sample_rate = AUDIO_SAMPLE_RATE,
            .channel = AUDIO_CHANNEL,
            .bits_per_sample = 16,
        },
        .video_info = {
            .format_id = VIDEO_FORMAT,
            .width = VIDEO_WIDTH,
            .height = VIDEO_HEIGHT,
            .fps = VIDEO_FPS,
        },
    };
    esp_capture_sink_handle_t sink_video = NULL;
    if (esp_capture_sink_setup(live_capture.capture, 0, &sink_cfg_video, &sink_video) != ESP_CAPTURE_ERR_OK || sink_video == NULL) {
        ESP_LOGE(TAG, "Failed to setup video sink");
        return -1;
    }
    (void)esp_capture_sink_enable(sink_video, ESP_CAPTURE_RUN_MODE_ALWAYS);
    (void)esp_capture_start(live_capture.capture);

    esp_capture_stream_frame_t frame = {0};
    uint32_t cur_time = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t end_time = cur_time + LIVE_PHOTO_DURATION_MS;
    uint32_t start_time = cur_time;
    int video_frame_count = 0;
    int audio_frame_count = 0;
    int video_frame_size = 0;
    int audio_frame_size = 0;
    int video_last_pts = 0;
    int audio_last_pts = 0;

    while (cur_time < end_time) {
        frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
        live_capture.stage_src.cur_pts = cur_time - start_time;
        while (esp_capture_sink_acquire_frame(sink_video, &frame, true) == ESP_CAPTURE_ERR_OK) {
            video_frame_size += frame.size;
            video_last_pts = frame.pts;
            video_frame_count++;
            esp_muxer_video_packet_t video_packet = {
                .pts = frame.pts,
                .data = frame.data,
                .len = frame.size,
            };
            (void)esp_muxer_add_video_packet(live_capture.muxer, video_index, &video_packet);
            (void)esp_capture_sink_release_frame(sink_video, &frame);
        }

        frame.stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
        while (esp_capture_sink_acquire_frame(sink_video, &frame, true) == ESP_CAPTURE_ERR_OK) {
            audio_frame_size += frame.size;
            audio_last_pts = frame.pts;
            audio_frame_count++;
            esp_muxer_audio_packet_t audio_packet = {
                .pts = frame.pts,
                .data = frame.data,
                .len = frame.size,
            };
            (void)esp_muxer_add_audio_packet(live_capture.muxer, audio_index, &audio_packet);
            (void)esp_capture_sink_release_frame(sink_video, &frame);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        cur_time = (uint32_t)(esp_timer_get_time() / 1000);
    }
    ESP_LOGI(TAG, "Video: frame_count:%d size:%d last_pts:%d", video_frame_count, video_frame_size, video_last_pts);
    ESP_LOGI(TAG, "Audio: frame_count:%d size:%d last_pts:%d", audio_frame_count, audio_frame_size, audio_last_pts);
    (void)esp_capture_stop(live_capture.capture);

    live_photo_set_stage(LIVE_PHOTO_CAPTURE_STAGE_COVER);
    esp_capture_sink_cfg_t sink_cfg_cover = {
        .video_info = {
            .format_id = ESP_CAPTURE_FMT_ID_MJPEG,
            .width = VIDEO_WIDTH,
            .height = VIDEO_HEIGHT,
            .fps = 1,
        },
    };
    esp_capture_sink_handle_t sink_cover = NULL;
    if (esp_capture_sink_setup(live_capture.capture, 0, &sink_cfg_cover, &sink_cover) != ESP_CAPTURE_ERR_OK || sink_cover == NULL) {
        ESP_LOGE(TAG, "Failed to setup cover sink");
        return -1;
    }
    (void)esp_capture_sink_enable(sink_cover, ESP_CAPTURE_RUN_MODE_ALWAYS);
    (void)esp_capture_start(live_capture.capture);

    cur_time = (uint32_t)(esp_timer_get_time() / 1000);
    end_time = cur_time + 500;  // Timeout avoid encode failed
    int cover_size = 0;
    frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
    while (cur_time < end_time) {
        if (esp_capture_sink_acquire_frame(sink_cover, &frame, true) == ESP_CAPTURE_ERR_OK) {
            esp_live_photo_muxer_cover_info_t cover_info = {
                .jpeg = frame.data,
                .jpeg_len = frame.size,
                .jpeg_pts = frame.pts,
            };
            (void)esp_live_photo_muxer_set_cover_jpeg(live_capture.muxer, &cover_info);
            (void)esp_capture_sink_release_frame(sink_cover, &frame);
            cover_size = frame.size;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        cur_time = (uint32_t)(esp_timer_get_time() / 1000);
    }
    (void)esp_capture_stop(live_capture.capture);
    ESP_LOGI(TAG, "Cover size: %d", cover_size);
    return 0;
}

int live_photo_capture_stop(void)
{
    if (live_capture.capture) {
        (void)esp_capture_stop(live_capture.capture);
    }
    if (live_capture.muxer) {
        (void)esp_muxer_close(live_capture.muxer);
        live_capture.muxer = NULL;
    }
    if (live_capture.stage_src.best_frame) {
        free(live_capture.stage_src.best_frame);
        live_capture.stage_src.best_frame = NULL;
        live_capture.stage_src.best_frame_size = 0;
        live_capture.stage_src.best_frame_pts = 0;
        live_capture.stage_src.best_frame_score = 0;
    }
    return 0;
}

int live_photo_capture(char *file_path)
{
    live_photo_path = file_path;
    (void)esp_live_photo_muxer_register();
    (void)mp4_muxer_register();
    (void)esp_audio_enc_register_default();
    (void)esp_video_enc_register_default();

    int ret = live_photo_capture_sys_create();
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to create capture system");
    } else {
        ret = live_photo_capture_start();
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to run live photo capture");
        }
        (void)live_photo_capture_stop();
    }

    (void)live_photo_capture_sys_destroy();
    (void)esp_audio_enc_unregister_default();
    (void)esp_video_enc_unregister_default();
    esp_muxer_unreg_all();
    return ret;
}
