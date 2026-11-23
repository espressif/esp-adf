/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT */
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "unity.h"
#include "esp_rtmp_service.h"
#include "esp_rtmp_service_ops.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_extractor_service.h"
#include "esp_extractor_service_ops.h"
#include "esp_media_dummy_service.h"
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
#include "esp_media_service_mcp.h"
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

typedef esp_err_t (*create_fn_t)(esp_service_t **service);

static esp_err_t create_rtmp(esp_service_t **service)
{
    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SRC);
    esp_rtmp_service_t *typed = NULL;
    esp_err_t ret = esp_rtmp_service_create(&cfg, &typed);
    if (ret == ESP_OK) {
        TEST_ESP_OK(esp_rtmp_service_set_url(typed, "rtmp://127.0.0.1/live/test"));
        esp_media_service_request_t request = {0};
        TEST_ESP_OK(esp_media_service_set_request(ESP_SERVICE_BASE(typed), ESP_MEDIA_DEFAULT_STREAM, &request));
    }
    *service = ESP_SERVICE_BASE(typed);
    return ret;
}

static esp_err_t create_rtsp(esp_service_t **service)
{
    esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SRC);
    esp_rtsp_service_t *typed = NULL;
    esp_err_t ret = esp_rtsp_service_create(&cfg, &typed);
    if (ret == ESP_OK) {
        TEST_ESP_OK(esp_rtsp_service_set_url(typed, "rtsp://127.0.0.1/live/test"));
        esp_media_service_request_t request = {0};
        TEST_ESP_OK(esp_media_service_set_request(ESP_SERVICE_BASE(typed), ESP_MEDIA_DEFAULT_STREAM, &request));
    }
    *service = ESP_SERVICE_BASE(typed);
    return ret;
}

static esp_err_t create_extractor(esp_service_t **service)
{
    esp_extractor_service_cfg_t cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    esp_extractor_service_t *typed = NULL;
    esp_err_t ret = esp_extractor_service_create(&cfg, &typed);
    if (ret == ESP_OK) {
        TEST_ESP_OK(esp_extractor_service_set_url(typed, "file:///unused.mp4"));
    }
    *service = ESP_SERVICE_BASE(typed);
    return ret;
}

static void check_derived(create_fn_t create)
{
    esp_service_t *service = NULL;
    /* Warm up shared allocations before measuring repeated ownership cycles. */
    TEST_ESP_OK(create(&service));
    TEST_ESP_OK(esp_media_service_deinit(service));
    free(service);

    size_t before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    for (unsigned i = 0; i < 30; i++) {
        TEST_ESP_OK(create(&service));
        TEST_ESP_OK(esp_media_service_deinit(service));
        free(service);
        TEST_ASSERT_TRUE(heap_caps_check_integrity_all(true));
    }
    TEST_ASSERT_GREATER_OR_EQUAL(before, heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

static void try_media_deinit(const adf_event_t *event, void *ctx)
{
    (void)event;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_media_service_deinit(ctx));
}

static esp_err_t fail_media_deinit_once(esp_service_t *service)
{
    unsigned *calls = service->user_data;
    return ++*calls == 1 ? ESP_FAIL : ESP_OK;
}

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
static esp_err_t media_mcp_probe(void)
{
    const esp_service_tool_t tool = {.name = "esp_media_service_unlink"};
    char result[128];
    return esp_media_service_tool_invoke(NULL, &tool,
                                         "{\"src_name\":\"media_deinit_ut\",\"sink_name\":\"media_deinit_ut\"}",
                                         result, sizeof(result));
}
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

TEST_CASE("RTMP media deinit - 30 owner cycles: no leaks", "[esp_service][destroy]")
{
    check_derived(create_rtmp);
}

TEST_CASE("RTSP media deinit - 30 owner cycles: no leaks", "[esp_service][destroy]")
{
    check_derived(create_rtsp);
}

TEST_CASE("extractor media deinit - 30 owner cycles: no leaks", "[esp_service][destroy]")
{
    check_derived(create_extractor);
}

TEST_CASE("media deinit preserves registration until successful owner cleanup", "[esp_service][destroy]")
{
    unsigned calls = 0;
    const esp_service_ops_t ops = {.on_deinit = fail_media_deinit_once};
    const esp_media_service_config_t cfg = {
        .name = "media_deinit_ut",
        .user_data = &calls,
        .service_ops = &ops,
    };
    esp_media_service_t media = {0};
    TEST_ESP_OK(esp_media_service_init(&media, &cfg));

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = 300;
    sub.handler = try_media_deinit;
    sub.handler_ctx = &media.base;
    TEST_ESP_OK(esp_service_event_subscribe(&media.base, &sub));
    TEST_ESP_OK(esp_service_publish_event(&media.base, 300, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(0, calls);
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_INITIALIZED, atomic_load(&media.base.state));
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    /* The service resolves by name but has no link operations. */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, media_mcp_probe());
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

    TEST_ASSERT_EQUAL(ESP_FAIL, esp_media_service_deinit(&media.base));
    TEST_ASSERT_EQUAL(ESP_SERVICE_STATE_STOPPING, atomic_load(&media.base.state));
#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, media_mcp_probe());
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */
    TEST_ESP_OK(esp_media_service_deinit(&media.base));
    TEST_ASSERT_EQUAL(2, calls);

#if CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE
    /* After media deinit succeeds, the name must no longer resolve in the MCP
     * table, and a base-only service with the same name must not alias it. */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, media_mcp_probe());
    esp_service_t base_only;
    const esp_service_config_t base_cfg = {.name = cfg.name};
    TEST_ESP_OK(esp_service_init(&base_only, &base_cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, media_mcp_probe());
    TEST_ESP_OK(esp_service_deinit(&base_only));
#endif  /* CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE */

    esp_media_service_t media2;
    TEST_ESP_OK(esp_media_service_init(&media2, &cfg));
    TEST_ESP_OK(esp_media_service_deinit(&media2.base));
    TEST_ASSERT_EQUAL(3, calls);
}

#if CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
TEST_CASE("typed destroy frees wrapper when base was already deinitialized", "[esp_service]")
{
    esp_media_dummy_service_cfg_t cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    cfg.name = "dummy_deinit_then_destroy";
    cfg.role = ESP_MEDIA_ROLE_SRC;
    esp_media_dummy_service_t *svc = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&cfg, &svc));
    TEST_ASSERT_NOT_NULL(svc);

    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(svc)));
    TEST_ESP_OK(esp_media_dummy_service_destroy(svc));

    TEST_ESP_OK(esp_media_dummy_service_create(&cfg, &svc));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(svc)));
    TEST_ESP_OK(esp_media_dummy_service_destroy(svc));
}
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */

void derived_destroy_force_link(void)
{
}
