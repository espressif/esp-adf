/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "unity.h"
#include "unity_test_utils.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD  2048

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
    printf("Running esp_llm_tongyi unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_llm_tongyi]", false);
    UNITY_END();
    printf("esp_llm_tongyi tests complete\n");
}
