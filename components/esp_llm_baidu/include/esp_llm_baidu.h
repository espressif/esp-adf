/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "sdkconfig.h"

#include "esp_llm_baidu_config.h"
#include "esp_llm_baidu_defaults.h"
#include "esp_llm_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_LLM_ADAPTER_BAIDU                     "baidu_rtc"
#define ESP_LLM_BAIDU_DEFAULT_CONNECT_TIMEOUT_MS  (15000U)
#define ESP_LLM_BAIDU_DEFAULT_ENGINE_STACK        (20480U)
#define ESP_LLM_BAIDU_DEFAULT_ENGINE_PRIO         (12U)
#define ESP_LLM_BAIDU_DEFAULT_QUERY_BUF_SIZE      (4096U)
#define ESP_LLM_BAIDU_DEFAULT_RECONNECT_DELAY_MS  (3000U)
#define ESP_LLM_BAIDU_DEFAULT_VISION_EXPIRE_SEC   (10U)
#define ESP_LLM_BAIDU_AGENT_SAMPLE_RATE           (16000U)
#define ESP_LLM_BAIDU_INTERCOM_SAMPLE_RATE        (8000U)

#define ESP_LLM_BAIDU_AUDIO_STREAM_CFG(en, rate) \
    ESP_LLM_STREAM_CFG_AUDIO((en), ESP_LLM_BAIDU_DEFAULT_AUDIO_FOURCC, (rate), 16, 1, \
                             ESP_LLM_BAIDU_DEFAULT_AUDIO_BITRATE)

#define ESP_LLM_BAIDU_VIDEO_STREAM_CFG(en)                             \
    ESP_LLM_STREAM_CFG_VIDEO((en), ESP_LLM_BAIDU_DEFAULT_VIDEO_FOURCC, \
                             ESP_LLM_BAIDU_DEFAULT_VIDEO_WIDTH, ESP_LLM_BAIDU_DEFAULT_VIDEO_HEIGHT, 3)

/**
 * @brief  Operating mode selected at create time
 */
typedef enum {
    ESP_LLM_BAIDU_MODE_AGENT    = 0,  /*!< Chat-agent engine (ASR / TTS / vision) */
    ESP_LLM_BAIDU_MODE_INTERCOM = 1,  /*!< P2P intercom room */
} esp_llm_baidu_mode_t;

/**
 * @brief  Agent scene applied on media setup
 */
typedef enum {
    ESP_LLM_BAIDU_SCENE_VOICE_CHAT          = 0,  /*!< Voice-only chat */
    ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING = 1,  /*!< Camera uplink for image understanding */
    ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION    = 2,  /*!< Generated-image downlink */
    ESP_LLM_BAIDU_SCENE_MULTI_MODAL         = 3,  /*!< Camera uplink and generated-image downlink */
} esp_llm_baidu_scene_type_t;

/**
 * @brief  Camera uplink vision mode (maps to BRTC VISION_MODE_*)
 */
typedef enum {
    ESP_LLM_BAIDU_VISION_MODE_NONE   = -1,  /*!< Vision uplink disabled */
    ESP_LLM_BAIDU_VISION_MODE_IMAGE  = 0,   /*!< Single-image snapshot uplink */
    ESP_LLM_BAIDU_VISION_MODE_STREAM = 1,   /*!< Continuous camera uplink */
} esp_llm_baidu_vision_mode_t;

/**
 * @brief  User-query enhancement mode (maps to BRTC ENHANCE_QUERY_TYPE_*)
 */
typedef enum {
    ESP_LLM_BAIDU_ENHANCE_QUERY_NONE    = 0,  /*!< No text is inserted */
    ESP_LLM_BAIDU_ENHANCE_QUERY_PRE     = 1,  /*!< Insert before the user query */
    ESP_LLM_BAIDU_ENHANCE_QUERY_POST    = 2,  /*!< Insert after the user query */
    ESP_LLM_BAIDU_ENHANCE_QUERY_BOTHWAY = 3,  /*!< Insert before and after the user query */
} esp_llm_baidu_enhance_query_t;

/**
 * @brief  Agent platform access region
 */
typedef enum {
    ESP_LLM_BAIDU_REGION_BD_DEV   = 0,  /*!< Baidu development region */
    ESP_LLM_BAIDU_REGION_MAINLAND = 1,  /*!< Mainland China */
    ESP_LLM_BAIDU_REGION_EUROPE   = 2,  /*!< Europe */
    ESP_LLM_BAIDU_REGION_AMERICA  = 3,  /*!< Americas */
    ESP_LLM_BAIDU_REGION_OVERSEAS = 4,  /*!< Overseas */
    ESP_LLM_BAIDU_REGION_GLOBAL   = 5,  /*!< Global */
} esp_llm_baidu_region_t;

/**
 * @brief  Agent platform account credentials
 */
typedef struct {
    const char *app_id;       /*!< Application ID; required in agent mode */
    const char *user_id;      /*!< User ID; required in agent mode */
    const char *license_key;  /*!< License key; empty string is allowed */
    const char *server_url;   /*!< Agent platform URL; required in agent mode */
    const char *workflow;     /*!< Workflow name; default VoiceChat */
    const char *llm;          /*!< LLM model name; default ernie-4.0 */
    const char *lang;         /*!< Language code; default zh */
    long long   instance_id;  /*!< Agent instance ID from the Baidu console */
} esp_llm_baidu_account_cfg_t;

#define ESP_LLM_BAIDU_ACCOUNT_CFG_DEFAULT() {          \
    .app_id      = NULL,                               \
    .user_id     = NULL,                               \
    .license_key = "",                                 \
    .server_url  = NULL,                               \
    .workflow    = ESP_LLM_BAIDU_DEFAULT_WORKFLOW,     \
    .llm         = ESP_LLM_BAIDU_DEFAULT_LLM,          \
    .lang        = ESP_LLM_BAIDU_DEFAULT_LANG,         \
    .instance_id = 0,                                  \
}

/**
 * @brief  Chat-agent engine parameters
 */
typedef struct {
    const char             *config_json_raw;         /*!< Optional raw engine JSON; overrides generated config */
    const char             *cer;                     /*!< Certificate path used by the SDK */
    const char             *dfda;                    /*!< DFDA flag string; default false */
    int                     level_voice_interrupt;   /*!< Local voice-interrupt energy; passed to the SDK */
    bool                    verbose;                 /*!< True to enable verbose SDK logs */
    bool                    enable_local_agent;      /*!< True to run the local chat-agent engine */
    bool                    enable_voice_interrupt;  /*!< True to allow barge-in */
    bool                    disable_send_mute_data;  /*!< True to skip muted uplink frames */
    esp_llm_baidu_region_t  region;                  /*!< Agent platform region */
} esp_llm_baidu_engine_cfg_t;

#define ESP_LLM_BAIDU_ENGINE_CFG_DEFAULT() {                                      \
    .config_json_raw        = NULL,                                               \
    .cer                    = ESP_LLM_BAIDU_DEFAULT_CER,                          \
    .dfda                   = ESP_LLM_BAIDU_DEFAULT_DFDA,                         \
    .level_voice_interrupt  = CONFIG_ESP_LLM_BAIDU_VOICE_INTERRUPT_LEVEL,         \
    .verbose                = false,                                              \
    .enable_local_agent     = true,                                               \
    .enable_voice_interrupt = CONFIG_ESP_LLM_BAIDU_VOICE_INTERRUPT_ENABLE,        \
    .disable_send_mute_data = true,                                               \
    .region                 = ESP_LLM_BAIDU_REGION_MAINLAND,                      \
}

/**
 * @brief  Vision prompt templates
 */
typedef struct {
    const char *vision_prompt;      /*!< Vision prompt body; default object-recognition text */
    const char *vision_prompt_cmd;  /*!< Vision prompt command template; default SDK format */
    const char *vision_model_type;  /*!< Vision model type string; default 2 */
    uint16_t    query_buf_size;     /*!< Query buffer size in bytes; 0 uses 4096 */
} esp_llm_baidu_prompt_cfg_t;

#define ESP_LLM_BAIDU_PROMPT_CFG_DEFAULT() {                       \
    .vision_prompt      = NULL,                                    \
    .vision_prompt_cmd  = NULL,                                    \
    .vision_model_type  = ESP_LLM_BAIDU_DEFAULT_VISION_MODEL_TYPE, \
    .query_buf_size     = ESP_LLM_BAIDU_DEFAULT_QUERY_BUF_SIZE,    \
}

/**
 * @brief  Agent scene type; internal scene attributes are derived from scene_type
 */
typedef struct {
    esp_llm_baidu_scene_type_t  scene_type;  /*!< Scene selected at create */
} esp_llm_baidu_scene_cfg_t;

#define ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT()  \
    ((esp_llm_baidu_scene_cfg_t) {                    \
        .scene_type = ESP_LLM_BAIDU_SCENE_VOICE_CHAT, \
    })

#define ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING_CFG_DEFAULT()  \
    ((esp_llm_baidu_scene_cfg_t) {                             \
        .scene_type = ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING, \
    })

#define ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION_CFG_DEFAULT()  \
    ((esp_llm_baidu_scene_cfg_t) {                          \
        .scene_type = ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION, \
    })

#define ESP_LLM_BAIDU_SCENE_MULTI_MODAL_CFG_DEFAULT()  \
    ((esp_llm_baidu_scene_cfg_t) {                     \
        .scene_type = ESP_LLM_BAIDU_SCENE_MULTI_MODAL, \
    })

/**
 * @brief  Agent-mode configuration (valid when mode is AGENT)
 */
typedef struct {
    esp_llm_baidu_account_cfg_t  account;                /*!< Platform account credentials */
    esp_llm_baidu_engine_cfg_t   engine;                 /*!< Chat-agent engine parameters */
    esp_llm_baidu_prompt_cfg_t   prompt;                 /*!< Vision prompt templates */
    esp_llm_baidu_scene_cfg_t    scene;                  /*!< Scene applied on media setup */
    bool                         auto_reconnect;         /*!< True to restart the engine after disconnect */
    const char                  *greeting_text;          /*!< Optional TTS greeting after join */
    const char                  *wakeup_interrupt_cmd;   /*!< Command sent on wake-up barge-in */
    uint16_t                     engine_stack_size;      /*!< Engine task stack in bytes; 0 uses 20480 */
    uint8_t                      engine_priority;        /*!< Engine task priority; 0 uses 12 */
    uint16_t                     reconnect_delay_ms;     /*!< Delay before reconnect; 0 uses 3000 */
    uint16_t                     reconnect_max_attempts; /*!< Max reconnects; 0 means unlimited */
} esp_llm_baidu_agent_cfg_t;

#define ESP_LLM_BAIDU_AGENT_CFG_DEFAULT() {                                 \
    .account                = ESP_LLM_BAIDU_ACCOUNT_CFG_DEFAULT(),          \
    .engine                 = ESP_LLM_BAIDU_ENGINE_CFG_DEFAULT(),           \
    .prompt                 = ESP_LLM_BAIDU_PROMPT_CFG_DEFAULT(),           \
    .scene                  = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT(), \
    .auto_reconnect         = true,                                         \
    .greeting_text          = ESP_LLM_BAIDU_DEFAULT_GREETING,               \
    .wakeup_interrupt_cmd   = ESP_LLM_BAIDU_WAKEUP_INTERRUPT_CMD,           \
    .engine_stack_size      = ESP_LLM_BAIDU_DEFAULT_ENGINE_STACK,           \
    .engine_priority        = ESP_LLM_BAIDU_DEFAULT_ENGINE_PRIO,            \
    .reconnect_delay_ms     = ESP_LLM_BAIDU_DEFAULT_RECONNECT_DELAY_MS,     \
    .reconnect_max_attempts = 0,                                            \
}

/**
 * @brief  Intercom-mode configuration (valid when mode is INTERCOM)
 */
typedef struct {
    const char *app_id;        /*!< Application ID; required */
    const char *server_url;    /*!< Signaling server URL; required */
    const char *room_name;     /*!< Room name; required */
    const char *user_id;       /*!< User ID; generated when NULL */
    const char *display_name;  /*!< Display name shown in the room */
    const char *token;         /*!< Room token; default no_token */
    const char *cer;           /*!< Certificate path; required */
} esp_llm_baidu_intercom_cfg_t;

#define ESP_LLM_BAIDU_INTERCOM_CFG_DEFAULT() { \
    .app_id       = NULL,                      \
    .server_url   = NULL,                      \
    .room_name    = NULL,                      \
    .user_id      = NULL,                      \
    .display_name = "espcall",                 \
    .token        = "no_token",                \
    .cer          = NULL,                      \
}

/**
 * @brief  Vision uplink settings shared by agent and intercom
 */
typedef struct {
    esp_llm_baidu_vision_mode_t  vision_mode;        /*!< Camera uplink mode */
    uint16_t                     vision_expire_sec;  /*!< Snapshot expire time in seconds; 0 uses 10 */
} esp_llm_baidu_vision_cfg_t;

#define ESP_LLM_BAIDU_VISION_CFG_DEFAULT() {                      \
    .vision_mode       = ESP_LLM_BAIDU_VISION_MODE_IMAGE,         \
    .vision_expire_sec = ESP_LLM_BAIDU_DEFAULT_VISION_EXPIRE_SEC, \
}

/**
 * @brief  Baidu RTC adapter configuration
 *
 *         Fill backend fields (`mode`, `agent` / `intercom`, …) and optional
 *         LLM callbacks/audio-AI policy on `service`. The application owns the
 *         capture/player graph. String fields are copied during create.
 */
typedef struct {
    esp_llm_service_cfg_t       service;             /*!< Service name, streams, callbacks, and audio_ai_cfg */
    esp_llm_baidu_mode_t        mode;                /*!< AGENT or INTERCOM */
    esp_llm_baidu_vision_cfg_t  vision;              /*!< Shared vision uplink settings */
    uint16_t                    connect_timeout_ms;  /*!< Join timeout in milliseconds; 0 uses 15000 */
    union {
        esp_llm_baidu_agent_cfg_t     agent;         /*!< Valid when mode is AGENT */
        esp_llm_baidu_intercom_cfg_t  intercom;      /*!< Valid when mode is INTERCOM */
    };
} esp_llm_baidu_cfg_t;

#define ESP_LLM_BAIDU_CFG_AGENT_DEFAULT()                                                  \
    ((esp_llm_baidu_cfg_t) {                                                               \
        .mode = ESP_LLM_BAIDU_MODE_AGENT,                                                  \
        .vision = ESP_LLM_BAIDU_VISION_CFG_DEFAULT(),                                      \
        .connect_timeout_ms = ESP_LLM_BAIDU_DEFAULT_CONNECT_TIMEOUT_MS,                    \
        .agent = ESP_LLM_BAIDU_AGENT_CFG_DEFAULT(),                                        \
        .service = {                                                                       \
            .streams = {                                                                   \
                [ESP_LLM_MEDIA_STREAM_AUDIO_UP] =                                          \
                    ESP_LLM_BAIDU_AUDIO_STREAM_CFG(true, ESP_LLM_BAIDU_AGENT_SAMPLE_RATE), \
                [ESP_LLM_MEDIA_STREAM_AUDIO_DN] =                                          \
                    ESP_LLM_BAIDU_AUDIO_STREAM_CFG(true, ESP_LLM_BAIDU_AGENT_SAMPLE_RATE), \
                [ESP_LLM_MEDIA_STREAM_VIDEO_UP] = ESP_LLM_BAIDU_VIDEO_STREAM_CFG(false),   \
                [ESP_LLM_MEDIA_STREAM_VIDEO_DN] = ESP_LLM_BAIDU_VIDEO_STREAM_CFG(false),   \
            },                                                                             \
        },                                                                                 \
    })

#define ESP_LLM_BAIDU_CFG_INTERCOM_DEFAULT()                                                  \
    ((esp_llm_baidu_cfg_t) {                                                                  \
        .mode = ESP_LLM_BAIDU_MODE_INTERCOM,                                                  \
        .vision = ESP_LLM_BAIDU_VISION_CFG_DEFAULT(),                                         \
        .connect_timeout_ms = ESP_LLM_BAIDU_DEFAULT_CONNECT_TIMEOUT_MS,                       \
        .intercom = ESP_LLM_BAIDU_INTERCOM_CFG_DEFAULT(),                                     \
        .service = {                                                                          \
            .streams = {                                                                      \
                [ESP_LLM_MEDIA_STREAM_AUDIO_UP] =                                             \
                    ESP_LLM_BAIDU_AUDIO_STREAM_CFG(true, ESP_LLM_BAIDU_INTERCOM_SAMPLE_RATE), \
                [ESP_LLM_MEDIA_STREAM_AUDIO_DN] =                                             \
                    ESP_LLM_BAIDU_AUDIO_STREAM_CFG(true, ESP_LLM_BAIDU_INTERCOM_SAMPLE_RATE), \
                [ESP_LLM_MEDIA_STREAM_VIDEO_UP] = ESP_LLM_BAIDU_VIDEO_STREAM_CFG(true),       \
                [ESP_LLM_MEDIA_STREAM_VIDEO_DN] = ESP_LLM_BAIDU_VIDEO_STREAM_CFG(true),       \
            },                                                                                \
        },                                                                                    \
    })

/**
 * @brief  Borrowed BRTC backend handles
 *
 *         Handles are void * so this header stays free of SDK types. Include
 *         `baidu_chat_agents_engine.h` / `baidu_rtc_client.h` and cast:
 *         engine is `BaiduChatAgentEngine *`, rtc_client is the handle
 *         returned by `brtc_create_client()`.
 */
typedef struct {
    esp_llm_baidu_mode_t  mode;        /*!< Mode the instance was created with */
    void                 *engine;      /*!< Chat-agent engine; NULL outside agent mode */
    void                 *rtc_client;  /*!< RTC client; NULL outside intercom mode */
} esp_llm_baidu_backend_t;

/**
 * @brief  Create a Baidu adapter-backed LLM service instance
 *
 * @note  Only one Baidu adapter instance may exist at a time. The underlying
 *         BRTC C SDK callbacks do not carry a user context, so a second
 *         esp_llm_baidu_create() returns ESP_ERR_INVALID_STATE.
 *
 * @param[in]   cfg          Baidu adapter configuration (backend + `service`)
 * @param[out]  out_service  Created service handle on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If cfg or out_service is NULL
 *       - ESP_ERR_INVALID_STATE  If another Baidu adapter instance is active
 *       - ESP_ERR_NO_MEM         If memory allocation failed
 *       - Others                 Error returned by esp_llm_service_create()
 */
esp_err_t esp_llm_baidu_create(const esp_llm_baidu_cfg_t *cfg,
                               esp_llm_service_t **out_service);

/**
 * @brief  Speak UTF-8 text through the Baidu agent TTS path
 *
 *         This is not a chat query. Use it for local prompts such as volume
 *         confirmations. Intercom mode does not support TTS.
 *
 * @note  Control-plane API. Do not overlap with other control calls.
 *
 * @param[in]  service  Handle returned by esp_llm_baidu_create()
 * @param[in]  text     NUL-terminated UTF-8 text; caller retains ownership
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL or text is empty
 *       - ESP_ERR_INVALID_STATE  If the agent engine is not running
 *       - ESP_ERR_NOT_SUPPORTED  If the instance is not in agent mode
 */
esp_err_t esp_llm_baidu_send_text_to_tts(esp_llm_service_t *service, const char *text);

/**
 * @brief  Answer a FunctionCall raised by the agent
 *
 *         The adapter publishes each FunctionCall as a backend-raw event named
 *         `FUNCTION_CALL` whose payload is the SDK argument JSON. The call id
 *         is not part of that payload; the adapter caches the id of the most
 *         recent FunctionCall instead.
 *
 * @note  Control-plane API. Do not overlap with other control calls.
 *         Pass NULL for id to answer the most recent FunctionCall. The backend
 *         raises FunctionCalls one turn at a time, so the cached id matches the
 *         event the application is handling. Pass an explicit id when the
 *         answer is produced asynchronously and another call may have arrived.
 *
 * @param[in]  service  Handle returned by esp_llm_baidu_create()
 * @param[in]  id       Call id, or NULL to use the most recent one
 * @param[in]  result   NUL-terminated JSON result, for example `{"result":"ok"}`
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If result is empty
 *       - ESP_ERR_INVALID_STATE  If the agent engine is not running, or id is
 *                                NULL and no FunctionCall has been received
 *       - ESP_ERR_NOT_SUPPORTED  If the instance is not in agent mode
 */
esp_err_t esp_llm_baidu_send_function_call_result(esp_llm_service_t *service,
                                                  const char *id,
                                                  const char *result);

/**
 * @brief  Wrap every user query with extra context before it reaches the LLM
 *
 *         For example, with BOTHWAY, pre "我现在在成都" and post "用2个字回答",
 *         the query "今晚吃什么" reaches the model as
 *         "我现在在成都，今晚吃什么，用2个字回答".
 *
 * @note  Control-plane API. Do not overlap with other control calls.
 *         Call after the session is connected. NONE clears the enhancement.
 *
 * @param[in]  service     Handle returned by esp_llm_baidu_create()
 * @param[in]  type        Insertion mode
 * @param[in]  pre_query   Text inserted before the query; NULL means none
 * @param[in]  post_query  Text inserted after the query; NULL means none
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If type is out of range
 *       - ESP_ERR_INVALID_STATE  If the agent engine is not running
 *       - ESP_ERR_NOT_SUPPORTED  If the instance is not in agent mode
 */
esp_err_t esp_llm_baidu_set_enhance_query(esp_llm_service_t *service,
                                          esp_llm_baidu_enhance_query_t type,
                                          const char *pre_query,
                                          const char *post_query);

/**
 * @brief  Borrow the raw BRTC handles to call the SDK directly
 *
 *         Escape hatch for backend features this component does not wrap.
 *         Prefer the wrappers above and the generic esp_llm_service APIs.
 *
 * @note  Must be paired with esp_llm_baidu_backend_release() on the same
 *         thread, with only short work in between. The borrow holds the engine
 *         mutex and pins the adapter context, so while it is outstanding the
 *         caller must not start, stop, or destroy the service, must not call
 *         any other esp_llm_baidu_* or esp_llm_service_* API, and must not
 *         borrow again. A missing release deadlocks destroy. The handles are
 *         valid only until release; do not store them.
 *
 * @param[in]   service      Handle returned by esp_llm_baidu_create()
 * @param[out]  out_backend  Borrowed handles on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service or out_backend is NULL
 *       - ESP_ERR_INVALID_STATE  If the service is not running, the adapter is
 *                                inactive, or a borrow is already outstanding
 */
esp_err_t esp_llm_baidu_backend_acquire(esp_llm_service_t *service,
                                        esp_llm_baidu_backend_t *out_backend);

/**
 * @brief  Return the handles borrowed by esp_llm_baidu_backend_acquire()
 *
 * @param[in]  service  Handle passed to esp_llm_baidu_backend_acquire()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If service is NULL
 *       - ESP_ERR_INVALID_STATE  If no borrow is outstanding
 */
esp_err_t esp_llm_baidu_backend_release(esp_llm_service_t *service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
