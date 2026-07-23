/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "unity.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD  512

extern void esp_capture_service_ut_force_link(void);

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
    esp_capture_service_ut_force_link();

    printf("Running esp_capture_service unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_capture_service]", false);
    UNITY_END();
    printf("ESP_CAPTURE_SERVICE_UT_DONE\n");
    fflush(stdout);
}
