/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "esp_system.h"
#include "unity.h"

static size_t s_heap_enter;
static size_t s_heap_tol;

/* Unity calls these around every TEST_CASE. A mismatch means that case leaked. */
void setUp(void)
{
    s_heap_enter = esp_get_free_heap_size();
    s_heap_tol = 0;
}

void test_heap_tolerance(size_t bytes)
{
    s_heap_tol = bytes;
}

void tearDown(void)
{
    size_t heap_exit = esp_get_free_heap_size();
    size_t delta;

    /* An earlier assertion already aborted the case before its cleanup. */
    if (Unity.CurrentTestFailed) {
        return;
    }
    delta = heap_exit > s_heap_enter ? heap_exit - s_heap_enter : s_heap_enter - heap_exit;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE((uint32_t)s_heap_tol, (uint32_t)delta,
                                             "free heap at exit differs from enter");
}

void app_main(void)
{
    printf("Running esp_compact_font unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_compact_font]", false);
    UNITY_END();
    printf("ESP_COMPACT_FONT_UT_DONE\n");
    fflush(stdout);
}
