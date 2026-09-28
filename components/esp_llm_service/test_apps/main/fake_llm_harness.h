/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#include "esp_llm_service.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define FAKE_LLM_DEFAULT_CAPS  ((esp_llm_capability_t)(                                   \
                                ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_TEXT_OUTPUT |        \
                                ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_AUDIO_OUTPUT |      \
                                ESP_LLM_CAP_VIDEO_INPUT | ESP_LLM_CAP_VIDEO_OUTPUT |      \
                                ESP_LLM_CAP_CANCEL_RESPONSE | ESP_LLM_CAP_FUNCTION_CALL | \
                                ESP_LLM_CAP_VISION | ESP_LLM_CAP_CUSTOM_DATA))

typedef struct {
    bool        fail_start;
    bool        emit_downlink_on_start;
    bool        emit_video_on_start;
    bool        use_custom_timestamps;
    int64_t     emit_pts;
    int64_t     emit_dts;
    int         destroy_fail_remaining;
    atomic_int  start_count;
    atomic_int  stop_count;
    atomic_int  destroy_count;
    atomic_int  send_frame_count;
    atomic_int  downlink_from_start;
} fake_llm_adapter_ctl_t;

typedef struct {
    esp_media_service_t           media;
    SemaphoreHandle_t             entered;
    SemaphoreHandle_t             release;
    bool                          hang;
    uint8_t                       payload[320];
    esp_media_track_info_t        track;
    esp_media_provider_ops_t      provider_ops;
    esp_media_provider_t          provider;
} fake_hang_src_t;

#define FAKE_LLM_ALL_STREAMS  ((1U << ESP_LLM_MEDIA_STREAM_COUNT) - 1U)

void fake_llm_adapter_ctl_init(fake_llm_adapter_ctl_t *ctl);

void fake_llm_adapter_desc_init(esp_llm_adapter_desc_t *desc, esp_llm_capability_t capabilities);

void fake_llm_cfg_enable_streams(esp_llm_service_cfg_t *cfg, uint32_t mask);

esp_err_t fake_hang_src_create(fake_hang_src_t **out_src);

esp_err_t fake_hang_src_create_video(fake_hang_src_t **out_src);

void fake_hang_src_destroy(fake_hang_src_t *src);

void fake_hang_src_set_hang(fake_hang_src_t *src, bool hang);

esp_err_t fake_hang_src_wait_entered(fake_hang_src_t *src, uint32_t timeout_ms);

void fake_hang_src_release(fake_hang_src_t *src);

esp_media_track_info_t fake_pcm_track_info(void);

esp_media_track_info_t fake_video_track_info(void);

esp_err_t fake_dummy_src_create(esp_media_dummy_service_t **out_src);

esp_err_t fake_dummy_sink_create(esp_media_dummy_service_t **out_sink);

bool fake_wait_atomic_ge(atomic_int *value, int min, uint32_t timeout_ms);

#ifdef CONFIG_ESP_LLM_SERVICE_TEST_HOOKS
void esp_llm_media_test_fail_uplink_start_call(int n);
void esp_llm_audio_ai_test_fail_next_detach(int count);
#endif

#ifdef __cplusplus
}
#endif  /* __cplusplus */
