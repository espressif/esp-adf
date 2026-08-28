/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_config_storage.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_service.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"
#include "media_lib_adapter.h"
#include "nvs_flash.h"

#include "rtmp_mcp.h"
#include "rtmp_scheduler.h"
#include "settings.h"
#include "simple_rtmp.h"

static const char *TAG = "RTMP_EX";

static esp_config_storage_t s_wifi_store;
static esp_wifi_service_profile_mgr_t s_wifi_profiles;
static esp_wifi_service_t *s_wifi_service;
static volatile bool s_simple_busy;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_wifi(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");

    static esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "rtmp_wifi",
        .key_primary = "prof_p",
        .key_backup = "prof_b",
    };
    ESP_RETURN_ON_ERROR(esp_config_storage_init_nvs(&nvs_cfg, &s_wifi_store), TAG, "wifi store");

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = 4,
        .storage = s_wifi_store,
        .crypto = NULL,
        .crypto_extra_size = 0,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_profile_mgr_init(&profile_cfg, &s_wifi_profiles), TAG, "profile mgr");

    esp_wifi_service_config_t wifi_cfg = {
        .name = "rtmp_wifi",
        .profile_manager = s_wifi_profiles,
        .prov_list = NULL,
        .prov_num = 0,
        .selector_policy = NULL,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_create(&wifi_cfg, &s_wifi_service), TAG, "wifi create");
    ESP_RETURN_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(s_wifi_service)), TAG, "wifi start");

    ESP_LOGI(TAG, "Connecting WiFi ssid:%s", RTMP_WIFI_SSID);
    esp_err_t ret = esp_wifi_service_request_connect(s_wifi_service,
                                                     (char *)RTMP_WIFI_SSID,
                                                     (char *)RTMP_WIFI_PASSWORD,
                                                     10,
                                                     RTMP_WIFI_WAIT_SEC);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi connect failed (%s); remote push/pull/server access may fail",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi connected");
    }
    return ESP_OK;
}

static void parse_simple_args(int argc, char **argv, uint32_t default_ms,
                              uint32_t *duration_ms, const char **url)
{
    *duration_ms = default_ms;
    *url = NULL;
    if (argc < 3) {
        return;
    }
    char *end = NULL;
    unsigned long value = strtoul(argv[2], &end, 10);
    if (end != argv[2] && *end == '\0') {
        if (value > 0) {
            *duration_ms = (uint32_t)value;
        }
        if (argc >= 4 && argv[3][0] != '\0') {
            *url = argv[3];
        }
        return;
    }
    if (argv[2][0] != '\0') {
        *url = argv[2];
    }
}

typedef struct {
    char case_name[16];
    char url[160];
    uint32_t duration_ms;
    bool has_url;
} simple_job_t;

static void simple_task(void *arg)
{
    simple_job_t *job = (simple_job_t *)arg;
    const char *url = job->has_url ? job->url : NULL;
    esp_err_t ret = ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "async simple %s start duration_ms=%" PRIu32 " url=%s",
             job->case_name, job->duration_ms, url ? url : "(default)");

    if (strcmp(job->case_name, "pusher") == 0) {
        ret = simple_rtmp_pusher(job->duration_ms, url);
    } else if (strcmp(job->case_name, "puller") == 0) {
        ret = simple_rtmp_puller(job->duration_ms, url);
    } else if (strcmp(job->case_name, "server") == 0) {
        ret = simple_rtmp_server(job->duration_ms, url);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "simple %s failed: %s", job->case_name, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "async simple %s done", job->case_name);
    }

    s_simple_busy = false;
    free(job);
    vTaskDelete(NULL);
}

static int simple_command(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: simple <pusher|puller|server> [duration_ms] [url]\n");
        return 1;
    }
    if (strcmp(argv[1], "pusher") != 0 && strcmp(argv[1], "puller") != 0 &&
        strcmp(argv[1], "server") != 0) {
        printf("Unknown simple case '%s'\n", argv[1]);
        return 1;
    }
    if (s_simple_busy) {
        printf("simple already running; wait for it to finish\n");
        return 1;
    }

    uint32_t duration_ms = RTMP_DURATION_MS;
    const char *url = NULL;
    parse_simple_args(argc, argv, RTMP_DURATION_MS, &duration_ms, &url);

    simple_job_t *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        printf("no mem for simple job\n");
        return 1;
    }
    snprintf(job->case_name, sizeof(job->case_name), "%s", argv[1]);
    job->duration_ms = duration_ms;
    if (url != NULL && url[0] != '\0') {
        snprintf(job->url, sizeof(job->url), "%s", url);
        job->has_url = true;
    }

    s_simple_busy = true;
    BaseType_t ok = xTaskCreate(simple_task, "rtmp_simple", 8192, job, 5, NULL);
    if (ok != pdPASS) {
        s_simple_busy = false;
        free(job);
        printf("failed to start simple task\n");
        return 1;
    }
    printf("simple %s started in background\n", argv[1]);
    return 0;
}

static int assert_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Triggering intentional fault (*(int *)0 = 0) for hang/assert debug\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(50));
    *(volatile int *)0 = 0;
    return 0;
}

static esp_err_t start_console(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "rtmp-cli";
    cfg.prompt = "rtmp>";

    esp_cli_service_t *cli = NULL;
    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &cli), TAG, "create CLI");
    const esp_console_cmd_t commands[] = {
        {
            .command = "simple",
            .help = "Async RTMP walkthrough: simple <pusher|puller|server> [duration_ms] [url]",
            .func = simple_command,
        },
        {
            .command = "assert",
            .help = "Force LoadProhibited fault (*(int*)0=0) to probe hang / watchdog behavior",
            .func = assert_command,
        },
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_cli_service_register_static_command(cli, &commands[i]),
                            TAG, "register command");
    }
    return esp_service_start((esp_service_t *)cli);
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_nvs());
    media_lib_add_default_adapter();
    (void)init_wifi();
    ESP_ERROR_CHECK(rtmp_scheduler_install());
    ESP_ERROR_CHECK(rtmp_mcp_start());
    ESP_ERROR_CHECK(start_console());
    ESP_LOGI(TAG, "RTMP service example is ready");
    ESP_LOGI(TAG, "RTMP_SERVICE_EXAMPLE_READY");
    ESP_LOGI(TAG, "Type 'simple ...' or 'assert'");
}
