/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "extractor_helper.h"
#include "esp_live_photo_extractor.h"
#include "esp_extractor_defaults.h"
#include "settings.h"
#include "esp_log.h"

#define TAG  "LIVE_PHOTO_VERIFY"

int live_photo_verify(char *file_path)
{
    // Register for live photo extractor
    esp_extractor_err_t ret = esp_live_photo_extractor_register();
    if (ret != ESP_EXTRACTOR_ERR_OK) {
        ESP_LOGE(TAG, "Failed to register live photo extractor");
        return -1;
    }
    esp_mp4_extractor_register();

    esp_extractor_handle_t muxed_ext = NULL;
    esp_extractor_config_t *muxed_cfg = NULL;
    int verify_ret = -1;
    do {
        muxed_cfg = esp_extractor_alloc_file_config(file_path, ESP_EXTRACT_MASK_AV, MAX_JPEG_SIZE * 2);
        if (!muxed_cfg) {
            ESP_LOGE(TAG, "Failed to alloc extractor config");
            break;
        }
        muxed_cfg->type = ESP_EXTRACTOR_TYPE_LIVE_PHOTO;

        if (esp_extractor_open(muxed_cfg, &muxed_ext) != ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGE(TAG, "Failed to open extractor");
            break;
        }

        if (esp_extractor_parse_stream(muxed_ext) != ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGE(TAG, "Failed to parse stream");
            break;
        }
        esp_extractor_type_t extractor_type = ESP_EXTRACTOR_TYPE_NONE;
        esp_extractor_get_extractor_type(muxed_ext, &extractor_type);
        if (extractor_type != ESP_EXTRACTOR_TYPE_LIVE_PHOTO) {
            ESP_LOGE(TAG, "Not a live photo extractor");
            break;
        }
        esp_extractor_frame_info_t frame = {};
        if (esp_live_photo_extractor_read_cover_frame(muxed_ext, &frame) != ESP_EXTRACTOR_ERR_OK) {
            ESP_LOGE(TAG, "Failed to read cover frame");
            break;
        }
        ESP_LOGI(TAG, "Cover Image size: %d header: %02x %02x tail:%02x %02x", (int)frame.frame_size,
                 frame.frame_buffer[0], frame.frame_buffer[1],
                 frame.frame_buffer[frame.frame_size - 2], frame.frame_buffer[frame.frame_size - 1]);
        esp_extractor_release_frame(muxed_ext, &frame);
        int audio_frame_count = 0;
        int video_frame_count = 0;
        int audio_frame_size = 0;
        int video_frame_size = 0;
        int audio_last_pts = 0;
        int video_last_pts = 0;
        while (1) {
            if (esp_extractor_read_frame(muxed_ext, &frame) != ESP_EXTRACTOR_ERR_OK) {
                break;
            }
            if (frame.frame_size) {
                if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO) {
                    audio_frame_size += frame.frame_size;
                    audio_last_pts = frame.pts;
                    audio_frame_count++;
                } else if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO) {
                    video_frame_size += frame.frame_size;
                    video_last_pts = frame.pts;
                    video_frame_count++;
                }
            }
            (void)esp_extractor_release_frame(muxed_ext, &frame);
        }
        ESP_LOGI(TAG, "Audio: frame_count:%d size:%d last_pts:%d", audio_frame_count, audio_frame_size, audio_last_pts);
        ESP_LOGI(TAG, "Video: frame_count:%d size:%d last_pts:%d", video_frame_count, video_frame_size, video_last_pts);
        verify_ret = 0;
    } while (0);
    // Close extractor
    if (muxed_ext) {
        (void)esp_extractor_close(muxed_ext);
    }
    // Close configuration
    if (muxed_cfg) {
        esp_extractor_free_config(muxed_cfg);
    }
    (void)esp_live_photo_extractor_unregister();
    (void)esp_mp4_extractor_unregister();
    return verify_ret;
}
