/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_muxer_default.h"
#include "esp_service.h"
#include "media_lib_adapter.h"
#include "esp_board_manager_includes.h"
#include "muxer_mcp.h"
#include "muxer_scheduler.h"
#include "settings.h"
#include "simple_muxer.h"

static const char *TAG = "MUXER_EX";

static bool parse_muxer_mode(const char *text, esp_muxer_service_mode_t *out_mode)
{
    if (text == NULL || out_mode == NULL) {
        return false;
    }
    if (strcmp(text, "storage") == 0 || strcmp(text, "storage_only") == 0) {
        *out_mode = ESP_MUXER_SERVICE_MODE_STORAGE_ONLY;
        return true;
    }
    if (strcmp(text, "streaming") == 0 || strcmp(text, "streaming_only") == 0) {
        *out_mode = ESP_MUXER_SERVICE_MODE_STREAMING_ONLY;
        return true;
    }
    if (strcmp(text, "both") == 0) {
        *out_mode = ESP_MUXER_SERVICE_MODE_BOTH;
        return true;
    }
    return false;
}

static int simple_command(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: simple <audio|av> [duration_ms] [streaming|storage|both]\n");
        return 1;
    }
    uint32_t duration_ms = MUXER_EXAMPLE_DURATION_MS;
    esp_muxer_service_mode_t mode = ESP_MUXER_SERVICE_MODE_STREAMING_ONLY;
    for (int i = 2; i < argc; i++) {
        esp_muxer_service_mode_t parsed_mode = ESP_MUXER_SERVICE_MODE_STORAGE_ONLY;
        if (parse_muxer_mode(argv[i], &parsed_mode)) {
            mode = parsed_mode;
            continue;
        }
        char *end = NULL;
        unsigned long value = strtoul(argv[i], &end, 10);
        if (end != argv[i] && end != NULL && *end == '\0' && value != 0) {
            duration_ms = (uint32_t)value;
            continue;
        }
        printf("Unknown simple argument '%s'\n", argv[i]);
        return 1;
    }
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (strcmp(argv[1], "audio") == 0) {
        ret = simple_muxer_audio(duration_ms, mode);
    } else if (strcmp(argv[1], "av") == 0) {
        ret = simple_muxer_av(duration_ms, mode);
    } else {
        printf("Unknown simple case '%s'\n", argv[1]);
        return 1;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "simple %s failed: %s", argv[1], esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static esp_err_t start_console(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "muxer-cli";
    cfg.prompt = "muxer>";

    esp_cli_service_t *cli = NULL;
    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &cli), TAG, "create CLI");
    const esp_console_cmd_t commands[] = {
        {
            .command = "simple",
            .help = "Run a copy-ready muxer walkthrough: simple <audio|av> [duration_ms] [streaming|storage|both]",
            .func = simple_command,
        },
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_cli_service_register_static_command(cli, &commands[i]),
                            TAG, "register command");
    }
    return esp_service_start((esp_service_t *)cli);
}

static esp_err_t init_sdcard(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed (%s);", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SD card ready");
    return ESP_OK;
}

void app_main(void)
{
    media_lib_add_default_adapter();
    ESP_ERROR_CHECK(esp_muxer_register_default());
    ESP_ERROR_CHECK(muxer_scheduler_install());
    ESP_ERROR_CHECK(muxer_mcp_start());
    (void)init_sdcard();
    ESP_ERROR_CHECK(start_console());

    ESP_LOGI(TAG, "Muxer service example is ready");
    ESP_LOGI(TAG, "MUXER_SERVICE_EXAMPLE_READY");
    ESP_LOGI(TAG, "Type 'simple audio 5000 streaming' or 'simple av 5000 both'");
}
