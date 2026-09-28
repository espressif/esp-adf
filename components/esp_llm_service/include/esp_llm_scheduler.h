/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Default service name for esp_llm_service_create()
 *
 *         Uplink consume tasks request scheduler overrides under this name when
 *         cfg->name is not set.
 */
#define ESP_LLM_SERVICE_NAME  "llm"

/**
 * @brief  Scheduler thread names for esp_llm_service
 *
 *         Tune stack / priority / core through `esp_service_scheduler_set_cb()`.
 *         Requests use `cfg.name` (`ESP_LLM_SERVICE_NAME` is `"llm"`).
 *
 *         Uplink consume flow (data moves left to right):
 *
 *             audio capture --> AUDIO_UP --> aud_up --> adapter send_frame
 *             video capture --> VIDEO_UP --> vid_up --> adapter send_frame
 *
 *         Downlink has no LLM-owned consume task. Frames are written from the
 *         adapter callback into a shared track manager, then pulled by the
 *         linked player.
 *
 *         Name mapping:
 *           aud_up - audio uplink consume task
 *           vid_up - video uplink consume task
 *
 *         Default: 8192-byte PSRAM stack, priority 5, core 0.
 */

/** Audio uplink consume thread (capture provider to adapter send_frame) */
#define ESP_LLM_SERVICE_AUDIO_UP_TASK_NAME  "aud_up"

/** Video uplink consume thread (camera provider to adapter send_frame) */
#define ESP_LLM_SERVICE_VIDEO_UP_TASK_NAME  "vid_up"

#ifdef __cplusplus
}
#endif  /* __cplusplus */
