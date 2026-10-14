/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_TONGYI_DEFAULT_WS_BASE_URL           "wss://dashscope.aliyuncs.com/api-ws/v1/inference"
#define ESP_TONGYI_DEFAULT_KEEP_ALIVE_SEC        (10)
#define ESP_TONGYI_DEFAULT_TASK_STACK            (4096)
#define ESP_TONGYI_DEFAULT_TASK_PRIO             (5)
#define ESP_TONGYI_DEFAULT_TASK_CORE             (-1)
#define ESP_TONGYI_DEFAULT_SAMPLE_RATE           (16000)
#define ESP_TONGYI_DEFAULT_FRAME_SIZE_MS         (60)
#define ESP_TONGYI_DEFAULT_VOLUME                (80)
#define ESP_TONGYI_DEFAULT_BIT_RATE              (32)
#define ESP_TONGYI_DEFAULT_TRANSMIT_RATE_LIMIT   (4096)
#define ESP_TONGYI_DEFAULT_VOICE                 "longanhuan"
#define ESP_TONGYI_DEFAULT_AUDIO_FORMAT          "raw-opus"
#define ESP_TONGYI_DEFAULT_UPSTREAM_TYPE         "AudioOnly"
#define ESP_TONGYI_UPSTREAM_TYPE_AUDIO_VIDEO     "AudioAndVideo"
#define ESP_TONGYI_DEFAULT_UPSTREAM_MODE         "duplex"
#define ESP_TONGYI_DEFAULT_RECONNECT_DELAY_MS    (1000)
#define ESP_TONGYI_DEFAULT_BUFFER_SIZE           (20480)
#define ESP_TONGYI_DEFAULT_SEND_TIMEOUT_MS       (3000)
#define ESP_TONGYI_DEFAULT_CONNECT_TIMEOUT_MS    (5000)
#define ESP_TONGYI_DEFAULT_NETWORK_TIMEOUT_MS    (5000)
#define ESP_TONGYI_DEFAULT_RECONNECT_TIMEOUT_MS  (1000)
#define ESP_TONGYI_DEFAULT_MAX_JSON_BYTES        (65536)
#define ESP_TONGYI_DEFAULT_MAX_BINARY_BYTES      (65536)
#define ESP_TONGYI_DEFAULT_MAX_IMAGE_BYTES       (262144)

/**
 * @brief  Tongyi session handle
 */
typedef struct esp_tongyi *esp_tongyi_handle_t;

/**
 * @brief  Session lifecycle state
 */
typedef enum {
    ESP_TONGYI_STATE_IDLE           = 0,  /*!< Created, not started */
    ESP_TONGYI_STATE_CONNECTING     = 1,  /*!< WebSocket connect in progress */
    ESP_TONGYI_STATE_STARTING       = 2,  /*!< Waiting for server Started and dialog_id */
    ESP_TONGYI_STATE_READY          = 3,  /*!< Media and directives may be sent */
    ESP_TONGYI_STATE_STOPPING       = 4,  /*!< Stop in progress */
    ESP_TONGYI_STATE_RECONNECT_WAIT = 5,  /*!< Waiting to reconnect */
    ESP_TONGYI_STATE_ERROR          = 6,  /*!< Failed; start or reconnect may recover */
} esp_tongyi_state_t;

/**
 * @brief  Server-reported dialog state
 */
typedef enum {
    ESP_TONGYI_DIALOG_STATE_IDLE       = 0,  /*!< Idle or stopped */
    ESP_TONGYI_DIALOG_STATE_LISTENING  = 1,  /*!< Listening for user input */
    ESP_TONGYI_DIALOG_STATE_THINKING   = 2,  /*!< Server is thinking */
    ESP_TONGYI_DIALOG_STATE_RESPONDING = 3,  /*!< Server is responding */
} esp_tongyi_dialog_state_t;

/**
 * @brief  Speech action sent as a DashScope continue-task directive
 */
typedef enum {
    ESP_TONGYI_SPEECH_ACTION_SEND                     = 0,  /*!< SendSpeech */
    ESP_TONGYI_SPEECH_ACTION_STOP                     = 1,  /*!< StopSpeech */
    ESP_TONGYI_SPEECH_ACTION_CANCEL                   = 2,  /*!< CancelSpeech */
    ESP_TONGYI_SPEECH_ACTION_REQUEST_TO_SPEAK         = 3,  /*!< RequestToSpeak, throttled to 1 Hz */
    ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_STARTED = 4,  /*!< LocalRespondingStarted */
    ESP_TONGYI_SPEECH_ACTION_LOCAL_RESPONDING_ENDED   = 5,  /*!< LocalRespondingEnded */
} esp_tongyi_speech_action_t;

/**
 * @brief  Text request kind for #esp_tongyi_send_text
 */
typedef enum {
    ESP_TONGYI_TEXT_PROMPT     = 0,  /*!< RequestToRespond type=prompt */
    ESP_TONGYI_TEXT_TRANSCRIPT = 1,  /*!< RequestToRespond type=transcript */
} esp_tongyi_text_kind_t;

/**
 * @brief  Image request kind for #esp_tongyi_send_image
 */
typedef enum {
    ESP_TONGYI_IMAGE_REQUEST_RESPONSE = 0,  /*!< RequestToRespond with prompt and image */
    ESP_TONGYI_IMAGE_UPDATE_INFO      = 1,  /*!< UpdateInfo with image only */
} esp_tongyi_image_action_t;

/**
 * @brief  Callback event types posted by the SDK
 *
 *         Payload pointers are only valid during callback execution.
 *         Callbacks run on the WebSocket or control task. Do not call blocking
 *         send APIs from a callback; use #esp_tongyi_post_directive instead.
 */
typedef enum {
    ESP_TONGYI_EVENT_CONNECTED          =  1,  /*!< WebSocket connected */
    ESP_TONGYI_EVENT_DISCONNECTED       =  2,  /*!< WebSocket disconnected */
    ESP_TONGYI_EVENT_SESSION_READY      =  3,  /*!< Server Started; dialog_id is available */
    ESP_TONGYI_EVENT_AUDIO              =  4,  /*!< Downlink audio bytes in event_data->audio */
    ESP_TONGYI_EVENT_DIALOG_STATE       =  5,  /*!< Dialog state in event_data->dialog_state */
    ESP_TONGYI_EVENT_SPEECH_STARTED     =  6,  /*!< No extra payload */
    ESP_TONGYI_EVENT_SPEECH_CONTENT     =  7,  /*!< ASR text in event_data->text */
    ESP_TONGYI_EVENT_RESPONDING_STARTED =  8,  /*!< No extra payload */
    ESP_TONGYI_EVENT_RESPONDING_CONTENT =  9,  /*!< Reply JSON in event_data->text */
    ESP_TONGYI_EVENT_RESPONDING_ENDED   = 10,  /*!< No extra payload */
    ESP_TONGYI_EVENT_ERROR              = 11,  /*!< Error JSON in event_data->text */
} esp_tongyi_event_t;

/**
 * @brief  Payload passed to #esp_tongyi_event_cb_t
 *
 *         Only the fields listed for @c event are set; others are zero or NULL.
 *         @c event_data itself is NULL for events without payload. Pointers are
 *         read-only and must not be retained after the callback returns.
 */
typedef struct {
    const uint8_t             *audio;         /*!< Downlink audio bytes; set only for ESP_TONGYI_EVENT_AUDIO */
    size_t                     audio_len;     /*!< Length of @p audio in bytes */
    const char                *text;          /*!< NUL-terminated text: ASR text for SPEECH_CONTENT (may be NULL),
                                                   reply JSON for RESPONDING_CONTENT, error JSON or name for ERROR;
                                                   NULL for other events */
    size_t                     text_len;      /*!< Length of @p text without the terminator; 0 when @p text is NULL */
    esp_tongyi_dialog_state_t  dialog_state;  /*!< Dialog state; set only for ESP_TONGYI_EVENT_DIALOG_STATE */
} esp_tongyi_event_data_t;

/**
 * @brief  Tongyi event callback
 *
 * @param[in]  handle      Tongyi session handle
 * @param[in]  event       Event type
 * @param[in]  event_data  Event payload; valid only during this callback, may be NULL
 * @param[in]  user_ctx    User context from #esp_tongyi_cfg_t
 */
typedef void (*esp_tongyi_event_cb_t)(esp_tongyi_handle_t handle,
                                      esp_tongyi_event_t event,
                                      const esp_tongyi_event_data_t *event_data,
                                      void *user_ctx);

/**
 * @brief  WebSocket worker task
 */
typedef struct {
    int  stack_size;  /*!< WebSocket task stack size in bytes; 0 uses 4096 */
    int  priority;    /*!< WebSocket task priority; 0 uses 5 */
    int  core_id;     /*!< CPU core to pin; negative means no affinity */
} esp_tongyi_task_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_task_cfg_t
 */
#define ESP_TONGYI_TASK_CFG_DEFAULT() {                     \
    .stack_size = ESP_TONGYI_DEFAULT_TASK_STACK,            \
    .priority   = ESP_TONGYI_DEFAULT_TASK_PRIO,             \
    .core_id    = ESP_TONGYI_DEFAULT_TASK_CORE,             \
}

/**
 * @brief  Transport and timeout settings
 *
 *         Timeout and buffer fields of 0 fall back to the corresponding Kconfig value.
 */
typedef struct {
    const char            *uri;                   /*!< WebSocket URI (`ws://` or `wss://`) */
    int                    buffer_size;           /*!< WebSocket RX and TX buffer size in bytes */
    int                    send_timeout_ms;       /*!< Send timeout in milliseconds */
    int                    connect_timeout_ms;    /*!< Wait for connected and Started in milliseconds */
    int                    network_timeout_ms;    /*!< Network I/O timeout in milliseconds */
    int                    reconnect_timeout_ms;  /*!< Reserved WebSocket client reconnect timeout */
    esp_tongyi_task_cfg_t  task;                  /*!< WebSocket worker task */
} esp_tongyi_transport_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_transport_cfg_t
 */
#define ESP_TONGYI_TRANSPORT_CFG_DEFAULT() {                              \
    .uri                  = ESP_TONGYI_DEFAULT_WS_BASE_URL,               \
    .buffer_size          = ESP_TONGYI_DEFAULT_BUFFER_SIZE,               \
    .send_timeout_ms      = ESP_TONGYI_DEFAULT_SEND_TIMEOUT_MS,           \
    .connect_timeout_ms   = ESP_TONGYI_DEFAULT_CONNECT_TIMEOUT_MS,        \
    .network_timeout_ms   = ESP_TONGYI_DEFAULT_NETWORK_TIMEOUT_MS,        \
    .reconnect_timeout_ms = ESP_TONGYI_DEFAULT_RECONNECT_TIMEOUT_MS,      \
    .task                 = ESP_TONGYI_TASK_CFG_DEFAULT(),                \
}

/**
 * @brief  Session-level reconnect after disconnect, error, or task-failed
 */
typedef struct {
    bool      enabled;   /*!< Enable automatic session restart */
    uint32_t  delay_ms;  /*!< Delay before reconnect in milliseconds; 0 uses 1000 ms */
} esp_tongyi_reconnect_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_reconnect_cfg_t
 */
#define ESP_TONGYI_RECONNECT_CFG_DEFAULT() {                 \
    .enabled  = true,                                        \
    .delay_ms = ESP_TONGYI_DEFAULT_RECONNECT_DELAY_MS,       \
}

/**
 * @brief  Resource limits
 *
 *         Fields of 0 fall back to Kconfig defaults.
 */
typedef struct {
    size_t  max_json_bytes;    /*!< Maximum assembled JSON message in bytes */
    size_t  max_binary_bytes;  /*!< Maximum assembled binary or audio message in bytes */
    size_t  max_image_bytes;   /*!< Maximum raw image accepted by #esp_tongyi_send_image */
} esp_tongyi_limits_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_limits_cfg_t
 */
#define ESP_TONGYI_LIMITS_CFG_DEFAULT() {                    \
    .max_json_bytes   = ESP_TONGYI_DEFAULT_MAX_JSON_BYTES,   \
    .max_binary_bytes = ESP_TONGYI_DEFAULT_MAX_BINARY_BYTES, \
    .max_image_bytes  = ESP_TONGYI_DEFAULT_MAX_IMAGE_BYTES,  \
}

/**
 * @brief  Start-payload upstream parameters
 */
typedef struct {
    const char  *type;          /*!< Upstream type, for example "AudioOnly" */
    const char  *mode;          /*!< Interaction mode, for example "duplex" */
    const char  *audio_format;  /*!< Uplink codec, for example "raw-opus" */
    int          sample_rate;   /*!< Uplink sample rate in Hz */
} esp_tongyi_upstream_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_upstream_cfg_t
 */
#define ESP_TONGYI_UPSTREAM_CFG_DEFAULT() {                  \
    .type         = ESP_TONGYI_DEFAULT_UPSTREAM_TYPE,        \
    .mode         = ESP_TONGYI_DEFAULT_UPSTREAM_MODE,        \
    .audio_format = ESP_TONGYI_DEFAULT_AUDIO_FORMAT,         \
    .sample_rate  = ESP_TONGYI_DEFAULT_SAMPLE_RATE,          \
}

/**
 * @brief  Start-payload downstream parameters
 */
typedef struct {
    const char  *voice;                /*!< TTS voice name */
    int          sample_rate;          /*!< Downlink sample rate in Hz */
    const char  *audio_format;         /*!< Downlink codec, for example "raw-opus" */
    int          frame_size;           /*!< Frame size in milliseconds */
    int          volume;               /*!< Output volume from 0 through 100 */
    int          bit_rate;             /*!< Bitrate in kbps */
    int          transmit_rate_limit;  /*!< Transmit rate limit in bytes */
} esp_tongyi_downstream_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_downstream_cfg_t
 */
#define ESP_TONGYI_DOWNSTREAM_CFG_DEFAULT() {                            \
    .voice               = ESP_TONGYI_DEFAULT_VOICE,                     \
    .sample_rate         = ESP_TONGYI_DEFAULT_SAMPLE_RATE,               \
    .audio_format        = ESP_TONGYI_DEFAULT_AUDIO_FORMAT,              \
    .frame_size          = ESP_TONGYI_DEFAULT_FRAME_SIZE_MS,             \
    .volume              = ESP_TONGYI_DEFAULT_VOLUME,                    \
    .bit_rate            = ESP_TONGYI_DEFAULT_BIT_RATE,                  \
    .transmit_rate_limit = ESP_TONGYI_DEFAULT_TRANSMIT_RATE_LIMIT,       \
}

/**
 * @brief  Start-payload client_info
 *
 *         The Start JSON always includes `user_id`. Empty `user_id` falls back
 *         to `task_id`. Empty `device_uuid` is omitted.
 */
typedef struct {
    const char  *user_id;      /*!< Application user identifier */
    const char  *device_uuid;  /*!< Device UUID placed under client_info.device.uuid */
} esp_tongyi_client_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_client_cfg_t
 */
#define ESP_TONGYI_CLIENT_CFG_DEFAULT() {                    \
    .user_id     = NULL,                                     \
    .device_uuid = NULL,                                     \
}

/**
 * @brief  Image upload request
 *
 *         @p data is caller-retained and is not used after the call returns.
 */
typedef struct {
    esp_tongyi_image_action_t  action;    /*!< RequestToRespond or UpdateInfo */
    const uint8_t             *data;      /*!< Encoded image bytes, for example JPEG */
    size_t                     data_len;  /*!< Size of @p data in bytes */
    const char                *prompt;    /*!< Required for #ESP_TONGYI_IMAGE_REQUEST_RESPONSE */
} esp_tongyi_image_req_t;

/**
 * @brief  Create-time configuration
 *
 *         Input only. String fields are deep-copied by #esp_tongyi_create().
 *         The event callback is fixed at create time.
 *
 *         If @p task_id is NULL or empty, a UUID v4 is generated internally.
 */
typedef struct {
    const char                  *api_key;                /*!< DashScope API key, required */
    const char                  *workspace_id;           /*!< Tongyi workspace ID, required */
    const char                  *app_id;                 /*!< Tongyi application ID, required */
    const char                  *task_id;                /*!< Task ID; generated if NULL */
    const char                  *dialog_id;              /*!< Optional dialog ID to resume on first start */
    const char                  *biz_params_json;        /*!< Optional extra JSON object */
    uint32_t                     keep_alive_time_sec;    /*!< Heartbeat interval in seconds; 0 disables */
    bool                         disconnect_after_stop;  /*!< Close WebSocket inside #esp_tongyi_stop() */
    esp_tongyi_event_cb_t        event_cb;               /*!< Event callback; may be NULL */
    void                        *event_ctx;              /*!< User context passed to @p event_cb */
    esp_tongyi_transport_cfg_t   transport;              /*!< URI, buffers, timeouts, and WebSocket task */
    esp_tongyi_reconnect_cfg_t   reconnect;              /*!< Automatic session restart */
    esp_tongyi_limits_cfg_t      limits;                 /*!< RX and image limits */
    esp_tongyi_upstream_cfg_t    upstream;               /*!< Start payload upstream */
    esp_tongyi_downstream_cfg_t  downstream;             /*!< Start payload downstream */
    esp_tongyi_client_cfg_t      client;                 /*!< Start payload client_info */
} esp_tongyi_cfg_t;

/**
 * @brief  Default initializer for #esp_tongyi_cfg_t
 */
#define ESP_TONGYI_CFG_DEFAULT() {                                        \
    .api_key               = NULL,                                        \
    .workspace_id          = NULL,                                        \
    .app_id                = NULL,                                        \
    .task_id               = NULL,                                        \
    .dialog_id             = NULL,                                        \
    .biz_params_json       = NULL,                                        \
    .keep_alive_time_sec   = ESP_TONGYI_DEFAULT_KEEP_ALIVE_SEC,           \
    .disconnect_after_stop = true,                                        \
    .event_cb              = NULL,                                        \
    .event_ctx             = NULL,                                        \
    .transport             = ESP_TONGYI_TRANSPORT_CFG_DEFAULT(),          \
    .reconnect             = ESP_TONGYI_RECONNECT_CFG_DEFAULT(),          \
    .limits                = ESP_TONGYI_LIMITS_CFG_DEFAULT(),             \
    .upstream              = ESP_TONGYI_UPSTREAM_CFG_DEFAULT(),           \
    .downstream            = ESP_TONGYI_DOWNSTREAM_CFG_DEFAULT(),         \
    .client                = ESP_TONGYI_CLIENT_CFG_DEFAULT(),             \
}

/**
 * @brief  Create a Tongyi session
 *
 *         Validates configuration, copies strings, and prepares the WebSocket
 *         client. Does not connect.
 *
 * @note  This function is not thread-safe and is not ISR-safe.
 *
 * @param[in]   cfg         Configuration; caller retains ownership
 * @param[out]  out_handle  Receives the created handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If an argument or configuration field is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - Others               Error returned while opening transport or control task
 */
esp_err_t esp_tongyi_create(const esp_tongyi_cfg_t *cfg, esp_tongyi_handle_t *out_handle);

/**
 * @brief  Destroy a Tongyi session
 *
 *         Stops the session if needed, waits for callbacks to finish, then
 *         releases all resources. @p handle is invalid after this call.
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p handle is NULL
 */
esp_err_t esp_tongyi_destroy(esp_tongyi_handle_t handle);

/**
 * @brief  Connect and start the dialog
 *
 *         Blocks until the server returns `Started` with a `dialog_id`, or
 *         until connect or start timeout. On success the session is READY and
 *         media may be sent. Idempotent if already READY.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle is NULL
 *       - ESP_ERR_INVALID_STATE  If the session is closing
 *       - ESP_ERR_NO_MEM         If the Start JSON cannot be built
 *       - ESP_ERR_TIMEOUT        If connect or Started timed out
 *       - Others                 Error returned by WebSocket start or send
 */
esp_err_t esp_tongyi_start(esp_tongyi_handle_t handle);

/**
 * @brief  Stop the dialog
 *
 *         Idempotent. Sends `Stop` when a dialog is active. Clears the current
 *         dialog ID. WebSocket is closed when `disconnect_after_stop` is true.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 *
 * @return
 *       - ESP_OK               On success, including when already stopped
 *       - ESP_ERR_INVALID_ARG  If @p handle is NULL
 */
esp_err_t esp_tongyi_stop(esp_tongyi_handle_t handle);

/**
 * @brief  Send uplink audio
 *
 *         Caller retains @p audio_data. Requires READY. Thread-safe with other
 *         send APIs.
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle          Session handle returned by #esp_tongyi_create()
 * @param[in]  audio_data      Encoded uplink audio bytes
 * @param[in]  audio_data_len  Size of @p audio_data in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle or @p audio_data is NULL
 *       - ESP_ERR_INVALID_STATE  If the session is not READY
 *       - ESP_ERR_INVALID_SIZE   If @p audio_data_len cannot be sent
 *       - ESP_FAIL               If the WebSocket send fails
 */
esp_err_t esp_tongyi_send_audio(esp_tongyi_handle_t handle,
                                const uint8_t *audio_data,
                                size_t audio_data_len);

/**
 * @brief  Send a continue-task directive
 *
 *         Blocks until the JSON is sent. Must not be called from the event callback.
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 * @param[in]  action  Speech action to send
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle is NULL or @p action is invalid
 *       - ESP_ERR_INVALID_STATE  If the session is not READY
 *       - ESP_ERR_NO_MEM         If the JSON cannot be built
 *       - Others                 Error returned by WebSocket send
 */
esp_err_t esp_tongyi_send_directive(esp_tongyi_handle_t handle,
                                    esp_tongyi_speech_action_t action);

/**
 * @brief  Queue a continue-task directive
 *
 *         Non-blocking. Safe to call from the event callback.
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 * @param[in]  action  Speech action to queue
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle is NULL or @p action is invalid
 *       - ESP_ERR_INVALID_STATE  If the control queue is unavailable
 *       - ESP_ERR_TIMEOUT        If the control queue is full
 */
esp_err_t esp_tongyi_post_directive(esp_tongyi_handle_t handle,
                                    esp_tongyi_speech_action_t action);

/**
 * @brief  Send RequestToRespond text
 *
 *         Caller retains @p text.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 * @param[in]  kind    Prompt or transcript
 * @param[in]  text    NUL-terminated request text
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle or @p text is NULL
 *       - ESP_ERR_INVALID_STATE  If the session is not READY
 *       - ESP_ERR_NO_MEM         If the JSON cannot be built
 *       - Others                 Error returned by WebSocket send
 */
esp_err_t esp_tongyi_send_text(esp_tongyi_handle_t handle,
                               esp_tongyi_text_kind_t kind,
                               const char *text);

/**
 * @brief  Open LiveAI `voicechat_video_channel` (探索视界)
 *
 *         Required before the cloud vision agent will accept images.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 */
esp_err_t esp_tongyi_connect_video_channel(esp_tongyi_handle_t handle);

/**
 * @brief  Send an image as RequestToRespond or UpdateInfo
 *
 *         Caller retains @p req and @p req->data.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 * @param[in]  req     Image request; caller retains ownership
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If an argument is invalid or prompt is missing
 *       - ESP_ERR_INVALID_STATE  If the session is not READY
 *       - ESP_ERR_INVALID_SIZE   If the image exceeds the configured limit
 *       - ESP_ERR_NO_MEM         If encoding or JSON construction fails
 *       - ESP_FAIL               If base64 encoding fails
 *       - Others                 Error returned by WebSocket send
 */
esp_err_t esp_tongyi_send_image(esp_tongyi_handle_t handle,
                                const esp_tongyi_image_req_t *req);

/**
 * @brief  Send a caller-built continue-task payload JSON object
 *
 *         @p payload_json must be a JSON object string. The caller retains it
 *         for the duration of the call. The SDK parses a copy internally.
 *
 * @note  This function is not ISR-safe. Do not call it from the event callback.
 *
 * @param[in]  handle        Session handle returned by #esp_tongyi_create()
 * @param[in]  payload_json  JSON object string; caller retains ownership
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle is NULL or @p payload_json is empty
 *       - ESP_ERR_INVALID_STATE  If the session is not READY
 *       - ESP_ERR_NO_MEM         If the JSON cannot be built
 *       - Others                 Error returned by WebSocket send
 */
esp_err_t esp_tongyi_send_payload_json(esp_tongyi_handle_t handle,
                                       const char *payload_json);

/**
 * @brief  Copy the current task ID into a caller buffer
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   handle   Session handle returned by #esp_tongyi_create()
 * @param[out]  buf      Caller-provided buffer
 * @param[in]   buf_len  Size of @p buf in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle or @p buf is NULL, or @p buf_len is 0
 *       - ESP_ERR_INVALID_STATE  If the task ID is unavailable
 *       - ESP_ERR_INVALID_SIZE   If @p buf is too small
 */
esp_err_t esp_tongyi_get_task_id(esp_tongyi_handle_t handle, char *buf, size_t buf_len);

/**
 * @brief  Copy the current dialog ID into a caller buffer
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   handle   Session handle returned by #esp_tongyi_create()
 * @param[out]  buf      Caller-provided buffer
 * @param[in]   buf_len  Size of @p buf in bytes
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p handle or @p buf is NULL, or @p buf_len is 0
 *       - ESP_ERR_INVALID_STATE  If the dialog ID is unavailable
 *       - ESP_ERR_INVALID_SIZE   If @p buf is too small
 */
esp_err_t esp_tongyi_get_dialog_id(esp_tongyi_handle_t handle, char *buf, size_t buf_len);

/**
 * @brief  Query whether the WebSocket is connected
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   handle         Session handle returned by #esp_tongyi_create()
 * @param[out]  out_connected  Set true if the WebSocket is connected
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p handle or @p out_connected is NULL
 */
esp_err_t esp_tongyi_is_connected(esp_tongyi_handle_t handle, bool *out_connected);

/**
 * @brief  Query whether the session is READY
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]   handle     Session handle returned by #esp_tongyi_create()
 * @param[out]  out_ready  Set true if media and directives may be sent
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p handle or @p out_ready is NULL
 */
esp_err_t esp_tongyi_is_ready(esp_tongyi_handle_t handle, bool *out_ready);

/**
 * @brief  Get the session lifecycle state
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 *
 * @return
 *       - Current #esp_tongyi_state_t
 *       - ESP_TONGYI_STATE_IDLE  If @p handle is NULL
 */
esp_tongyi_state_t esp_tongyi_get_state(esp_tongyi_handle_t handle);

/**
 * @brief  Get the last server-reported dialog state
 *
 * @note  This function is not ISR-safe.
 *
 * @param[in]  handle  Session handle returned by #esp_tongyi_create()
 *
 * @return
 *       - Current #esp_tongyi_dialog_state_t
 *       - ESP_TONGYI_DIALOG_STATE_IDLE  If @p handle is NULL
 */
esp_tongyi_dialog_state_t esp_tongyi_get_dialog_state(esp_tongyi_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
