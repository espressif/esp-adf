/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "unity.h"
#include "unity_test_utils_memory.h"

#define TEST_MEMORY_LEAK_THRESHOLD           512
#define TEST_AI_AUDIO_MEMORY_LEAK_THRESHOLD  6144

static int s_memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;

extern void esp_audio_capture_service_ut_force_link(void);
extern void esp_ai_audio_ut_force_link(void);

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(s_memory_leak_threshold);
}

void app_main(void)
{
    esp_audio_capture_service_ut_force_link();
    esp_ai_audio_ut_force_link();

    printf("Running esp_audio_capture_service unit tests\n");
    UNITY_BEGIN();
    s_memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;
    unity_run_tests_by_tag("[esp_audio_capture_service]", false);
    s_memory_leak_threshold = TEST_AI_AUDIO_MEMORY_LEAK_THRESHOLD;
    unity_run_tests_by_tag("[ai_audio]", false);
    UNITY_END();
    printf("ESP_AUDIO_CAPTURE_SERVICE_UT_DONE\n");
    fflush(stdout);
}
