/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Performance benchmarks for adf_event_hub.
 *
 * Measures publish latency under various configurations to guide
 * optimization decisions.  Results are printed as tables; no assertions
 * on absolute timing (values are platform-dependent).  Compare relative
 * ratios across configurations.
 *
 * Benchmarks:
 *   1. Publish latency vs subscriber count (event_id filter overhead)
 *   2. Wildcard subscriber ratio impact
 *   3. domain_find latency vs domain count
 *   4. Envelope alloc/release round-trip cost
 *   5. Multi-domain sequential throughput (lock overhead)
 *   6. Stack footprint estimation
 *   7. Free-list O(1) alloc scalability (improvement verification)
 *   8. subscriber_head early-out speedup (improvement verification)
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "test_port.h"
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#endif  /* ESP_PLATFORM */

/* ── Helpers ──────────────────────────────────────────────────────────── */

static void bench_noop_cb(const adf_event_t *ev, void *ctx)
{
    (void)ev;
    (*(int *)ctx)++;
}

static void bench_release_noop(const void *payload, void *ctx)
{
    (void)payload;
    (*(int *)ctx)++;
}

#define BENCH_WARMUP  200
#define BENCH_ROUNDS  5000

/* ════════════════════════════════════════════════════════════════════════
 * Benchmark 1: Publish latency vs subscriber count
 *
 * N subscribers each with a unique event_id.  Publish event_id = 0
 * (only 1 subscriber matches).  Shows O(N) linear scan overhead.
 * ════════════════════════════════════════════════════════════════════════ */

TEST_CASE("bench - publish latency vs subscriber count (event_id filter)",
          "[bench]")
{
    int sub_counts[] = {1, 4, 8, 16, 32, 64};
    int num_cfgs = (int)(sizeof(sub_counts) / sizeof(sub_counts[0]));

    printf("\n"
           "┌──────────────────────────────────────────────────────────┐\n"
           "│ Publish latency vs subscriber count (1 match out of N)  │\n"
           "├───────────┬──────────────┬────────────────────────────── │\n"
           "│ total_sub │ avg_pub (us) │ ratio (vs N=1 baseline)      │\n"
           "├───────────┼──────────────┼────────────────────────────── │\n");

    double baseline_us = 0;

    for (int c = 0; c < num_cfgs; c++) {
        int N = sub_counts[c];
        adf_event_hub_t hub = NULL;
        adf_event_hub_create("b_sub_scan", &hub);

        int dummy = 0;
        for (int i = 0; i < N; i++) {
            adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
            info.event_domain = "b_sub_scan";
            info.event_id = (uint16_t)i;
            info.handler = bench_noop_cb;
            info.handler_ctx = &dummy;
            adf_event_hub_subscribe(hub, &info);
        }

        adf_event_t ev = {.domain = "b_sub_scan", .event_id = 0};
        for (int i = 0; i < BENCH_WARMUP; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
        }

        int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ROUNDS; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
        }
        double avg = (double)(esp_timer_get_time() - t0) / BENCH_ROUNDS;

        if (c == 0) {
            baseline_us = avg;
        }

        printf("│ %9d │ %12.2f │ %12.2fx                   │\n",
               N, avg, baseline_us > 0 ? avg / baseline_us : 1.0);

        adf_event_hub_destroy(hub);
    }

    printf("└───────────┴──────────────┴────────────────────────────── ┘\n"
           "  → ratio >> 1.0 with many subs = event_id bucketing worthwhile\n\n");
}

TEST_CASE("bench - wildcard subscriber ratio impact on publish latency",
          "[bench]")
{
#define TOTAL_SUBS  16
    int wc_counts[] = {0, 4, 8, 12, 16};
    int num_cfgs = (int)(sizeof(wc_counts) / sizeof(wc_counts[0]));

    printf("\n"
           "┌──────────────────────────────────────────────────────────────┐\n"
           "│ Wildcard ratio impact (total_sub=%d, publish event_id=0)     │\n"
           "├──────────┬──────────┬──────────────┬─────────────────────────┤\n"
           "│ wildcard │ specific │ avg_pub (us) │ matched (per publish)   │\n"
           "├──────────┼──────────┼──────────────┼─────────────────────────┤\n",
           TOTAL_SUBS);

    for (int c = 0; c < num_cfgs; c++) {
        int M = wc_counts[c];
        int S = TOTAL_SUBS - M;

        adf_event_hub_t hub = NULL;
        adf_event_hub_create("b_wildcard", &hub);

        int dummy = 0;
        for (int i = 0; i < S; i++) {
            adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
            info.event_domain = "b_wildcard";
            info.event_id = (uint16_t)i;
            info.handler = bench_noop_cb;
            info.handler_ctx = &dummy;
            adf_event_hub_subscribe(hub, &info);
        }
        for (int i = 0; i < M; i++) {
            adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
            info.event_domain = "b_wildcard";
            info.event_id = ADF_EVENT_ANY_ID;
            info.handler = bench_noop_cb;
            info.handler_ctx = &dummy;
            adf_event_hub_subscribe(hub, &info);
        }

        adf_event_t ev = {.domain = "b_wildcard", .event_id = 0};
        for (int i = 0; i < BENCH_WARMUP; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
        }

        dummy = 0;
        int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ROUNDS; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
        }
        double avg = (double)(esp_timer_get_time() - t0) / BENCH_ROUNDS;
        int matched = dummy / BENCH_ROUNDS;

        printf("│ %8d │ %8d │ %12.2f │ %23d │\n", M, S, avg, matched);

        adf_event_hub_destroy(hub);
    }

    printf("└──────────┴──────────┴──────────────┴─────────────────────────┘\n"
           "  → if wildcard ratio > 50%%, bucketing gain is offset\n\n");
#undef TOTAL_SUBS
}

TEST_CASE("bench - domain_find latency vs domain count",
          "[bench]")
{
    int dom_counts[] = {1, 5, 10, 20};
    int num_cfgs = (int)(sizeof(dom_counts) / sizeof(dom_counts[0]));

    printf("\n"
           "┌──────────────────────────────────────────────────────────┐\n"
           "│ domain_find latency (publish to last domain, worst case)│\n"
           "├──────────┬──────────────┬────────────────────────────────┤\n"
           "│ domains  │ avg_pub (us) │ ratio (vs D=1 baseline)       │\n"
           "├──────────┼──────────────┼────────────────────────────────┤\n");

    double baseline_us = 0;

    for (int c = 0; c < num_cfgs; c++) {
        int D = dom_counts[c];
        adf_event_hub_t hubs[20];
        char names[20][16];

        for (int d = 0; d < D; d++) {
            snprintf(names[d], sizeof(names[d]), "bd_%02d", d);
            adf_event_hub_create(names[d], &hubs[d]);
        }

        int dummy = 0;
        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = names[D - 1];
        info.handler = bench_noop_cb;
        info.handler_ctx = &dummy;
        adf_event_hub_subscribe(hubs[D - 1], &info);

        adf_event_t ev = {.domain = names[D - 1], .event_id = 1};
        for (int i = 0; i < BENCH_WARMUP; i++) {
            adf_event_hub_publish(hubs[D - 1], &ev, NULL, NULL);
        }

        int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ROUNDS; i++) {
            adf_event_hub_publish(hubs[D - 1], &ev, NULL, NULL);
        }
        double avg = (double)(esp_timer_get_time() - t0) / BENCH_ROUNDS;

        if (c == 0) {
            baseline_us = avg;
        }

        printf("│ %8d │ %12.2f │ %12.2fx                    │\n",
               D, avg, baseline_us > 0 ? avg / baseline_us : 1.0);

        for (int d = 0; d < D; d++) {
            adf_event_hub_destroy(hubs[d]);
        }
    }

    printf("└──────────┴──────────────┴────────────────────────────────┘\n"
           "  → ratio >> 1.0 at D=20 = hash table worthwhile\n\n");
}

TEST_CASE("bench - envelope alloc/release round-trip cost",
          "[bench]")
{
    adf_event_hub_t hub = NULL;
    adf_event_hub_create("b_envelope", &hub);

    QueueHandle_t inbox = xQueueCreate(4, sizeof(adf_event_delivery_t));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "b_envelope";
    info.target_queue = inbox;
    adf_event_hub_subscribe(hub, &info);

    adf_event_t ev = {.domain = "b_envelope", .event_id = 1};
    int rel_count = 0;

    for (int i = 0; i < BENCH_WARMUP; i++) {
        adf_event_hub_publish(hub, &ev, bench_release_noop, &rel_count);
        adf_event_delivery_t d;
        xQueueReceive(inbox, &d, portMAX_DELAY);
        adf_event_hub_delivery_done(hub, &d);
    }

    rel_count = 0;
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < BENCH_ROUNDS; i++) {
        adf_event_hub_publish(hub, &ev, bench_release_noop, &rel_count);
        adf_event_delivery_t d;
        xQueueReceive(inbox, &d, portMAX_DELAY);
        adf_event_hub_delivery_done(hub, &d);
    }
    double total_us = (double)(esp_timer_get_time() - t0);
    double avg_cycle = total_us / BENCH_ROUNDS;

    printf("\n"
           "┌──────────────────────────────────────────────────────────┐\n"
           "│ Envelope alloc + release round-trip (1 queue subscriber)│\n"
           "├──────────────────────────┬───────────────────────────────┤\n"
           "│ metric                   │ value                        │\n"
           "├──────────────────────────┼───────────────────────────────┤\n"
           "│ avg round-trip (us)      │ %12.2f                    │\n"
           "│ rounds                   │ %12d                    │\n"
           "│ release_cb calls         │ %12d                    │\n"
           "└──────────────────────────┴───────────────────────────────┘\n\n",
           avg_cycle, BENCH_ROUNDS, rel_count);

    vQueueDelete(inbox);
    adf_event_hub_destroy(hub);
}

TEST_CASE("bench - multi-domain round-robin publish (lock overhead)",
          "[bench]")
{
#define MAX_BENCH_DOMAINS  10
    int dom_counts[] = {1, 2, 4, 10};
    int num_cfgs = (int)(sizeof(dom_counts) / sizeof(dom_counts[0]));

    printf("\n"
           "┌──────────────────────────────────────────────────────────┐\n"
           "│ Multi-domain round-robin publish (sequential)           │\n"
           "├──────────┬──────────────┬────────────────────────────────┤\n"
           "│ domains  │ avg_pub (us) │ total time %d pubs (ms)     │\n"
           "├──────────┼──────────────┼────────────────────────────────┤\n",
           BENCH_ROUNDS);

    for (int c = 0; c < num_cfgs; c++) {
        int D = dom_counts[c];
        adf_event_hub_t hubs[MAX_BENCH_DOMAINS];
        char names[MAX_BENCH_DOMAINS][16];
        int dummies[MAX_BENCH_DOMAINS];
        memset(dummies, 0, sizeof(dummies));

        for (int d = 0; d < D; d++) {
            snprintf(names[d], sizeof(names[d]), "brr_%02d", d);
            adf_event_hub_create(names[d], &hubs[d]);

            adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
            info.event_domain = names[d];
            info.handler = bench_noop_cb;
            info.handler_ctx = &dummies[d];
            adf_event_hub_subscribe(hubs[d], &info);
        }

        /* warmup */
        for (int i = 0; i < BENCH_WARMUP; i++) {
            int d = i % D;
            adf_event_t ev = {.domain = names[d], .event_id = 1};
            adf_event_hub_publish(hubs[d], &ev, NULL, NULL);
        }

        int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ROUNDS; i++) {
            int d = i % D;
            adf_event_t ev = {.domain = names[d], .event_id = 1};
            adf_event_hub_publish(hubs[d], &ev, NULL, NULL);
        }
        int64_t elapsed = esp_timer_get_time() - t0;
        double avg = (double)elapsed / BENCH_ROUNDS;
        double total_ms = (double)elapsed / 1000.0;

        printf("│ %8d │ %12.2f │ %14.2f                  │\n",
               D, avg, total_ms);

        for (int d = 0; d < D; d++) {
            adf_event_hub_destroy(hubs[d]);
        }
    }

    printf("└──────────┴──────────────┴────────────────────────────────┘\n"
           "  → if D>1 much slower than D=1, per-domain lock helps\n\n");
#undef MAX_BENCH_DOMAINS
}

TEST_CASE("bench - stack footprint estimation",
          "[bench]")
{
    /**
     * adf_evt_hub_sub_target_t layout (internal, sizes estimated):
     *   uint8_t is_queue + padding + union { QueueHandle_t; struct{handler,ctx} }
     * ADF_PUBLISH_MAX_SUBS = 32
     */
    size_t event_sz = sizeof(adf_event_t);
    size_t delivery_sz = sizeof(adf_event_delivery_t);

    printf("\n"
           "┌──────────────────────────────────────────────────────────┐\n"
           "│ Stack footprint estimation                              │\n"
           "├──────────────────────────────┬───────────────────────────┤\n"
           "│ item                         │ bytes (this platform)    │\n"
           "├──────────────────────────────┼───────────────────────────┤\n"
           "│ sizeof(adf_event_t)          │ %8zu                  │\n"
           "│ sizeof(adf_event_delivery_t) │ %8zu                  │\n"
           "│ sizeof(void *)               │ %8zu                  │\n"
           "│ snap[32] estimated*          │ %8zu                  │\n"
           "│ snap[4] alternative*         │ %8zu                  │\n"
           "│ savings with snap[4]         │ %8zu                  │\n"
           "└──────────────────────────────┴───────────────────────────┘\n",
           event_sz,
           delivery_sz,
           sizeof(void *),
           /* snap[32]: each entry ~= 1 + padding + max(ptr, 2*ptr) */
           (size_t)(32 * (sizeof(void *) <= 4 ? 12 : 24)),
           (size_t)(4 * (sizeof(void *) <= 4 ? 12 : 24)),
           (size_t)(28 * (sizeof(void *) <= 4 ? 12 : 24)));

    printf("  * snap entry ≈ uint8_t + union{QueueHandle_t; {handler,ctx}}\n"
           "  * 32-bit platform: ~12 bytes/entry; 64-bit: ~24 bytes/entry\n"
           "  * ESP32 (32-bit): snap[32] = ~384 bytes on stack per publish\n"
           "  * With snap[4]:   snap[4]  = ~48 bytes  (saving ~336 bytes)\n\n");

    TEST_ASSERT_TRUE(event_sz > 0);
}

TEST_CASE("bench - free-list O(1) alloc scalability (opt #1 verify)",
          "[bench]")
{
    int pool_sizes[] = {4, 32, 128, 512};
    int num_cfgs = (int)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));

    printf("\n"
           "┌───────────────────────────────────────────────────────────────────┐\n"
           "│ Free-list O(1) alloc scalability (Optimization #1)               │\n"
           "│ N-1 slots in_use, 1 slot free; measure alloc+release cycle       │\n"
           "├───────────┬──────────────┬────────────┬──────────────────────────┤\n"
           "│ pool_size │ avg_cyc (us) │ ratio/N=4  │ old O(N) est (us/alloc) │\n"
           "├───────────┼──────────────┼────────────┼──────────────────────────┤\n");

    double baseline_us = 0;

    for (int c = 0; c < num_cfgs; c++) {
        int N = pool_sizes[c];

        adf_event_hub_t hub = NULL;
        adf_event_hub_create("b_freelist", &hub);

        QueueHandle_t inbox = xQueueCreate(N + 2, sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(inbox);

        adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
        info.event_domain = "b_freelist";
        info.target_queue = inbox;
        adf_event_hub_subscribe(hub, &info);

        adf_event_t ev = {.domain = "b_freelist", .event_id = 1};

        adf_event_delivery_t *held = malloc((size_t)N * sizeof(adf_event_delivery_t));
        TEST_ASSERT_NOT_NULL(held);

        for (int i = 0; i < N; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
            xQueueReceive(inbox, &held[i], portMAX_DELAY);
        }

        adf_event_hub_delivery_done(hub, &held[N - 1]);

        for (int i = 0; i < BENCH_WARMUP && i < BENCH_ROUNDS; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
            adf_event_delivery_t d;
            xQueueReceive(inbox, &d, portMAX_DELAY);
            adf_event_hub_delivery_done(hub, &d);
        }

        int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < BENCH_ROUNDS; i++) {
            adf_event_hub_publish(hub, &ev, NULL, NULL);
            adf_event_delivery_t d;
            xQueueReceive(inbox, &d, portMAX_DELAY);
            adf_event_hub_delivery_done(hub, &d);
        }
        double avg = (double)(esp_timer_get_time() - t0) / BENCH_ROUNDS;

        if (c == 0) {
            baseline_us = avg;
        }
        double ratio = baseline_us > 0 ? avg / baseline_us : 1.0;

        double old_scan_est = (double)(N - 1) * 0.005;

        printf("│ %9d │ %12.2f │ %10.2fx │ %24.2f │\n",
               N, avg, ratio, old_scan_est);

        for (int i = 0; i < N - 1; i++) {
            adf_event_hub_delivery_done(hub, &held[i]);
        }
        free(held);
        vQueueDelete(inbox);
        adf_event_hub_destroy(hub);
    }

    printf("└───────────┴──────────────┴────────────┴──────────────────────────┘\n"
           "  → ratio ≈ 1.0 across pool sizes = free-list O(1) proven\n"
           "  → old O(N) estimate: per-slot check ~5ns on PC; more on ESP32\n"
           "  → at pool_size=512, old code adds ~2.5us/alloc; free-list: ~0us\n\n");
}

TEST_CASE("bench - subscriber_head early-out speedup (opt #3 verify)",
          "[bench]")
{
    printf("\n"
           "┌───────────────────────────────────────────────────────────────┐\n"
           "│ subscriber_head early-out (Optimization #3)                  │\n"
           "├──────────────────────┬──────────────┬────────────────────────┤\n"
           "│ scenario             │ avg_pub (us) │ speedup vs full-path  │\n"
           "├──────────────────────┼──────────────┼────────────────────────┤\n");

    /* --- Case A: 0 subscribers (early-out path) --- */
    adf_event_hub_t hub_a = NULL;
    adf_event_hub_create("b_earlyout_a", &hub_a);

    adf_event_t ev = {.domain = "b_earlyout_a", .event_id = 1};
    int rel_count_a = 0;

    for (int i = 0; i < BENCH_WARMUP; i++) {
        adf_event_hub_publish(hub_a, &ev, bench_release_noop, &rel_count_a);
    }

    rel_count_a = 0;
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < BENCH_ROUNDS; i++) {
        adf_event_hub_publish(hub_a, &ev, bench_release_noop, &rel_count_a);
    }
    double avg_a = (double)(esp_timer_get_time() - t0) / BENCH_ROUNDS;

    TEST_ASSERT_EQUAL_INT(BENCH_ROUNDS, rel_count_a);

    printf("│ A: 0 subs (early-out)│ %12.2f │ baseline (1.00x)       │\n",
           avg_a);

    adf_event_hub_destroy(hub_a);

    /* --- Case B: 1 callback subscriber --- */
    adf_event_hub_t hub_b = NULL;
    adf_event_hub_create("b_earlyout_b", &hub_b);

    int dummy_b = 0;
    adf_event_subscribe_info_t info_b = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info_b.event_domain = "b_earlyout_b";
    info_b.handler = bench_noop_cb;
    info_b.handler_ctx = &dummy_b;
    adf_event_hub_subscribe(hub_b, &info_b);

    adf_event_t ev_b = {.domain = "b_earlyout_b", .event_id = 1};

    for (int i = 0; i < BENCH_WARMUP; i++) {
        adf_event_hub_publish(hub_b, &ev_b, NULL, NULL);
    }

    int64_t t1 = esp_timer_get_time();
    for (int i = 0; i < BENCH_ROUNDS; i++) {
        adf_event_hub_publish(hub_b, &ev_b, NULL, NULL);
    }
    double avg_b = (double)(esp_timer_get_time() - t1) / BENCH_ROUNDS;

    double speedup_b = avg_a > 0 ? avg_b / avg_a : 1.0;
    printf("│ B: 1 callback sub   │ %12.2f │ %5.2fx slower           │\n",
           avg_b, speedup_b);

    adf_event_hub_destroy(hub_b);

    /* --- Case C: 1 queue subscriber (full envelope path) --- */
    adf_event_hub_t hub_c = NULL;
    adf_event_hub_create("b_earlyout_c", &hub_c);

    QueueHandle_t inbox_c = xQueueCreate(4, sizeof(adf_event_delivery_t));
    TEST_ASSERT_NOT_NULL(inbox_c);

    adf_event_subscribe_info_t info_c = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info_c.event_domain = "b_earlyout_c";
    info_c.target_queue = inbox_c;
    adf_event_hub_subscribe(hub_c, &info_c);

    adf_event_t ev_c = {.domain = "b_earlyout_c", .event_id = 1};
    int rel_count_c = 0;

    for (int i = 0; i < BENCH_WARMUP; i++) {
        adf_event_hub_publish(hub_c, &ev_c, bench_release_noop, &rel_count_c);
        adf_event_delivery_t d;
        xQueueReceive(inbox_c, &d, portMAX_DELAY);
        adf_event_hub_delivery_done(hub_c, &d);
    }

    rel_count_c = 0;
    int64_t t2 = esp_timer_get_time();
    for (int i = 0; i < BENCH_ROUNDS; i++) {
        adf_event_hub_publish(hub_c, &ev_c, bench_release_noop, &rel_count_c);
        adf_event_delivery_t d;
        xQueueReceive(inbox_c, &d, portMAX_DELAY);
        adf_event_hub_delivery_done(hub_c, &d);
    }
    double avg_c = (double)(esp_timer_get_time() - t2) / BENCH_ROUNDS;

    double speedup_c = avg_a > 0 ? avg_c / avg_a : 1.0;
    printf("│ C: 1 queue sub       │ %12.2f │ %5.2fx slower           │\n",
           avg_c, speedup_c);

    printf("└──────────────────────┴──────────────┴────────────────────────┘\n"
           "  → A << B,C proves the early-out saves the snapshot+dispatch cost\n"
           "  → Without early-out, case A would be ~= B (still enters snapshot loop)\n"
           "  → release_cb correctly called even on early-out: %d/%d\n\n",
           rel_count_a, BENCH_ROUNDS);

    vQueueDelete(inbox_c);
    adf_event_hub_destroy(hub_c);
}
