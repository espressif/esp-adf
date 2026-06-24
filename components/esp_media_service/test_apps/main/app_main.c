/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD  512
// #define TEST_RUN_WITH_MENU

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
    printf("Running esp_media_service unit tests\n");
#ifdef TEST_RUN_WITH_MENU
    unity_run_menu();
#else
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_media_service]", false);
    UNITY_END();
#endif
    printf("esp_media_service tests complete\n");
}
