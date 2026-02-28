/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_board_manager_includes.h"
#include "esp_capture.h"
#include "settings.h"
#include "esp_log.h"

#define TAG  "LIVE_PHOTO_MAIN"

int live_photo_capture(char *file_path);

int live_photo_verify(char *file_path);

static void capture_scheduler(const char *thread_name, esp_capture_thread_schedule_cfg_t *schedule_cfg)
{
    if (strcmp(thread_name, "venc_0") == 0) {
        // For H264 may need huge stack if use hardware encoder can set it to small value
        schedule_cfg->core_id = 0;
        schedule_cfg->stack_size = 40 * 1024;
        schedule_cfg->priority = 1;
    } else if (strcmp(thread_name, "aenc_0") == 0) {
        // For OPUS encoder it need huge stack, when use G711 can set it to small value
        schedule_cfg->stack_size = 40 * 1024;
        schedule_cfg->priority = 2;
        schedule_cfg->core_id = 1;
    } else if (strcmp(thread_name, "AUD_SRC") == 0) {
        schedule_cfg->priority = 15;
    }
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    char *live_photo_path = LIVE_PHOTO_STORAGE_PATH;
    esp_err_t ret;
    ret = esp_board_device_init(ESP_BOARD_DEVICE_NAME_CAMERA);
    if (ret == ESP_OK) {
        ret = esp_board_device_init(ESP_BOARD_DEVICE_NAME_AUDIO_ADC);
        if (ret == ESP_OK) {
            dev_audio_codec_handles_t *device_handle = NULL;
            esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&device_handle);
            if (device_handle && device_handle->codec_dev) {
                esp_codec_dev_set_in_gain(device_handle->codec_dev, 32);
            }
        }
    }
    if (ret == ESP_OK) {
        ret = esp_board_device_init(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Skip muxer for mount sdcard failed");
        return;
    }
    // Set scheduler
    esp_capture_set_thread_scheduler(capture_scheduler);

    if (live_photo_capture(live_photo_path) == 0) {
        if (live_photo_verify(live_photo_path) == 0) {
            ESP_LOGI(TAG, "Live photo capture and verify success");
        } else {
            ESP_LOGE(TAG, "Live photo verify failed");
        }
    } else {
        ESP_LOGE(TAG, "Live photo capture failed");
    }
}
