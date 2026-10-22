#ifndef __WEBSOCKET_H__
#define __WEBSOCKET_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "volc_osal.h"
#include "tls_client.h"
#define CONFIG_WEBSOCKET_TLS

typedef enum {
    VOLC_WS_STATE_ERROR = -1,
    VOLC_WS_STATE_UNKNOW = 0,
    VOLC_WS_STATE_INIT,
    VOLC_WS_STATE_CONNECTED,
    VOLC_WS_STATE_WAIT_TIMEOUT,
    VOLC_WS_STATE_CLOSING,
} volc_ws_state_e;

typedef enum {
    VOLC_WS_OPCODES_CONT = 0x00,   // 附加数据帧
    VOLC_WS_OPCODES_TEXT = 0x01,   // 文本数据帧
    VOLC_WS_OPCODES_BINARY = 0x02, // 二进制数据帧
    VOLC_WS_OPCODES_CLOSE = 0x08,  // 连接关闭
    VOLC_WS_OPCODES_PING = 0x09,   // 心跳
    VOLC_WS_OPCODES_PONG = 0x0a,
    VOLC_WS_OPCODES_FIN = 0x80,
    VOLC_WS_OPCODES_NONE = 0x100,
} volc_ws_opcode_e;

typedef enum {
    VOLC_WS_EVENT_ANY = -1,
    VOLC_WS_EVENT_ERROR = 0,
    VOLC_WS_EVENT_CONNECTED,
    VOLC_WS_EVENT_DISCONNECTED,
    VOLC_WS_EVENT_DATA,
    VOLC_WS_EVENT_CLOSED,
    VOLC_WS_EVENT_MAX
} volc_ws_event_id_e;

typedef struct {
    uint8_t opcode;
    bool fin;
    char mask_key[4];
    int payload_len;
    int bytes_remaining;
    bool header_received;
} volc_ws_frame_state_t;

typedef struct {
    char* path;
    char* buffer;
    char* sub_protocol;
    char* user_agent;
    char* headers;
    bool propagate_control_frames;
    volc_ws_frame_state_t frame_state;
} transport_ws_t;

typedef struct {
    uint64_t last_print_ms;
    int sent;
    int received;
} ws_stats_t;

typedef void (*volc_ws_event_handler_t)(void* user_context, int32_t event_id, void* event_data);

typedef struct {
    char* host;
    char* path;
    char* scheme;
    char* signature;
    int port;
    volc_ws_state_e state;
    //  uint64_t					  keepalive_tick_ms;
    uint64_t reconnect_tick_ms;
    uint64_t ping_tick_ms;
    uint64_t pingpong_tick_ms;
    int auto_reconnect;
    volatile bool run;
    volatile bool exit;
    bool wait_for_pong_resp;
    char* rx_buffer;
    char* tx_buffer;
    int buffer_size;
    int rx_retry;
    bool last_fin;
    volc_ws_opcode_e last_opcode;
    int payload_len;
    int payload_offset;
    transport_ws_t* ws_transport;
    int sockfd;
    int is_tls;
#if defined(CONFIG_WEBSOCKET_TLS)
    MbedTLSSession* ssl;
#endif
    void* user_context;
    volc_ws_event_handler_t ws_event_handler;
    volc_osal_mutex_t mutex;
    volc_osal_tid_t tid;
    ws_stats_t stats;
} volc_ws_client_t;

typedef struct {
    const char* uri;
    //	  bool						  disable_auto_reconnect;
    int rx_retry;
    void* user_context;
    int buffer_size;
    const char* subprotocol;
    const char* user_agent;
    const char* headers;
    //	bool						disable_pingpong_discon;
    volc_ws_event_handler_t ws_event_handler;
} volc_ws_config_t;

/**
 * @brief Websocket event data
 */
typedef struct {
    char* data_ptr;
    int data_len;
    bool fin;
    uint8_t op_code;
    int payload_len;
    int payload_offset;
} volc_ws_event_data_t;

volc_ws_client_t* volc_ws_client_init(const volc_ws_config_t* input);
int volc_ws_client_destroy(volc_ws_client_t* client);
#if defined(PLATFORM_MACOS)
void* volc_ws_client_task(void* thread_param);
#else
void volc_ws_client_task(void* thread_param);
#endif
int volc_ws_client_start(volc_ws_client_t* client);
int volc_ws_client_stop(volc_ws_client_t* client);

int volc_ws_client_send_text(volc_ws_client_t* client, const char* data, int len, int timeout);

#ifdef __cplusplus
}
#endif
#endif /* __WEBSOCKET_H__ */
