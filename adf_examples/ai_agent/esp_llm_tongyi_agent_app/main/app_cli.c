/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

#include "esp_cli_service.h"
#include "esp_log.h"

#include "app_cli.h"

static const char *TAG = "APP_CLI";

static esp_cli_service_t   *s_cli;
static tongyi_app_media_t  *s_media;
static esp_llm_service_t   *s_llm;

#if CONFIG_TONGYI_APP_MODE_VISION
static char *join_argv(int argc, char **argv, int start)
{
    if (argc <= start || argv == NULL) {
        return NULL;
    }
    size_t len = 0;
    for (int i = start; i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        len += strlen(argv[i]) + 1;
    }
    if (len == 0) {
        return NULL;
    }
    char *out = malloc(len);
    if (out == NULL) {
        return NULL;
    }
    out[0] = '\0';
    bool first = true;
    for (int i = start; i < argc; i++) {
        if (argv[i] == NULL || argv[i][0] == '\0') {
            continue;
        }
        if (!first) {
            strcat(out, " ");
        }
        strcat(out, argv[i]);
        first = false;
    }
    if (out[0] == '\0') {
        free(out);
        return NULL;
    }
    return out;
}

static int cmd_snap(int argc, char **argv)
{
    if (s_media == NULL || s_llm == NULL) {
        ESP_LOGE(TAG, "Snap failed: agent is not ready");
        return 1;
    }

    char *joined = join_argv(argc, argv, 1);
    const char *prompt = (joined != NULL) ? joined :
                         (CONFIG_TONGYI_VISION_PROMPT[0] != '\0' ? CONFIG_TONGYI_VISION_PROMPT : NULL);

    uint8_t *jpeg = NULL;
    size_t jpeg_size = 0;
    esp_err_t ret = tongyi_app_media_capture_jpeg(s_media, &jpeg, &jpeg_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Snap failed: capture JPEG returned %s", esp_err_to_name(ret));
        free(joined);
        return 1;
    }

    esp_media_frame_t frame = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .data = jpeg,
        .size = jpeg_size,
        .user_data = (void *)prompt,
    };
    ret = esp_llm_service_send_frame(s_llm, &frame);
    free(jpeg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Snap failed: send frame returned %s", esp_err_to_name(ret));
        free(joined);
        return 1;
    }
    ESP_LOGI(TAG, "Snap sent %u JPEG bytes%s%s", (unsigned)jpeg_size,
             prompt != NULL ? " prompt=" : "", prompt != NULL ? prompt : "");
    free(joined);
    return 0;
}
#endif  /* CONFIG_TONGYI_APP_MODE_VISION */

esp_err_t app_cli_start(tongyi_app_media_t *media, esp_llm_service_t *llm)
{
    if (s_cli != NULL) {
        ESP_LOGE(TAG, "Start failed: CLI is already running");
        return ESP_ERR_INVALID_STATE;
    }

    s_media = media;
    s_llm = llm;

    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "tongyi-cli";
    cfg.prompt = "tongyi>";
    cfg.task_stack = 8192;
    cfg.max_cmdline_length = 512;

    esp_err_t ret = esp_cli_service_create(&cfg, &s_cli);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create CLI failed: %s", esp_err_to_name(ret));
        s_media = NULL;
        s_llm = NULL;
        return ret;
    }

#if CONFIG_TONGYI_APP_MODE_VISION
    const esp_console_cmd_t snap_cmd = {
        .command = "snap",
        .help = "Capture one JPEG and send it to Tongyi: snap [prompt...]",
        .hint = "[prompt]",
        .func = cmd_snap,
    };
    ret = esp_cli_service_register_static_command(s_cli, &snap_cmd);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register snap failed: %s", esp_err_to_name(ret));
        app_cli_stop();
        return ret;
    }
#endif

    ret = esp_service_start(ESP_SERVICE_BASE(s_cli));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start CLI failed: %s", esp_err_to_name(ret));
        app_cli_stop();
        return ret;
    }

#if CONFIG_TONGYI_APP_MODE_VISION
    ESP_LOGI(TAG, "CLI ready: type 'snap' or 'snap <prompt>'");
#else
    ESP_LOGI(TAG, "CLI ready: sys_heap / sys_chip / help");
#endif
    return ESP_OK;
}

void app_cli_stop(void)
{
    if (s_cli == NULL) {
        return;
    }
    (void)esp_cli_service_destroy(s_cli);
    s_cli = NULL;
    s_media = NULL;
    s_llm = NULL;
}
