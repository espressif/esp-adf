/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_board_manager_includes.h"
#include "dev_fs_fat.h"
#include "esp_capture_defaults.h"
#include "esp_gmf_err.h"
#include "mp4_muxer.h"
#include "av_rec_config.h"

static const char *TAG = "AV_REC_CAPTURE";

static void record_display_scheduler(const char *thread_name, esp_capture_thread_schedule_cfg_t *schedule_cfg)
{
    if (strcmp(thread_name, "venc_0") == 0) {
        schedule_cfg->stack_size = 4 * 1024;
        schedule_cfg->priority = 2;
        schedule_cfg->core_id = RECORD_CORE_ID;
    } else if (strcmp(thread_name, "venc_1") == 0) {
        schedule_cfg->stack_size = 4 * 1024;
        schedule_cfg->priority = 2;
        schedule_cfg->core_id = DISPLAY_CORE_ID;
    } else if (strcmp(thread_name, "aenc_0") == 0) {
        schedule_cfg->stack_size = 6 * 1024;
        schedule_cfg->priority = 2;
        schedule_cfg->core_id = DISPLAY_CORE_ID;
    } else if (strcmp(thread_name, "AUD_SRC") == 0) {
        schedule_cfg->stack_size = 4 * 1024;
        schedule_cfg->priority = 15;
        schedule_cfg->core_id = RECORD_CORE_ID;
    } else if (strcmp(thread_name, "Muxer") == 0) {
        schedule_cfg->stack_size = 4 * 1024;
        schedule_cfg->priority = 5;
        schedule_cfg->core_id = RECORD_CORE_ID;
    }
}

static esp_capture_audio_src_if_t *create_audio_source(void)
{
    dev_audio_codec_handles_t *codec_handle = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&codec_handle);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return NULL, "Failed to get audio ADC device handle");
    ESP_GMF_CHECK(TAG, codec_handle != NULL && codec_handle->codec_dev != NULL, return NULL, "Audio ADC device handle invalid");
    esp_capture_audio_dev_src_cfg_t codec_cfg = {
        .record_handle = codec_handle->codec_dev,
    };
    return esp_capture_new_audio_dev_src(&codec_cfg);
}

static esp_capture_video_src_if_t *create_video_source(void)
{
    dev_camera_handle_t *camera_handle = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&camera_handle);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, return NULL, "Failed to get camera device handle");
    ESP_GMF_CHECK(TAG, camera_handle != NULL && camera_handle->dev_path != NULL, return NULL, "Camera device handle invalid");
    esp_capture_video_v4l2_src_cfg_t v4l2_cfg = {
        .buf_count = 2,
    };
    strncpy(v4l2_cfg.dev_name, camera_handle->dev_path, sizeof(v4l2_cfg.dev_name) - 1);
    v4l2_cfg.dev_name[sizeof(v4l2_cfg.dev_name) - 1] = '\0';
    return esp_capture_new_video_v4l2_src(&v4l2_cfg);
}

static const char *get_storage_mount_point(void)
{
    dev_fs_fat_handle_t *sdcard = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&sdcard);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && sdcard != NULL && sdcard->mount_point != NULL,
                  return NULL, "Failed to get SD card mount point");
    return sdcard->mount_point;
}

static int av_rec_storage_slice_handler(esp_muxer_slice_info_t *info, void *ctx)
{
    (void)ctx;
    const char *mount_point = get_storage_mount_point();
    if (mount_point == NULL) {
        return -1;
    }
    snprintf(info->file_path, info->len, "%s/audio_video_record_%03d.mp4", mount_point, info->slice_index);
    ESP_LOGI(TAG, "Record file: %s", info->file_path);
    return 0;
}

esp_err_t av_rec_build_capture(av_record_live_display_sys_t *sys)
{
    esp_capture_set_thread_scheduler(record_display_scheduler);

    sys->audio_src = create_audio_source();
    ESP_GMF_CHECK(TAG, sys->audio_src != NULL, return ESP_FAIL, "Failed to create audio source");
    sys->video_src = create_video_source();
    ESP_GMF_CHECK(TAG, sys->video_src != NULL, {
        free(sys->audio_src);
        sys->audio_src = NULL;
        return ESP_FAIL;
    }, "Failed to create video source");

#if CONFIG_IDF_TARGET_ESP32S3
    esp_capture_video_info_t fixed_caps = {
        .format_id = sys->display_info.format_id,
        .width = RECORD_WIDTH,
        .height = RECORD_HEIGHT,
        .fps = RECORD_FPS,
    };
    sys->video_src->set_fixed_caps(sys->video_src, &fixed_caps);
#endif  /* CONFIG_IDF_TARGET_ESP32S3 */

    esp_capture_cfg_t capture_cfg = {
        .sync_mode = ESP_CAPTURE_SYNC_MODE_AUDIO,
        .audio_src = sys->audio_src,
        .video_src = sys->video_src,
    };
    esp_capture_err_t ret = esp_capture_open(&capture_cfg, &sys->capture);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK && sys->capture != NULL,
                  return ESP_FAIL, "Failed to open capture");
    return ESP_OK;
}

esp_err_t av_rec_setup_record_sink(av_record_live_display_sys_t *sys)
{
    esp_capture_sink_cfg_t sink_cfg = {
        .video_info = {
            .format_id = RECORD_FORMAT_ID,
            .width = RECORD_WIDTH,
            .height = RECORD_HEIGHT,
            .fps = RECORD_FPS,
        },
        .audio_info = {
            .format_id = REC_AUDIO_FMT,
            .sample_rate = REC_AUDIO_SAMPLE_RATE,
            .channel = REC_AUDIO_CHANNEL,
            .bits_per_sample = REC_AUDIO_BITS,
        },
    };
    esp_capture_err_t ret = esp_capture_sink_setup(sys->capture, 0, &sink_cfg, &sys->record_sink);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to setup record sink");
    ret = esp_capture_sink_set_bitrate(sys->record_sink, ESP_CAPTURE_STREAM_TYPE_VIDEO, RECORD_BITRATE);
    if (ret != ESP_CAPTURE_ERR_OK) {
        ESP_LOGW(TAG, "Failed to set record bitrate");
    }

    mp4_muxer_config_t mp4_cfg = {
        .base_config = {
            .muxer_type = ESP_MUXER_TYPE_MP4,
            .url_pattern_ex = av_rec_storage_slice_handler,
            .slice_duration = DEFAULT_SLICE_DURATION_MS,
            .ram_cache_size = 8 * 1024,
        },
    };
    esp_capture_muxer_cfg_t muxer_cfg = {
        .base_config = &mp4_cfg.base_config,
        .cfg_size = sizeof(mp4_cfg),
        .muxer_mask = ESP_CAPTURE_MUXER_MASK_ALL,
    };
    ret = esp_capture_sink_add_muxer(sys->record_sink, &muxer_cfg);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to add MP4 muxer");
    ret = esp_capture_sink_enable_muxer(sys->record_sink, true);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to enable muxer");
    ret = esp_capture_sink_disable_stream(sys->record_sink, ESP_CAPTURE_STREAM_TYPE_AUDIO);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to disable audio stream");
    ret = esp_capture_sink_disable_stream(sys->record_sink, ESP_CAPTURE_STREAM_TYPE_VIDEO);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to disable video stream");
    ret = esp_capture_sink_enable(sys->record_sink, ESP_CAPTURE_RUN_MODE_ALWAYS);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to enable record sink");
    return ESP_OK;
}

esp_err_t av_rec_setup_display_sink(av_record_live_display_sys_t *sys)
{
    esp_capture_sink_cfg_t sink_cfg = {
        .video_info = sys->display_info,
    };
    esp_capture_err_t ret = esp_capture_sink_setup(sys->capture, 1, &sink_cfg, &sys->display_sink);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to setup display sink");
    ret = esp_capture_sink_enable(sys->display_sink, ESP_CAPTURE_RUN_MODE_ALWAYS);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to enable display sink");
    return ESP_OK;
}

esp_err_t av_rec_run_live_session(av_record_live_display_sys_t *sys, int duration_ms)
{
    esp_capture_err_t cap_ret = esp_capture_start(sys->capture);
    ESP_GMF_CHECK(TAG, cap_ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to start capture");

    esp_err_t display_ret = av_rec_run_display(sys, duration_ms);

    cap_ret = esp_capture_stop(sys->capture);
    if (cap_ret != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to stop capture, ret=%d", cap_ret);
    }

    ESP_GMF_CHECK(TAG, display_ret == ESP_OK,
                  return display_ret, "Failed to run display");
    ESP_GMF_CHECK(TAG, cap_ret == ESP_CAPTURE_ERR_OK,
                  return ESP_FAIL, "Failed to stop capture");
    return ESP_OK;
}

esp_err_t av_rec_check_record_file(int *file_size)
{
    ESP_GMF_CHECK(TAG, file_size != NULL, return ESP_ERR_INVALID_ARG, "Invalid file size buffer");
    *file_size = -1;
    char file_path[64] = {0};
    const char *mount_point = get_storage_mount_point();
    if (mount_point == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    snprintf(file_path, sizeof(file_path), "%s/audio_video_record_000.mp4", mount_point);
    FILE *fp = fopen(file_path, "rb");
    ESP_GMF_CHECK(TAG, fp != NULL, return ESP_ERR_NOT_FOUND, "Record file not found");
    fseek(fp, 0, SEEK_END);
    *file_size = ftell(fp);
    if (fclose(fp) != 0) {
        ESP_LOGW(TAG, "Failed to close record file");
    }
    ESP_LOGI(TAG, "Record file size: %s -> %d bytes", file_path, *file_size);
    return ESP_OK;
}

void av_rec_release_capture(av_record_live_display_sys_t *sys)
{
    if (sys->capture) {
        esp_capture_close(sys->capture);
        sys->capture = NULL;
    }
    if (sys->audio_src) {
        free(sys->audio_src);
        sys->audio_src = NULL;
    }
    if (sys->video_src) {
        free(sys->video_src);
        sys->video_src = NULL;
    }
    if (sys->display_done_sem) {
        vSemaphoreDelete(sys->display_done_sem);
        sys->display_done_sem = NULL;
    }
}
