/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * Run the adf_event_hub multi-service simulation to completion.
 *
 * Must be called from a FreeRTOS task (it calls vTaskDelay and relies on the
 * scheduler already running).  Blocks for ~SIM_DURATION_MS while the services
 * exercise each other, then tears everything down before returning.
 */
void event_hub_example_run(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
