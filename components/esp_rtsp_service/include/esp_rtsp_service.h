/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_media_service.h"
#include "esp_rtsp.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_RTSP_SERVICE_NAME              "esp_rtsp_service"
#define ESP_RTSP_SERVICE_DEFAULT_PATH      "/live"
#define ESP_RTSP_SERVICE_DEFAULT_PORT      554

#define ESP_RTSP_SERVICE_PUSH_TASK_NAME    "rtsp_push"
#define ESP_RTSP_SERVICE_SRC_TASK_NAME     "rtsp_src"
#define ESP_RTSP_SERVICE_SERVER_TASK_NAME  "rtsp_server"

typedef struct esp_rtsp_service esp_rtsp_service_t;

/**
 * @brief  RTSP service role
 *
 *         SERVER is a media sink (local push) that also runs an RTSP server for
 *         one remote puller. Remote pull-as-source uses ROLE_SRC (client play).
 */
typedef enum {
    ESP_RTSP_SERVICE_ROLE_SERVER = 0,  /*!< Local push sink + RTSP server (one remote puller) */
    ESP_RTSP_SERVICE_ROLE_SRC,         /*!< RTSP client play source */
    ESP_RTSP_SERVICE_ROLE_SINK,        /*!< RTSP client push sink */
} esp_rtsp_service_role_t;

/**
 * @brief  RTSP service event identifiers mapped from esp_rtsp_state_t
 *
 *         Base lifecycle changes are published separately as
 *         ESP_SERVICE_EVENT_STATE_CHANGED.
 */
typedef enum {
    ESP_RTSP_SERVICE_EVENT_OPTIONS    = 101,  /*!< OPTIONS completed */
    ESP_RTSP_SERVICE_EVENT_ANNOUNCE   = 102,  /*!< ANNOUNCE completed (SINK) */
    ESP_RTSP_SERVICE_EVENT_DESCRIBING = 103,  /*!< DESCRIBE request started (SRC) */
    ESP_RTSP_SERVICE_EVENT_DESCRIBE   = 104,  /*!< DESCRIBE completed (SRC) */
    ESP_RTSP_SERVICE_EVENT_SETUP      = 105,  /*!< Media SETUP completed */
    ESP_RTSP_SERVICE_EVENT_PLAY       = 106,  /*!< PLAY active (SRC/SERVER client) */
    ESP_RTSP_SERVICE_EVENT_RECORD     = 107,  /*!< RECORD active (SINK) */
    ESP_RTSP_SERVICE_EVENT_TEARDOWN   = 108,  /*!< Session torn down */
} esp_rtsp_service_event_t;

/**
 * @brief  Payload published with RTSP service events
 */
typedef struct {
    esp_rtsp_service_role_t role;   /*!< Service role that emitted the event */
    esp_rtsp_state_t        state;  /*!< Native RTSP state mapped to the event ID */
} esp_rtsp_service_event_payload_t;

/**
 * @brief  RTSP service create configuration
 *
 *         Stream/session settings are applied by esp_rtsp_service_setup().
 *         Tear down with esp_media_service_deinit() then free the handle.
 */
typedef struct {
    const char              *name;  /*!< Service name, NULL uses ESP_RTSP_SERVICE_NAME */
    esp_rtsp_service_role_t  role;  /*!< Service role */
} esp_rtsp_service_cfg_t;

#define ESP_RTSP_SERVICE_CFG_DEFAULT(_role) {  \
    .name = ESP_RTSP_SERVICE_NAME,             \
    .role = (_role),                           \
}

/**
 * @brief  Optional stream/session setup before start
 *
 *         SINK/SERVER: audio/video enable and codecs come from linked provider
 *         tracks at start (not from this struct). SRC uses audio_enable /
 *         video_enable as client play preferences.
 */
typedef struct {
    uint16_t              local_port;        /*!< SERVER local port, 0 uses default */
    esp_rtsp_transport_t  transport;         /*!< Client transport for SRC/SINK (and SERVER default) */
    bool                  audio_enable;      /*!< SRC: request audio */
    bool                  video_enable;      /*!< SRC: request video */
    uint32_t              audio_cache_size;  /*!< SRC: per-audio track cache, 0 uses default */
    uint32_t              video_cache_size;  /*!< SRC: per-video track cache, 0 uses default */
    uint32_t              aud_frame_size;    /*!< Audio RTP frame buffer; 0 uses default (4 KB) */
    uint32_t              vid_frame_size;    /*!< Video max frame length; 0 uses default (64 KB) */
} esp_rtsp_service_setup_t;

#define ESP_RTSP_SERVICE_SETUP_DEFAULT() {           \
    .local_port = ESP_RTSP_SERVICE_DEFAULT_PORT,     \
    .transport = RTSP_TRANSPORT_UDP,                 \
    .audio_enable = true,                            \
    .video_enable = true,                            \
    .audio_cache_size = 0,                           \
    .video_cache_size = 0,                           \
    .aud_frame_size = 0,                             \
    .vid_frame_size = 0,                             \
}

/**
 * @brief  Create an RTSP service
 *
 *         Recommended flow:
 *         create -> setup / set_url / set_ip -> optional link -> start -> stop
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
esp_err_t esp_rtsp_service_create(const esp_rtsp_service_cfg_t *cfg, esp_rtsp_service_t **out_service);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
