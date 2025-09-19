
/**
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2025 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdbool.h>

#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "dev_audio_codec.h"

#include "basic_board.h"

static const char *TAG = "BASIC_BOARD";

// Helper function to check if board name matches
static inline bool is_board_name(const char *board_name, const char *target_name)
{
    return strcmp(board_name, target_name) == 0;
}

esp_err_t basic_board_init(basic_board_periph_t *periph)
{
    esp_err_t ret = ESP_OK;
    memset(periph, 0, sizeof(basic_board_periph_t));

    dev_audio_codec_handles_t *play_dev_handles = NULL;
    dev_audio_codec_handles_t *rec_dev_handles = NULL;

    esp_board_manager_init();
    esp_board_manager_print();

    esp_board_device_init_all();

    esp_board_device_show("fs_sdcard");

    ret = esp_board_device_get_handle("audio_dac", (void **)&play_dev_handles);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get audio_dac handle");
        return ret;
    }
    ret = esp_board_device_get_handle("audio_adc", (void **)&rec_dev_handles);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get audio_adc handle");
        return ret;
    }
    periph->play_dev = play_dev_handles->codec_dev;
    periph->rec_dev = rec_dev_handles->codec_dev;

    if (periph->play_dev) {
        esp_codec_dev_set_out_vol(periph->play_dev, 70);
    }
    if (periph->rec_dev) {
        esp_codec_dev_set_in_gain(periph->rec_dev, 32.0);
        esp_codec_dev_set_in_channel_gain(periph->rec_dev, BIT(2), 32.0);
    }

    if (is_board_name(g_esp_board_info.name, "esp32s3_korvo2_v3") || is_board_name(g_esp_board_info.name, "echoear_core_board_v1_2")
        || is_board_name(g_esp_board_info.name, "esp_box_3")) {
        strcpy(periph->mic_layout, "RMNM");
        periph->sample_rate = 16000;
        periph->sample_bits = 32;
        periph->channels = 2;
    } else if (is_board_name(g_esp_board_info.name, "esp32_s3_korvo2l_v1")) {
        strcpy(periph->mic_layout, "MR");
        periph->sample_rate = 16000;
        periph->sample_bits = 16;
        periph->channels = 2;
    } else {
        // TODO: Add new board config
        ESP_LOGE(TAG, "Unsupported board: %s", g_esp_board_info.name);
        return ESP_FAIL;
    }
    return ESP_OK;
}