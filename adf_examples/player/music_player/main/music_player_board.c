/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_gmf_err.h"
#include "esp_board_manager_includes.h"
#include "dev_fs_fat.h"
#include "esp_codec_dev.h"
#include "music_player_board.h"
#include "music_player_config.h"

static const char *TAG = "MUSIC_PLAYER_BOARD";

static bool s_ldo_inited = false;
static bool s_sdcard_inited = false;
static bool s_dac_inited = false;

esp_err_t music_player_board_init(music_player_board_t *board)
{
    ESP_GMF_CHECK(TAG, board != NULL, return ESP_ERR_INVALID_ARG, "Board is NULL");
    ESP_GMF_CHECK(TAG, !(s_sdcard_inited || s_dac_inited), return ESP_ERR_INVALID_STATE, "Board already initialized");

    memset(board, 0, sizeof(*board));

    esp_err_t ret = ESP_OK;
#if CONFIG_IDF_TARGET_ESP32P4
    ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to init MIPI LDO");
    s_ldo_inited = true;
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to init SD card");
    s_sdcard_inited = true;

    dev_fs_fat_handle_t *sd = NULL;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_FS_SDCARD, (void **)&sd);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && sd != NULL && sd->mount_point != NULL,
                  { ret = ESP_FAIL; goto err_cleanup;}, "Failed to get SD card mount point");
    snprintf(board->mount_point, sizeof(board->mount_point), "%s", sd->mount_point);
    snprintf(board->scan_dir, sizeof(board->scan_dir), "%s", sd->mount_point);

    ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to init audio DAC");
    s_dac_inited = true;

    dev_audio_codec_handles_t *play_dev = NULL;
    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&play_dev);
    ESP_GMF_CHECK(TAG, ret == ESP_OK && play_dev != NULL && play_dev->codec_dev != NULL,
                  { ret = ESP_FAIL; goto err_cleanup;}, "Failed to get playback codec handle");
    board->playback_codec = play_dev->codec_dev;

    ret = esp_codec_dev_set_out_vol(board->playback_codec, MUSIC_PLAYER_DEFAULT_VOLUME);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to set playback volume");

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = CONFIG_AUDIO_SIMPLE_PLAYER_RESAMPLE_DEST_RATE,
        .channel = CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST,
#if CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_16BIT
        .bits_per_sample = 16,
#elif CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_24BIT
        .bits_per_sample = 24,
#elif CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_32BIT
        .bits_per_sample = 32,
#else
        .bits_per_sample = 16,
#endif  /* CONFIG_AUDIO_SIMPLE_PLAYER_BIT_CVT_DEST_16BIT */
    };
    ret = esp_codec_dev_open(board->playback_codec, &fs);
    ESP_GMF_RET_ON_ERROR(TAG, ret, goto err_cleanup, "Failed to open playback codec");

    ESP_LOGI(TAG, "SD card mounted at %s, scan dir: %s", board->mount_point, board->scan_dir);
    return ESP_OK;

err_cleanup:
    music_player_board_deinit(board);
    return ret;
}

void music_player_board_deinit(music_player_board_t *board)
{
    if (!s_ldo_inited && !s_sdcard_inited && !s_dac_inited) {
        return;
    }

    if (board != NULL && board->playback_codec != NULL) {
        esp_codec_dev_close(board->playback_codec);
        board->playback_codec = NULL;
    }

    if (s_dac_inited) {
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
        s_dac_inited = false;
    }
    if (s_sdcard_inited) {
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
        s_sdcard_inited = false;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    if (s_ldo_inited) {
        esp_board_periph_deinit(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
        s_ldo_inited = false;
    }
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */
}
