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
#include "esp_codec_dev.h"
#include "esp_log.h"

#include "esp_audio_player_service_setup.h"
#include "esp_audio_capture_service.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_capture_advance.h"
#include "esp_capture_service.h"
#include "esp_capture_service_ops.h"
#include "esp_gmf_err.h"
#include "esp_gmf_video_enc.h"
#include "esp_media_service.h"
#include "esp_player_service.h"
#include "esp_service.h"
#include "esp_sip_service.h"
#include "esp_sip_service_ops.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

#include "sip_example.h"
#include "sip_session.h"
#include "sip_settings.h"

#define SIP_PEER_MAX_LEN  (96)

/**
 * @brief  Where the call currently is, mirrored from service events
 */
typedef enum {
    CALL_IDLE = 0,
    CALL_RINGING,
    CALL_OUTGOING,
    CALL_ACTIVE,
} call_state_t;

/**
 * @brief  Resources and state owned by one SIP session
 */
typedef struct {
    bool                   active;
    sip_session_opts_t     opts;
    char                   user[32];
    char                   password[48];
    char                   server[64];
    char                   transport[8];
    esp_capture_service_t *capture;  /*!< Microphone, plus camera when video is enabled */
    esp_player_service_t  *player;   /*!< Speaker and LCD draining the downlink */
    esp_sip_service_t     *sip;      /*!< Full-duplex SIP service */
    bool                   linked_up;
    bool                   linked_down;
    bool                   registered;
    bool                   auto_answer;
    call_state_t           call;
    char                   peer[SIP_PEER_MAX_LEN];
} sip_session_t;

static const char *TAG = "SIP_SESSION";

static sip_session_t s_session;

static const char *call_state_name(call_state_t state)
{
    switch (state) {
        case CALL_RINGING:
            return "ringing";
        case CALL_OUTGOING:
            return "calling";
        case CALL_ACTIVE:
            return "in call";
        default:
            return "idle";
    }
}

static const char *codec_name(esp_media_codec_fourcc_t codec)
{
    switch (codec) {
        case ESP_CAPTURE_FMT_ID_G711A:
            return "g711a";
        case ESP_CAPTURE_FMT_ID_G711U:
            return "g711u";
        case ESP_CAPTURE_FMT_ID_OPUS:
            return "opus";
        case ESP_CAPTURE_FMT_ID_H264:
            return "h264";
        case ESP_CAPTURE_FMT_ID_MJPEG:
            return "mjpeg";
        default:
            return "none";
    }
}

static const char *srtp_mode_name(esp_rtc_srtp_mode_t mode)
{
    switch (mode) {
        case ESP_RTC_SRTP_PREFER:
            return "prefer";
        case ESP_RTC_SRTP_REQUIRED:
            return "required";
        default:
            return "off";
    }
}

static esp_gmf_element_handle_t get_uplink_vid_enc(void)
{
    if (s_session.capture == NULL || s_session.opts.video_codec == 0) {
        return NULL;
    }
    esp_capture_sink_handle_t sink = NULL;
    if (esp_capture_service_get_sink_handle(s_session.capture, ESP_MEDIA_DEFAULT_STREAM, &sink) != ESP_OK) {
        return NULL;
    }
    esp_gmf_element_handle_t venc = NULL;
    if (esp_capture_sink_get_element_by_tag(sink, ESP_CAPTURE_STREAM_TYPE_VIDEO, "vid_enc", &venc) != ESP_OK) {
        return NULL;
    }
    return venc;
}

static void apply_mic_gain(float db)
{
    dev_audio_codec_handles_t *adc = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc) != ESP_OK ||
        adc == NULL || adc->codec_dev == NULL) {
        ESP_LOGW(TAG, "No audio ADC on this board");
        return;
    }
    if (esp_codec_dev_set_in_gain(adc->codec_dev, db) != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Set mic gain %.1f dB failed", db);
        return;
    }
    ESP_LOGI(TAG, "Mic gain %.1f dB", db);
}

static void configure_uplink_encoder(void)
{
    esp_gmf_element_handle_t venc = get_uplink_vid_enc();
    if (venc == NULL) {
        return;
    }
    /* Capture starts at REGISTER, so the encoder is mid-GOP when RTP begins and
       the peer shows nothing until the next SPS/PPS. A short GOP bounds that
       wait to two seconds. */
    uint32_t gop = (uint32_t)s_session.opts.fps * 2U;
    if (gop < 10) {
        gop = 10;
    }
    (void)esp_gmf_video_enc_set_gop(venc, gop);
    ESP_LOGI(TAG, "Uplink H.264 GOP %u", (unsigned)gop);
}

static const void *sip_event_typed_payload(const adf_event_t *event, size_t need)
{
    if (event == NULL || event->payload == NULL || event->payload_len < need) {
        return NULL;
    }
    return event->payload;
}

/* Answering, DTMF replies and similar policy live here rather than in the
   service, which only reports what the protocol did. */
static void sip_event_handler(const adf_event_t *event, void *ctx)
{
    esp_service_t *service = (esp_service_t *)ctx;
    const char *name = "UNKNOWN";
    const char *resolved = NULL;
    if (esp_service_get_event_name(service, event->event_id, &resolved) == ESP_OK && resolved != NULL) {
        name = resolved;
    }

    switch (event->event_id) {
        case ESP_SIP_SERVICE_EVENT_REGISTERED:
            s_session.registered = true;
            ESP_LOGI(TAG, "%s: ready to place and receive calls", name);
            break;
        case ESP_SIP_SERVICE_EVENT_UNREGISTERED:
            s_session.registered = false;
            ESP_LOGW(TAG, "%s: registration lost", name);
            break;
        case ESP_SIP_SERVICE_EVENT_INCOMING: {
            const esp_sip_service_call_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            /* The protocol repeats this while the peer keeps ringing, so the
               auto answer must only fire on the first one. */
            if (s_session.call != CALL_RINGING) {
                s_session.call = CALL_RINGING;
                if (payload != NULL) {
                    snprintf(s_session.peer, sizeof(s_session.peer), "%s", payload->peer);
                }
                ESP_LOGI(TAG, "%s from %s", name, s_session.peer);
                if (s_session.auto_answer) {
                    esp_err_t ret = esp_sip_service_answer(s_session.sip);
                    ESP_LOGI(TAG, "Auto answering: %s", esp_err_to_name(ret));
                } else {
                    ESP_LOGI(TAG, "Run 'sip answer' to pick up, or 'sip bye' to reject");
                }
            }
            break;
        }
        case ESP_SIP_SERVICE_EVENT_CALLING: {
            const esp_sip_service_call_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            s_session.call = CALL_OUTGOING;
            if (payload != NULL) {
                snprintf(s_session.peer, sizeof(s_session.peer), "%s", payload->peer);
            }
            ESP_LOGI(TAG, "%s %s", name, s_session.peer);
            break;
        }
        case ESP_SIP_SERVICE_EVENT_CALL_ANSWERED: {
            const esp_sip_service_call_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            s_session.call = CALL_ACTIVE;
            ESP_LOGI(TAG, "%s: talking to %s%s", name, s_session.peer,
                     (payload != NULL && payload->srtp_active) ? " (SRTP)" : "");
            break;
        }
        case ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_BEGIN:
            ESP_LOGI(TAG, "%s: video RTP started", name);
            break;
        case ESP_SIP_SERVICE_EVENT_HANGUP: {
            const esp_sip_service_hangup_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            s_session.call = CALL_IDLE;
            s_session.peer[0] = '\0';
            ESP_LOGI(TAG, "%s%s%s", name, (payload != NULL && payload->reason[0] != '\0') ? ": " : "",
                     (payload != NULL) ? payload->reason : "");
            break;
        }
        case ESP_SIP_SERVICE_EVENT_ERROR: {
            const esp_sip_service_error_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            /* A rejected INVITE reports this instead of HANGUP, so the call has
               to be cleared here as well. */
            s_session.call = CALL_IDLE;
            s_session.peer[0] = '\0';
            if (payload != NULL && payload->reject_reason != ESP_RTC_REJECT_NONE) {
                ESP_LOGE(TAG, "%s: peer rejected the SRTP offer (reason %d)", name,
                         (int)payload->reject_reason);
            } else {
                ESP_LOGE(TAG, "%s: call failed%s%s", name,
                         (payload != NULL && payload->reason[0] != '\0') ? ": " : "",
                         (payload != NULL) ? payload->reason : "");
            }
            break;
        }
        case ESP_SIP_SERVICE_EVENT_MESSAGE: {
            const esp_sip_service_message_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            /* Deciding what to reply is application policy, so the example
               just prints it. The body is already a copy owned by the event. */
            ESP_LOGI(TAG, "%s from %s [%s]: %.*s%s", name, (payload != NULL) ? payload->peer : "",
                     (payload != NULL) ? payload->content_type : "",
                     (payload != NULL) ? payload->body_len : 0,
                     (payload != NULL) ? payload->body : "",
                     (payload != NULL && payload->body_truncated) ? " ..." : "");
            break;
        }
        case ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED: {
            const esp_sip_service_dtmf_payload_t *payload =
                sip_event_typed_payload(event, sizeof(*payload));
            ESP_LOGI(TAG, "%s: %s", name, (payload != NULL) ? payload->dtmf : "");
            break;
        }
        case ESP_SERVICE_EVENT_STATE_CHANGED:
            break;
        default:
            ESP_LOGI(TAG, "%s", name);
            break;
    }
}

static uint32_t video_bitrate(const sip_session_opts_t *opts)
{
    if (opts->bitrate != 0) {
        return opts->bitrate;
    }
    return (uint32_t)opts->width * opts->height * opts->fps / SIP_VIDEO_BITRATE_DIVISOR;
}

static esp_media_audio_info_t uplink_audio_info(const sip_session_opts_t *opts)
{
    return (esp_media_audio_info_t) {
        .codec = opts->audio_codec,
        .sample_rate = opts->sample_rate,
        .bits_per_sample = SIP_AUDIO_BITS,
        .channel = SIP_AUDIO_CHANNEL,
        .bitrate = SIP_AUDIO_BITRATE,
    };
}

/* Pure audio uses esp_audio_capture_service; A/V uses esp_video_capture_service.
   A zero codec leaves that track out of the elementary-stream output. */
static esp_err_t build_capture(const sip_session_opts_t *opts, esp_capture_service_t **out_capture)
{
    esp_capture_service_t *capture = NULL;
    esp_err_t ret;

    if (opts->video_codec == 0) {
        esp_audio_capture_service_cfg_t cfg = ESP_AUDIO_CAPTURE_SERVICE_CFG_DEFAULT();
        cfg.service_name = "sip_capture";
        cfg.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
        cfg.max_stream_num = 1;
        ESP_RETURN_ON_ERROR(esp_audio_capture_service_create(&cfg, &capture), TAG, "Create audio capture service");

        esp_audio_capture_service_setup_t setup = {
            .stream_num = 1,
            .fixed_src_sample_rate = opts->sample_rate,
            .streams[0] = {
                .enabled = true,
                .audio_info = uplink_audio_info(opts),
            },
        };
        ret = esp_audio_capture_service_apply_setup(capture, &setup);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Apply audio capture setup failed: %s", esp_err_to_name(ret));
            (void)esp_capture_service_destroy(capture);
            return ret;
        }
        *out_capture = capture;
        return ESP_OK;
    }

    void *cam = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_CAMERA, (void **)&cam) != ESP_OK) {
        ESP_LOGE(TAG, "No camera on this board; start again without video");
        return ESP_ERR_NOT_FOUND;
    }

    esp_video_capture_service_cfg_t cfg = ESP_VIDEO_CAPTURE_SERVICE_CFG_DEFAULT();
    cfg.service_name = "sip_capture";
    cfg.audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    cfg.video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA;
    cfg.max_stream_num = 1;
    ESP_RETURN_ON_ERROR(esp_video_capture_service_create(&cfg, &capture), TAG, "Create capture service");

    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = (opts->audio_codec != 0) ? opts->sample_rate : 0,
        .streams[0].enabled = true,
        .streams[0].video_info = (esp_media_video_info_t) {
            .codec = opts->video_codec,
            .width = opts->width,
            .height = opts->height,
            .fps = opts->fps,
            .bitrate = video_bitrate(opts),
        },
    };
    if (opts->audio_codec != 0) {
        setup.streams[0].audio_info = uplink_audio_info(opts);
    }

    ret = esp_video_capture_service_apply_setup(capture, &setup);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Apply capture setup failed: %s", esp_err_to_name(ret));
        (void)esp_capture_service_destroy(capture);
        return ret;
    }

    *out_capture = capture;
    return ESP_OK;
}

static esp_err_t build_player(const sip_session_opts_t *opts, esp_player_service_t **out_player)
{
    esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
    cfg.name = "sip_player";
    cfg.max_stream_num = 1;

    esp_player_service_t *player = NULL;
    ESP_RETURN_ON_ERROR(esp_video_player_service_create(&cfg, &player), TAG, "Create player service");

    /* The microphone and the speaker share one I2S clock on these boards, and
       a call runs both at once, so the DAC has to open at the same rate the
       capture service pins. Leaving it at the 48 kHz default makes the codec
       device refuse the duplex bus and the uplink goes silent. */
    esp_audio_player_service_setup_t audio_setup = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
    audio_setup.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
    audio_setup.fixed_out_sample_info.sample_rate = opts->sample_rate;
    esp_err_t ret = esp_audio_player_service_apply_setup(player, &audio_setup);
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

static esp_err_t build_sip(const sip_session_opts_t *opts, esp_sip_service_t **out_sip)
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    esp_sip_service_t *sip = NULL;
    ESP_RETURN_ON_ERROR(esp_sip_service_create(&cfg, &sip), TAG, "Create SIP service");

    esp_service_t *base = ESP_SERVICE_BASE(sip);
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = sip_event_handler;
    info.handler_ctx = base;

    esp_err_t ret = esp_service_event_subscribe(base, &info);
    if (ret == ESP_OK) {
        /* The linked capture tracks override these and also bring the video
           geometry; the codecs here are what a receive-only run falls back to. */
        esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
        setup.audio_codec = opts->audio_codec;
        setup.video_codec = opts->video_codec;
        setup.p2p_mode = opts->p2p_mode;
        ret = esp_sip_service_setup(sip, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_sip_service_set_srtp_mode(sip, opts->srtp_mode);
    }
    if (ret == ESP_OK) {
        ret = esp_sip_service_set_local_port(sip, opts->local_port);
    }
    if (ret == ESP_OK) {
        ret = esp_sip_service_set_cache_size(sip, SIP_RECV_AUDIO_CACHE, SIP_RECV_VIDEO_CACHE);
    }
    if (ret == ESP_OK) {
        esp_sip_service_account_t account = {
            .transport = opts->transport,
            .user = opts->user,
            .password = opts->password,
            .server = opts->server,
            .port = opts->server_port,
        };
        ret = esp_sip_service_set_account(sip, &account);
    }
    if (ret == ESP_OK) {
        /* The stack advertises this in Via / Contact. Leaving it NULL produces
           REGISTER lines like "UDP :12103" and the registrar cannot reply. */
        char ip[16];
        sip_example_fill_ip(ip, sizeof(ip));
        if (strcmp(ip, "DEVICE_IP") == 0) {
            ESP_LOGE(TAG, "No station IP; connect Wi-Fi before 'sip start'");
            ret = ESP_ERR_INVALID_STATE;
        } else {
            ret = esp_sip_service_set_local_addr(sip, ip);
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set up SIP service failed: %s", esp_err_to_name(ret));
        (void)esp_media_service_deinit(base);
        free(sip);
        return ret;
    }

    *out_sip = sip;
    return ESP_OK;
}

static void session_release(void)
{
    /* Producers stop first so consumers drain instead of blocking on an empty
       queue: capture feeds the uplink, and the SIP downlink feeds the player. */
    if (s_session.capture != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.capture));
    }
    if (s_session.sip != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.sip));
    }
    if (s_session.player != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(s_session.player));
    }

    if (s_session.linked_up) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(s_session.sip), ESP_SIP_SERVICE_STREAM_UPLINK);
        s_session.linked_up = false;
    }
    if (s_session.linked_down) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(s_session.sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                       ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        s_session.linked_down = false;
    }

    if (s_session.sip != NULL) {
        (void)esp_media_service_deinit(ESP_SERVICE_BASE(s_session.sip));
        free(s_session.sip);
        s_session.sip = NULL;
    }
    if (s_session.capture != NULL) {
        (void)esp_capture_service_destroy(s_session.capture);
        s_session.capture = NULL;
    }
    if (s_session.player != NULL) {
        (void)esp_player_service_destroy(s_session.player);
        s_session.player = NULL;
    }

    s_session.active = false;
    s_session.registered = false;
    s_session.call = CALL_IDLE;
    s_session.peer[0] = '\0';
}

void sip_session_opts_default(sip_session_opts_t *opts)
{
    if (opts == NULL) {
        return;
    }
    *opts = (sip_session_opts_t) {
        .user = SIP_ACCOUNT_USER,
        .password = SIP_ACCOUNT_PASSWORD,
        .server = SIP_ACCOUNT_SERVER,
        .server_port = SIP_ACCOUNT_PORT,
        .transport = SIP_ACCOUNT_TRANSPORT,
        .p2p_mode = false,
        .srtp_mode = SIP_DEFAULT_SRTP_MODE,
        .local_port = 0,
        .audio_codec = SIP_AUDIO_CODEC,
        .video_codec = SIP_VIDEO_CODEC,
        .sample_rate = SIP_AUDIO_SAMPLE_RATE,
        .width = SIP_VIDEO_WIDTH,
        .height = SIP_VIDEO_HEIGHT,
        .fps = SIP_VIDEO_FPS,
        .bitrate = 0,
    };
}

esp_err_t sip_session_start(const sip_session_opts_t *opts)
{
    ESP_RETURN_ON_FALSE(opts != NULL, ESP_ERR_INVALID_ARG, TAG, "No options");
    ESP_RETURN_ON_FALSE(!s_session.active, ESP_ERR_INVALID_STATE, TAG,
                        "A session is running; stop it with 'sip stop' first");
    ESP_RETURN_ON_FALSE(opts->user != NULL && opts->user[0] != '\0' && opts->server != NULL &&
                            opts->server[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "Set the SIP user and server in menuconfig");
    ESP_RETURN_ON_FALSE(opts->audio_codec != 0 || opts->video_codec != 0, ESP_ERR_INVALID_ARG, TAG,
                        "Enable at least one of audio and video");

    /* The option strings come from the CLI stack, so keep private copies. */
    s_session.opts = *opts;
    snprintf(s_session.user, sizeof(s_session.user), "%s", opts->user);
    snprintf(s_session.password, sizeof(s_session.password), "%s", opts->password ? opts->password : "");
    snprintf(s_session.server, sizeof(s_session.server), "%s", opts->server);
    snprintf(s_session.transport, sizeof(s_session.transport), "%s", opts->transport ? opts->transport : "udp");
    s_session.opts.user = s_session.user;
    s_session.opts.password = s_session.password;
    s_session.opts.server = s_session.server;
    s_session.opts.transport = s_session.transport;

    esp_err_t ret = build_capture(&s_session.opts, &s_session.capture);
    if (ret == ESP_OK) {
        ret = build_player(&s_session.opts, &s_session.player);
    }
    if (ret == ESP_OK) {
        ret = build_sip(&s_session.opts, &s_session.sip);
    }
    /* Link before starting so the SIP side can read codecs from the capture
       tracks, and so the player can hold the downlink provider. */
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.capture), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(s_session.sip), ESP_SIP_SERVICE_STREAM_UPLINK);
        s_session.linked_up = (ret == ESP_OK);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(s_session.sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                     ESP_SERVICE_BASE(s_session.player), ESP_MEDIA_DEFAULT_STREAM);
        s_session.linked_down = (ret == ESP_OK);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.player));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.sip));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(s_session.capture));
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start SIP session failed: %s", esp_err_to_name(ret));
        session_release();
        return ret;
    }

    s_session.active = true;
    apply_mic_gain(SIP_MIC_GAIN);
    configure_uplink_encoder();
    ESP_LOGI(TAG, "SIP session online as %s@%s:%u over %s (audio: %s, video: %s, srtp: %s)",
             s_session.user, s_session.server, (unsigned)s_session.opts.server_port, s_session.transport,
             codec_name(s_session.opts.audio_codec), codec_name(s_session.opts.video_codec),
             srtp_mode_name(s_session.opts.srtp_mode));
    if (s_session.opts.p2p_mode) {
        ESP_LOGI(TAG, "P2P mode: no registration, call the peer directly");
    }
    return ESP_OK;
}

esp_err_t sip_session_stop(void)
{
    if (!s_session.active) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Stopping the SIP session");
    session_release();
    return ESP_OK;
}

esp_err_t sip_session_call(const char *peer)
{
    ESP_RETURN_ON_FALSE(s_session.active, ESP_ERR_INVALID_STATE, TAG, "Run 'sip start' first");
    ESP_RETURN_ON_FALSE(peer != NULL && peer[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "No peer given");
    ESP_RETURN_ON_FALSE(s_session.call == CALL_IDLE, ESP_ERR_INVALID_STATE, TAG,
                        "A call is already %s", call_state_name(s_session.call));
    return esp_sip_service_call(s_session.sip, peer);
}

esp_err_t sip_session_answer(void)
{
    ESP_RETURN_ON_FALSE(s_session.active, ESP_ERR_INVALID_STATE, TAG, "Run 'sip start' first");
    ESP_RETURN_ON_FALSE(s_session.call == CALL_RINGING, ESP_ERR_INVALID_STATE, TAG, "Nothing is ringing");
    return esp_sip_service_answer(s_session.sip);
}

esp_err_t sip_session_hangup(void)
{
    ESP_RETURN_ON_FALSE(s_session.active, ESP_ERR_INVALID_STATE, TAG, "Run 'sip start' first");
    return esp_sip_service_bye(s_session.sip);
}

esp_err_t sip_session_send_dtmf(char digit)
{
    ESP_RETURN_ON_FALSE(s_session.active, ESP_ERR_INVALID_STATE, TAG, "Run 'sip start' first");

    /* RFC2833 numbers the digits 0-9, then '*', '#', then 'A'-'D'. */
    int event = -1;
    if (digit >= '0' && digit <= '9') {
        event = digit - '0';
    } else if (digit == '*') {
        event = 10;
    } else if (digit == '#') {
        event = 11;
    } else if (digit >= 'A' && digit <= 'D') {
        event = 12 + (digit - 'A');
    } else if (digit >= 'a' && digit <= 'd') {
        event = 12 + (digit - 'a');
    }
    ESP_RETURN_ON_FALSE(event >= 0, ESP_ERR_INVALID_ARG, TAG, "'%c' is not a DTMF digit", digit);

    esp_sip_service_dtmf_t dtmf = {
        .event = (uint8_t)event,
        .volume = SIP_DTMF_VOLUME,
        .duration_ms = SIP_DTMF_DURATION_MS,
    };
    return esp_sip_service_send_dtmf(s_session.sip, &dtmf);
}

esp_err_t sip_session_send_message(const char *peer, const char *text)
{
    ESP_RETURN_ON_FALSE(s_session.active, ESP_ERR_INVALID_STATE, TAG, "Run 'sip start' first");
    ESP_RETURN_ON_FALSE(text != NULL && text[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "No text given");

    esp_sip_service_msg_t msg = {
        .content_type = "text/plain",
        .body = text,
        .body_len = (int)strlen(text),
        .peer_uri = peer,
    };
    return esp_sip_service_send_message(s_session.sip, &msg);
}

void sip_session_set_auto_answer(bool enable)
{
    s_session.auto_answer = enable;
    ESP_LOGI(TAG, "Auto answer %s", enable ? "enabled" : "disabled");
}

bool sip_session_is_active(void)
{
    return s_session.active;
}

void sip_session_print_info(void)
{
    char ip[16];
    sip_example_fill_ip(ip, sizeof(ip));
    printf("device ip   : %s\n", ip);

    if (!s_session.active) {
        printf("session     : offline\n");
        printf("hint        : run 'sip start', then 'sip call <user>'\n");
        return;
    }

    const sip_session_opts_t *opts = &s_session.opts;
    printf("session     : online\n");
    printf("account     : %s@%s:%u (%s)\n", s_session.user, s_session.server,
           (unsigned)opts->server_port, s_session.transport);
    printf("registered  : %s\n", opts->p2p_mode ? "n/a (p2p)" : (s_session.registered ? "yes" : "no"));
    printf("call        : %s%s%s\n", call_state_name(s_session.call),
           s_session.peer[0] != '\0' ? " with " : "", s_session.peer);
    printf("auto answer : %s\n", s_session.auto_answer ? "on" : "off");

    bool srtp_active = false;
    if (esp_sip_service_is_srtp_active(s_session.sip, &srtp_active) == ESP_OK) {
        printf("srtp        : %s (%s)\n", srtp_active ? "active" : "inactive",
               srtp_mode_name(opts->srtp_mode));
    }

    if (opts->audio_codec != 0) {
        printf("audio       : %s %" PRIu32 " Hz %d ch\n", codec_name(opts->audio_codec),
               opts->sample_rate, SIP_AUDIO_CHANNEL);
    } else {
        printf("audio       : disabled\n");
    }
    if (opts->video_codec != 0) {
        printf("video       : %s %ux%u@%ufps %" PRIu32 " kbps\n", codec_name(opts->video_codec),
               (unsigned)opts->width, (unsigned)opts->height, (unsigned)opts->fps, video_bitrate(opts) / 1000);
    } else {
        printf("video       : disabled\n");
    }
}
