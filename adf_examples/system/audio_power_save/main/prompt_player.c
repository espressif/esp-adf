/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_EXAMPLE_ENABLE_PROMPT_PLAYBACK

#include <string.h>

#include "esp_audio_simple_player.h"
#include "esp_audio_simple_player_advance.h"
#include "esp_board_manager_includes.h"
#include "esp_codec_dev.h"
#include "esp_gmf_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"

#include "prompt_player.h"

static const char *TAG = "PROMPT_PLAYER";

#define DEFAULT_PLAY_VOLUME       80
#define LITTLEFS_BASE_PATH        "/littlefs"
#define LITTLEFS_PARTITION_LABEL  "storage"
#define PROMPT_URI_SLEEP          "file://littlefs/enter_sleep.mp3"
#define PROMPT_URI_WAKEUP         "file://littlefs/exit_sleep.mp3"

static esp_codec_dev_handle_t s_playback_handle;
static esp_asp_handle_t s_player;
static bool s_playback_codec_initialized;
static bool s_littlefs_initialized;

static int player_out_cb(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_handle_t dev = (esp_codec_dev_handle_t)ctx;
    esp_codec_dev_write(dev, data, data_size);
    return 0;
}

static int player_event_cb(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        if (event->payload == NULL || event->payload_size < sizeof(esp_asp_music_info_t)) {
            ESP_LOGW(TAG, "Ignore MUSIC_INFO event with invalid payload (size=%u)", (unsigned)event->payload_size);
            return 0;
        }
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, sizeof(info));
        ESP_LOGI(TAG, "Prompt info, rate:%d, channels:%d, bits:%d", info.sample_rate, info.channels, info.bits);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        if (event->payload == NULL || event->payload_size < sizeof(esp_asp_state_t)) {
            ESP_LOGW(TAG, "Ignore STATE event with invalid payload (size=%u)", (unsigned)event->payload_size);
            return 0;
        }
        esp_asp_state_t state = ESP_ASP_STATE_NONE;
        memcpy(&state, event->payload, sizeof(state));
        ESP_LOGI(TAG, "Prompt state: %s", esp_audio_simple_player_state_to_str(state));
    }
    return 0;
}

static esp_err_t setup_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_BASE_PATH,
        .partition_label = LITTLEFS_PARTITION_LABEL,
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to register LittleFS");

    size_t total = 0;
    size_t used = 0;
    ret = esp_littlefs_info(LITTLEFS_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to get LittleFS partition information, ret=%s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LittleFS partition size: total: %u, used: %u", (unsigned)total, (unsigned)used);
    }
    s_littlefs_initialized = true;
    return ESP_OK;
}

static esp_err_t setup_playback_codec(void)
{
    dev_audio_codec_handles_t *dac_handle = NULL;
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to initialize audio DAC");
    s_playback_codec_initialized = true;

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac_handle);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to get audio DAC device handle");
    ESP_GMF_CHECK(TAG, dac_handle != NULL && dac_handle->codec_dev != NULL, return ESP_FAIL,
                  "Failed to get audio DAC handle");

    s_playback_handle = dac_handle->codec_dev;
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = CONFIG_AUDIO_SIMPLE_PLAYER_RESAMPLE_DEST_RATE,
        .channel = CONFIG_AUDIO_SIMPLE_PLAYER_CH_CVT_DEST,
        .bits_per_sample = 16,
    };
    ret = esp_codec_dev_open(s_playback_handle, &fs);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to open playback codec");
    ret = esp_codec_dev_set_out_vol(s_playback_handle, DEFAULT_PLAY_VOLUME);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to set playback volume");
    return ESP_OK;
}

static esp_err_t create_player(void)
{
    esp_asp_cfg_t cfg = {
        .out.cb = player_out_cb,
        .out.user_ctx = s_playback_handle,
        .task_prio = 5,
        .task_stack = 6 * 1024,
    };

    esp_gmf_err_t gmf_ret = esp_audio_simple_player_new(&cfg, &s_player);
    ESP_GMF_RET_ON_NOT_OK(TAG, gmf_ret, return ESP_FAIL, "Failed to create simple player");
    gmf_ret = esp_audio_simple_player_set_event(s_player, player_event_cb, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, gmf_ret, goto cleanup, "Failed to set simple player event");
    return ESP_OK;

cleanup:
    esp_audio_simple_player_destroy(s_player);
    s_player = NULL;
    return ESP_FAIL;
}

static esp_err_t play_prompt(const char *uri, const char *name)
{
    ESP_LOGI(TAG, "Play %s prompt: %s", name, uri);
    esp_gmf_err_t gmf_ret = esp_audio_simple_player_run_to_end(s_player, uri, NULL);
    esp_audio_simple_player_stop(s_player);
    ESP_GMF_RET_ON_NOT_OK(TAG, gmf_ret, return ESP_FAIL, "Failed to play prompt");
    ESP_LOGI(TAG, "%s prompt finished", name);
    return ESP_OK;
}

void prompt_player_deinit(void);

esp_err_t prompt_player_init(void)
{
    ESP_GMF_RET_ON_ERROR(TAG, setup_littlefs(), return err_rc_, "Failed to setup LittleFS");
    ESP_GMF_RET_ON_ERROR(TAG, setup_playback_codec(), return err_rc_, "Failed to setup playback codec");
    esp_err_t ret = create_player();
    if (ret != ESP_OK) {
        prompt_player_deinit();
        return ret;
    }
    return ESP_OK;
}

esp_err_t prompt_player_play_sleep(void)
{
    return play_prompt(PROMPT_URI_SLEEP, "sleep");
}

esp_err_t prompt_player_play_wakeup(void)
{
    return play_prompt(PROMPT_URI_WAKEUP, "wakeup");
}

void prompt_player_deinit(void)
{
    if (s_player) {
        esp_audio_simple_player_stop(s_player);
        esp_audio_simple_player_destroy(s_player);
        s_player = NULL;
    }
    if (s_playback_codec_initialized) {
        if (s_playback_handle) {
            esp_codec_dev_close(s_playback_handle);
        }
        esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
        s_playback_codec_initialized = false;
        s_playback_handle = NULL;
    }
    if (s_littlefs_initialized) {
        esp_vfs_littlefs_unregister(LITTLEFS_PARTITION_LABEL);
        s_littlefs_initialized = false;
    }
}

#else  /* !CONFIG_EXAMPLE_ENABLE_PROMPT_PLAYBACK */

#include "esp_err.h"
#include "esp_log.h"

#include "prompt_player.h"

static const char *TAG = "PROMPT_PLAYER";

esp_err_t prompt_player_init(void)
{
    ESP_LOGI(TAG, "Prompt playback disabled by config");
    return ESP_OK;
}

esp_err_t prompt_player_play_sleep(void)
{
    return ESP_OK;
}

esp_err_t prompt_player_play_wakeup(void)
{
    return ESP_OK;
}

void prompt_player_deinit(void)
{
}

#endif  /* CONFIG_EXAMPLE_ENABLE_PROMPT_PLAYBACK */
