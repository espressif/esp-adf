/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_service_scheduler.h"
#include "esp_sip_scheduler.h"
#include "esp_sip_service_err.h"
#include "esp_sip_service_ops.h"
#include "esp_sip_service_priv.h"

static const char *TAG = "SIP_SVC";

typedef union {
    esp_sip_service_call_payload_t     call;
    esp_sip_service_hangup_payload_t   hangup;
    esp_sip_service_error_payload_t    error;
    esp_sip_service_message_payload_t  message;
} sip_event_snapshot_t;

static esp_rtc_data_cb_t s_sip_data_cb = {
#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    .send_audio = sip_uplink_send_audio,
    .send_video = sip_uplink_send_video,
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */
#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    .receive_audio = sip_downlink_receive_audio,
    .receive_video = sip_downlink_receive_video,
    .receive_dtmf  = sip_downlink_receive_dtmf,
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */
};

static void sip_fill_rtc_thread_cfg(const esp_sip_service_t *service, const char *thread_name,
                                    uint32_t def_stack, int def_prio, int def_core,
                                    esp_rtc_thread_cfg_t *out)
{
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = def_stack,
        .priority = def_prio,
        .core_id = def_core,
    };
    esp_service_thread_cfg_t cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name ? service->media.base.name : ESP_SIP_SERVICE_NAME,
        .thread_name = thread_name,
    };
    (void)esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &cfg);
    out->stack_size = (uint16_t)cfg.stack_size;
    out->priority = cfg.priority;
    out->core_id = (cfg.core_id < 0) ? def_core : cfg.core_id;
}

static void sip_fill_rtc_threads(const esp_sip_service_t *service, esp_rtc_config_t *cfg)
{
    sip_fill_rtc_thread_cfg(service, ESP_SIP_SCHED_SESSION_TASK,
                            ESP_RTC_THREAD_SIP_STACK, ESP_RTC_THREAD_SIP_PRIO,
                            ESP_RTC_THREAD_SIP_CORE, &cfg->sip_task);
    sip_fill_rtc_thread_cfg(service, ESP_SIP_SCHED_LISTEN_TASK,
                            ESP_RTC_THREAD_LISTEN_STACK, ESP_RTC_THREAD_LISTEN_PRIO,
                            ESP_RTC_THREAD_LISTEN_CORE, &cfg->listen_task);
    sip_fill_rtc_thread_cfg(service, ESP_SIP_SCHED_AUDIO_RECV_TASK,
                            ESP_RTC_THREAD_AUDIO_RECV_STACK, ESP_RTC_THREAD_AUDIO_RECV_PRIO,
                            ESP_RTC_THREAD_AUDIO_RECV_CORE, &cfg->audio_recv);
    sip_fill_rtc_thread_cfg(service, ESP_SIP_SCHED_VIDEO_RECV_TASK,
                            ESP_RTC_THREAD_VIDEO_RECV_STACK, ESP_RTC_THREAD_VIDEO_RECV_PRIO,
                            ESP_RTC_THREAD_VIDEO_RECV_CORE, &cfg->video_recv);
}

static char *sip_strdup_or_null(const char *str)
{
    return str == NULL ? NULL : strdup(str);
}

static void release_malloced_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);
}

static bool sip_transport_is_valid(const char *transport, size_t len)
{
    return len == 3 &&
           (memcmp(transport, "udp", len) == 0 ||
            memcmp(transport, "tcp", len) == 0 ||
            memcmp(transport, "tls", len) == 0);
}

static esp_sip_service_event_t sip_rtc_event_to_service_event(esp_rtc_event_t event)
{
    switch (event) {
        case ESP_RTC_EVENT_REGISTERED:
            return ESP_SIP_SERVICE_EVENT_REGISTERED;
        case ESP_RTC_EVENT_UNREGISTERED:
            return ESP_SIP_SERVICE_EVENT_UNREGISTERED;
        case ESP_RTC_EVENT_INCOMING:
            return ESP_SIP_SERVICE_EVENT_INCOMING;
        case ESP_RTC_EVENT_CALLING:
            return ESP_SIP_SERVICE_EVENT_CALLING;
        case ESP_RTC_EVENT_CALL_ANSWERED:
            return ESP_SIP_SERVICE_EVENT_CALL_ANSWERED;
        case ESP_RTC_EVENT_HANGUP:
            return ESP_SIP_SERVICE_EVENT_HANGUP;
        case ESP_RTC_EVENT_ERROR:
            return ESP_SIP_SERVICE_EVENT_ERROR;
        case ESP_RTC_EVENT_MESSAGE:
            return ESP_SIP_SERVICE_EVENT_MESSAGE;
        case ESP_RTC_EVENT_MESSAGE_SENT:
            return ESP_SIP_SERVICE_EVENT_MESSAGE_SENT;
        case ESP_RTC_EVENT_AUDIO_SESSION_BEGIN:
            return ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_BEGIN;
        case ESP_RTC_EVENT_AUDIO_SESSION_END:
            return ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_END;
        case ESP_RTC_EVENT_VIDEO_SESSION_BEGIN:
            return ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_BEGIN;
        case ESP_RTC_EVENT_VIDEO_SESSION_END:
            return ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_END;
        case ESP_RTC_EVENT_KEEPALIVE:
            return ESP_SIP_SERVICE_EVENT_KEEPALIVE;
        default:
            return 0;
    }
}

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/**
 * @brief  Snapshot protocol-owned strings before they go out of scope
 *
 *         esp_rtc_get_peer / get_hangup_msg / get_message hand back pointers
 *         into stack-internal storage that is only valid until this callback
 *         returns, so everything a subscriber may need is copied here.
 *
 * @return
 *       - >0  Size of the typed payload to publish
 *       - 0   Event is handled but has no payload
 *       - -1  Event needs no snapshot work
 */
static int snapshot_event(esp_sip_service_t *service, esp_rtc_event_t event, sip_event_snapshot_t *out)
{
    memset(out, 0, sizeof(*out));

    switch (event) {
        case ESP_RTC_EVENT_INCOMING:
            service->call_state = SIP_CALL_INCOMING;
            copy_str(out->call.peer, sizeof(out->call.peer), esp_rtc_get_peer(service->handle));
            return (int)sizeof(out->call);
        case ESP_RTC_EVENT_CALLING:
            service->call_state = SIP_CALL_OUTGOING;
            copy_str(out->call.peer, sizeof(out->call.peer), esp_rtc_get_peer(service->handle));
            return (int)sizeof(out->call);
        case ESP_RTC_EVENT_CALL_ANSWERED:
            service->call_state = SIP_CALL_ACTIVE;
            service->srtp_active = esp_rtc_is_srtp_active(service->handle);
            copy_str(out->call.peer, sizeof(out->call.peer), esp_rtc_get_peer(service->handle));
            out->call.srtp_active = service->srtp_active;
            return (int)sizeof(out->call);
        case ESP_RTC_EVENT_HANGUP: {
            esp_rtc_hangup_msg_t msg = {0};
            if (esp_rtc_get_hangup_msg(service->handle, &msg) == ESP_OK) {
                copy_str(out->hangup.reason, sizeof(out->hangup.reason), msg.reason);
            }
            service->call_state = SIP_CALL_IDLE;
            service->srtp_active = false;
            service->msg_pending = false;
            return (int)sizeof(out->hangup);
        }
        case ESP_RTC_EVENT_ERROR: {
            out->error.reject_reason = esp_rtc_get_reject_reason(service->handle);
            /* A rejected INVITE (480, 486, ...) ends the dialog with this event
             * alone, no HANGUP follows, so the call has to be closed here or
             * every later call is refused with "call already in progress". */
            esp_rtc_hangup_msg_t msg = {0};
            if (esp_rtc_get_hangup_msg(service->handle, &msg) == ESP_OK) {
                copy_str(out->error.reason, sizeof(out->error.reason), msg.reason);
            }
            service->call_state = SIP_CALL_IDLE;
            service->srtp_active = false;
            service->msg_pending = false;
            return (int)sizeof(out->error);
        }
        case ESP_RTC_EVENT_MESSAGE: {
            esp_rtc_msg_data_t msg = {0};
            if (esp_rtc_get_message(service->handle, &msg) == ESP_OK) {
                copy_str(out->message.content_type, sizeof(out->message.content_type), msg.content_type);
                copy_str(out->message.peer, sizeof(out->message.peer), msg.peer_uri);
                int body_len = msg.body_len > 0 ? msg.body_len : (msg.body ? (int)strlen(msg.body) : 0);
                if (body_len >= (int)sizeof(out->message.body)) {
                    body_len = (int)sizeof(out->message.body) - 1;
                    out->message.body_truncated = true;
                }
                if (msg.body != NULL && body_len > 0) {
                    memcpy(out->message.body, msg.body, body_len);
                }
                out->message.body[body_len > 0 ? body_len : 0] = '\0';
                out->message.body_len = body_len;
            }
            return (int)sizeof(out->message);
        }
        case ESP_RTC_EVENT_MESSAGE_SENT:
            service->msg_pending = false;
            return 0;
        default:
            return -1;
    }
}

static int sip_service_event_handler(esp_rtc_event_t event, void *ctx)
{
    esp_sip_service_t *service = (esp_sip_service_t *)ctx;
    if (service == NULL) {
        return -1;
    }
    esp_sip_service_event_t mapped = sip_rtc_event_to_service_event(event);
    if (mapped == 0) {
        return 0;
    }

    sip_event_snapshot_t snapshot;
    if (sip_service_lock(service) != ESP_OK) {
        return -1;
    }
    if (service->handle == NULL) {
        sip_service_unlock(service);
        return 0;
    }
    int payload_size = snapshot_event(service, event, &snapshot);
    sip_service_unlock(service);

#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    /* Track work happens outside the lock: the track manager may block.
     * prepare_call is once per call, here, not inside each session_begin. */
    if (event == ESP_RTC_EVENT_CALL_ANSWERED) {
        /* 200 OK without a preceding SESSION_BEGIN still needs a drained,
         * non-aborted manager before RTP arrives. */
        sip_downlink_prepare_call(service);
        /* Early media deliberately left the video out, so start it here. */
        (void)sip_downlink_session_begin(service, ESP_MEDIA_TRACK_TYPE_VIDEO);
    } else if (event == ESP_RTC_EVENT_AUDIO_SESSION_BEGIN) {
        sip_downlink_prepare_call(service);
        (void)sip_downlink_session_begin(service, ESP_MEDIA_TRACK_TYPE_AUDIO);
    } else if (event == ESP_RTC_EVENT_VIDEO_SESSION_BEGIN) {
        sip_downlink_prepare_call(service);
        (void)sip_downlink_session_begin(service, ESP_MEDIA_TRACK_TYPE_VIDEO);
    } else if (event == ESP_RTC_EVENT_HANGUP || event == ESP_RTC_EVENT_ERROR) {
        /* Wake a player blocked on read instead of letting it wait forever.
         * A rejected call can already have had early media, so it is torn down
         * the same way as a normal hangup. */
        sip_downlink_abort(service);
        sip_downlink_end_call(service);
    }
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */

    const void *src = payload_size > 0 ? &snapshot : NULL;
    size_t size = payload_size > 0 ? (size_t)payload_size : 0;
    return sip_service_publish_event(service, (uint16_t)mapped, src, size) == ESP_OK ? 0 : -1;
}

/**
 * @brief  Resolve the codecs handed to the protocol stack
 *
 *         A stream reaches the SDP once a codec is known for it: from a
 *         linked uplink track, or from setup for a receive-only device. A
 *         linked track wins. Neither leaves the stream out, so nothing known
 *         at all is a signaling-only start.
 */
static esp_err_t sip_resolve_codecs(esp_sip_service_t *service)
{
    const esp_sip_service_setup_t *setup = &service->setup;
    service->acodec = RTC_ACODEC_NULL;
    service->vcodec = RTC_VCODEC_NULL;

    memset(&service->vcodec_info, 0, sizeof(service->vcodec_info));
    service->vcodec_info.width = SIP_DEFAULT_VID_WIDTH;
    service->vcodec_info.height = SIP_DEFAULT_VID_HEIGHT;
    service->vcodec_info.fps = SIP_DEFAULT_VID_FPS;

#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    ESP_RETURN_ON_ERROR(sip_uplink_apply_tracks(service), TAG, "uplink tracks");
    /* A track was linked on purpose, so an unsupported codec is an error
     * rather than a silent fall back to the setup codec. */
    ESP_RETURN_ON_FALSE(!service->uplink.audio_rejected, ESP_ERR_NOT_SUPPORTED, TAG,
                        "linked audio codec is not supported");
    ESP_RETURN_ON_FALSE(!service->uplink.video_rejected, ESP_ERR_NOT_SUPPORTED, TAG,
                        "linked video codec is not supported");
    if (service->uplink.audio_info_set) {
        service->acodec = sip_to_acodec(service->uplink.audio_info.info.audio.codec);
    }
    if (service->uplink.video_info_set) {
        service->vcodec = sip_to_vcodec(service->uplink.video_info.info.video.codec);
        const esp_media_video_info_t *info = &service->uplink.video_info.info.video;
        if (info->width != 0) {
            service->vcodec_info.width = info->width;
        }
        if (info->height != 0) {
            service->vcodec_info.height = info->height;
        }
        if (info->fps != 0) {
            service->vcodec_info.fps = info->fps;
        }
    }
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */

    if (service->acodec == RTC_ACODEC_NULL && setup->audio_codec != 0) {
        service->acodec = sip_to_acodec(setup->audio_codec);
        ESP_RETURN_ON_FALSE(service->acodec != RTC_ACODEC_NULL, ESP_ERR_NOT_SUPPORTED, TAG,
                            "audio_codec must be G711A, G711U or OPUS");
    }
    if (service->vcodec == RTC_VCODEC_NULL && setup->video_codec != 0) {
        service->vcodec = sip_to_vcodec(setup->video_codec);
        ESP_RETURN_ON_FALSE(service->vcodec != RTC_VCODEC_NULL, ESP_ERR_NOT_SUPPORTED, TAG,
                            "video_codec must be H264 or MJPEG");
    }

    service->vcodec_info.vcodec = service->vcodec;
    service->vcodec_info.len = sip_opt_video_max_frame_size(&service->opt);
    return ESP_OK;
}

static esp_err_t sip_on_start(esp_service_t *base)
{
    esp_sip_service_t *service = (esp_sip_service_t *)base;
    ESP_RETURN_ON_FALSE(service->uri != NULL, ESP_ERR_INVALID_STATE, TAG, "uri not set");

    ESP_RETURN_ON_ERROR(sip_resolve_codecs(service), TAG, "resolve codecs");

#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    ESP_RETURN_ON_ERROR(sip_downlink_ensure_mngr(service), TAG, "downlink manager failed");
    sip_downlink_reset(service);
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */

    service->call_state = SIP_CALL_IDLE;
    service->msg_pending = false;
    service->srtp_active = false;

    const sip_options_t *opt = &service->opt;
    esp_rtc_config_t cfg = {
        .ctx = service,
        .local_addr = service->local_addr,
        .uri = service->uri,
        .acodec_type = service->acodec,
        .vcodec_info = service->vcodec == RTC_VCODEC_NULL ? NULL : &service->vcodec_info,
        .data_cb = &s_sip_data_cb,
        .event_handler = sip_service_event_handler,
        .use_public_addr = opt->use_public_addr,
        .send_options = opt->send_options,
        .suspend_reg_on_call = opt->suspend_reg_on_call,
        .keepalive = opt->keepalive_sec,
        .rw_timeout_ms = opt->rw_timeout_ms,
        .connect_timeout_ms = opt->connect_timeout_ms,
        .register_interval = opt->register_interval_sec,
        .aud_frame_size = sip_opt_audio_max_frame_size(opt),
        .user_agent = opt->user_agent,
        .fixed_local_port = opt->local_port,
        .p2p_mode = service->setup.p2p_mode,
        .srtp_mode = opt->srtp_mode,
        .domain = opt->domain,
        .video_payload_type = opt->video_payload_type,
    };
    sip_fill_rtc_threads(service, &cfg);

    service->handle = esp_rtc_service_init(&cfg);
    ESP_RETURN_ON_FALSE(service->handle != NULL, ESP_FAIL, TAG, "rtc init failed");
    return ESP_OK;
}

static esp_err_t sip_on_stop(esp_service_t *base)
{
    esp_sip_service_t *service = (esp_sip_service_t *)base;
    bool in_call = false;
    esp_rtc_handle_t handle = NULL;

    if (sip_service_lock(service) == ESP_OK) {
        handle = service->handle;
        in_call = handle != NULL && service->call_state != SIP_CALL_IDLE;
        sip_service_unlock(service);
    } else {
        handle = service->handle;
        in_call = handle != NULL && service->call_state != SIP_CALL_IDLE;
    }
    if (handle == NULL) {
        return ESP_OK;
    }
    if (in_call) {
        /* Send BYE so the peer is not left with a dangling dialog. */
        (void)esp_rtc_bye(handle);
    }

#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    sip_uplink_abort(service);
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */
#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    sip_downlink_abort(service);
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */

    /* Drop the published handle before deinit so call/answer/bye see NULL
     * instead of a pointer the stack is about to free. */
    if (sip_service_lock(service) == ESP_OK) {
        if (service->handle == handle) {
            service->handle = NULL;
        }
        service->call_state = SIP_CALL_IDLE;
        service->msg_pending = false;
        service->srtp_active = false;
        sip_service_unlock(service);
    } else {
        service->handle = NULL;
    }
    (void)esp_rtc_service_deinit(handle);
    return ESP_OK;
}

static esp_err_t sip_on_deinit(esp_service_t *base)
{
    esp_sip_service_t *service = (esp_sip_service_t *)base;
    (void)sip_on_stop(base);

#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    sip_downlink_destroy(service);
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */
    free(service->uri);
    service->uri = NULL;
    free(service->local_addr);
    service->local_addr = NULL;
    /* Copies made by esp_sip_service_set_identity(). */
    free(service->opt.user_agent);
    service->opt.user_agent = NULL;
    free(service->opt.domain);
    service->opt.domain = NULL;
    if (service->lock != NULL) {
        media_lib_mutex_destroy(service->lock);
        service->lock = NULL;
    }
    return ESP_OK;
}

static esp_err_t sip_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    (void)base;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* One instance serves the uplink sink and the downlink source, so the role
     * is the union of whichever directions are built in. */
    unsigned role = ESP_MEDIA_ROLE_NONE;
#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    role |= (unsigned)ESP_MEDIA_ROLE_SINK;
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */
#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    role |= (unsigned)ESP_MEDIA_ROLE_SRC;
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */
    *out_role = (esp_media_role_t)role;
    return ESP_OK;
}

static esp_err_t sip_get_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                  esp_media_provider_t *out_provider)
{
#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    return sip_downlink_get_provider((esp_sip_service_t *)base, stream, out_provider);
#else
    (void)base;
    (void)stream;
    (void)out_provider;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */
}

static esp_err_t sip_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                  const esp_media_provider_t *provider)
{
#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    return sip_uplink_set_provider((esp_sip_service_t *)base, stream, provider);
#else
    (void)base;
    (void)stream;
    (void)provider;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */
}

static esp_err_t sip_get_request(esp_service_t *base, esp_media_stream_id_t stream,
                                 esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT
    return sip_uplink_get_request((esp_sip_service_t *)base, stream, request);
#else
    (void)base;
    (void)stream;
    (void)request;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT */
}

static esp_err_t sip_set_request(esp_service_t *base, esp_media_stream_id_t stream,
                                 const esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    return sip_downlink_set_request((esp_sip_service_t *)base, stream, request);
#else
    (void)base;
    (void)stream;
    (void)request;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */
}

static const char *sip_service_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_SIP_SERVICE_EVENT_REGISTERED:
            return "REGISTERED";
        case ESP_SIP_SERVICE_EVENT_UNREGISTERED:
            return "UNREGISTERED";
        case ESP_SIP_SERVICE_EVENT_INCOMING:
            return "INCOMING";
        case ESP_SIP_SERVICE_EVENT_CALLING:
            return "CALLING";
        case ESP_SIP_SERVICE_EVENT_CALL_ANSWERED:
            return "CALL_ANSWERED";
        case ESP_SIP_SERVICE_EVENT_HANGUP:
            return "HANGUP";
        case ESP_SIP_SERVICE_EVENT_ERROR:
            return "ERROR";
        case ESP_SIP_SERVICE_EVENT_MESSAGE:
            return "MESSAGE";
        case ESP_SIP_SERVICE_EVENT_MESSAGE_SENT:
            return "MESSAGE_SENT";
        case ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_BEGIN:
            return "AUDIO_SESSION_BEGIN";
        case ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_END:
            return "AUDIO_SESSION_END";
        case ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_BEGIN:
            return "VIDEO_SESSION_BEGIN";
        case ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_END:
            return "VIDEO_SESSION_END";
        case ESP_SIP_SERVICE_EVENT_KEEPALIVE:
            return "KEEPALIVE";
        case ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED:
            return "DTMF_RECEIVED";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_sip_service_ops = {
    .on_start      = sip_on_start,
    .on_stop       = sip_on_stop,
    .on_deinit     = sip_on_deinit,
    .event_to_name = sip_service_event_to_name,
};

static const esp_media_service_ops_t s_sip_media_ops = {
    .get_role     = sip_get_role,
    .get_provider = sip_get_provider,
    .set_provider = sip_set_provider,
    .get_request  = sip_get_request,
    .set_request  = sip_set_request,
};

/**
 * @brief  Split a SIP URI and keep the peer host and port
 *
 *         User and host are required. An empty password is `user:@host`.
 *         Also used by esp_sip_service_call() to build a P2P target.
 */
esp_err_t sip_parse_uri(const char *uri, char *peer_host, size_t peer_host_sz, uint16_t *out_port)
{
    const char *after_scheme = strstr(uri, "://");
    ESP_RETURN_ON_FALSE(after_scheme != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "uri needs a transport, e.g. udp://user:pass@host:port");
    ESP_RETURN_ON_FALSE(sip_transport_is_valid(uri, (size_t)(after_scheme - uri)), ESP_ERR_INVALID_ARG, TAG,
                        "uri transport must be udp, tcp or tls");
    after_scheme += 3;

    /* Rightmost '@' so a password may contain one */
    const char *at = strrchr(after_scheme, '@');
    ESP_RETURN_ON_FALSE(at != NULL && at > after_scheme, ESP_ERR_INVALID_ARG, TAG, "uri needs user@host");

    const char *colon = memchr(after_scheme, ':', (size_t)(at - after_scheme));
    const char *user_end = colon != NULL ? colon : at;
    ESP_RETURN_ON_FALSE(user_end > after_scheme, ESP_ERR_INVALID_ARG, TAG, "uri needs a non-empty user");

    const char *host = at + 1;
    const char *host_end = strchr(host, '/');
    if (host_end == NULL) {
        host_end = host + strlen(host);
    }
    const char *port_sep = memchr(host, ':', (size_t)(host_end - host));
    const char *name_end = port_sep != NULL ? port_sep : host_end;
    ESP_RETURN_ON_FALSE(peer_host != NULL && out_port != NULL && name_end > host &&
                            (size_t)(name_end - host) < peer_host_sz,
                        ESP_ERR_INVALID_ARG, TAG, "uri host is empty or too long");

    uint32_t peer_port = ESP_SIP_SERVICE_DEFAULT_PORT;
    if (port_sep != NULL) {
        const char *port = port_sep + 1;
        ESP_RETURN_ON_FALSE(port < host_end, ESP_ERR_INVALID_ARG, TAG, "uri port is empty");
        peer_port = 0;
        for (; port < host_end; port++) {
            ESP_RETURN_ON_FALSE(*port >= '0' && *port <= '9', ESP_ERR_INVALID_ARG, TAG,
                                "uri port must be numeric");
            uint32_t digit = (uint32_t)(*port - '0');
            ESP_RETURN_ON_FALSE(peer_port <= (UINT16_MAX - digit) / 10, ESP_ERR_INVALID_ARG, TAG,
                                "uri port is out of range");
            peer_port = peer_port * 10 + digit;
        }
        ESP_RETURN_ON_FALSE(peer_port != 0, ESP_ERR_INVALID_ARG, TAG, "uri port must be non-zero");
    }

    memcpy(peer_host, host, (size_t)(name_end - host));
    peer_host[name_end - host] = '\0';
    *out_port = (uint16_t)peer_port;
    return ESP_OK;
}

rtc_payload_acodec_t sip_to_acodec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_G711A) {
        return RTC_ACODEC_G711A;
    }
    if (codec == ESP_MEDIA_CODEC_G711U) {
        return RTC_ACODEC_G711U;
    }
    if (codec == ESP_MEDIA_CODEC_OPUS) {
        return RTC_ACODEC_OPUS;
    }
    return RTC_ACODEC_NULL;
}

rtc_payload_vcodec_t sip_to_vcodec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_H264) {
        return RTC_VCODEC_H264;
    }
    if (codec == ESP_MEDIA_CODEC_MJPEG) {
        return RTC_VCODEC_MJPEG;
    }
    return RTC_VCODEC_NULL;
}

esp_media_codec_fourcc_t sip_acodec_to_fourcc(rtc_payload_acodec_t codec)
{
    switch (codec) {
        case RTC_ACODEC_G711A:
            return ESP_MEDIA_CODEC_G711A;
        case RTC_ACODEC_G711U:
            return ESP_MEDIA_CODEC_G711U;
        case RTC_ACODEC_OPUS:
            return ESP_MEDIA_CODEC_OPUS;
        default:
            return 0;
    }
}

esp_media_codec_fourcc_t sip_vcodec_to_fourcc(rtc_payload_vcodec_t codec)
{
    switch (codec) {
        case RTC_VCODEC_H264:
            return ESP_MEDIA_CODEC_H264;
        case RTC_VCODEC_MJPEG:
            return ESP_MEDIA_CODEC_MJPEG;
        default:
            return 0;
    }
}

uint32_t sip_acodec_sample_rate(rtc_payload_acodec_t codec)
{
    /* G.711 is always narrowband; OPUS is negotiated at 16 kHz by the stack. */
    return codec == RTC_ACODEC_OPUS ? 16000 : 8000;
}

esp_err_t sip_service_lock(esp_sip_service_t *service)
{
    if (service == NULL || service->lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return media_lib_mutex_lock(service->lock, MEDIA_LIB_MAX_LOCK_TIME) == 0 ? ESP_OK : ESP_FAIL;
}

void sip_service_unlock(esp_sip_service_t *service)
{
    if (service != NULL && service->lock != NULL) {
        media_lib_mutex_unlock(service->lock);
    }
}

esp_err_t sip_service_ensure_stopped(esp_sip_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_INITIALIZED,
                        ESP_ERR_INVALID_STATE, TAG, "bad state");
    return ESP_OK;
}

esp_err_t sip_service_check_running(esp_sip_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_RUNNING && service->handle != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "service not running");
    return ESP_OK;
}

esp_err_t sip_service_get_handle(esp_sip_service_t *service, esp_rtc_handle_t *out_handle)
{
    ESP_RETURN_ON_FALSE(service != NULL && out_handle != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");
    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    esp_rtc_handle_t handle = service->handle;
    sip_service_unlock(service);
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_STATE, TAG, "service not running");
    *out_handle = handle;
    return ESP_OK;
}

esp_err_t sip_service_publish_event(esp_sip_service_t *service, uint16_t event,
                                    const void *src, size_t size)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "publish event: invalid arg");
    ESP_RETURN_ON_FALSE((src == NULL && size == 0) || (src != NULL && size > 0), ESP_ERR_INVALID_ARG, TAG,
                        "publish event: payload size");
    void *payload = NULL;
    if (size > 0) {
        payload = malloc(size);
        ESP_RETURN_ON_FALSE(payload != NULL, ESP_ERR_NO_MEM, TAG, "publish event: no mem");
        memcpy(payload, src, size);
    }
    /* The hub only shallow-copies the pointer, so each event owns a dedicated
     * heap copy. release_cb runs on every return path; do not free payload. */
    esp_err_t ret = esp_service_publish_event(ESP_SERVICE_BASE(service), event, payload, size,
                                              payload != NULL ? release_malloced_payload : NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Publish event %u failed: %s", (unsigned)event, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_sip_service_create(const esp_sip_service_cfg_t *cfg, esp_sip_service_t **out_service)
{
    ESP_RETURN_ON_FALSE(out_service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    esp_sip_service_t *service = calloc(1, sizeof(*service));
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    service->setup = (esp_sip_service_setup_t)ESP_SIP_SERVICE_SETUP_DEFAULT();
    service->downlink.audio_cache_size = SIP_DEFAULT_AUDIO_CACHE_SIZE;
    service->downlink.video_cache_size = SIP_DEFAULT_VIDEO_CACHE_SIZE;

    if (media_lib_mutex_create(&service->lock) != 0 || service->lock == NULL) {
        free(service);
        RET_FOR(ESP_ERR_NO_MEM, "mutex create failed");
    }

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = (cfg != NULL && cfg->name != NULL) ? cfg->name : ESP_SIP_SERVICE_NAME;
    media_cfg.service_ops = &s_sip_service_ops;
    media_cfg.media_ops = &s_sip_media_ops;
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        media_lib_mutex_destroy(service->lock);
        free(service);
        RET_FOR(ret, "media init failed");
    }

#ifdef CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT
    /* A link may query the downlink provider before start. */
    ret = sip_downlink_ensure_mngr(service);
    if (ret != ESP_OK) {
        esp_media_service_deinit(ESP_SERVICE_BASE(service));
        media_lib_mutex_destroy(service->lock);
        free(service);
        RET_FOR(ret, "downlink manager init failed");
    }
#endif  /* CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT */

    *out_service = service;
    return ESP_OK;
}

esp_err_t esp_sip_service_setup(esp_sip_service_t *service, const esp_sip_service_setup_t *setup)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->setup = setup != NULL ? *setup : (esp_sip_service_setup_t)ESP_SIP_SERVICE_SETUP_DEFAULT();
    return ESP_OK;
}

esp_err_t esp_sip_service_set_local_port(esp_sip_service_t *service, uint16_t port)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.local_port = port;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_timeout(esp_sip_service_t *service, uint16_t connect_timeout_ms,
                                      uint16_t rw_timeout_ms)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.connect_timeout_ms = connect_timeout_ms;
    service->opt.rw_timeout_ms = rw_timeout_ms;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_nat_traversal(esp_sip_service_t *service, const esp_sip_service_nat_t *nat)
{
    ESP_RETURN_ON_FALSE(service != NULL && nat != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.keepalive_sec = nat->keepalive_sec;
    service->opt.send_options = nat->send_options;
    service->opt.use_public_addr = nat->use_public_addr;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_register_refresh(esp_sip_service_t *service, uint16_t interval_sec,
                                               bool suspend_on_call)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.register_interval_sec = interval_sec;
    service->opt.suspend_reg_on_call = suspend_on_call;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_srtp_mode(esp_sip_service_t *service, esp_rtc_srtp_mode_t mode)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(mode <= ESP_RTC_SRTP_REQUIRED, ESP_ERR_INVALID_ARG, TAG, "srtp mode out of range");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.srtp_mode = mode;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_identity(esp_sip_service_t *service, const char *user_agent, const char *domain)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    char *ua_copy = sip_strdup_or_null(user_agent);
    char *domain_copy = sip_strdup_or_null(domain);
    if ((user_agent != NULL && ua_copy == NULL) || (domain != NULL && domain_copy == NULL)) {
        free(ua_copy);
        free(domain_copy);
        RET_FOR(ESP_ERR_NO_MEM, "no mem for identity strings");
    }
    free(service->opt.user_agent);
    free(service->opt.domain);
    service->opt.user_agent = ua_copy;
    service->opt.domain = domain_copy;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_frame_size(esp_sip_service_t *service, uint32_t audio_max_bytes,
                                         uint32_t video_max_bytes)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.audio_max_frame_size = audio_max_bytes;
    service->opt.video_max_frame_size = video_max_bytes;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_cache_size(esp_sip_service_t *service, uint32_t audio_bytes, uint32_t video_bytes)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->downlink.audio_cache_size = audio_bytes != 0 ? audio_bytes : SIP_DEFAULT_AUDIO_CACHE_SIZE;
    service->downlink.video_cache_size = video_bytes != 0 ? video_bytes : SIP_DEFAULT_VIDEO_CACHE_SIZE;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_video_payload_type(esp_sip_service_t *service, uint8_t payload_type)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    service->opt.video_payload_type = payload_type;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_uri(esp_sip_service_t *service, const char *uri)
{
    ESP_RETURN_ON_FALSE(service != NULL && uri != NULL && uri[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    char peer_host[SIP_PEER_HOST_MAX] = {0};
    uint16_t peer_port = 0;
    ESP_RETURN_ON_ERROR(sip_parse_uri(uri, peer_host, sizeof(peer_host), &peer_port), TAG, "bad uri");

    char *copy = strdup(uri);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    free(service->uri);
    service->uri = copy;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_account(esp_sip_service_t *service, const esp_sip_service_account_t *account)
{
    ESP_RETURN_ON_FALSE(service != NULL && account != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(account->user != NULL && account->user[0] != '\0' &&
                            account->server != NULL && account->server[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "account needs a user and a server");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    const char *scheme = (account->transport != NULL && account->transport[0] != '\0') ? account->transport : "udp";
    ESP_RETURN_ON_FALSE(sip_transport_is_valid(scheme, strlen(scheme)), ESP_ERR_INVALID_ARG, TAG,
                        "transport must be udp, tcp or tls");
    /* Keep the ':' so an empty password is "" rather than NULL. Digest rejects
     * NULL and still computes HA1 from an empty string. */
    const char *pass = account->password != NULL ? account->password : "";
    uint16_t dst_port = account->port != 0 ? account->port : ESP_SIP_SERVICE_DEFAULT_PORT;

    int len = snprintf(NULL, 0, "%s://%s:%s@%s:%u", scheme, account->user, pass, account->server,
                       (unsigned)dst_port);
    ESP_RETURN_ON_FALSE(len > 0, ESP_FAIL, TAG, "uri format failed");

    char *uri = calloc(1, (size_t)len + 1);
    ESP_RETURN_ON_FALSE(uri != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    snprintf(uri, (size_t)len + 1, "%s://%s:%s@%s:%u", scheme, account->user, pass, account->server,
             (unsigned)dst_port);

    char peer_host[SIP_PEER_HOST_MAX] = {0};
    uint16_t peer_port = 0;
    esp_err_t ret = sip_parse_uri(uri, peer_host, sizeof(peer_host), &peer_port);
    if (ret != ESP_OK) {
        free(uri);
        RET_FOR(ret, "account produced a bad uri");
    }

    free(service->uri);
    service->uri = uri;
    return ESP_OK;
}

esp_err_t esp_sip_service_set_local_addr(esp_sip_service_t *service, const char *ip)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_ensure_stopped(service), TAG, "service not stopped");

    char *copy = NULL;
    if (ip != NULL) {
        ESP_RETURN_ON_FALSE(ip[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "empty ip");
        copy = strdup(ip);
        ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    }
    free(service->local_addr);
    service->local_addr = copy;
    return ESP_OK;
}
