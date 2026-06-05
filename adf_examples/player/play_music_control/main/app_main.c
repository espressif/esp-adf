/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_config_storage.h"
#include "esp_gmf_err.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi_service.h"

#include "esp_board_manager_includes.h"
#include "esp_cli_service.h"
#include "esp_audio_simple_player.h"
#include "cli_music_control.h"
#include "esp_littlefs.h"

#define SIMPLE_PLAYER_TASK_PRIO   5
#define SIMPLE_PLAYER_TASK_STACK  (5 * 1024)
#define SIMPLE_PLAYER_TASK_CORE   1
#define CLI_SERVICE_TASK_PRIO     15
#define CLI_SERVICE_TASK_STACK    5120
#define DEFAULT_PLAYBACK_VOLUME   80
#define WIFI_CONNECT_WAIT_SEC     30
#define WIFI_PROFILE_MAX_NUM      4
#define WIFI_SSID                 CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASSWORD             CONFIG_EXAMPLE_WIFI_PASSWORD

static const char *TAG = "PLAY_MUSIC_CONTROL";

static esp_wifi_service_t *s_wifi_service = NULL;
static esp_wifi_service_profile_mgr_t s_profile_mgr = NULL;
static esp_config_storage_t s_profile_storage = NULL;

static const esp_config_storage_nvs_t s_wifi_profile_store = {
    .nvs_namespace = "play_music_wifi",
    .key_primary   = "prof_p",
    .key_backup    = "prof_b",
};

static const char *s_http_source_urls[] = {
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3",
    "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac",
    "https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.wav",
};

static const char *s_littlefs_tone_urls[] = {
    "file://littlefs/alarm.mp3",
    "file://littlefs/dingdong.mp3",
    "file://littlefs/haode.mp3",
    "file://littlefs/new_message.mp3",
};

static int out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_handle_t dev = (esp_codec_dev_handle_t)ctx;
    esp_codec_dev_write(dev, data, data_size);
    return 0;
}

static int simple_player_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    if (event->type == ESP_ASP_EVENT_TYPE_MUSIC_INFO) {
        if (event->payload == NULL || event->payload_size < sizeof(esp_asp_music_info_t)) {
            ESP_LOGW(TAG, "Ignore MUSIC_INFO event with invalid payload (size=%u)", (unsigned)event->payload_size);
            return 0;
        }
        esp_asp_music_info_t info = {0};
        memcpy(&info, event->payload, sizeof(info));
        ESP_LOGI(TAG, "Music info - rate:%d, channels:%d, bits:%d", info.sample_rate, info.channels, info.bits);
    } else if (event->type == ESP_ASP_EVENT_TYPE_STATE) {
        if (event->payload == NULL || event->payload_size < sizeof(esp_asp_state_t)) {
            ESP_LOGW(TAG, "Ignore STATE event with invalid payload (size=%u)", (unsigned)event->payload_size);
            return 0;
        }
        esp_asp_state_t state = 0;
        memcpy(&state, event->payload, sizeof(state));
        ESP_LOGI(TAG, "Player state: %s", esp_audio_simple_player_state_to_str(state));

        switch (state) {
            case ESP_ASP_STATE_RUNNING:
                esp_cli_music_player_set_playback_state(ESP_CLI_MUSIC_PLAYER_STATE_PLAYING);
                break;
            case ESP_ASP_STATE_PAUSED:
                esp_cli_music_player_set_playback_state(ESP_CLI_MUSIC_PLAYER_STATE_PAUSED);
                break;
            case ESP_ASP_STATE_STOPPED:
            case ESP_ASP_STATE_ERROR:
                esp_cli_music_player_set_playback_state(ESP_CLI_MUSIC_PLAYER_STATE_STOPPED);
                break;
            case ESP_ASP_STATE_FINISHED:
                ESP_LOGI(TAG, "Song finished");
                esp_cli_music_player_on_playback_finished();
                break;
            default:
                break;
        }
    }
    return 0;
}

static esp_err_t init_simple_player(esp_asp_handle_t *player)
{
    ESP_GMF_NULL_CHECK(TAG, player, return ESP_FAIL);

    dev_audio_codec_handles_t *dac_dev_handle = NULL;
    esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac_dev_handle);
    ESP_GMF_NULL_CHECK(TAG, dac_dev_handle, return ESP_FAIL);

    esp_asp_cfg_t cfg = {
        .in.cb = NULL,
        .in.user_ctx = NULL,
        .out.cb = out_data_callback,
        .out.user_ctx = dac_dev_handle->codec_dev,
        .task_prio = SIMPLE_PLAYER_TASK_PRIO,
        .task_stack = SIMPLE_PLAYER_TASK_STACK,
        .task_core = SIMPLE_PLAYER_TASK_CORE,
        .task_stack_in_ext = false,
        .prev = NULL,
        .prev_ctx = NULL
    };

    esp_gmf_err_t gmf_ret = esp_audio_simple_player_new(&cfg, player);
    ESP_GMF_RET_ON_NOT_OK(TAG, gmf_ret, return ESP_FAIL, "Failed to create simple player");

    gmf_ret = esp_audio_simple_player_set_event(*player, simple_player_event_callback, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, gmf_ret, goto cleanup, "Failed to set event callback");

    return ESP_OK;

cleanup:
    if (*player) {
        esp_audio_simple_player_destroy(*player);
        *player = NULL;
    }
    return ESP_FAIL;
}

static esp_err_t setup_peripheral(void)
{
    ESP_GMF_RET_ON_ERROR(TAG, esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD),
                         return err_rc_, "Failed to init SD card");
    ESP_GMF_RET_ON_ERROR(TAG, esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC),
                         return err_rc_, "Failed to init audio DAC");

    dev_audio_codec_handles_t *dac_dev_handle = NULL;
    ESP_GMF_RET_ON_ERROR(TAG, esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC,
                                                                  (void **)&dac_dev_handle),
                         return err_rc_, "Failed to get audio DAC device handle");
    ESP_GMF_CHECK(TAG, dac_dev_handle != NULL && dac_dev_handle->codec_dev != NULL,
                  return ESP_ERR_NOT_FOUND, "Audio DAC handle is NULL");

    esp_codec_dev_handle_t playback_handle = dac_dev_handle->codec_dev;
    ESP_GMF_RET_ON_ERROR(TAG, esp_codec_dev_set_out_vol(playback_handle, DEFAULT_PLAYBACK_VOLUME),
                         return err_rc_, "Failed to set output volume");

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
    ESP_GMF_RET_ON_ERROR(TAG, esp_codec_dev_open(playback_handle, &fs), return err_rc_,
                         "Failed to open playback codec");

    return ESP_OK;
}

static void close_playback_codec(void)
{
    dev_audio_codec_handles_t *dac_dev_handle = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac_dev_handle) != ESP_OK ||
        dac_dev_handle == NULL || dac_dev_handle->codec_dev == NULL) {
        return;
    }
    int ret = esp_codec_dev_close(dac_dev_handle->codec_dev);
    if (ret != 0) {
        ESP_LOGW(TAG, "Close playback codec failed: %d", ret);
    }
}

static void wifi_disconnect(void)
{
    if (s_wifi_service) {
        esp_wifi_service_destroy(s_wifi_service);
        s_wifi_service = NULL;
    }
    if (s_profile_mgr) {
        esp_wifi_service_profile_mgr_deinit(s_profile_mgr);
        s_profile_mgr = NULL;
    }
    if (s_profile_storage) {
        esp_config_storage_deinit(s_profile_storage);
        s_profile_storage = NULL;
    }
}

static esp_err_t wifi_connect(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_GMF_RET_ON_ERROR(TAG, nvs_flash_erase(), return err_rc_, "nvs_flash_erase failed");
        ret = nvs_flash_init();
    }
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "nvs_flash_init failed");

    ESP_GMF_RET_ON_ERROR(TAG, esp_config_storage_init_nvs(&s_wifi_profile_store, &s_profile_storage),
                         return err_rc_, "WiFi profile storage init failed");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = WIFI_PROFILE_MAX_NUM,
        .storage = s_profile_storage,
        .crypto = NULL,
        .crypto_extra_size = 0,
    };
    ret = esp_wifi_service_profile_mgr_init(&profile_cfg, &s_profile_mgr);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi profile manager init failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    esp_wifi_service_config_t service_cfg = {
        .name = "wifi_service",
        .profile_manager = s_profile_mgr,
        .prov_list = NULL,
        .prov_num = 0,
        .selector_policy = NULL,
    };
    ret = esp_wifi_service_create(&service_cfg, &s_wifi_service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi service create failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_wifi_service_request_connect(s_wifi_service, WIFI_SSID, WIFI_PASSWORD, 10, WIFI_CONNECT_WAIT_SEC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    ESP_LOGI(TAG, "WiFi connected");
    return ESP_OK;

cleanup:
    wifi_disconnect();
    return ret;
}

static esp_err_t init_littlefs(void)
{
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } else {
        ESP_LOGI(TAG, "Partition size: total: %zu, used: %zu", total, used);
    }
    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("CLI_MUSIC_CONTROL", ESP_LOG_INFO);
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);

    ESP_LOGI(TAG, "=== Play Music Control (multi-source) ===");

    bool wifi_connected = false;
    bool littlefs_mounted = false;
    esp_asp_handle_t simple_player = NULL;
    esp_cli_service_t *cli_service = NULL;

    ESP_LOGI(TAG, "[ 1 ] Setup peripheral for audio codec and SD card");
    if (setup_peripheral() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup peripheral");
        goto _cleanup;
    }

    ESP_LOGI(TAG, "[ 2 ] Mount LittleFS tone partition");
    if (init_littlefs() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LittleFS");
        goto _cleanup;
    }
    littlefs_mounted = true;

    ESP_LOGI(TAG, "[ 3 ] Initialize simple player");
    if (init_simple_player(&simple_player) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize simple player");
        goto _cleanup;
    }

    ESP_LOGI(TAG, "[ 4 ] Initialize CLI music player and register commands");
    if (esp_cli_music_player_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CLI music control");
        goto _cleanup;
    }
    esp_cli_music_player_set_simple_player(simple_player);

    esp_cli_music_player_add_source_urls(s_http_source_urls,
                                         sizeof(s_http_source_urls) / sizeof(s_http_source_urls[0]));
    esp_cli_music_player_add_source_urls(s_littlefs_tone_urls,
                                         sizeof(s_littlefs_tone_urls) / sizeof(s_littlefs_tone_urls[0]));

    ESP_LOGI(TAG, "[ 5 ] Connect to WiFi");
    if (wifi_connect() != ESP_OK) {
        goto _cleanup;
    }
    wifi_connected = true;

    esp_cli_service_config_t cli_cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cli_cfg.task_prio = CLI_SERVICE_TASK_PRIO;
    cli_cfg.base_cfg.name = "music_cli";
    cli_cfg.prompt = "gmf> ";
    cli_cfg.task_stack = CLI_SERVICE_TASK_STACK;
    esp_err_t ret = esp_cli_service_create(&cli_cfg, &cli_service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create CLI service: %s", esp_err_to_name(ret));
        goto _cleanup;
    }
    ret = esp_cli_music_player_register_commands(cli_service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register CLI commands: %s", esp_err_to_name(ret));
        goto _cleanup;
    }
    ret = esp_service_start((esp_service_t *)cli_service);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start CLI service: %s", esp_err_to_name(ret));
        goto _cleanup;
    }
    ESP_LOGI(TAG, "CLI ready (type 'help' for commands)");
    return;

_cleanup:
    if (wifi_connected) {
        wifi_disconnect();
    }
    if (cli_service) {
        esp_cli_service_destroy(cli_service);
        cli_service = NULL;
    }
    esp_cli_music_player_deinit();
    if (simple_player) {
        esp_audio_simple_player_destroy(simple_player);
        simple_player = NULL;
    }
    close_playback_codec();
    if (littlefs_mounted) {
        esp_vfs_littlefs_unregister("storage");
    }
    esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
}
