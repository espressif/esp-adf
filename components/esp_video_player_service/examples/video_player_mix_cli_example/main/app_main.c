/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"

#include "esp_audio_dec_default.h"
#include "esp_cli_service.h"
#include "esp_extractor_defaults.h"
#include "esp_service.h"
#include "esp_video_dec_default.h"

#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

#include "mix_cli.h"
#include "mix_sources.h"
#include "video_player_board.h"

static const char *TAG = "VPM_CLI_EX";

static esp_err_t create_player_service(esp_player_service_t **out_service)
{
    esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.max_stream_num = 2;

    esp_player_service_t *service = NULL;
    ESP_RETURN_ON_ERROR(esp_video_player_service_create(&cfg, &service), TAG, "Create failed");

    esp_video_player_service_setup_t setup_cfg = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup_cfg.display_dev_name = video_player_board_lcd_name();
    esp_err_t ret = esp_video_player_service_apply_setup(service, &setup_cfg);
    if (ret != ESP_OK) {
        esp_player_service_destroy(service);
        return ret;
    }

    *out_service = service;
    return ESP_OK;
}

void app_main(void)
{
    esp_log_level_set("ESP_GMF_TASK", ESP_LOG_WARN);
    esp_log_level_set("ESP_GMF_PORT", ESP_LOG_WARN);

    ESP_LOGI(TAG, "Start 'video_player_mix_cli_example'");

    ESP_LOGI(TAG, "[ 1 ] Initialize board (LCD + DAC + SD card)");
    video_player_board_init();
    if (video_player_board_lcd_name() == NULL) {
        ESP_LOGW(TAG, "No display came up; movie audio still mixes with TTS");
    }
    ESP_ERROR_CHECK(esp_extractor_register_default());
    ESP_ERROR_CHECK(esp_audio_dec_register_default());
    ESP_ERROR_CHECK(esp_video_dec_register_default());

    ESP_LOGI(TAG, "[ 2 ] Create player service (stream 0 movie/ES/link, stream 1 TTS)");
    esp_player_service_t *player = NULL;
    ESP_ERROR_CHECK(create_player_service(&player));
    ESP_ERROR_CHECK(mix_sources_init(player));

    ESP_LOGI(TAG, "[ 3 ] Start player service");
    ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(player)));

    ESP_LOGI(TAG, "[ 4 ] Start CLI (prompt vpm>)");
    esp_cli_service_t *cli = NULL;
    esp_cli_service_config_t cli_cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cli_cfg.base_cfg.name = "VPM_CLI";
    cli_cfg.prompt = "vpm> ";
    cli_cfg.task_stack = 8192;
    ESP_ERROR_CHECK(esp_cli_service_create(&cli_cfg, &cli));
    ESP_ERROR_CHECK(mix_cli_register_commands(cli));
    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)cli));

    ESP_LOGI(TAG, "CLI ready");
    int cmd_ret = 0;
    (void)esp_console_run("help", &cmd_ret);
}
