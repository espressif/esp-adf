/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>

#include "esp_extractor_defaults.h"
#include "media_lib_adapter.h"
#include "unity.h"

extern void esp_extractor_service_ut_force_link(void);

void app_main(void)
{
    media_lib_add_default_adapter();
    esp_extractor_register_default();
    /* Keep ut_*.c in the link graph so TEST_CASE constructors run. */
    esp_extractor_service_ut_force_link();

    printf("Running esp_extractor_service unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_extractor_service]", false);
    UNITY_END();
    printf("ESP_EXTRACTOR_SERVICE_UT_DONE\n");
    fflush(stdout);
    esp_extractor_unregister_default();
}
