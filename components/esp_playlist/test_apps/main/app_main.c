/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_utils.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

#define TEST_MEMORY_LEAK_THRESHOLD  (1000)

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
    unity_run_menu();
}
