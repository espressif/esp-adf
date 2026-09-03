/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"

#include "esp_capture_service.h"
#include "esp_media_service.h"
#include "esp_player_service.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

#include "rtsp_example.h"
#include "rtsp_session.h"
#include "rtsp_settings.h"

typedef enum {
    SESSION_ROLE_NONE = 0,
    SESSION_ROLE_SERVER,
    SESSION_ROLE_PUSH,
    SESSION_ROLE_PULL,
} session_role_t;

typedef struct {
    session_role_t         role;
    rtsp_session_opts_t    opts;
    char                   url[RTSP_URL_MAX_LEN];
    esp_capture_service_t *capture;  /*!< Send roles: camera and microphone source */
    esp_player_service_t  *player;   /*!< Pull role: LCD and speaker sink */
    esp_rtsp_service_t    *rtsp;
    esp_rtsp_state_t       state;
    bool                   linked;
    bool                   peer_gone;  /*!< Peer tore down, or the service failed */
} rtsp_session_t;

static const char *TAG = "RTSP_SESSION";

static rtsp_session_t s_session;

static const char *role_name(session_role_t role)
{
    switch (role) {
        case SESSION_ROLE_SERVER:
            return "server";
        case SESSION_ROLE_PUSH:
            return "push";
        case SESSION_ROLE_PULL:
            return "pull";
        default:
            return "idle";
    }
}

static const char *codec_name(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_FMT_ID_H264:
            return "h264";
        case ESP_CAPTURE_FMT_ID_MJPEG:
            return "mjpeg";
        case ESP_CAPTURE_FMT_ID_AAC:
            return "aac";
        case ESP_CAPTURE_FMT_ID_G711A:
            return "g711a";
        case ESP_CAPTURE_FMT_ID_G711U:
            return "g711u";
        default:
            return "none";
    }
}

static const char *state_name(esp_rtsp_state_t state)
{
    switch (state) {
        case RTSP_STATE_NONE:
            return "none";
        case RTSP_STATE_OPTIONS:
            return "options";
        case RTSP_STATE_ANNOUNCE:
            return "announce";
        case RTSP_STATE_DESCRIBING:
            return "describing";
        case RTSP_STATE_DESCRIBE:
            return "describe";
        case RTSP_STATE_SETUP:
            return "setup";
        case RTSP_STATE_PLAY:
            return "play";
        case RTSP_STATE_RECORD:
            return "record";
        case RTSP_STATE_TEARDOWN:
            return "teardown";
        default:
            return "unknown";
    }
}

static const char *transport_name(esp_rtsp_transport_t transport)
{
    return (transport == RTSP_TRANSPORT_TCP) ? "tcp" : "udp";
}

/* "rtsp://host:port/live" -> "/live" */
static const char *url_path(const char *url)
{
    const char *host = strstr(url, "://");
    host = (host != NULL) ? host + 3 : url;
    const char *path = strchr(host, '/');
    return (path != NULL) ? path : "/";
}

/* Session states are the fastest way to see where an RTSP handshake stopped,
   so every one of them is logged. */
static void rtsp_event_handler(const adf_event_t *event, void *ctx)
{
    esp_service_t *service = (esp_service_t *)ctx;
    const char *name = "UNKNOWN";
    const char *resolved = NULL;
    if (service != NULL &&
        esp_service_get_event_name(service, event->event_id, &resolved) == ESP_OK && resolved != NULL) {
        name = resolved;
    }

    if (event->event_id >= ESP_RTSP_SERVICE_EVENT_OPTIONS &&
        event->event_id <= ESP_RTSP_SERVICE_EVENT_TEARDOWN &&
        event->payload != NULL && event->payload_len >= sizeof(esp_rtsp_service_event_payload_t)) {
        const esp_rtsp_service_event_payload_t *payload = event->payload;
        s_session.state = payload->state;
        ESP_LOGI(TAG, "%s (state: %s)", name, state_name(payload->state));

        /* A server keeps listening for the next player; the client roles are done */
        if (event->event_id == ESP_RTSP_SERVICE_EVENT_TEARDOWN &&
            payload->role != ESP_RTSP_SERVICE_ROLE_SERVER) {
            s_session.peer_gone = true;
            ESP_LOGW(TAG, "Peer closed the session; 'rtsp stop' or the next start releases it");
        }
        return;
    }

    if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED && event->payload != NULL &&
        event->payload_len >= sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *st = event->payload;
        if (st->new_state == ESP_SERVICE_STATE_ERROR) {
            s_session.peer_gone = true;
            ESP_LOGE(TAG, "RTSP service entered the error state");
        }
    }
}

static esp_err_t subscribe_events(esp_rtsp_service_t *rtsp)
{
    esp_service_t *base = ESP_SERVICE_BASE(rtsp);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = rtsp_event_handler;
    info.handler_ctx = base;
    return esp_service_event_subscribe(base, &info);
}

static void rtsp_service_delete(esp_rtsp_service_t *rtsp)
{
    if (rtsp == NULL) {
        return;
    }
    (void)esp_media_service_deinit(ESP_SERVICE_BASE(rtsp));
    free(rtsp);
}

static uint32_t video_bitrate(const rtsp_session_opts_t *opts)
{
    if (opts->bitrate != 0) {
        return opts->bitrate;
    }
    return (uint32_t)opts->width * opts->height * opts->fps / RTSP_VIDEO_BITRATE_DIVISOR;
}

/* Camera plus microphone into one elementary-stream output. A zero codec leaves
   that track out, which is how '-v none' and '-a none' work. */
static esp_err_t build_capture(const rtsp_session_opts_t *opts, esp_capture_service_t **out_capture)
{
    if (opts->video_codec != 0) {
        void *cam = NULL;
        if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&cam) != ESP_OK) {
            ESP_LOGE(TAG, "No camera on this board; retry with '-v none' for an audio-only stream");
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_video_capture_service_cfg_t cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    cfg.service_name = "rtsp_capture";
    cfg.audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    cfg.max_stream_num = 1;

    esp_capture_service_t *capture = NULL;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&cfg, &capture), TAG, "Create capture service");

    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = (opts->audio_codec != 0) ? opts->sample_rate : 0,
        .streams[0].enabled = true,
    };
    if (opts->video_codec != 0) {
        setup.streams[0].video_info = (esp_media_video_info_t) {
            .codec = opts->video_codec,
            .width = opts->width,
            .height = opts->height,
            .fps = opts->fps,
            .bitrate = video_bitrate(opts),
        };
    }
    if (opts->audio_codec != 0) {
        setup.streams[0].audio_info = (esp_media_audio_info_t) {
            .codec = opts->audio_codec,
            .sample_rate = opts->sample_rate,
            .bits_per_sample = RTSP_AUDIO_BITS_PER_SAMPLE,
            .channel = RTSP_AUDIO_CHANNEL,
            .bitrate = RTSP_AUDIO_BITRATE,
        };
    }

    esp_err_t ret = esp_video_capture_service_apply_setup(capture, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply capture setup failed: %s", esp_err_to_name(ret));
        (void)esp_capture_service_destroy(capture);
        return ret;
    }

    *out_capture = capture;
    return ESP_OK;
}

static esp_err_t build_player(esp_player_service_t **out_player)
{
    esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.name = "rtsp_player";
    cfg.max_stream_num = 1;

    esp_player_service_t *player = NULL;
    ESP_RETURN_ON_ERROR(esp_video_player_service_create(&cfg, &player), TAG, "Create player service");

    esp_video_player_service_setup_t setup = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.display_dev_name = ESP_BOARD_DEVICE_NAME_DISPLAY_LCD;
    void *lcd = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd) != ESP_OK) {
        ESP_LOGW(TAG, "No display on this board; received video is dropped");
        setup.display_dev_name = NULL;
    }

    esp_err_t ret = esp_video_player_service_apply_setup(player, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply player setup failed: %s", esp_err_to_name(ret));
        (void)esp_player_service_destroy(player);
        return ret;
    }

    *out_player = player;
    return ESP_OK;
}

static esp_err_t build_rtsp(session_role_t role, const rtsp_session_opts_t *opts, esp_rtsp_service_t **out_rtsp)
{
    esp_rtsp_service_role_t rtsp_role = ESP_RTSP_SERVICE_ROLE_SERVER;
    if (role == SESSION_ROLE_PUSH) {
        rtsp_role = ESP_RTSP_SERVICE_ROLE_SINK;
    } else if (role == SESSION_ROLE_PULL) {
        rtsp_role = ESP_RTSP_SERVICE_ROLE_SRC;
    }

    esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(rtsp_role);
    esp_rtsp_service_t *rtsp = NULL;
    ESP_RETURN_ON_ERROR(esp_rtsp_service_create(&cfg, &rtsp), TAG, "Create RTSP service");

    esp_err_t ret = subscribe_events(rtsp);
    if (ret == ESP_OK) {
        /* local_port only matters to SERVER, and the enable flags and caches only
           matter to SRC: SINK and SERVER take their tracks and codecs from the
           linked provider at start. */
        esp_rtsp_service_setup_t setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
        setup.local_port = opts->port;
        setup.transport = opts->transport;
        setup.audio_enable = (opts->audio_codec != 0);
        setup.video_enable = (opts->video_codec != 0);
        setup.audio_cache_size = opts->audio_cache;
        setup.video_cache_size = opts->video_cache;
        ret = esp_rtsp_service_setup(rtsp, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_rtsp_service_set_url(rtsp, opts->url);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set up RTSP service failed: %s", esp_err_to_name(ret));
        rtsp_service_delete(rtsp);
        return ret;
    }

    *out_rtsp = rtsp;
    return ESP_OK;
}

static void session_release(void)
{
    /* Producers stop first so consumers drain instead of blocking on an empty queue */
    if (s_session.role == SESSION_ROLE_PULL) {
        if (s_session.rtsp != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(s_session.rtsp));
        }
        if (s_session.player != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(s_session.player));
        }
    } else {
        if (s_session.capture != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(s_session.capture));
        }
        if (s_session.rtsp != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(s_session.rtsp));
        }
    }

    if (s_session.linked) {
        if (s_session.role == SESSION_ROLE_PULL) {
            (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.rtsp), ESP_MEDIA_DEFAULT_STREAM,
                                           ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        } else {
            (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                           ESP_SERVICE_BASE(s_session.rtsp), ESP_MEDIA_DEFAULT_STREAM);
        }
        s_session.linked = false;
    }

    rtsp_service_delete(s_session.rtsp);
    s_session.rtsp = NULL;

    if (s_session.capture != NULL) {
        (void)esp_capture_service_destroy(s_session.capture);
        s_session.capture = NULL;
    }
    if (s_session.player != NULL) {
        (void)esp_player_service_destroy(s_session.player);
        s_session.player = NULL;
    }

    s_session.role = SESSION_ROLE_NONE;
    s_session.state = RTSP_STATE_NONE;
    s_session.peer_gone = false;
    s_session.url[0] = '\0';
}

static esp_err_t session_claim(session_role_t role, const rtsp_session_opts_t *opts)
{
    if (opts == NULL || opts->url == NULL || opts->url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    /* A peer that already left does not need a manual 'rtsp stop' first */
    if (s_session.peer_gone) {
        ESP_LOGI(TAG, "Releasing the finished %s session", role_name(s_session.role));
        session_release();
    }
    if (s_session.role != SESSION_ROLE_NONE) {
        ESP_LOGE(TAG, "A %s session is running; stop it with 'rtsp stop' first", role_name(s_session.role));
        return ESP_ERR_INVALID_STATE;
    }

    s_session.role = role;
    s_session.opts = *opts;
    s_session.state = RTSP_STATE_NONE;
    snprintf(s_session.url, sizeof(s_session.url), "%s", opts->url);
    s_session.opts.url = s_session.url;
    return ESP_OK;
}

/* Capture (SRC) -> RTSP (SERVER or SINK) */
static esp_err_t start_send_role(session_role_t role, const rtsp_session_opts_t *opts)
{
    if (opts->video_codec == 0 && opts->audio_codec == 0) {
        ESP_LOGE(TAG, "Nothing to send: enable at least one of audio and video");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(session_claim(role, opts), TAG, "Claim session");

    esp_err_t ret = build_capture(&s_session.opts, &s_session.capture);
    if (ret == ESP_OK) {
        ret = build_rtsp(role, &s_session.opts, &s_session.rtsp);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(s_session.rtsp), ESP_MEDIA_DEFAULT_STREAM);
        s_session.linked = (ret == ESP_OK);
    }
    /* Link before starting so the RTSP side can read track info from the provider */
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.rtsp));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.capture));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start %s session failed: %s", role_name(role), esp_err_to_name(ret));
        session_release();
        return ret;
    }

    ESP_LOGI(TAG, "%s session running on %s (%s, video: %s, audio: %s)",
             role_name(role), s_session.url, transport_name(opts->transport),
             codec_name(opts->video_codec), codec_name(opts->audio_codec));
    if (opts->video_codec != 0) {
        ESP_LOGI(TAG, "Video encoded at %ux%u %u fps, %" PRIu32 " kbps",
                 (unsigned)opts->width, (unsigned)opts->height, (unsigned)opts->fps,
                 video_bitrate(opts) / 1000);
    }
    if (role == SESSION_ROLE_SERVER) {
        char ip[16];
        rtsp_example_fill_ip(ip, sizeof(ip));
        ESP_LOGI(TAG, "Pull it with: ffplay -rtsp_transport %s rtsp://%s:%u%s",
                 transport_name(opts->transport), ip, (unsigned)opts->port, url_path(s_session.url));
    }
    return ESP_OK;
}

/* RTSP (SRC) -> video player (SINK). Tracks arrive from the SDP negotiation. */
static esp_err_t start_pull_role(const rtsp_session_opts_t *opts)
{
    if (opts->video_codec == 0 && opts->audio_codec == 0) {
        ESP_LOGE(TAG, "Nothing to play: enable at least one of audio and video");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(session_claim(SESSION_ROLE_PULL, opts), TAG, "Claim session");

    esp_err_t ret = build_player(&s_session.player);
    if (ret == ESP_OK) {
        ret = build_rtsp(SESSION_ROLE_PULL, &s_session.opts, &s_session.rtsp);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.rtsp), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        s_session.linked = (ret == ESP_OK);
    }
    /* Sink first: the RTSP client starts receiving as soon as PLAY succeeds */
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.player));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.rtsp));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start pull session failed: %s", esp_err_to_name(ret));
        session_release();
        return ret;
    }

    ESP_LOGI(TAG, "pull session running on %s (%s)", s_session.url, transport_name(opts->transport));
    return ESP_OK;
}

void rtsp_session_opts_default(rtsp_session_opts_t *opts)
{
    if (opts == NULL) {
        return;
    }
    *opts = (rtsp_session_opts_t) {
        .url = NULL,
        .port = RTSP_SERVER_PORT,
        .transport = RTSP_DEFAULT_TRANSPORT,
        .video_codec = RTSP_VIDEO_CODEC,
        .audio_codec = RTSP_AUDIO_CODEC,
        .width = RTSP_VIDEO_WIDTH,
        .height = RTSP_VIDEO_HEIGHT,
        .fps = RTSP_VIDEO_FPS,
        .bitrate = 0,
        .sample_rate = RTSP_AUDIO_SAMPLE_RATE,
        .audio_cache = RTSP_RECV_AUDIO_CACHE,
        .video_cache = RTSP_RECV_VIDEO_CACHE,
    };
}

esp_err_t rtsp_session_start_server(const rtsp_session_opts_t *opts)
{
    return start_send_role(SESSION_ROLE_SERVER, opts);
}

esp_err_t rtsp_session_start_push(const rtsp_session_opts_t *opts)
{
    return start_send_role(SESSION_ROLE_PUSH, opts);
}

esp_err_t rtsp_session_start_pull(const rtsp_session_opts_t *opts)
{
    return start_pull_role(opts);
}

esp_err_t rtsp_session_stop(void)
{
    if (s_session.role == SESSION_ROLE_NONE) {
        return ESP_OK;
    }
    /* lwIP does not wake a blocked select() when another thread closes the
       socket, so the RTSP control task only leaves it when that call times out.
       Releasing runs on the console task and holds the port and the capture
       devices until then, which is also why a new session cannot start earlier. */
    ESP_LOGI(TAG, "Stopping the %s session; waiting for the RTSP control connection to time out",
             role_name(s_session.role));
    session_release();
    return ESP_OK;
}

bool rtsp_session_is_active(void)
{
    return s_session.role != SESSION_ROLE_NONE;
}

void rtsp_session_print_info(void)
{
    char ip[16];
    rtsp_example_fill_ip(ip, sizeof(ip));
    printf("device ip  : %s\n", ip);

    if (s_session.role == SESSION_ROLE_NONE) {
        printf("session    : idle\n");
        printf("hint       : 'rtsp server' then ffplay rtsp://%s:%d%s\n",
               ip, RTSP_SERVER_PORT, RTSP_SERVER_PATH);
        return;
    }

    const rtsp_session_opts_t *opts = &s_session.opts;
    printf("session    : %s%s\n", role_name(s_session.role), s_session.peer_gone ? " (peer left)" : "");
    printf("rtsp state : %s\n", state_name(s_session.state));
    printf("url        : %s\n", s_session.url);
    printf("transport  : %s\n", transport_name(opts->transport));

    if (s_session.role == SESSION_ROLE_PULL) {
        /* Codecs come from the SDP, so only the enable switches are known here */
        printf("video      : %s\n", opts->video_codec != 0 ? "enabled (from SDP)" : "disabled");
        printf("audio      : %s\n", opts->audio_codec != 0 ? "enabled (from SDP)" : "disabled");
        return;
    }

    if (opts->video_codec != 0) {
        printf("video      : %s %ux%u@%ufps %" PRIu32 " kbps%s\n", codec_name(opts->video_codec),
               (unsigned)opts->width, (unsigned)opts->height, (unsigned)opts->fps,
               video_bitrate(opts) / 1000, opts->bitrate != 0 ? "" : " (derived)");
    } else {
        printf("video      : disabled\n");
    }
    if (opts->audio_codec != 0) {
        printf("audio      : %s %" PRIu32 " Hz %d ch\n", codec_name(opts->audio_codec),
               opts->sample_rate, RTSP_AUDIO_CHANNEL);
    } else {
        printf("audio      : disabled\n");
    }
    if (s_session.role == SESSION_ROLE_SERVER) {
        printf("remote pull: ffplay -rtsp_transport %s rtsp://%s:%u%s\n",
               transport_name(opts->transport), ip, (unsigned)opts->port, url_path(s_session.url));
    }
}
