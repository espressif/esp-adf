/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 *
 * Test commands (CLI service UART REPL):
 *   wifi profile list
 *   wifi profile add <ssid> <password> [priority 0..20]
 *   wifi profile del <ssid>
 *   wifi profile del_idx <index>     (0 .. count-1)
 *   wifi profile clear               (remove all profiles)
 *   wifi connect <ssid> <password|- for open AP> [priority 0..20] [wait_sec]
 *   wifi prov start|stop
 *   wifi reeval
 *   reboot                       (restart device)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_cli_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"
#include "wifi_service_console.h"

static const char *TAG = "WIFI_SVC_CONSOLE";

static esp_wifi_service_profile_mgr_t s_profile;
static esp_wifi_service_t *s_service;
static esp_cli_service_t *s_cli;

static int cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("ok: rebooting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_restart();
    return 0;
}

static bool console_list_profile_cb(const esp_wifi_service_profile_t *p, void *ctx)
{
    (void)ctx;
    bool enabled = (p->flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0;
    printf("  ssid=\"%s\" pri=%u en=%d\n", p->ssid, (unsigned)p->priority, enabled ? 1 : 0);
    return true;
}

static int cmd_wifi_profile(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: wifi profile list | add ... | del ... | clear\n");
        return 1;
    }

    const char *sub = argv[2];

    if (strcmp(sub, "list") == 0) {
        char last_working_ssid[ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN + 1] = {0};
        bool has_last = (esp_wifi_service_profile_mgr_get_last_working(s_profile, last_working_ssid,
                                                                       sizeof(last_working_ssid))
                         == ESP_OK);
        printf("last_working=%s\n", has_last ? last_working_ssid : "(none)");
        esp_wifi_service_profile_mgr_foreach(s_profile, console_list_profile_cb, NULL);
        return 0;
    }

    if (strcmp(sub, "clear") == 0) {
        if (esp_wifi_service_profile_mgr_clear_all(s_profile) != ESP_OK) {
            printf("clear: failed\n");
            return 1;
        }
        printf("ok: all profiles removed\n");
        return 0;
    }

    if (strcmp(sub, "add") == 0) {
        if (argc < 5) {
            printf("Usage: wifi profile add <ssid> <password> [priority 0..20]\n");
            return 1;
        }
        const char *ssid = argv[3];
        const char *pass = argv[4];
        unsigned pri = 10;
        if (argc >= 6) {
            pri = (unsigned)strtoul(argv[5], NULL, 0);
            if (pri > ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX) {
                pri = ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX;
            }
        }
        if (strlen(ssid) > ESP_WIFI_SERVICE_PROFILE_SSID_MAX_LEN) {
            printf("ssid too long\n");
            return 1;
        }
        if (strlen(pass) > ESP_WIFI_SERVICE_PROFILE_PASS_MAX_LEN) {
            printf("password too long\n");
            return 1;
        }

        esp_wifi_service_profile_t profile = {
            .flags = ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED,
            .priority = (uint8_t)pri,
        };
        strlcpy(profile.ssid, ssid, sizeof(profile.ssid));
        strlcpy(profile.password, pass, sizeof(profile.password));
        if (esp_wifi_service_profile_mgr_add(s_profile, &profile) != ESP_OK) {
            printf("add: failed (table full or storage error)\n");
            return 1;
        }
        printf("ok: profile added or updated\n");
        return 0;
    }

    if (strcmp(sub, "del") == 0) {
        if (argc < 4) {
            printf("Usage: wifi profile del <ssid>\n");
            return 1;
        }
        if (esp_wifi_service_profile_mgr_delete(s_profile, argv[3]) != ESP_OK) {
            printf("ssid not found or delete failed\n");
            return 1;
        }
        printf("ok: removed \"%s\"\n", argv[3]);
        return 0;
    }

    printf("unknown subcommand \"%s\"\n", sub);
    return 1;
}

static int cmd_wifi_prov(int argc, char **argv)
{
    if (argc < 3) {
        printf("Usage: wifi prov start|stop\n");
        return 1;
    }

    if (strcmp(argv[2], "start") == 0) {
        esp_err_t ret = esp_wifi_service_start_provisioning(s_service);
        if (ret != ESP_OK) {
            printf("prov start failed: %s\n", esp_err_to_name(ret));
            return 1;
        }
        printf("ok: provisioning started\n");
        return 0;
    }

    if (strcmp(argv[2], "stop") == 0) {
        esp_err_t ret = esp_wifi_service_stop_provisioning(s_service);
        if (ret != ESP_OK) {
            printf("prov stop failed: %s\n", esp_err_to_name(ret));
            return 1;
        }
        printf("ok: provisioning stopped\n");
        return 0;
    }

    printf("Usage: wifi prov start|stop\n");
    return 1;
}

static int cmd_wifi_connect(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: wifi connect <ssid> <password|- for open AP> [priority 0..20] [wait_sec]\n");
        return 1;
    }

    char *ssid = argv[2];
    char *password = argv[3];
    char empty_password[] = "";
    if (strcmp(password, "-") == 0) {
        password = empty_password;
    }

    unsigned pri = 10;
    if (argc >= 5) {
        pri = (unsigned)strtoul(argv[4], NULL, 0);
        if (pri > ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX) {
            pri = ESP_WIFI_SERVICE_PROFILE_PRIORITY_MAX;
        }
    }

    uint32_t wait_sec = 30;
    if (argc >= 6) {
        wait_sec = (uint32_t)strtoul(argv[5], NULL, 0);
    }

    esp_err_t ret = esp_wifi_service_request_connect(s_service, ssid, password, (uint8_t)pri, wait_sec);
    if (ret != ESP_OK) {
        printf("connect request failed: %s\n", esp_err_to_name(ret));
        return 1;
    }

    printf("ok: connect requested for \"%s\" priority=%u wait_sec=%u\n", ssid, pri, (unsigned)wait_sec);
    return 0;
}

static int cmd_wifi(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: wifi profile ... | connect ... | prov start|stop | reeval\n");
        return 1;
    }

    if (strcmp(argv[1], "profile") == 0) {
        return cmd_wifi_profile(argc, argv);
    }
    if (strcmp(argv[1], "connect") == 0) {
        return cmd_wifi_connect(argc, argv);
    }
    if (strcmp(argv[1], "prov") == 0) {
        return cmd_wifi_prov(argc, argv);
    }
    if (strcmp(argv[1], "reeval") == 0) {
        esp_err_t ret = esp_wifi_service_request_reeval(s_service);
        if (ret != ESP_OK) {
            printf("reeval failed: %s\n", esp_err_to_name(ret));
            return 1;
        }
        printf("ok: selector re-evaluation requested\n");
        return 0;
    }

    printf("unknown group \"%s\"\n", argv[1]);
    printf("Usage: wifi profile ... | connect ... | prov start|stop | reeval\n");
    return 1;
}

void wifi_service_console_start(esp_wifi_service_profile_mgr_t profile, esp_wifi_service_t *service)
{
    s_profile = profile;
    s_service = service;

    esp_cli_service_config_t cli_cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cli_cfg.base_cfg.name = "wifi_service_cli";
    cli_cfg.prompt = "wifi> ";
    cli_cfg.task_stack = 5120;
    ESP_ERROR_CHECK(esp_cli_service_create(&cli_cfg, &s_cli));

    const esp_console_cmd_t cmd = {
        .command = "wifi",
        .help = "profile [list|add|del|del_idx|clear] | connect <ssid> <password|- for open AP> "
                "[priority] [wait_sec] | prov [start|stop] | reeval",
        .hint = NULL,
        .func = &cmd_wifi};
    ESP_ERROR_CHECK(esp_cli_service_register_static_command(s_cli, &cmd));

    const esp_console_cmd_t reboot_cmd = {
        .command = "reboot",
        .help = "restart device",
        .hint = NULL,
        .func = &cmd_reboot};
    ESP_ERROR_CHECK(esp_cli_service_register_static_command(s_cli, &reboot_cmd));

    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)s_cli));
    ESP_LOGI(TAG, "CLI service: type 'help', 'wifi ...' or 'reboot'");
}
