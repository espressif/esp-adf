/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_log.h"

#include "esp_audio_player_service_setup.h"
#include "esp_capture_service.h"
#include "esp_media_service.h"
#include "esp_player_service.h"
#include "esp_rtmp_service.h"
#include "esp_rtmp_service_ops.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */

#include "rtmp_example.h"
#include "rtmp_session.h"
#include "rtmp_settings.h"

#define RTMP_NAME_MAX_LEN  (64)

/* The three slots are independent services that can run alone or together. RTMP
   splits them differently from RTSP: a server carries no media of its own, so
   serving the local camera means running the server and the push slot at once. */
typedef enum {
    RTMP_SLOT_SERVER = 0,
    RTMP_SLOT_PUSH,
    RTMP_SLOT_PULL,
    RTMP_SLOT_NUM,
} rtmp_slot_t;

typedef struct {
    esp_rtmp_service_t *server;  /*!< Relay server, no media link */
    char                server_url[RTMP_URL_MAX_LEN];
    uint16_t            server_port;
    char                server_app[RTMP_NAME_MAX_LEN];
    char                server_stream[RTMP_NAME_MAX_LEN];

    esp_capture_service_t *capture;  /*!< Push slot: camera and microphone source */
    esp_rtmp_service_t    *sink;
    char                   push_url[RTMP_URL_MAX_LEN];
    bool                   push_linked;
    bool                   push_peer_gone;

    esp_rtmp_service_t   *src;     /*!< Pull slot: remote stream source */
    esp_player_service_t *player;  /*!< Pull slot: LCD and speaker sink */
    char                  pull_url[RTMP_URL_MAX_LEN];
    bool                  pull_linked;
    bool                  pull_peer_gone;

    rtmp_session_opts_t  media;     /*!< Media settings of the last client slot started */
    bool                 stopping;  /*!< Set while a teardown is in progress */
} rtmp_session_t;

static const char *TAG = "RTMP_SESSION";

static rtmp_session_t s_session;

/* Subscriptions hand the slot back to the event handler, which is all it needs
   to tell the pusher and the puller apart. */
static const rtmp_slot_t s_slot_ids[RTMP_SLOT_NUM] = {
    RTMP_SLOT_SERVER,
    RTMP_SLOT_PUSH,
    RTMP_SLOT_PULL,
};

static const char *slot_name(rtmp_slot_t slot)
{
    switch (slot) {
        case RTMP_SLOT_SERVER:
            return "server";
        case RTMP_SLOT_PUSH:
            return "push";
        case RTMP_SLOT_PULL:
            return "pull";
        default:
            return "unknown";
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
        case ESP_CAPTURE_FMT_ID_PCM:
            return "pcm";
        case ESP_CAPTURE_FMT_ID_G711A:
            return "g711a";
        case ESP_CAPTURE_FMT_ID_G711U:
            return "g711u";
        default:
            return "none";
    }
}

static bool url_is_secure(const char *url)
{
    return strncmp(url, "rtmps://", strlen("rtmps://")) == 0;
}

static esp_rtmp_service_t *slot_rtmp(rtmp_slot_t slot)
{
    switch (slot) {
        case RTMP_SLOT_SERVER:
            return s_session.server;
        case RTMP_SLOT_PUSH:
            return s_session.sink;
        case RTMP_SLOT_PULL:
            return s_session.src;
        default:
            return NULL;
    }
}

static void mark_peer_gone(rtmp_slot_t slot)
{
    if (slot == RTMP_SLOT_PUSH) {
        s_session.push_peer_gone = true;
    } else if (slot == RTMP_SLOT_PULL) {
        s_session.pull_peer_gone = true;
    }
}

/* Server events name the stream a client works on, which is the only way to see
   who is publishing and who is playing without a PC-side tool. */
static void rtmp_event_handler(const adf_event_t *event, void *ctx)
{
    rtmp_slot_t slot = *(const rtmp_slot_t *)ctx;
    esp_rtmp_service_t *rtmp = slot_rtmp(slot);

    const char *name = "UNKNOWN";
    const char *resolved = NULL;
    if (rtmp != NULL &&
        esp_service_get_event_name(ESP_SERVICE_BASE(rtmp), event->event_id, &resolved) == ESP_OK &&
        resolved != NULL) {
        name = resolved;
    }

    if (event->event_id >= ESP_RTMP_SERVICE_EVENT_PEER_CLOSED &&
        event->event_id <= ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STOPPED &&
        event->payload != NULL && event->payload_len >= sizeof(esp_rtmp_service_event_payload_t)) {
        const esp_rtmp_service_event_payload_t *payload = event->payload;
        if (payload->stream_name[0] != '\0') {
            ESP_LOGI(TAG, "%s: %s (stream: %s)", slot_name(slot), name, payload->stream_name);
        } else {
            ESP_LOGI(TAG, "%s: %s", slot_name(slot), name);
        }

        if (event->event_id == ESP_RTMP_SERVICE_EVENT_PEER_CLOSED) {
            mark_peer_gone(slot);
            /* Closing our own socket raises this too, so the hint would tell the
               user to run the command they are already running */
            if (!s_session.stopping) {
                ESP_LOGW(TAG, "%s peer closed the connection; 'rtmp stop' or the next start releases it",
                         slot_name(slot));
            }
        }
        return;
    }

    if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED && event->payload != NULL &&
        event->payload_len >= sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *st = event->payload;
        if (st->new_state == ESP_SERVICE_STATE_ERROR) {
            mark_peer_gone(slot);
            ESP_LOGE(TAG, "%s RTMP service entered the error state", slot_name(slot));
        }
    }
}

static esp_err_t subscribe_events(rtmp_slot_t slot, esp_rtmp_service_t *rtmp)
{
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = rtmp_event_handler;
    info.handler_ctx = (void *)&s_slot_ids[slot];
    return esp_service_event_subscribe(ESP_SERVICE_BASE(rtmp), &info);
}

static void rtmp_service_delete(esp_rtmp_service_t *rtmp)
{
    if (rtmp == NULL) {
        return;
    }
    (void)esp_media_service_deinit(ESP_SERVICE_BASE(rtmp));
    free(rtmp);
}

/* An rtmps:// URL verifies the ingest server against the IDF certificate bundle.
   '--insecure' leaves the TLS config empty, which esp-tls accepts only because
   sdkconfig.defaults enables CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY. */
static void apply_client_tls(esp_rtmp_service_setup_t *setup, const char *url, bool insecure)
{
    if (!url_is_secure(url)) {
        return;
    }
    if (insecure) {
        ESP_LOGW(TAG, "Connecting to %s without verifying its certificate", url);
        return;
    }
#ifdef CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    setup->ssl_cfg.client.crt_bundle_attach = esp_crt_bundle_attach;
#else
    ESP_LOGW(TAG, "No certificate bundle in this build; retry with '--insecure'");
#endif  /* CONFIG_MBEDTLS_CERTIFICATE_BUNDLE */
}

static uint32_t video_bitrate(const rtmp_session_opts_t *opts)
{
    if (opts->bitrate != 0) {
        return opts->bitrate;
    }
    return (uint32_t)opts->width * opts->height * opts->fps / RTMP_VIDEO_BITRATE_DIVISOR;
}

/* Skips a leading '/' so '--app /live' and '--app live' behave the same */
static const char *bare_name(const char *name, const char *fallback)
{
    if (name == NULL || name[0] == '\0') {
        return fallback;
    }
    return (name[0] == '/') ? name + 1 : name;
}

/* Points an on-board client role at the on-board server over the loopback interface */
static void build_local_url(char *buf, size_t buf_len, const rtmp_session_opts_t *opts)
{
    snprintf(buf, buf_len, "rtmp://%s:%u/%s/%s", RTMP_LOCAL_HOST, (unsigned)opts->port,
             bare_name(opts->app, RTMP_SERVER_APP), bare_name(opts->stream, RTMP_SERVER_STREAM));
}

static void remember_media(const rtmp_session_opts_t *opts)
{
    s_session.media = *opts;
    /* The caller owns these strings; the per-slot buffers hold the copies */
    s_session.media.url = NULL;
    s_session.media.app = NULL;
    s_session.media.stream = NULL;
}

/* Camera plus microphone into one elementary-stream output. A zero codec leaves
   that track out, which is how '-v none' and '-a none' work. */
static esp_err_t build_capture(const rtmp_session_opts_t *opts, esp_capture_service_t **out_capture)
{
    if (opts->video_codec != 0) {
        void *cam = NULL;
        if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&cam) != ESP_OK) {
            ESP_LOGE(TAG, "No camera on this board; retry with '-v none' for an audio-only stream");
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_video_capture_service_cfg_t cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    cfg.service_name = "rtmp_capture";
    cfg.audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    cfg.max_stream_num = 1;

    esp_capture_service_t *capture = NULL;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&cfg, &capture), TAG, "Create capture service");

    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = (opts->audio_codec != 0) ? RTMP_AUDIO_SAMPLE_RATE : 0,
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
            .bits_per_sample = RTMP_AUDIO_BITS_PER_SAMPLE,
            .channel = RTMP_AUDIO_CHANNEL,
            .bitrate = RTMP_AUDIO_BITRATE,
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
    cfg.name = "rtmp_player";
    cfg.max_stream_num = 1;

    esp_player_service_t *player = NULL;
    ESP_RETURN_ON_ERROR(esp_video_player_service_create(&cfg, &player), TAG, "Create player service");

    esp_err_t ret = ESP_OK;

    /* ADC and DAC share one I2S clock on these boards, so the DAC opens at
       RTMP_AUDIO_SAMPLE_RATE as well. The renderer resamples into that rate. */
    esp_audio_player_service_setup_t audio_setup = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    audio_setup.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    audio_setup.fixed_out_sample_info.sample_rate = RTMP_AUDIO_SAMPLE_RATE;
    ret = esp_audio_player_service_apply_setup(player, &audio_setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply speaker setup failed: %s", esp_err_to_name(ret));
        (void)esp_player_service_destroy(player);
        return ret;
    }

    esp_video_player_service_setup_t setup = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
    setup.display_dev_name = ESP_BOARD_DEVICE_NAME_DISPLAY_LCD;
    setup.audio_dev_name = NULL;
    void *lcd = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd) != ESP_OK) {
        ESP_LOGW(TAG, "No display on this board; received video is dropped");
        setup.display_dev_name = NULL;
    }

    ret = esp_video_player_service_apply_setup(player, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply player setup failed: %s", esp_err_to_name(ret));
        (void)esp_player_service_destroy(player);
        return ret;
    }

    *out_player = player;
    return ESP_OK;
}

/* Create, configure and subscribe one RTMP service. The handle lands in the slot
   before the subscription so the event handler can always resolve it. */
static esp_err_t build_rtmp(rtmp_slot_t slot, const rtmp_session_opts_t *opts, const char *url,
                            esp_rtmp_service_t **slot_handle)
{
    esp_rtmp_service_role_t role = ESP_RTMP_SERVICE_ROLE_SERVER;
    esp_rtmp_service_setup_t setup = ESP_RTMP_SERVICE_SERVER_SETUP_DEFAULT();

    if (slot == RTMP_SLOT_PUSH) {
        role = ESP_RTMP_SERVICE_ROLE_SINK;
        setup = (esp_rtmp_service_setup_t)ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT();
        apply_client_tls(&setup, url, opts->tls_insecure);
    } else if (slot == RTMP_SLOT_PULL) {
        role = ESP_RTMP_SERVICE_ROLE_SRC;
        setup = (esp_rtmp_service_setup_t)ESP_RTMP_SERVICE_SRC_SETUP_DEFAULT();
        setup.src.audio_cache_size = opts->audio_cache;
        setup.src.video_cache_size = opts->video_cache;
        apply_client_tls(&setup, url, opts->tls_insecure);
    } else {
        setup.server.port = opts->port;
        setup.server.app_name = bare_name(opts->app, RTMP_SERVER_APP);
        setup.server.max_clients = opts->max_clients;
    }
    setup.chunk_size = opts->chunk_size;

    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(role);
    cfg.name = slot_name(slot);

    esp_rtmp_service_t *rtmp = NULL;
    ESP_RETURN_ON_ERROR(esp_rtmp_service_create(&cfg, &rtmp), TAG, "Create RTMP service");
    *slot_handle = rtmp;

    esp_err_t ret = subscribe_events(slot, rtmp);
    if (ret == ESP_OK) {
        ret = esp_rtmp_service_setup(rtmp, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_rtmp_service_set_url(rtmp, url);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set up the %s RTMP service failed: %s", slot_name(slot), esp_err_to_name(ret));
        rtmp_service_delete(rtmp);
        *slot_handle = NULL;
    }
    return ret;
}

static void server_slot_release(void)
{
    if (s_session.server != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.server));
        rtmp_service_delete(s_session.server);
        s_session.server = NULL;
    }
    s_session.server_url[0] = '\0';
}

static void push_slot_release(void)
{
    /* The producer stops first so the sink drains instead of blocking on an empty queue */
    if (s_session.capture != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.capture));
    }
    if (s_session.sink != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.sink));
    }
    if (s_session.push_linked) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(s_session.sink), ESP_MEDIA_DEFAULT_STREAM);
        s_session.push_linked = false;
    }

    rtmp_service_delete(s_session.sink);
    s_session.sink = NULL;
    if (s_session.capture != NULL) {
        (void)esp_capture_service_destroy(s_session.capture);
        s_session.capture = NULL;
    }
    s_session.push_url[0] = '\0';
    s_session.push_peer_gone = false;
}

static void pull_slot_release(void)
{
    if (s_session.src != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.src));
    }
    if (s_session.player != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.player));
    }
    if (s_session.pull_linked) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        s_session.pull_linked = false;
    }

    rtmp_service_delete(s_session.src);
    s_session.src = NULL;
    if (s_session.player != NULL) {
        (void)esp_player_service_destroy(s_session.player);
        s_session.player = NULL;
    }
    s_session.pull_url[0] = '\0';
    s_session.pull_peer_gone = false;
}

void rtmp_session_opts_default(rtmp_session_opts_t *opts)
{
    if (opts == NULL) {
        return;
    }
    *opts = (rtmp_session_opts_t) {
        .url = NULL,
        .port = RTMP_SERVER_PORT,
        .app = RTMP_SERVER_APP,
        .stream = RTMP_SERVER_STREAM,
        .max_clients = RTMP_SERVER_MAX_CLIENTS,
        .chunk_size = RTMP_CHUNK_SIZE,
        .video_codec = RTMP_VIDEO_CODEC,
        .audio_codec = RTMP_AUDIO_CODEC,
        .width = RTMP_VIDEO_WIDTH,
        .height = RTMP_VIDEO_HEIGHT,
        .fps = RTMP_VIDEO_FPS,
        .bitrate = 0,
        .sample_rate = RTMP_AUDIO_SAMPLE_RATE,
        .audio_cache = RTMP_RECV_AUDIO_CACHE,
        .video_cache = RTMP_RECV_VIDEO_CACHE,
        .tls_insecure = false,
    };
}

esp_err_t rtmp_session_start_server(const rtmp_session_opts_t *opts)
{
    if (opts == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_session.server != NULL) {
        ESP_LOGI(TAG, "Restarting the local server");
        server_slot_release();
    }

    const char *app = bare_name(opts->app, RTMP_SERVER_APP);
    snprintf(s_session.server_app, sizeof(s_session.server_app), "%s", app);
    snprintf(s_session.server_stream, sizeof(s_session.server_stream), "%s",
             bare_name(opts->stream, RTMP_SERVER_STREAM));
    s_session.server_port = opts->port;
    snprintf(s_session.server_url, sizeof(s_session.server_url), "rtmp://0.0.0.0:%u/%s",
             (unsigned)opts->port, app);

    esp_err_t ret = build_rtmp(RTMP_SLOT_SERVER, opts, s_session.server_url, &s_session.server);
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.server));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start the local server failed: %s", esp_err_to_name(ret));
        server_slot_release();
        return ret;
    }

    char ip[16];
    rtmp_example_fill_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "Local RTMP server listening on port %u, app '%s', up to %u clients",
             (unsigned)opts->port, app, (unsigned)opts->max_clients);
    ESP_LOGI(TAG, "Publish into it with: ffmpeg ... -f flv rtmp://%s:%u/%s/%s",
             ip, (unsigned)opts->port, app, s_session.server_stream);
    ESP_LOGI(TAG, "Play from it with:    ffplay rtmp://%s:%u/%s/%s",
             ip, (unsigned)opts->port, app, s_session.server_stream);
    return ESP_OK;
}

/* Capture (SRC) -> RTMP (SINK) */
esp_err_t rtmp_session_start_push(const rtmp_session_opts_t *opts)
{
    if (opts == NULL || opts->url == NULL || opts->url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (opts->video_codec == 0 && opts->audio_codec == 0) {
        ESP_LOGE(TAG, "Nothing to publish: enable at least one of audio and video");
        return ESP_ERR_INVALID_ARG;
    }
    if (s_session.capture != NULL || s_session.sink != NULL) {
        ESP_LOGI(TAG, "Restarting the publisher");
        push_slot_release();
    }

    snprintf(s_session.push_url, sizeof(s_session.push_url), "%s", opts->url);
    remember_media(opts);

    esp_err_t ret = build_capture(opts, &s_session.capture);
    if (ret == ESP_OK) {
        ret = build_rtmp(RTMP_SLOT_PUSH, opts, s_session.push_url, &s_session.sink);
    }
    /* Link before starting so the sink can read track info from the provider */
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(s_session.sink), ESP_MEDIA_DEFAULT_STREAM);
        s_session.push_linked = (ret == ESP_OK);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.sink));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.capture));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start the publisher failed: %s", esp_err_to_name(ret));
        push_slot_release();
        return ret;
    }

    ESP_LOGI(TAG, "Publishing to %s (video: %s, audio: %s)", s_session.push_url,
             codec_name(opts->video_codec), codec_name(opts->audio_codec));
    if (opts->video_codec != 0) {
        ESP_LOGI(TAG, "Video encoded at %ux%u %u fps, %" PRIu32 " kbps",
                 (unsigned)opts->width, (unsigned)opts->height, (unsigned)opts->fps,
                 video_bitrate(opts) / 1000);
    }
    return ESP_OK;
}

/* RTMP (SRC) -> video player (SINK). Tracks arrive from the RTMP metadata. */
esp_err_t rtmp_session_start_pull(const rtmp_session_opts_t *opts)
{
    if (opts == NULL || opts->url == NULL || opts->url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_session.src != NULL || s_session.player != NULL) {
        ESP_LOGI(TAG, "Restarting the player");
        pull_slot_release();
    }

    snprintf(s_session.pull_url, sizeof(s_session.pull_url), "%s", opts->url);
    remember_media(opts);

    esp_err_t ret = build_player(&s_session.player);
    if (ret == ESP_OK) {
        ret = build_rtmp(RTMP_SLOT_PULL, opts, s_session.pull_url, &s_session.src);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        s_session.pull_linked = (ret == ESP_OK);
    }
    /* Sink first: the RTMP client receives as soon as the play command succeeds.
       RTMP announces audio and video as separate FLV headers once the session is
       running, so the player picks the second one up mid-flight. */
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.player));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.src));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start the player failed: %s", esp_err_to_name(ret));
        pull_slot_release();
        return ret;
    }

    ESP_LOGI(TAG, "Playing %s", s_session.pull_url);
    return ESP_OK;
}

esp_err_t rtmp_session_start_live(const rtmp_session_opts_t *opts)
{
    if (opts == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(rtmp_session_start_server(opts), TAG, "Start the local server");

    /* Give the listen socket time to bind before the local publisher connects */
    vTaskDelay(pdMS_TO_TICKS(RTMP_SERVER_SETTLE_MS));

    char url[RTMP_URL_MAX_LEN];
    build_local_url(url, sizeof(url), opts);

    rtmp_session_opts_t local = *opts;
    local.url = url;
    esp_err_t ret = rtmp_session_start_push(&local);
    if (ret != ESP_OK) {
        server_slot_release();
        return ret;
    }

    char ip[16];
    rtmp_example_fill_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "Live stream ready: ffplay rtmp://%s:%u/%s/%s", ip, (unsigned)opts->port,
             s_session.server_app, s_session.server_stream);
    return ESP_OK;
}

esp_err_t rtmp_session_start_loopback(const rtmp_session_opts_t *opts)
{
    ESP_RETURN_ON_ERROR(rtmp_session_start_live(opts), TAG, "Start the local live stream");

    /* The puller only finds the stream once the publisher has announced it */
    vTaskDelay(pdMS_TO_TICKS(RTMP_SERVER_SETTLE_MS));

    char url[RTMP_URL_MAX_LEN];
    build_local_url(url, sizeof(url), opts);

    rtmp_session_opts_t local = *opts;
    local.url = url;
    esp_err_t ret = rtmp_session_start_pull(&local);
    if (ret != ESP_OK) {
        push_slot_release();
        server_slot_release();
        return ret;
    }

    ESP_LOGI(TAG, "Loopback running: camera -> SINK -> local server -> SRC -> LCD");
    return ESP_OK;
}

esp_err_t rtmp_session_stop(void)
{
    if (!rtmp_session_is_active()) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping every RTMP slot");
    s_session.stopping = true;
    /* Clients leave before the server, so it never tears down with sessions
       attached. Among the clients the puller goes first: a server that loses its
       publisher drops the subscribers itself, and that cascade would race with
       this teardown. */
    pull_slot_release();
    push_slot_release();
    server_slot_release();
    s_session.stopping = false;
    ESP_LOGI(TAG, "All RTMP slots released");
    return ESP_OK;
}

bool rtmp_session_is_active(void)
{
    return s_session.server != NULL || s_session.sink != NULL || s_session.capture != NULL ||
           s_session.src != NULL || s_session.player != NULL;
}

void rtmp_session_print_info(void)
{
    char ip[16];
    rtmp_example_fill_ip(ip, sizeof(ip));
    printf("device ip  : %s\n", ip);

    if (!rtmp_session_is_active()) {
        printf("slots      : idle\n");
        printf("hint       : 'rtmp live' then ffplay rtmp://%s:%d/%s/%s\n",
               ip, RTMP_SERVER_PORT, RTMP_SERVER_APP, RTMP_SERVER_STREAM);
        return;
    }

    const rtmp_session_opts_t *media = &s_session.media;

    if (s_session.server != NULL) {
        printf("server     : listening on port %u, app '%s'\n",
               (unsigned)s_session.server_port, s_session.server_app);
        printf("play from  : ffplay rtmp://%s:%u/%s/%s\n", ip, (unsigned)s_session.server_port,
               s_session.server_app, s_session.server_stream);
    } else {
        printf("server     : stopped\n");
    }

    if (s_session.sink != NULL) {
        printf("push       : %s%s\n", s_session.push_url, s_session.push_peer_gone ? " (peer left)" : "");
        if (media->video_codec != 0) {
            printf("  video    : %s %ux%u@%ufps %" PRIu32 " kbps%s\n", codec_name(media->video_codec),
                   (unsigned)media->width, (unsigned)media->height, (unsigned)media->fps,
                   video_bitrate(media) / 1000, media->bitrate != 0 ? "" : " (derived)");
        } else {
            printf("  video    : disabled\n");
        }
        if (media->audio_codec != 0) {
            printf("  audio    : %s %" PRIu32 " Hz %d ch\n", codec_name(media->audio_codec),
                   media->sample_rate, RTMP_AUDIO_CHANNEL);
        } else {
            printf("  audio    : disabled\n");
        }
    } else {
        printf("push       : stopped\n");
    }

    if (s_session.src != NULL) {
        /* Codecs come from the RTMP metadata, so they are not known up front */
        printf("pull       : %s%s\n", s_session.pull_url, s_session.pull_peer_gone ? " (peer left)" : "");
        printf("  tracks   : from the stream metadata\n");
    } else {
        printf("pull       : stopped\n");
    }

    if (s_session.server != NULL) {
        printf("server sessions:\n");
        (void)esp_rtmp_service_query(s_session.server);
    }
}
