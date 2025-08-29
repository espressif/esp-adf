/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * PC entry point for the adf_event_hub multi-service example.  Starts the
 * FreeRTOS POSIX/GCC simulator and runs the shared example body in a
 * FreeRTOS task, then exits cleanly when the simulation completes.  The
 * shared body and services live in ../common/.
 */

#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "example_run.h"

static void example_task(void *arg)
{
    (void)arg;
    event_hub_example_run();
    exit(0);
}

int main(void)
{
    xTaskCreate(example_task, "eh_example",
                configMINIMAL_STACK_SIZE * 4, NULL,
                tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
    fprintf(stderr, "ERROR: vTaskStartScheduler returned unexpectedly\n");
    return 1;
}
