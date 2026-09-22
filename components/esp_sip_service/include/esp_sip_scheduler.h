/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_sip_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Scheduler task name for the SIP session worker
 */
#define ESP_SIP_SCHED_SESSION_TASK  ESP_SIP_SERVICE_TASK_NAME

/* Where the work runs:
 *
 *   esp_sip_service_start()
 *     └─ esp_rtc, the protocol stack, owns every thread from here down
 *          ├─ signaling  REGISTER / INVITE / MESSAGE, and every
 *          │             esp_sip_service_event_cb_t callback
 *          ├─ uplink     pulls frames from the linked media provider
 *          └─ downlink   writes frames into the downlink media tracks
 *
 * This service starts no task of its own, so none of the above is ours to
 * name or size. Never block in the event callback: it runs on a stack thread
 * that is also driving signaling.
 *
 * esp_rtc_config_t carries no stack, priority or core field, so a scheduler
 * entry for ESP_SIP_SCHED_SESSION_TASK has no effect yet. This header stays a
 * placeholder until the stack exposes them. */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
