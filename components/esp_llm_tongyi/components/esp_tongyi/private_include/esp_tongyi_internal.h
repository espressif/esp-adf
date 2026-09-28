/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_gmf_oal_thread.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"

#include "esp_tongyi.h"
#include "esp_tongyi_protocol.h"
#include "esp_tongyi_transport.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_tongyi esp_tongyi_t;

#define ESP_TONGYI_BIT_CONNECTED     (1 << 0)
#define ESP_TONGYI_BIT_READY         (1 << 1)
#define ESP_TONGYI_BIT_DISCONNECTED  (1 << 2)
#define ESP_TONGYI_BIT_FAIL          (1 << 3)
#define ESP_TONGYI_BIT_CTRL_DONE     (1 << 4)

#define ESP_TONGYI_ID_MAX          64
#define ESP_TONGYI_CTRL_QUEUE_LEN  16
#define ESP_TONGYI_CTRL_STACK      6144

typedef enum {
    ESP_TONGYI_CTRL_QUIT      = 0,
    ESP_TONGYI_CTRL_HEARTBEAT = 1,
    ESP_TONGYI_CTRL_RECONNECT = 2,
    ESP_TONGYI_CTRL_DIRECTIVE = 3,
} esp_tongyi_ctrl_cmd_t;

typedef struct {
    esp_tongyi_ctrl_cmd_t       cmd;        /*!< Control command */
    esp_tongyi_speech_action_t  directive;  /*!< Directive payload for ESP_TONGYI_CTRL_DIRECTIVE */
} esp_tongyi_ctrl_msg_t;

typedef struct {
    char  *type;          /*!< Owned upstream type */
    char  *mode;          /*!< Owned interaction mode */
    char  *audio_format;  /*!< Owned uplink codec */
    int    sample_rate;   /*!< Uplink sample rate in Hz */
} esp_tongyi_owned_upstream_t;

typedef struct {
    char  *voice;                /*!< Owned TTS voice */
    char  *audio_format;         /*!< Owned downlink codec */
    int    sample_rate;          /*!< Downlink sample rate in Hz */
    int    frame_size;           /*!< Frame size in milliseconds */
    int    volume;               /*!< Output volume */
    int    bit_rate;             /*!< Bitrate in kbps */
    int    transmit_rate_limit;  /*!< Transmit rate limit in bytes */
} esp_tongyi_owned_downstream_t;

typedef struct {
    char  *user_id;      /*!< Owned user identifier */
    char  *device_uuid;  /*!< Owned device UUID */
} esp_tongyi_owned_client_t;

typedef struct {
    char                         *api_key;                /*!< Owned DashScope API key */
    char                         *workspace_id;           /*!< Owned workspace ID */
    char                         *app_id;                 /*!< Owned application ID */
    char                         *task_id;                /*!< Owned task ID */
    char                         *dialog_id;              /*!< Optional dialog ID copied at create */
    char                         *biz_params_json;        /*!< Optional extra JSON object */
    uint32_t                      keep_alive_time_sec;    /*!< Heartbeat interval in seconds */
    bool                          disconnect_after_stop;  /*!< Close WebSocket inside stop */
    esp_tongyi_event_cb_t         event_cb;               /*!< Event callback */
    void                         *event_ctx;              /*!< User context for @p event_cb */
    char                         *uri;                    /*!< Owned WebSocket URI */
    int                           buffer_size;            /*!< WebSocket RX and TX buffer size */
    int                           send_timeout_ms;        /*!< Send timeout in milliseconds */
    int                           connect_timeout_ms;     /*!< Connect and Started timeout */
    int                           network_timeout_ms;     /*!< Network I/O timeout */
    int                           reconnect_timeout_ms;   /*!< WebSocket client reconnect timeout */
    esp_tongyi_task_cfg_t         task;                   /*!< WebSocket worker task */
    esp_tongyi_reconnect_cfg_t    reconnect;              /*!< Automatic session restart */
    esp_tongyi_limits_cfg_t       limits;                 /*!< RX and image limits */
    esp_tongyi_owned_upstream_t   upstream;               /*!< Owned upstream parameters */
    esp_tongyi_owned_downstream_t downstream;             /*!< Owned downstream parameters */
    esp_tongyi_owned_client_t     client;                 /*!< Owned client_info */
} esp_tongyi_owned_cfg_t;

struct esp_tongyi_transport {
    esp_websocket_client_handle_t  client;           /*!< WebSocket client handle */
    SemaphoreHandle_t              send_lock;        /*!< Serializes text and binary sends */
    int                            send_timeout_ms;  /*!< Send timeout in milliseconds */
    esp_tongyi_rx_assembler_t      rx;               /*!< RX fragment assembler */
    struct esp_tongyi             *owner;            /*!< Owning session instance */
};

struct esp_tongyi {
    esp_tongyi_owned_cfg_t    cfg;                      /*!< Deep-copied create configuration */
    SemaphoreHandle_t         lock;                     /*!< Protects state and dialog IDs */
    SemaphoreHandle_t         session_sem;              /*!< Serializes start and stop */
    EventGroupHandle_t        event_group;              /*!< Connect, ready, fail, and quit bits */
    QueueHandle_t             ctrl_queue;               /*!< Control-thread command queue */
    esp_gmf_oal_thread_t      ctrl_thread;              /*!< Control thread with optional PSRAM stack */
    esp_timer_handle_t        keep_alive_timer;         /*!< Heartbeat timer */
    esp_timer_handle_t        reconnect_timer;          /*!< Reconnect delay timer */
    esp_tongyi_transport_t    transport;                /*!< WebSocket transport */
    char                     *dialog_id;                /*!< Current dialog ID */
    char                     *saved_dialog_id;          /*!< Dialog ID kept for reconnect */
    esp_tongyi_state_t        state;                    /*!< Session lifecycle state */
    esp_tongyi_dialog_state_t dialog_state;             /*!< Last server dialog state */
    int64_t                   last_request_to_speak_ms; /*!< RequestToSpeak throttle timestamp */
    atomic_bool               ready;                    /*!< True while the session is READY */
    atomic_bool               closing;                  /*!< True while destroy is in progress */
    atomic_bool               user_stopped;             /*!< True after an explicit stop, until the next start */
    atomic_bool               reconnect_pending;        /*!< True while a reconnect is scheduled */
    atomic_int                in_callback;              /*!< Outstanding event-callback count */
};

void esp_tongyi_owned_cfg_deinit(esp_tongyi_owned_cfg_t *cfg);

void esp_tongyi_session_emit(esp_tongyi_t *inst,
                             esp_tongyi_event_t event,
                             const esp_tongyi_event_data_t *event_data);

esp_err_t esp_tongyi_session_ctrl_start(esp_tongyi_t *inst);

void esp_tongyi_session_ctrl_stop(esp_tongyi_t *inst);

esp_err_t esp_tongyi_session_start(esp_tongyi_t *inst);

esp_err_t esp_tongyi_session_stop(esp_tongyi_t *inst, bool explicit_stop);

esp_err_t esp_tongyi_session_send_json(esp_tongyi_t *inst, char *json);

esp_err_t esp_tongyi_session_send_directive(esp_tongyi_t *inst,
                                            esp_tongyi_speech_action_t directive,
                                            bool async);

esp_err_t esp_tongyi_session_snapshot_dialog_id(esp_tongyi_t *inst, char *buf, size_t buf_len);

void esp_tongyi_session_fill_start_cfg(const esp_tongyi_t *inst, esp_tongyi_proto_start_cfg_t *out);

void esp_tongyi_session_on_connected(esp_tongyi_t *inst);

void esp_tongyi_session_on_disconnected(esp_tongyi_t *inst);

void esp_tongyi_session_on_error(esp_tongyi_t *inst);

void esp_tongyi_session_on_text(esp_tongyi_t *inst, const char *message);

void esp_tongyi_session_on_binary(esp_tongyi_t *inst, const uint8_t *data, size_t len);

void esp_tongyi_session_on_assembler_error(esp_tongyi_t *inst, esp_err_t err);

void esp_tongyi_session_schedule_reconnect(esp_tongyi_t *inst);

esp_err_t esp_tongyi_transport_open(esp_tongyi_t *inst);

void esp_tongyi_transport_close_ws(esp_tongyi_t *inst);

void esp_tongyi_transport_destroy(esp_tongyi_t *inst);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
