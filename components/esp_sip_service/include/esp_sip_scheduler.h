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
 * @brief  Scheduler task names for the protocol-stack threads
 *
 *         start fills esp_rtc_config_t thread fields from
 *         esp_service_scheduler_get_thread_cfg() using these names
 *         and the service name from create. Direct esp_rtc users set
 *         the same fields on esp_rtc_config_t; stack_size 0 keeps the
 *         protocol defaults:
 *         sip_task          10K, prio 20, core 0
 *         listen_task        2K, prio 20, core 0
 *         _rtp_audio_recv    4K, prio 20, core 0
 *         _rtp_video_recv    3K, prio 15, core 1
 */
#define ESP_SIP_SCHED_SESSION_TASK     "sip_task"
#define ESP_SIP_SCHED_LISTEN_TASK      "listen_task"
#define ESP_SIP_SCHED_AUDIO_RECV_TASK  "_rtp_audio_recv"
#define ESP_SIP_SCHED_VIDEO_RECV_TASK  "_rtp_video_recv"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
