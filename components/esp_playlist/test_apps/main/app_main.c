/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include "unity.h"

void app_main(void)
{
    printf("Running esp_playlist unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_playlist]", false);
    UNITY_END();
}
