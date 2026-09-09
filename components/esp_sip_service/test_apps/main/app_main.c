/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "sdkconfig.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "media_lib_adapter.h"
#include "unity.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD  40960

extern void esp_sip_service_ut_force_link(void);
#if CONFIG_ESP_SIP_SERVICE_MCP_ENABLE
extern void esp_sip_service_mcp_ut_force_link(void);
#endif  /* CONFIG_ESP_SIP_SERVICE_MCP_ENABLE */

static esp_netif_t *s_ap_netif;

/**
 * @brief  Bring up a SoftAP so the loopback SIP sockets have a live netif
 */
static void init_softap(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        TEST_ESP_OK(nvs_flash_erase());
        TEST_ESP_OK(nvs_flash_init());
    } else {
        TEST_ESP_OK(ret);
    }

    ret = esp_netif_init();
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
    ret = esp_event_loop_create_default();
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        TEST_ASSERT_NOT_NULL(s_ap_netif);
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ESP_OK(esp_wifi_init(&init_cfg));
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "sip_ut",
            .ssid_len = 6,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    TEST_ESP_OK(esp_wifi_set_mode(WIFI_MODE_AP));
    TEST_ESP_OK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    TEST_ESP_OK(esp_wifi_start());
}

static void deinit_softap(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    esp_event_loop_delete_default();
    esp_netif_deinit();
    nvs_flash_deinit();
}

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    media_lib_add_default_adapter();
    esp_sip_service_ut_force_link();
#if CONFIG_ESP_SIP_SERVICE_MCP_ENABLE
    esp_sip_service_mcp_ut_force_link();
#endif  /* CONFIG_ESP_SIP_SERVICE_MCP_ENABLE */
    init_softap();

    printf("Running esp_sip_service unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_sip_service]", false);
    UNITY_END();
    deinit_softap();
    printf("ESP_SIP_SERVICE_UT_DONE\n");
    fflush(stdout);
}
