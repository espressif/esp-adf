/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <time.h>
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "unity.h"

static int64_t scripted_get_time(void);
static int scripted_nanosleep(const struct timespec *duration, struct timespec *remaining);

#undef CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS
#define CONFIG_ESP_MCP_HTTPD_BODY_TIMEOUT_MS       300
#define CONFIG_HTTPD_WS_SUPPORT                    1
#define CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT   1
#define CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT  1
#define esp_fill_random                            scripted_fill_random
#define esp_timer_get_time                         scripted_get_time
#define nanosleep                                  scripted_nanosleep
#include "ut_transport_web_fixture.inc"

/* Advance only scripted reads; emulator scheduling must not consume the test budget. */
static int64_t s_scripted_time_us = 1;

static int64_t scripted_get_time(void)
{
    return s_scripted_time_us;
}

static int scripted_nanosleep(const struct timespec *duration, struct timespec *remaining)
{
    (void)remaining;
    s_scripted_time_us += duration->tv_sec * 1000000 + duration->tv_nsec / 1000;
    return 0;
}

TEST_CASE("httpd - stop failure: transports retain state and release once after retry", "[transport]")
{
    s_total_pass = 0;
    s_total_fail = 0;
    stub_fds_reset();
    section_t1_httpd_util();
    section_t2_upgrade_check();
    section_t3_frames();
    section_t4_start();
    section_t5_ws_broadcast();
    section_t6_sse();
    section_t7_reject_order();
    section_t8_sse_get_lifecycle();
    section_t10_stop_retry();
    section_t11_start_rollback_retry();
    TEST_ASSERT_EQUAL(0, s_total_fail);
    TEST_ASSERT_GREATER_THAN(200, s_total_pass);
}

void transport_web_fault_force_link(void)
{
}
