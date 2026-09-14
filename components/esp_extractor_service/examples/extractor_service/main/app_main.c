/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_config_storage.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_extractor_defaults.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_service.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_profile_mgr.h"
#include "media_lib_adapter.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#if CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
#include "esp_hls_extractor.h"
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */

#include "extractor_demo.h"
#include "extractor_mcp.h"
#include "settings.h"

static const char *TAG = "EXTRACTOR_EX";

static esp_config_storage_t s_wifi_store;
static esp_wifi_service_profile_mgr_t s_wifi_profiles;
static esp_wifi_service_t *s_wifi_service;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_sdcard(void)
{
    esp_err_t ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card mount failed (%s); file:// cases need %s",
                 esp_err_to_name(ret), EXTRACTOR_URL_SD_MP4);
        return ret;
    }
    ESP_LOGI(TAG, "SD card ready at %s", EXTRACTOR_SD_MOUNT);
    return ESP_OK;
}

static esp_err_t init_wifi(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");

    static esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "ext_wifi",
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
    ESP_RETURN_ON_ERROR(esp_wifi_service_profile_mgr_init(&profile_cfg, &s_wifi_profiles),
                        TAG, "profile mgr");

    esp_wifi_service_config_t wifi_cfg = {
        .name = "extractor_wifi",
        .profile_manager = s_wifi_profiles,
        .prov_list = NULL,
        .prov_num = 0,
        .selector_policy = NULL,
    };
    ESP_RETURN_ON_ERROR(esp_wifi_service_create(&wifi_cfg, &s_wifi_service), TAG, "wifi create");
    ESP_RETURN_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(s_wifi_service)), TAG, "wifi start");

    ESP_LOGI(TAG, "Connecting WiFi ssid:%s", EXTRACTOR_WIFI_SSID);
    esp_err_t ret = esp_wifi_service_request_connect(s_wifi_service,
                                                     (char *)EXTRACTOR_WIFI_SSID,
                                                     (char *)EXTRACTOR_WIFI_PASSWORD,
                                                     10,
                                                     EXTRACTOR_WIFI_WAIT_SEC);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi connect failed (%s); HTTP/HLS cases may fail", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi connected");
    }
    return ret;
}

static const char *lookup_example_url(const char *name)
{
    for (size_t i = 0; i < EXTRACTOR_EXAMPLE_URL_COUNT; i++) {
        if (strcmp(EXTRACTOR_EXAMPLE_URLS[i].name, name) == 0) {
            return EXTRACTOR_EXAMPLE_URLS[i].url;
        }
    }
    return NULL;
}

static void print_example_urls(void)
{
    printf("Named URLs:\n");
    for (size_t i = 0; i < EXTRACTOR_EXAMPLE_URL_COUNT; i++) {
        printf("  %-10s %s\n", EXTRACTOR_EXAMPLE_URLS[i].name, EXTRACTOR_EXAMPLE_URLS[i].url);
    }
}

static int simple_command(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: simple <provider|link> [name|url] [duration_ms]\n");
        print_example_urls();
        return 1;
    }
    extractor_demo_mode_t mode;
    if (strcmp(argv[1], "provider") == 0) {
        mode = EXTRACTOR_DEMO_MODE_MANUAL_PROVIDER;
    } else if (strcmp(argv[1], "link") == 0) {
        mode = EXTRACTOR_DEMO_MODE_LINK_DUMMY_SINK;
    } else {
        printf("Unknown mode '%s'\n", argv[1]);
        return 1;
    }

    uint32_t duration_ms = EXTRACTOR_TEST_DURATION_MS;
    const char *url = EXTRACTOR_EXAMPLE_URLS[0].url;
    if (argc >= 3) {
        char *end = NULL;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (end != argv[2] && *end == '\0') {
            if (value > 0) {
                duration_ms = (uint32_t)value;
            }
        } else {
            const char *named = lookup_example_url(argv[2]);
            url = named ? named : argv[2];
            if (argc >= 4) {
                duration_ms = (uint32_t)strtoul(argv[3], NULL, 10);
                if (duration_ms == 0) {
                    duration_ms = EXTRACTOR_TEST_DURATION_MS;
                }
            }
        }
    }
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "stress") == 0) {
            for (int i = 0; i < 500; i++) {
                printf("---------Stress for %s %d--------\n", url, i);
                duration_ms = 15000;
                extractor_demo_run(url, duration_ms, mode);
            }
            return 0;
        }
    }

    esp_err_t ret = extractor_demo_run(url, duration_ms, mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "simple %s failed: %s", argv[1], esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int repeat_command(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: repeat <provider|link> [name|url] [repeat_count] [duration_ms]\n");
        print_example_urls();
        return 1;
    }
    extractor_demo_mode_t mode;
    if (strcmp(argv[1], "provider") == 0) {
        mode = EXTRACTOR_DEMO_MODE_MANUAL_PROVIDER;
    } else if (strcmp(argv[1], "link") == 0) {
        mode = EXTRACTOR_DEMO_MODE_LINK_DUMMY_SINK;
    } else {
        printf("Unknown mode '%s'\n", argv[1]);
        return 1;
    }

    uint32_t duration_ms = EXTRACTOR_TEST_DURATION_MS;
    uint32_t repeat_count = EXTRACTOR_REPEAT_COUNT;
    const char *url = EXTRACTOR_EXAMPLE_URLS[0].url;
    if (argc >= 3) {
        char *end = NULL;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (end != argv[2] && *end == '\0') {
            if (value > 0) {
                repeat_count = (uint32_t)value;
            }
            if (argc >= 4) {
                duration_ms = (uint32_t)strtoul(argv[3], NULL, 10);
                if (duration_ms == 0) {
                    duration_ms = EXTRACTOR_TEST_DURATION_MS;
                }
            }
        } else {
            const char *named = lookup_example_url(argv[2]);
            url = named ? named : argv[2];
            if (argc >= 4) {
                repeat_count = (uint32_t)strtoul(argv[3], NULL, 10);
                if (repeat_count == 0) {
                    repeat_count = EXTRACTOR_REPEAT_COUNT;
                }
            }
            if (argc >= 5) {
                duration_ms = (uint32_t)strtoul(argv[4], NULL, 10);
                if (duration_ms == 0) {
                    duration_ms = EXTRACTOR_TEST_DURATION_MS;
                }
            }
        }
    }

    esp_err_t ret = extractor_demo_run_repeat(url, duration_ms, mode, repeat_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "repeat %s failed: %s", argv[1], esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static esp_err_t start_console(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "extractor-cli";
    cfg.prompt = "extractor>";

    esp_cli_service_t *cli = NULL;
    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &cli), TAG, "create CLI");
    const esp_console_cmd_t commands[] = {
        {
            .command = "simple",
            .help = "Run extractor: simple <provider|link> [name|url] [duration_ms] (provider seeks after 5s)",
            .func = simple_command,
        },
        {
            .command = "repeat",
            .help = "Repeat extractor: repeat <provider|link> [name|url] [repeat_count] [duration_ms]",
            .func = repeat_command,
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
    esp_extractor_register_default();
#if CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT
    esp_hls_extractor_register();
#endif  /* CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT */

    (void)init_sdcard();
    (void)init_wifi();
    ESP_ERROR_CHECK(extractor_mcp_start());
    ESP_ERROR_CHECK(start_console());

    ESP_LOGI(TAG, "Extractor service example is ready");
    ESP_LOGI(TAG, "EXTRACTOR_SERVICE_EXAMPLE_READY");
    ESP_LOGI(TAG, "Type 'simple provider sd_mp4' (reads 5s, seeks, continues) or 'repeat provider sd_mp4 3'");
}
