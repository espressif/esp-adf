/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * ESP-IDF entry point for the adf_event_hub multi-service example.  IDF has
 * already started the FreeRTOS scheduler by the time `app_main` runs, so we
 * simply spawn the shared example body in its own task and let the runtime
 * continue afterwards.
 */

#include "adf_event_hub.h"
#include "example_run.h"

static const char *TAG = "EH_EXAMPLE";

#define EXAMPLE_TASK_STACK     (8 * 1024)
#define EXAMPLE_TASK_PRIORITY  (tskIDLE_PRIORITY + 3)

static void example_task(void *arg)
{
    (void)arg;
    event_hub_example_run();
    ESP_LOGI(TAG, "Example finished. You can restart the chip to re-run.");
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(example_task, "eh_example",
                EXAMPLE_TASK_STACK, NULL,
                EXAMPLE_TASK_PRIORITY, NULL);
}
