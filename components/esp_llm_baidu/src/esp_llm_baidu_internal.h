/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_gmf_oal_thread.h"
#include "esp_llm_baidu.h"

#include "baidu_chat_agents_engine.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define BAIDU_JOIN_EVENT_BIT          BIT0
#define BAIDU_ENGINE_DONE_BIT         BIT1
#define BAIDU_CALLBACK_DONE_BIT       BIT2
#define BAIDU_RECONNECT_CANCEL_BIT    BIT3
#define BAIDU_RECONNECT_DONE_BIT      BIT4
#define BAIDU_ENGINE_START_EXTRA_MS   (5000U)
#define BAIDU_ENGINE_STACK_IN_EXT     (true)
#define BAIDU_ENGINE_CORE_ID          (0)
#define BAIDU_SNAPSHOT_LOCK_MS        50
#define BAIDU_INTERCOM_PCM_RATE       8000
#define BAIDU_TTS_PCM_RATE            16000
#define BAIDU_INTERCOM_AUDIO_CODEC   "pcmu"
#define BAIDU_FUNCTION_CALL_ID_LEN    64
#define BAIDU_INTERCOM_BROADCAST_ID  "0"

/**
 * @brief  Scene attributes derived from esp_llm_baidu_scene_type_t
 */
typedef struct {
    const char *scene_name;             /*!< SDK workflow / scene name */
    bool        enable_visual;          /*!< True when camera uplink is required */
    bool        enable_image_generate;  /*!< True when generated-image downlink is required */
} esp_llm_baidu_resolved_scene_t;

typedef struct esp_llm_baidu_ctx esp_llm_baidu_ctx_t;

typedef struct {
    esp_err_t (*start)(esp_llm_baidu_ctx_t *ctx);
    esp_err_t (*stop)(esp_llm_baidu_ctx_t *ctx);
    esp_err_t (*send_text)(esp_llm_baidu_ctx_t *ctx, const char *text);
    esp_err_t (*send_frame)(esp_llm_baidu_ctx_t *ctx, const esp_media_frame_t *frame);
    esp_err_t (*send_custom_data)(esp_llm_baidu_ctx_t *ctx, const char *data);
    esp_err_t (*cancel_response)(esp_llm_baidu_ctx_t *ctx);
} esp_llm_baidu_mode_ops_t;

/** @brief  Static capability superset for the Baidu adapter descriptor */
#define ESP_LLM_BAIDU_CAPABILITIES  ((esp_llm_capability_t)(                                 \
                                     ESP_LLM_CAP_AUDIO_INPUT | ESP_LLM_CAP_AUDIO_OUTPUT |    \
                                     ESP_LLM_CAP_TEXT_INPUT | ESP_LLM_CAP_CANCEL_RESPONSE |  \
                                     ESP_LLM_CAP_VISION | ESP_LLM_CAP_FUNCTION_CALL |        \
                                     ESP_LLM_CAP_CUSTOM_DATA |                               \
                                     ESP_LLM_CAP_VIDEO_INPUT | ESP_LLM_CAP_VIDEO_OUTPUT))

struct esp_llm_baidu_ctx {
    esp_llm_event_dispatch_cb_t       event_cb;
    void                             *event_ctx;
    esp_llm_downlink_frame_cb_t       downlink_frame;
    void                             *downlink_frame_ctx;
    esp_llm_baidu_cfg_t               cfg;
    char                             *config_strings;
    esp_llm_baidu_resolved_scene_t    resolved_scene;
    const esp_llm_baidu_mode_ops_t   *mode_ops;
    char                             *query_text;
    char                              intercom_user_id[20];
    char                              fc_id[BAIDU_FUNCTION_CALL_ID_LEN];
    BaiduChatAgentEngine             *engine;
    void                             *rtc_client;
    EventGroupHandle_t                evt_group;
    esp_gmf_oal_thread_t              engine_task;
    SemaphoreHandle_t                 engine_mux;
    SemaphoreHandle_t                 snap_mux;
    uint8_t                          *uplink_jpeg;
    size_t                            uplink_jpeg_size;
    volatile bool                     connected;
    volatile bool                     started;
    volatile bool                     engine_start_ok;
    volatile bool                     pending_snapshot;
    volatile bool                     sdk_inited;
    volatile bool                     stream_up;
    volatile bool                     media_ready;
    bool                              tts_stream_start;
    bool                              tts_session_open;
    volatile bool                     reconnect_pending;
    volatile bool                     reconnect_in_progress;
    volatile bool                     reconnect_cancel;
    uint32_t                          reconnect_attempts;
    uint32_t                          callback_refs;
    bool                              callbacks_enabled;
    esp_media_codec_fourcc_t          downlink_codec;
    uint32_t                          downlink_rate;
    uint8_t                           downlink_channel;
    uint8_t                           downlink_bits;
    uint16_t                          downlink_fps;
    uint64_t                          pcm_bytes;
    uint64_t                          video_frames;
    uint32_t                          uplink_send_log_count;
};

static inline const char *baidu_str_or(const char *value, const char *fallback)
{
    /* Pointer select only; no static buffer, so concurrent callers are safe. */
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

esp_err_t baidu_activate_ctx(esp_llm_baidu_ctx_t *ctx);
esp_llm_baidu_ctx_t *baidu_callback_enter(void);
void baidu_callback_exit(esp_llm_baidu_ctx_t *ctx);
void baidu_deactivate_ctx(esp_llm_baidu_ctx_t *ctx);

/**
 * @brief  Resolve the active adapter context for a public control-plane API
 *
 * @note  On success the engine mutex is held and the caller must call
 *         baidu_exit_control(). require_mode is checked against cfg.mode.
 *
 * @param[in]   service       Handle from esp_llm_baidu_create()
 * @param[in]   require_mode  Mode the caller requires
 * @param[out]  out_ctx       Active context on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or out_ctx is NULL
 *       - ESP_ERR_INVALID_STATE  If the service or backend session is not running
 *       - ESP_ERR_NOT_SUPPORTED  If the instance runs in another mode
 */
esp_err_t baidu_enter_control(esp_llm_service_t *service,
                              esp_llm_baidu_mode_t require_mode,
                              esp_llm_baidu_ctx_t **out_ctx);

/**
 * @brief  Release the engine mutex taken by baidu_enter_control()
 *
 * @param[in]  ctx  Context returned by baidu_enter_control()
 */
void baidu_exit_control(esp_llm_baidu_ctx_t *ctx);

void baidu_resolve_scene(esp_llm_baidu_scene_type_t scene_type,
                         esp_llm_baidu_resolved_scene_t *out);

bool baidu_scene_needs_camera(const esp_llm_baidu_resolved_scene_t *scene);

esp_err_t baidu_emit(esp_llm_baidu_ctx_t *ctx,
                     esp_llm_event_common_id_t common_id,
                     const char *backend_event_name,
                     esp_llm_event_payload_type_t payload_type,
                     esp_llm_role_t role,
                     const void *payload,
                     size_t payload_len);

esp_err_t baidu_feed_tts(esp_llm_baidu_ctx_t *ctx, const uint8_t *data, size_t len, bool eos);

void baidu_snapshot_reset_locked(esp_llm_baidu_ctx_t *ctx);
void baidu_snapshot_release(esp_llm_baidu_ctx_t *ctx);

bool baidu_engine_lock(esp_llm_baidu_ctx_t *ctx);
void baidu_engine_unlock(esp_llm_baidu_ctx_t *ctx);

void baidu_apply_downlink_audio_profile(esp_llm_baidu_ctx_t *ctx);

static inline bool baidu_stream_on(const esp_llm_baidu_cfg_t *cfg, esp_llm_media_stream_t stream)
{
    return cfg != NULL && stream < ESP_LLM_MEDIA_STREAM_COUNT &&
           cfg->service.streams[stream].enabled;
}

static inline const esp_media_track_info_t *baidu_stream_info(const esp_llm_baidu_cfg_t *cfg,
                                                              esp_llm_media_stream_t stream)
{
    return &cfg->service.streams[stream].info;
}

esp_err_t baidu_validate_streams(const esp_llm_baidu_cfg_t *cfg,
                                 const esp_llm_baidu_resolved_scene_t *scene);
const char *baidu_agent_sdk_uplink_codec(const esp_llm_baidu_cfg_t *cfg);
const char *baidu_agent_sdk_rtc_ac(const esp_llm_baidu_cfg_t *cfg);
bool baidu_rtc_ac_is_opus(const char *rtc_ac);
const char *baidu_sdk_video_codec(const esp_llm_baidu_cfg_t *cfg);

void baidu_agent_cancel_reconnect(esp_llm_baidu_ctx_t *ctx);

void baidu_ctx_cleanup(esp_llm_baidu_ctx_t *ctx, bool release_active);

void baidu_on_video_data(esp_llm_baidu_ctx_t *ctx, const uint8_t *data, size_t len,
                         int img_type, int width, int height);

extern const esp_llm_baidu_mode_ops_t baidu_agent_mode_ops;
extern const esp_llm_baidu_mode_ops_t baidu_intercom_mode_ops;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
