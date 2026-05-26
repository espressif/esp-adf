/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include "unity.h"

#include "esp_netif.h"
#include "esp_service.h"
#include "esp_wifi.h"
#include "esp_wifi_service.h"
#include "esp_wifi_service_prov.h"
#include "test_wifi_service_storage.h"

#define TEST_WIFI_SSID         CONFIG_WIFI_SERVICE_TEST_PRIMARY_SSID
#define TEST_WIFI_SECOND_SSID  CONFIG_WIFI_SERVICE_TEST_SECONDARY_SSID
#define TEST_WIFI_PASSWORD     CONFIG_WIFI_SERVICE_TEST_PRIMARY_PASSWORD
#define TEST_WIFI_SECOND_PASS  CONFIG_WIFI_SERVICE_TEST_SECONDARY_PASSWORD

static void create_profile_mgr(wifi_service_test_storage_ctx_t *ram,
                               esp_config_storage_t *storage,
                               esp_wifi_service_profile_mgr_t *manager)
{
    wifi_service_test_storage_reset(ram);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_service_test_storage_open(ram, storage));
    const esp_wifi_service_profile_mgr_cfg_t cfg = {
        .max_profiles = 3,
        .storage = *storage,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_init(&cfg, manager));
}

typedef struct {
    struct esp_wifi_service_prov_base  base;
    unsigned                           start_count;
    unsigned                           stop_count;
    bool                               started;
} fake_prov_agent_t;

static esp_err_t fake_prov_start(esp_wifi_service_prov_t prov)
{
    fake_prov_agent_t *agent = (fake_prov_agent_t *)prov;
    agent->start_count++;
    agent->started = true;
    return esp_wifi_service_prov_dispatch_event(prov, ESP_WIFI_SERVICE_PROV_EVT_STARTED, NULL);
}

static esp_err_t fake_prov_stop(esp_wifi_service_prov_t prov)
{
    fake_prov_agent_t *agent = (fake_prov_agent_t *)prov;
    agent->stop_count++;
    agent->started = false;
    return esp_wifi_service_prov_dispatch_event(prov, ESP_WIFI_SERVICE_PROV_EVT_STOPPED, NULL);
}

static esp_err_t fake_prov_send(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len)
{
    (void)prov;
    (void)data;
    (void)data_len;
    return ESP_ERR_NOT_SUPPORTED;
}

static const esp_wifi_service_prov_ops_t s_fake_prov_ops = {
    .start = fake_prov_start,
    .stop  = fake_prov_stop,
    .send  = fake_prov_send,
};

static void fake_prov_agent_init(fake_prov_agent_t *agent)
{
    *agent = (fake_prov_agent_t) {0};
    const esp_wifi_service_prov_config_t cfg = {
        .name = "fake-provisioning",
        .ops = &s_fake_prov_ops,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_prov_init((esp_wifi_service_prov_t)agent, &cfg));
}

TEST_CASE("wifi service rejects invalid create arguments", "[wifi_service][lifecycle]")
{
    esp_wifi_service_t *service = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(NULL, &service));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(&(esp_wifi_service_config_t) {0}, NULL));

    esp_wifi_service_config_t cfg = {
        .profile_manager = NULL,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(&cfg, &service));

    cfg.profile_manager = (esp_wifi_service_profile_mgr_t)0x1;
    cfg.prov_num = 1;
    cfg.prov_list = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NULL(service);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_destroy(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_get_profile_manager(NULL, &cfg.profile_manager));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_is_provisioning_running(NULL, &(bool) {false}));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_request_reeval(NULL));
}

TEST_CASE("wifi service creates without provisioning agents and reports idle state", "[wifi_service][lifecycle][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);

    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    TEST_ASSERT_EQUAL(ESP_OK, esp_service_get_state((const esp_service_t *)service, &state));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, state);

    esp_wifi_service_profile_mgr_t got_manager = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_get_profile_manager(service, &got_manager));
    TEST_ASSERT_EQUAL_PTR(manager, got_manager);

    bool running = true;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_is_provisioning_running(service, &running));
    TEST_ASSERT_FALSE(running);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_wifi_service_request_reeval(service));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service accepts selector retry policy", "[wifi_service][selector][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    const uint32_t scan_retry_ms[] = {250, 1000, 5000};
    const esp_wifi_service_selector_cfg_t selector_cfg = {
        .retry = {
            .scan_retry_ms = scan_retry_ms,
            .scan_retry_num = (uint8_t)(sizeof(scan_retry_ms) / sizeof(scan_retry_ms[0])),
            .max_connect_retry = 3,
        },
    };
    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-retry-policy-ut",
        .profile_manager = manager,
        .selector_policy = &selector_cfg,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(service);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service rejects invalid selector retry policy", "[wifi_service][selector][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    esp_wifi_service_selector_cfg_t selector_cfg = {
        .retry = {
            .scan_retry_num = 1,
        },
    };
    esp_wifi_service_config_t cfg = {
        .name = "wifi-service-retry-policy-invalid-ut",
        .profile_manager = manager,
        .selector_policy = &selector_cfg,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NULL(service);

    const uint32_t zero_retry_ms[] = {1000, 0};
    selector_cfg.retry.scan_retry_ms = zero_retry_ms;
    selector_cfg.retry.scan_retry_num = (uint8_t)(sizeof(zero_retry_ms) / sizeof(zero_retry_ms[0]));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NULL(service);

    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service initializes and deinitializes provisioning callbacks", "[wifi_service][prov][lifecycle][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    fake_prov_agent_t agent;
    fake_prov_agent_init(&agent);
    esp_wifi_service_prov_t prov_list[] = {
        (esp_wifi_service_prov_t)&agent,
    };

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-prov-lifecycle-ut",
        .profile_manager = manager,
        .prov_list = prov_list,
        .prov_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));
    TEST_ASSERT_NOT_NULL(agent.base.event_cb);
    TEST_ASSERT_NOT_NULL(agent.base.event_ctx);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    TEST_ASSERT_NULL(agent.base.event_cb);
    TEST_ASSERT_NULL(agent.base.event_ctx);

    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

#if CONFIG_WIFI_SERVICE_TEST_WITH_SSID
TEST_CASE("wifi service request connect stores configured profile unattended", "[wifi_service][connect][ssid][timeout=30]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-connect-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char ssid[] = TEST_WIFI_SSID;
    char password[] = TEST_WIFI_PASSWORD;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_request_connect(service, ssid, password, 5, 0));

    esp_wifi_service_profile_t saved = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, TEST_WIFI_SSID, &saved));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, saved.ssid);
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, saved.password);
    TEST_ASSERT_TRUE((saved.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0);
    TEST_ASSERT_EQUAL_UINT8(5, saved.priority);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service request connect stops active provisioning", "[wifi_service][connect][prov][ssid][timeout=45]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    fake_prov_agent_t agent;
    fake_prov_agent_init(&agent);
    esp_wifi_service_prov_t prov_list[] = {
        (esp_wifi_service_prov_t)&agent,
    };

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-prov-connect-ut",
        .profile_manager = manager,
        .prov_list = prov_list,
        .prov_num = 1,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_start_provisioning(service));
    bool running = false;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_is_provisioning_running(service, &running));
    TEST_ASSERT_TRUE(running);
    TEST_ASSERT_TRUE(agent.started);
    TEST_ASSERT_EQUAL_UINT(1, agent.start_count);

    char ssid[] = TEST_WIFI_SSID;
    char password[] = TEST_WIFI_PASSWORD;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_request_connect(service, ssid, password, 5, 0));

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_is_provisioning_running(service, &running));
    TEST_ASSERT_FALSE(running);
    TEST_ASSERT_FALSE(agent.started);
    TEST_ASSERT_EQUAL_UINT(1, agent.stop_count);

    esp_wifi_service_profile_t saved = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, TEST_WIFI_SSID, &saved));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_PASSWORD, saved.password);
    TEST_ASSERT_TRUE((saved.flags & ESP_WIFI_SERVICE_PROFILE_FLAG_ENABLED) != 0);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service request connect joins configured AP and gets IP", "[wifi_service][connect][ssid][wifi][timeout=90]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-real-connect-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char ssid[] = TEST_WIFI_SSID;
    char password[] = TEST_WIFI_PASSWORD;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_request_connect(service, ssid, password, 5, 45));

    wifi_ap_record_t ap = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_sta_get_ap_info(&ap));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, (const char *)ap.ssid);

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    TEST_ASSERT_NOT_NULL(sta_netif);
    esp_netif_ip_info_t ip_info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_get_ip_info(sta_netif, &ip_info));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, ip_info.ip.addr);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}

TEST_CASE("wifi service request connect switches between configured APs", "[wifi_service][connect][ssid][wifi][timeout=150]")
{
    wifi_service_test_storage_ctx_t ram;
    esp_config_storage_t storage = NULL;
    esp_wifi_service_profile_mgr_t manager = NULL;
    create_profile_mgr(&ram, &storage, &manager);

    esp_wifi_service_t *service = NULL;
    const esp_wifi_service_config_t cfg = {
        .name = "wifi-service-switch-connect-ut",
        .profile_manager = manager,
    };
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_create(&cfg, &service));

    char first_ssid[] = TEST_WIFI_SSID;
    char password[] = TEST_WIFI_PASSWORD;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_request_connect(service, first_ssid, password, 5, 45));

    wifi_ap_record_t ap = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_sta_get_ap_info(&ap));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SSID, (const char *)ap.ssid);

    char second_ssid[] = TEST_WIFI_SECOND_SSID;
    char second_password[] = TEST_WIFI_SECOND_PASS;
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_request_connect(service, second_ssid, second_password, 10, 60));

    memset(&ap, 0, sizeof(ap));
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_sta_get_ap_info(&ap));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SECOND_SSID, (const char *)ap.ssid);

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    TEST_ASSERT_NOT_NULL(sta_netif);
    esp_netif_ip_info_t ip_info = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_netif_get_ip_info(sta_netif, &ip_info));
    TEST_ASSERT_NOT_EQUAL_UINT32(0, ip_info.ip.addr);

    esp_wifi_service_profile_t saved = {0};
    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_profile_mgr_get(manager, TEST_WIFI_SECOND_SSID, &saved));
    TEST_ASSERT_EQUAL_STRING(TEST_WIFI_SECOND_PASS, saved.password);
    TEST_ASSERT_EQUAL_UINT8(10, saved.priority);

    TEST_ASSERT_EQUAL(ESP_OK, esp_wifi_service_destroy(service));
    esp_wifi_service_profile_mgr_deinit(manager);
    esp_config_storage_deinit(storage);
}
#endif  /* CONFIG_WIFI_SERVICE_TEST_WITH_SSID */
