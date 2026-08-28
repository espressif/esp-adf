/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_media_service.h"
#include "media_lib_tls.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_RTMP_SERVICE_NAME              "esp_rtmp_service"
#define ESP_RTMP_SERVICE_DEFAULT_APP_NAME  "live"
#define ESP_RTMP_SERVICE_DEFAULT_PORT      1935

/**
 * @brief  Forward declaration of esp_rtmp_service_t
 */
typedef struct esp_rtmp_service esp_rtmp_service_t;

typedef enum {
    ESP_RTMP_SERVICE_ROLE_SERVER = 0,  /*!< RTMP server/listener (no media data flow) */
    ESP_RTMP_SERVICE_ROLE_SRC    = 1,  /*!< RTMP puller source */
    ESP_RTMP_SERVICE_ROLE_SINK   = 2,  /*!< RTMP pusher sink */
} esp_rtmp_service_role_t;

#define ESP_RTMP_SERVICE_STREAM_NAME_MAX  128

/**
 * @brief  RTMP service event identifiers
 *
 *         Base lifecycle changes are published separately as
 *         ESP_SERVICE_EVENT_STATE_CHANGED.
 */
typedef enum {
    ESP_RTMP_SERVICE_EVENT_PEER_CLOSED             = 101,  /*!< SRC/SINK peer closed the connection */
    ESP_RTMP_SERVICE_EVENT_SERVER_CLIENT_CONNECTED = 102,  /*!< SERVER accepted a new client */
    ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STARTED   = 103,  /*!< SERVER puller became active */
    ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STOPPED   = 104,  /*!< SERVER puller became inactive */
} esp_rtmp_service_event_t;

/**
 * @brief  Payload published with RTMP service events
 */
typedef struct {
    esp_rtmp_service_role_t role;                                      /*!< Service role that emitted the event */
    char                    stream_name[ESP_RTMP_SERVICE_STREAM_NAME_MAX]; /*!< SERVER stream, empty when unavailable */
} esp_rtmp_service_event_payload_t;

/**
 * @brief  RTMP service create configuration
 *
 *         Role-specific settings are applied by esp_rtmp_service_setup().
 *         Tear down with esp_media_service_deinit() then free the handle.
 */
typedef struct {
    const char             *name;  /*!< Service name, NULL uses ESP_RTMP_SERVICE_NAME */
    esp_rtmp_service_role_t role;  /*!< Service role */
} esp_rtmp_service_cfg_t;

#define ESP_RTMP_SERVICE_CFG_DEFAULT(_role) {  \
    .name = ESP_RTMP_SERVICE_NAME,             \
    .role = (_role),                           \
}

/**
 * @brief  Optional server role setup (used when role is SERVER)
 *
 *         set_url(rtmp[s]://host:port/app) overrides port and app_name when present.
 */
typedef struct {
    uint16_t     port;         /*!< Fallback port when URL has none, 0 uses default */
    const char  *app_name;     /*!< Fallback app name when URL has none */
    uint8_t      max_clients;  /*!< Max clients, 0 uses protocol default */
} esp_rtmp_service_server_setup_t;

/**
 * @brief  Optional source role setup (used when role is SRC)
 */
typedef struct {
    size_t  cache_size;        /*!< Global interleaved cache size, 0 uses default */
    size_t  audio_cache_size;  /*!< Per-audio track cache when global cache off, 0 uses default */
    size_t  video_cache_size;  /*!< Per-video track cache when global cache off, 0 uses default */
} esp_rtmp_service_src_setup_t;

/**
 * @brief  Optional role setup applied before start
 *
 *         If never called, built-in defaults are used.
 *         Sink role only uses the common fields.
 */
typedef struct {
    uint32_t  chunk_size;  /*!< RTMP chunk size, 0 uses protocol default */
    union {
        esp_rtmp_service_server_setup_t  server; /*!< Server role setup */
        esp_rtmp_service_src_setup_t     src;    /*!< Source role setup */
    };
    union {
        media_lib_tls_cfg_t         client;  /*!< SRC/SINK RTMPS client TLS */
        media_lib_tls_server_cfg_t  server;  /*!< SERVER RTMPS TLS */
    } ssl_cfg;  /*!< RTMPS TLS configuration. Any certificate, key, or password
                     pointer is borrowed. If a key is provided, its buffer must
                     remain valid from setup until the full RTMPS handshake
                     completes */
} esp_rtmp_service_setup_t;

#define ESP_RTMP_SERVICE_SERVER_SETUP_DEFAULT() {       \
    .chunk_size = 4096,                                 \
    .ssl_cfg.server = {0},                              \
    .server = {                                         \
        .port = ESP_RTMP_SERVICE_DEFAULT_PORT,          \
        .app_name = ESP_RTMP_SERVICE_DEFAULT_APP_NAME,  \
        .max_clients = 4,                               \
    },                                                  \
}

#define ESP_RTMP_SERVICE_SRC_SETUP_DEFAULT() {  \
    .chunk_size = 4096,                          \
    .ssl_cfg.client = {0},                      \
    .src = {                                    \
        .cache_size = 0,                        \
        .audio_cache_size = 0,                  \
        .video_cache_size = 0,                  \
    },                                          \
}

#define ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT() {  \
    .chunk_size = 4096,                           \
    .ssl_cfg.client = {0},                       \
}

/**
 * @brief  Create an RTMP service based on its role, supported roles are:
 *         - SERVER: listens and relays RTMP clients (no elementary media role)
 *         - SRC: pulls a remote stream and publishes tracks as a media source
 *         - SINK: pushes linked elementary frames to a remote RTMP URL
 *
 *         Recommended flow:
 *         create -> setup / set_url -> optional link -> start -> stop
 *         -> unlink -> esp_media_service_deinit() -> free().
 *
 * @param[in]   cfg          Create configuration (name + role)
 * @param[out]  out_service  Receives the created service handle
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    cfg or out_service is NULL
 *       - ESP_ERR_NOT_SUPPORTED  Role is disabled by Kconfig
 *       - ESP_ERR_NO_MEM         Allocation failed
 *       - Others                 Error returned by esp_media_service_init()
 */
esp_err_t esp_rtmp_service_create(const esp_rtmp_service_cfg_t *cfg, esp_rtmp_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
