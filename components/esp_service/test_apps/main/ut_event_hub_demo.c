/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include "esp_heap_caps.h"
#include "unity.h"
#include "demo_button_service.h"
#include "demo_wifi_service.h"
#include "demo_ota_service.h"
#include "demo_player_service.h"
#include "demo_led_service.h"

typedef struct {
    demo_led_service_t    *led;
    demo_player_service_t *player;
    demo_ota_service_t    *ota;
    demo_wifi_service_t   *wifi;
    demo_button_service_t *button;
} demo_context_t;

static int released;

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    released++;
    free((void *)payload);
}

static void reject_callback_destroy(const adf_event_t *event, void *ctx)
{
    (void)event;
    demo_context_t *demo = ctx;
    demo_led_service_destroy(demo->led);
    demo_player_service_destroy(demo->player);
    demo_ota_service_destroy(demo->ota);
    demo_wifi_service_destroy(demo->wifi);
    demo_button_service_destroy(demo->button);
}

TEST_CASE("event hub demos join workers and drain queued payloads before release", "[demo]")
{
    adf_event_hub_stats_t before = {0};
    adf_event_hub_t publisher = NULL;
    TEST_ESP_OK(adf_event_hub_create(BUTTON_DOMAIN, &publisher));
    TEST_ESP_OK(adf_event_hub_get_stats(&before));
    for (int cycle = 0; cycle < 20; cycle++) {
        demo_led_service_t *led = demo_led_service_create();
        demo_player_service_t *player = demo_player_service_create(0);
        demo_ota_service_t *ota = demo_ota_service_create(0, 0);
        demo_wifi_service_t *wifi = demo_wifi_service_create(0);
        demo_button_service_t *button = demo_button_service_create(0);
        TEST_ASSERT_NOT_NULL(led);
        TEST_ASSERT_NOT_NULL(player);
        TEST_ASSERT_NOT_NULL(ota);
        TEST_ASSERT_NOT_NULL(wifi);
        TEST_ASSERT_NOT_NULL(button);
        demo_context_t demo = {led, player, ota, wifi, button};
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.handler = reject_callback_destroy;
        info.handler_ctx = &demo;
        TEST_ESP_OK(adf_event_hub_subscribe(publisher, &info));
        released = 0;
        int *payload = malloc(sizeof(*payload));
        TEST_ASSERT_NOT_NULL(payload);
        *payload = 42;
        adf_event_t event = {
            .domain = BUTTON_DOMAIN,
            .event_id = BUTTON_EVT_VOL_UP,
            .payload = payload,
            .payload_len = sizeof(*payload),
        };
        TEST_ESP_OK(adf_event_hub_publish(publisher, &event, release_payload, NULL));
        /* Callback-side destroy was refused; the owner can still use and destroy LED. */
        TEST_ASSERT_GREATER_OR_EQUAL(0, demo_led_service_get_total_processed(led));
        TEST_ESP_OK(adf_event_hub_unsubscribe_all(publisher));
        demo_led_service_destroy(led);
        demo_player_service_destroy(player);
        demo_ota_service_destroy(ota);
        demo_wifi_service_destroy(wifi);
        demo_button_service_destroy(button);
        TEST_ASSERT_EQUAL(1, released);
        adf_event_hub_stats_t after = {0};
        TEST_ESP_OK(adf_event_hub_get_stats(&after));
        TEST_ASSERT_EQUAL(before.domain_count, after.domain_count);
        TEST_ASSERT_EQUAL(0, after.envelopes_in_use);
        TEST_ASSERT_TRUE(heap_caps_check_integrity_all(true));
    }
    TEST_ESP_OK(adf_event_hub_destroy(publisher));
}

void transport_demo_force_link(void)
{
}
