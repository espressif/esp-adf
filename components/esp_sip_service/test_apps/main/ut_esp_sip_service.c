/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_sip_service.h"
#include "esp_sip_service_ops.h"

#define SIP_LOOPBACK_PORT  5062
#define SIP_STATE_PORT     5063
#define SIP_A_USER         "1001"
#define SIP_B_USER         "1002"
#define SIP_LOOPBACK_IP    "127.0.0.1"

#define CALL_SETTLE_MS  4000

/**
 * @brief  Event bookkeeping for the loopback call test
 */
typedef struct {
    volatile bool      incoming;
    volatile bool      calling;
    volatile bool      answered;
    volatile bool      audio_begin;
    volatile bool      hangup;
    esp_sip_service_t *service;
    bool               auto_answer;
} sip_ut_events_t;

static void sip_service_delete(esp_sip_service_t *service)
{
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(service)));
    free(service);
}

static void sip_set_loopback_account(esp_sip_service_t *service, const char *user, uint16_t port)
{
    esp_sip_service_account_t account = {
        .transport = "udp",
        .user = user,
        .server = SIP_LOOPBACK_IP,
        .port = port,
    };
    TEST_ESP_OK(esp_sip_service_set_account(service, &account));
}

static void sip_event_handler(const adf_event_t *event, void *ctx)
{
    sip_ut_events_t *state = (sip_ut_events_t *)ctx;
    if (event == NULL || state == NULL) {
        return;
    }
    ESP_LOGI("SIP_UT", "event %d", (int)event->event_id);
    switch (event->event_id) {
        case ESP_SIP_SERVICE_EVENT_CALLING:
            state->calling = true;
            break;
        case ESP_SIP_SERVICE_EVENT_INCOMING:
            /* INCOMING repeats while ringing, so answer only the first one. */
            if (!state->incoming) {
                state->incoming = true;
                if (state->auto_answer) {
                    esp_sip_service_answer(state->service);
                }
            }
            break;
        case ESP_SIP_SERVICE_EVENT_CALL_ANSWERED:
            state->answered = true;
            break;
        case ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_BEGIN:
            state->audio_begin = true;
            break;
        case ESP_SIP_SERVICE_EVENT_HANGUP:
            state->hangup = true;
            break;
        default:
            break;
    }
}

static void subscribe_events(esp_sip_service_t *sip, sip_ut_events_t *state)
{
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = sip_event_handler;
    info.handler_ctx = state;
    TEST_ESP_OK(esp_service_event_subscribe(ESP_SERVICE_BASE(sip), &info));
}

static void sip_ut_add_opus_track(esp_media_dummy_service_t *dummy)
{
    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_OPUS,
            .sample_rate = 48000,
            .channel = 1,
            .bits_per_sample = 16,
        },
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy, ESP_MEDIA_DEFAULT_STREAM, &audio));
}

void esp_sip_service_ut_force_link(void)  { }

TEST_CASE("sip create deinit and role", "[esp_sip_service]")
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(&cfg, &sip));
    TEST_ASSERT_NOT_NULL(sip);

    /* SIP is full duplex, so one instance is both source and sink. */
    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    TEST_ESP_OK(esp_media_service_get_role(ESP_SERVICE_BASE(sip), &role));
    TEST_ASSERT_EQUAL(ESP_MEDIA_ROLE_SRC_SINK, role);

    sip_service_delete(sip);

    /* A NULL config is allowed and falls back to the default name. */
    sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));
    const char *name = NULL;
    TEST_ESP_OK(esp_service_get_name(ESP_SERVICE_BASE(sip), &name));
    TEST_ASSERT_EQUAL_STRING(ESP_SIP_SERVICE_NAME, name);
    sip_service_delete(sip);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_create(&cfg, NULL));
}

TEST_CASE("sip event names", "[esp_sip_service]")
{
    static const struct {
        uint16_t  id;
        const char *name;
    } cases[] = {
        {ESP_SIP_SERVICE_EVENT_REGISTERED, "REGISTERED"},
        {ESP_SIP_SERVICE_EVENT_UNREGISTERED, "UNREGISTERED"},
        {ESP_SIP_SERVICE_EVENT_INCOMING, "INCOMING"},
        {ESP_SIP_SERVICE_EVENT_CALLING, "CALLING"},
        {ESP_SIP_SERVICE_EVENT_CALL_ANSWERED, "CALL_ANSWERED"},
        {ESP_SIP_SERVICE_EVENT_HANGUP, "HANGUP"},
        {ESP_SIP_SERVICE_EVENT_ERROR, "ERROR"},
        {ESP_SIP_SERVICE_EVENT_MESSAGE, "MESSAGE"},
        {ESP_SIP_SERVICE_EVENT_MESSAGE_SENT, "MESSAGE_SENT"},
        {ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_BEGIN, "AUDIO_SESSION_BEGIN"},
        {ESP_SIP_SERVICE_EVENT_AUDIO_SESSION_END, "AUDIO_SESSION_END"},
        {ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_BEGIN, "VIDEO_SESSION_BEGIN"},
        {ESP_SIP_SERVICE_EVENT_VIDEO_SESSION_END, "VIDEO_SESSION_END"},
        {ESP_SIP_SERVICE_EVENT_KEEPALIVE, "KEEPALIVE"},
        {ESP_SIP_SERVICE_EVENT_DTMF_RECEIVED, "DTMF_RECEIVED"},
    };

    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *name = NULL;
        TEST_ESP_OK(esp_service_get_event_name(ESP_SERVICE_BASE(sip), cases[i].id, &name));
        TEST_ASSERT_EQUAL_STRING(cases[i].name, name);
    }

    const char *name = "unchanged";
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      esp_service_get_event_name(ESP_SERVICE_BASE(sip), UINT16_MAX, &name));
    TEST_ASSERT_NULL(name);
    sip_service_delete(sip);
}

TEST_CASE("sip stream addressing", "[esp_sip_service]")
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(&cfg, &sip));

    /* The downlink exports a provider, the uplink consumes one. */
    esp_media_provider_t provider = {0};
    TEST_ESP_OK(esp_media_service_get_provider(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK, &provider));
    TEST_ASSERT_NOT_NULL(provider.ops);

    esp_media_service_request_t request = {0};
    TEST_ESP_OK(esp_media_service_get_request(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK, &request));

    /* Streams are direction specific and anything else is rejected. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_media_service_get_provider(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK,
                                                     &provider));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_media_service_get_request(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                                    &request));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_media_service_get_provider(ESP_SERVICE_BASE(sip), (esp_media_stream_id_t)7, &provider));

    sip_service_delete(sip);
}

TEST_CASE("sip setup and uri validation", "[esp_sip_service]")
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(&cfg, &sip));

    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.p2p_mode = true;
    setup.audio_codec = ESP_FOURCC_ALAW;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    /* NULL restores defaults instead of failing. */
    TEST_ESP_OK(esp_sip_service_setup(sip, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_setup(NULL, &setup));

    /* Everything with a default sits behind its own setter. */
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, SIP_LOOPBACK_PORT));
    TEST_ESP_OK(esp_sip_service_set_timeout(sip, 5000, 5000));
    esp_sip_service_nat_t nat = {.keepalive_sec = 60, .send_options = true, .use_public_addr = true};
    TEST_ESP_OK(esp_sip_service_set_nat_traversal(sip, &nat));
    TEST_ESP_OK(esp_sip_service_set_register_refresh(sip, 600, true));
    TEST_ESP_OK(esp_sip_service_set_srtp_mode(sip, ESP_RTC_SRTP_PREFER));
    TEST_ESP_OK(esp_sip_service_set_identity(sip, "ESP SIP UT", "example.com"));
    TEST_ESP_OK(esp_sip_service_set_identity(sip, NULL, NULL));
    TEST_ESP_OK(esp_sip_service_set_frame_size(sip, 2048, 32768));
    TEST_ESP_OK(esp_sip_service_set_cache_size(sip, 4096, 65536));
    TEST_ESP_OK(esp_sip_service_set_video_payload_type(sip, 96));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_local_port(NULL, 5060));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_srtp_mode(sip, (esp_rtc_srtp_mode_t)7));
    /* 0 means "back to the default" rather than a rejected value. */
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, 0));
    TEST_ESP_OK(esp_sip_service_set_cache_size(sip, 0, 0));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, ""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "1001:pass@127.0.0.1:5060"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass"));
    TEST_ESP_OK(esp_sip_service_set_uri(sip, "udp://1001:@127.0.0.1:5060"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://:pass@127.0.0.1:5060"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass@"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "quic://1001:pass@127.0.0.1:5060"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:abc"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:5060x"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:65536"));
    TEST_ESP_OK(esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:5060"));
    TEST_ESP_OK(esp_sip_service_set_uri(sip, "udp://1001:p@ss@127.0.0.1:5060"));
    TEST_ESP_OK(esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1"));

    esp_sip_service_account_t account = {.transport = "udp", .user = "", .server = SIP_LOOPBACK_IP};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_account(sip, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_account(sip, &account));
    account.user = SIP_A_USER;
    account.server = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_account(sip, &account));
    account.server = SIP_LOOPBACK_IP;
    account.transport = "quic";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_account(sip, &account));
    account.transport = NULL;
    TEST_ESP_OK(esp_sip_service_set_account(sip, &account));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_local_addr(sip, ""));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, NULL));

    /* Call control needs a running service. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_call(sip, SIP_B_USER));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_answer(sip));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_bye(sip));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_call(sip, NULL));
    esp_sip_service_dtmf_t dtmf = {.event = 16, .volume = 10, .duration_ms = 200};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_send_dtmf(sip, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_send_dtmf(sip, &dtmf));

    /* SRTP state is readable while stopped and defaults to inactive. */
    bool srtp_active = true;
    TEST_ESP_OK(esp_sip_service_is_srtp_active(sip, &srtp_active));
    TEST_ASSERT_FALSE(srtp_active);

    sip_service_delete(sip);
}

TEST_CASE("sip query and header validation", "[esp_sip_service]")
{
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));

    bool active = false;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_is_srtp_active(NULL, &active));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_is_srtp_active(sip, NULL));

    char peer[16] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_get_peer(NULL, peer, sizeof(peer)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_get_peer(sip, NULL, sizeof(peer)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_get_peer(sip, peer, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_get_peer(sip, peer, sizeof(peer)));

    int header_len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_sip_service_read_raw_headers(NULL, peer, sizeof(peer), &header_len));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_read_raw_headers(sip, peer, 0, &header_len));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_read_raw_headers(sip, NULL, 0, &header_len));

    esp_sip_service_headers_t headers = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_invite_info(NULL, &headers));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_invite_info(sip, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_set_invite_info(sip, &headers));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_set_private_header(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_set_private_header(sip, NULL));

    esp_sip_service_msg_t msg = {.body = "hello"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_send_message(NULL, &msg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_send_message(sip, NULL));
    msg.body = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_sip_service_send_message(sip, &msg));
    msg.body = "hello";
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_send_message(sip, &msg));

    sip_service_delete(sip);
}

TEST_CASE("sip start requires uri", "[esp_sip_service]")
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(&cfg, &sip));

    /* No URI yet, so start must fail instead of registering nowhere. */
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    /* A codec SIP cannot carry must be refused instead of silently dropped. */
    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.audio_codec = ESP_FOURCC_AAC;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:5060"));
    TEST_ASSERT_NOT_EQUAL(ESP_OK, esp_service_start(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    /* No codec and nothing linked is signaling-only: start must still succeed. */
    setup = (esp_sip_service_setup_t)ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.p2p_mode = true;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, 5064));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    sip_set_loopback_account(sip, SIP_A_USER, 5064);
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    sip_service_delete(sip);
}

TEST_CASE("sip start receive-only uses setup codec", "[esp_sip_service]")
{
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));

    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.audio_codec = ESP_FOURCC_ALAW;
    setup.p2p_mode = true;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, 5065));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    sip_set_loopback_account(sip, SIP_A_USER, 5065);

    /* No uplink track: codec comes from setup. */
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    sip_service_delete(sip);
}

TEST_CASE("sip running state guards and restart", "[esp_sip_service]")
{
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(NULL, &sip));

    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.p2p_mode = true;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, SIP_STATE_PORT));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    sip_set_loopback_account(sip, SIP_A_USER, SIP_STATE_PORT);

    esp_media_dummy_service_cfg_t dummy_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_cfg.max_stream_num = 1;
    dummy_cfg.name = "dummy_state_src";
    esp_media_dummy_service_t *dummy_src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_cfg, &dummy_src));
    sip_ut_add_opus_track(dummy_src);
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sip)));
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_setup(sip, &setup));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_sip_service_set_uri(sip, "udp://1001:pass@127.0.0.1:5063"));
    esp_sip_service_account_t running_account = {
        .transport = "udp",
        .user = SIP_A_USER,
        .server = SIP_LOOPBACK_IP,
        .port = SIP_STATE_PORT,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_set_account(sip, &running_account));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_sip_service_answer(sip));
    TEST_ESP_OK(esp_sip_service_bye(sip));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_media_service_set_provider(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK,
                                                     NULL));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sip)));
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
    sip_service_delete(sip);
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_src));
}

/**
 * @brief  One instance invites itself over loopback
 *
 *         A two instance test is not possible: in P2P mode the protocol stack
 *         replaces fixed_local_port with the port from its own URI and binds
 *         INADDR_ANY, so two peers on one device would have to share a port.
 *         Pointing one instance at its own port instead keeps the exchange on
 *         the device and still covers call(), the INVITE built from the
 *         resolved codec, the CALLING bridge and bye().
 *
 *         Both links are attached to the single service, so the uplink sink
 *         and the downlink source of a SRC_SINK role are wired at once.
 *
 *         OPUS is the codec here because the dummy source only generates PCM,
 *         AAC, MP3 and OPUS patterns, and OPUS is the only one of those that
 *         SIP can negotiate. Nothing decodes the frames, so the pattern's own
 *         48 kHz rate is kept rather than forcing a telephony rate.
 */
TEST_CASE("sip p2p self invite", "[esp_sip_service]")
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    cfg.name = "sip_peer";
    esp_sip_service_t *sip = NULL;
    TEST_ESP_OK(esp_sip_service_create(&cfg, &sip));

    /* Audio needs no codec here: the linked uplink track supplies it. */
    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.p2p_mode = true;
    TEST_ESP_OK(esp_sip_service_setup(sip, &setup));
    TEST_ESP_OK(esp_sip_service_set_local_port(sip, SIP_LOOPBACK_PORT));
    TEST_ESP_OK(esp_sip_service_set_local_addr(sip, SIP_LOOPBACK_IP));
    sip_set_loopback_account(sip, SIP_A_USER, SIP_LOOPBACK_PORT);

    esp_media_dummy_service_cfg_t dummy_src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_src_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_src_cfg.max_stream_num = 1;
    dummy_src_cfg.name = "dummy_src";
    esp_media_dummy_service_t *dummy_src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_src_cfg, &dummy_src));

    esp_media_dummy_service_cfg_t dummy_sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    dummy_sink_cfg.max_stream_num = 1;
    dummy_sink_cfg.name = "dummy_sink";
    esp_media_dummy_service_t *dummy_sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_sink_cfg, &dummy_sink));

    sip_ut_add_opus_track(dummy_src);

    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                       ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    sip_ut_events_t events = {.service = sip, .auto_answer = false};
    subscribe_events(sip, &events);

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sip)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_src)));

    TEST_ESP_OK(esp_sip_service_call(sip, SIP_B_USER));

    for (int waited = 0; waited < CALL_SETTLE_MS && !events.calling; waited += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    TEST_ASSERT_TRUE_MESSAGE(events.calling, "call() never reported CALLING");

    TEST_ESP_OK(esp_sip_service_bye(sip));
    vTaskDelay(pdMS_TO_TICKS(500));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sip)));

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_UPLINK));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                         ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    sip_service_delete(sip);
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_sink));
}
