/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "test_fixtures.h"

/* Every create/destroy cycle logs the 512-byte domain name twice, costing
 * ~100 ms per cycle on a 115200-baud UART console; keep the churn within the
 * join budget on real hardware. */
#define STATS_CHURN_CYCLES   10
#define STATS_CHURN_JOIN_MS  10000

typedef struct {
    const char        *name;
    SemaphoreHandle_t  done;
} stats_churn_t;

static void stats_churn(void *arg)
{
    stats_churn_t *ctx = arg;
    for (int i = 0; i < STATS_CHURN_CYCLES; i++) {
        adf_event_hub_t hub = NULL;
        configASSERT(adf_event_hub_create(ctx->name, &hub) == ESP_OK);
        vTaskDelay(2);
        configASSERT(adf_event_hub_destroy(hub) == ESP_OK);
    }
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

TEST_CASE("stats - long name snapshots survive concurrent domain removal", "[adf_event_hub][reentrant]")
{
    adf_event_hub_t bootstrap = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stats_bootstrap", &bootstrap));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(bootstrap));
    char source[513], copy[513];
    memset(source, 'x', sizeof(source) - 1);
    source[sizeof(source) - 1] = '\0';
    stats_churn_t ctx = {.name = source, .done = xSemaphoreCreateBinary()};
    TEST_ASSERT_NOT_NULL(ctx.done);
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(stats_churn, "stats_churn", TEST_STACK_SZ, &ctx, 2, NULL));
    adf_event_domain_stat_t domain = {.domain = copy, .domain_capacity = sizeof(copy)};
    adf_event_hub_stats_t stats = {.domains = &domain, .domains_capacity = 1};
    int observed = 0;
    for (int i = 0; i < 400; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_get_stats(&stats));
        if (stats.domain_count) {
            TEST_ASSERT_EQUAL_STRING(source, copy);
            observed++;
        }
        vTaskDelay(1);
    }
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(ctx.done, pdMS_TO_TICKS(STATS_CHURN_JOIN_MS)));
    TEST_ASSERT_GREATER_THAN(0, observed);
    TEST_ASSERT_EQUAL_STRING(source, copy);
    vSemaphoreDelete(ctx.done);
}

TEST_CASE("stats - copied name survives removal and undersized buffer supports retry", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stats_owned_name", &hub));
    char name[32] = "unchanged";
    adf_event_domain_stat_t domain = {.domain = name, .domain_capacity = 2};
    adf_event_hub_stats_t stats = {.domains = &domain, .domains_capacity = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, adf_event_hub_get_stats(&stats));
    TEST_ASSERT_EQUAL_STRING("unchanged", name);
    domain.domain_capacity = sizeof(name);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_get_stats(&stats));
    TEST_ASSERT_EQUAL_STRING("stats_owned_name", name);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    TEST_ASSERT_EQUAL_STRING("stats_owned_name", name);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stats_owned_name", &hub));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    TEST_ASSERT_EQUAL_STRING("stats_owned_name", name);
}

TEST_CASE("stats - absent name buffer is rejected without touching other entries", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stats_missing", &hub));
    adf_event_domain_stat_t domain = {.cb_subscriber_count = 123};
    adf_event_hub_stats_t stats = {.domains = &domain, .domains_capacity = 1};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, adf_event_hub_get_stats(&stats));
    TEST_ASSERT_EQUAL_UINT32(123, domain.cb_subscriber_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
}

TEST_CASE("queue drops - cumulative count increases and payload is released once", "[adf_event_hub]")
{
    adf_event_hub_t hub = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_create("stats_drops", &hub));
    QueueHandle_t inbox = xQueueCreate(1, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.target_queue = inbox;
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_subscribe(hub, &info));
    adf_event_hub_stats_t before = {0}, after = {0};
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_get_stats(&before));
    cb_ctx_t releases = {0};
    for (int i = 0; i < 10; i++) {
        uint32_t *payload = make_payload(i);
        TEST_ASSERT_NOT_NULL(payload);
        adf_event_t event = {.domain = "stats_drops", .event_id = 1, .payload = payload};
        TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_publish(hub, &event, release_count_cb, &releases));
        TEST_ASSERT_EQUAL_INT(i, releases.release_count);
    }
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_get_stats(&after));
    TEST_ASSERT_EQUAL_UINT32(before.queue_drop_count + 9, after.queue_drop_count);
    TEST_ASSERT_EQUAL_UINT32(before.envelopes_in_use + 1, after.envelopes_in_use);
    adf_event_delivery_t delivery;
    TEST_ASSERT_EQUAL(pdTRUE, xQueueReceive(inbox, &delivery, 0));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_delivery_done(hub, &delivery));
    TEST_ASSERT_EQUAL_INT(10, releases.release_count);
    TEST_ASSERT_EQUAL(ESP_OK, adf_event_hub_destroy(hub));
    vQueueDelete(inbox);
}
