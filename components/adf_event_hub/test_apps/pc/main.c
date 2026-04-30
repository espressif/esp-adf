/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * PC test runner for adf_event_hub.
 *
 * PC-side entry point. The actual test bodies live in ../common/ and are
 * shared with the IDF target build in ../main/.
 *
 * Starts the FreeRTOS POSIX scheduler, runs every TEST_CASE registered via
 * the test_compat.h constructor shim inside a single FreeRTOS task, then
 * exits the process with pass/fail status.  When invoked under CTest the
 * exit code is what drives the result; ctest --output-junit produces XML.
 */

#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "unity.h"
#include "test_compat.h"

test_entry_t g_test_registry[TEST_COMPAT_MAX_TESTS];
int g_test_count = 0;

void setUp(void)  { }
void tearDown(void)  { }

static void test_runner_task(void *arg)
{
    (void)arg;

    printf("\n========================================\n");
    printf("  ADF Event Hub - PC Unit Tests\n");
    printf("  %d test(s) registered\n", g_test_count);
    printf("========================================\n\n");

    UNITY_BEGIN();
    for (int i = 0; i < g_test_count; i++) {
        UnityDefaultTestRun(g_test_registry[i].func,
                            g_test_registry[i].name, 0);
    }
    int failures = UNITY_END();

    printf("\n========================================\n");
    if (failures == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TEST(S) FAILED\n", failures);
    }
    printf("========================================\n");

    exit(failures ? 1 : 0);
}

int main(void)
{
    xTaskCreate(test_runner_task, "test_runner",
                configMINIMAL_STACK_SIZE * 8, NULL,
                tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();

    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
