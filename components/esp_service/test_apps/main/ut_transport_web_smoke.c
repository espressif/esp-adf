/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/* On-device MCP smoke tests for the web transports. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "unity.h"
#include "player_service.h"
#include "led_service.h"
#include "led_service_mcp.h"
#include "player_service_mcp.h"
#include "ut_transport_smoke_helpers.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT
#include "esp_service_mcp_trans_http.h"
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT */
#ifdef CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT
#include "esp_service_mcp_trans_sse.h"
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT */
#ifdef CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT
#include "esp_service_mcp_trans_ws.h"
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT */

#define WIFI_STA_SSID      CONFIG_ESP_SVC_TEST_WIFI_SSID
#define WIFI_STA_PASSWORD  CONFIG_ESP_SVC_TEST_WIFI_PASSWORD

static const char *TAG = "WEB_TRANSPORT_SMOKE";

static volatile bool s_sta_have_ip;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected (reason %d), retrying...", ((wifi_event_sta_disconnected_t *)event_data)->reason);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_sta_have_ip = true;
    }
}

static esp_err_t wifi_init_sta(void)
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, &instance_any_id);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, &instance_got_ip);
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    s_sta_have_ip = false;
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "WiFi initialized, connecting to SSID: %s", WIFI_STA_SSID);
    return ESP_OK;
}

static void ut_transport_wait_sta_ip(volatile bool *got_ip, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while (!*got_ip && (xTaskGetTickCount() - start) < timeout_ticks) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    TEST_ASSERT_TRUE_MESSAGE(*got_ip, "STA did not get IP within timeout");
}

static void web_smoke_setup(esp_service_manager_t **out_mgr, player_service_t **out_player, led_service_t **out_led)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_EQUAL(ESP_OK, wifi_init_sta());
    ut_transport_wait_sta_ip(&s_sta_have_ip, pdMS_TO_TICKS(45000));

    esp_service_manager_config_t mgr_cfg = ESP_SERVICE_MANAGER_CONFIG_DEFAULT();
    mgr_cfg.auto_start_services = true;

    esp_service_manager_t *mgr = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_create(&mgr_cfg, &mgr));

    player_service_t *player = NULL;
    player_service_cfg_t player_cfg = {
        .uri = "http://example.com/audio.mp3",
        .volume = 50,
        .sim_actions = 0,
    };
    TEST_ASSERT_EQUAL(ESP_OK, player_service_create(&player_cfg, &player));
    esp_service_registration_t preg = {
        .service = (esp_service_t *)player,
        .category = "audio",
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &preg));

    led_service_t *led = NULL;
    led_service_cfg_t led_cfg = {.gpio_num = 2, .blink_period = 500};
    TEST_ASSERT_EQUAL(ESP_OK, led_service_create(&led_cfg, &led));
    esp_service_registration_t lreg = {
        .service = (esp_service_t *)led,
        .category = "display",
        .tool_desc = led_service_mcp_schema(),
        .tool_invoke = led_service_tool_invoke,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_manager_register(mgr, &lreg));

    *out_mgr = mgr;
    *out_player = player;
    *out_led = led;
}

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT
static void http_transport_smoke_body(void)
{
    esp_service_manager_t *mgr = NULL;
    player_service_t *player = NULL;
    led_service_t *led = NULL;
    web_smoke_setup(&mgr, &player, &led);

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_http_config_t http_cfg = ESP_SERVICE_MCP_HTTP_CONFIG_DEFAULT();
    http_cfg.port = 8080;
    http_cfg.uri_path = "/mcp";
    http_cfg.enable_cors = true;
    if (CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN[0] != '\0') {
        http_cfg.auth_token = CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN;
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_http_create(&http_cfg, &transport));

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = transport;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcp_cfg, &mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_start(mcp));

    ut_transport_assert_mcp_tools_list(mcp);

#if CONFIG_ESP_SVC_TEST_HTTP_HOLD_AFTER_SMOKE
    ESP_LOGI(TAG, "HTTP MCP kept running (ESP_SVC_TEST_HTTP_HOLD_AFTER_SMOKE). On PC run:");
    ESP_LOGI(TAG, "  (from test_apps/) python3 scripts/test_mcp.py http <device-ip>");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
#else
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_stop(mcp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_destroy(mcp));
    mcp = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_destroy(transport));
    transport = NULL;

    esp_service_manager_destroy(mgr);
    mgr = NULL;

    led_service_destroy(led);
    player_service_destroy(player);
#endif  /* CONFIG_ESP_SVC_TEST_HTTP_HOLD_AFTER_SMOKE */
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT
#if defined(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS) && defined(CONFIG_ESP_MCP_TRANSPORT_SSE)
int test_sse_lifecycle_stress(esp_service_manager_t *mgr);
#endif  /* defined(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS) && defined(CONFIG_ESP_MCP_TRANSPORT_SSE) */

static void sse_transport_smoke_body(void)
{
    esp_service_manager_t *mgr = NULL;
    player_service_t *player = NULL;
    led_service_t *led = NULL;
    web_smoke_setup(&mgr, &player, &led);

#if defined(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS) && defined(CONFIG_ESP_MCP_TRANSPORT_SSE)
    TEST_ASSERT_EQUAL(0, test_sse_lifecycle_stress(mgr));
#endif  /* defined(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS) && defined(CONFIG_ESP_MCP_TRANSPORT_SSE) */

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_sse_config_t sse_cfg = ESP_SERVICE_MCP_SSE_CONFIG_DEFAULT();
    if (CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN[0] != '\0') {
        sse_cfg.auth_token = CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN;
    }
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_sse_create(&sse_cfg, &transport));

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = transport;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcp_cfg, &mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_start(mcp));

    ut_transport_assert_mcp_tools_list(mcp);

    /* No SSE clients connected in smoke run — broadcast may be NOT_SUPPORTED. */
    esp_err_t ret = esp_service_mcp_server_notify(mcp, "notifications/tools/list_changed", NULL);
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED);

#if CONFIG_ESP_SVC_TEST_SSE_HOLD_AFTER_SMOKE
    ESP_LOGI(TAG, "SSE MCP kept running (ESP_SVC_TEST_SSE_HOLD_AFTER_SMOKE). On PC run:");
    ESP_LOGI(TAG, "  (from test_apps/) python3 scripts/test_mcp.py sse <device-ip> %d",
             (int)sse_cfg.port);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
#else
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_stop(mcp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_destroy(mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_destroy(transport));

    esp_service_manager_destroy(mgr);

    led_service_destroy(led);
    player_service_destroy(player);
#endif  /* CONFIG_ESP_SVC_TEST_SSE_HOLD_AFTER_SMOKE */
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT
static void ws_transport_smoke_body(void)
{
    esp_service_manager_t *mgr = NULL;
    player_service_t *player = NULL;
    led_service_t *led = NULL;
    web_smoke_setup(&mgr, &player, &led);

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_ws_config_t ws_cfg = ESP_SERVICE_MCP_WS_CONFIG_DEFAULT();
    ws_cfg.port = 8081;
    ws_cfg.uri_path = "/mcp";
    /* Same-origin only: a PC websockets client sends no Origin header and is
     * accepted; a browser page from another host gets 403 */
    ws_cfg.enable_cors = false;
    ws_cfg.max_clients = 4;
    if (CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN[0] != '\0') {
        ws_cfg.auth_token = CONFIG_ESP_SVC_TEST_MCP_AUTH_TOKEN;
    }

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_ws_create(&ws_cfg, &transport));

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = transport;

    esp_service_mcp_server_t *mcp = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_create(&mcp_cfg, &mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_start(mcp));

    ut_transport_assert_mcp_tools_list(mcp);

#if CONFIG_ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE
    ESP_LOGI(TAG, "WS MCP kept running (ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE). On PC run:");
    ESP_LOGI(TAG, "  (from test_apps/) python3 scripts/test_mcp.py ws <device-ip> 8081");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_err_t ret = esp_service_mcp_server_notify(mcp, "notifications/tools/list_changed", NULL);
        ESP_LOGI(TAG, "Broadcast tools/list_changed: %s", esp_err_to_name(ret));
    }
#else
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_stop(mcp));
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_server_destroy(mcp));

    TEST_ASSERT_EQUAL(ESP_OK, esp_service_mcp_trans_destroy(transport));

    esp_service_manager_destroy(mgr);

    led_service_destroy(led);
    player_service_destroy(player);
#endif  /* CONFIG_ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE */
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT
void test_http_transport(void)
{
    ESP_LOGI(TAG, "HTTP MCP transport smoke (Unity)");
    UNITY_BEGIN();
    RUN_TEST(http_transport_smoke_body);
    UNITY_END();
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT
void test_sse_transport(void)
{
    ESP_LOGI(TAG, "HTTP+SSE MCP transport smoke (Unity)");
    UNITY_BEGIN();
    RUN_TEST(sse_transport_smoke_body);
    UNITY_END();
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT */

#ifdef CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT
void test_ws_transport(void)
{
    ESP_LOGI(TAG, "WebSocket MCP transport smoke (Unity)");
    UNITY_BEGIN();
    RUN_TEST(ws_transport_smoke_body);
    UNITY_END();
}
#endif  /* CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT */
