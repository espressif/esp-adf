/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_sip_service.h"
#include "esp_sip_service_ops.h"

#include "settings.h"
#include "simple_sip.h"

/**
 * @brief  Call progress collected from the service events
 */
typedef struct {
    esp_sip_service_t *sip;       /*!< SIP service instance */
    volatile bool      calling;   /*!< The INVITE is on the wire */
    volatile bool      answered;  /*!< The call is established */
    volatile bool      hangup;    /*!< The call has ended */
} sip_call_t;

static const char *TAG = "SIMPLE_SIP";

static void on_sip_event(const adf_event_t *event, void *ctx)
{
    sip_call_t *call = (sip_call_t *)ctx;
    if (event == NULL || call == NULL) {
        return;
    }
    const char *name = NULL;
    if (esp_service_get_event_name(ESP_SERVICE_BASE(call->sip), event->event_id, &name) != ESP_OK) {
        name = "UNKNOWN";
    }

    switch (event->event_id) {
        case ESP_SIP_SERVICE_EVENT_CALLING:
            call->calling = true;
            ESP_LOGI(TAG, "%s", name);
            break;
        case ESP_SIP_SERVICE_EVENT_INCOMING: {
            const esp_sip_service_call_payload_t *payload =
                (event->payload != NULL && event->payload_len >= sizeof(*payload)) ? event->payload : NULL;
            ESP_LOGI(TAG, "%s from %s", name, payload ? payload->peer : "");
            break;
        }
        case ESP_SIP_SERVICE_EVENT_CALL_ANSWERED: {
            const esp_sip_service_call_payload_t *payload =
                (event->payload != NULL && event->payload_len >= sizeof(*payload)) ? event->payload : NULL;
            call->answered = true;
            ESP_LOGI(TAG, "%s peer=%s srtp=%d", name, payload ? payload->peer : "",
                     payload ? (int)payload->srtp_active : 0);
            break;
        }
        case ESP_SIP_SERVICE_EVENT_HANGUP: {
            const esp_sip_service_hangup_payload_t *payload =
                (event->payload != NULL && event->payload_len >= sizeof(*payload)) ? event->payload : NULL;
            call->hangup = true;
            ESP_LOGI(TAG, "%s reason=%s", name, payload ? payload->reason : "");
            break;
        }
        case ESP_SIP_SERVICE_EVENT_ERROR: {
            const esp_sip_service_error_payload_t *payload =
                (event->payload != NULL && event->payload_len >= sizeof(*payload)) ? event->payload : NULL;
            ESP_LOGW(TAG, "%s reject_reason=%d", name, payload ? (int)payload->reject_reason : 0);
            break;
        }
        default:
            ESP_LOGI(TAG, "%s", name);
            break;
    }
}

static esp_err_t sip_create(sip_call_t *call)
{
    esp_sip_service_cfg_t cfg = ESP_SIP_SERVICE_CFG_DEFAULT();
    cfg.name = "sip_peer";
    ESP_RETURN_ON_ERROR(esp_sip_service_create(&cfg, &call->sip), TAG, "create");

    /* Audio needs no codec here: the linked dummy source supplies it. */
    esp_sip_service_setup_t setup = ESP_SIP_SERVICE_SETUP_DEFAULT();
    setup.p2p_mode = true;
    ESP_RETURN_ON_ERROR(esp_sip_service_setup(call->sip, &setup), TAG, "setup");
    ESP_RETURN_ON_ERROR(esp_sip_service_set_local_port(call->sip, SIP_PEER_PORT), TAG, "local port");
    ESP_RETURN_ON_ERROR(esp_sip_service_set_local_addr(call->sip, SIP_LOCAL_IP), TAG, "local addr");
    /* In P2P mode the "server" in the URI is simply the other endpoint. */
    esp_sip_service_account_t account = {
        .transport = "udp",
        .user = SIP_LOCAL_USER,
        .server = SIP_PEER_IP,
        .port = SIP_PEER_PORT,
    };
    ESP_RETURN_ON_ERROR(esp_sip_service_set_account(call->sip, &account), TAG, "account");

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = on_sip_event;
    info.handler_ctx = call;
    return esp_service_event_subscribe(ESP_SERVICE_BASE(call->sip), &info);
}

esp_err_t simple_sip_p2p_call(uint32_t run_ms)
{
    sip_call_t call = {0};
    esp_media_dummy_service_t *dummy_src = NULL;
    esp_media_dummy_service_t *dummy_sink = NULL;
    bool linked_up = false;
    bool linked_down = false;
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(sip_create(&call), cleanup, TAG, "sip setup");

    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    src_cfg.name = "dummy_src";
    ESP_GOTO_ON_ERROR(esp_media_dummy_service_create(&src_cfg, &dummy_src), cleanup, TAG, "dummy src");

    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    sink_cfg.name = "dummy_sink";
    ESP_GOTO_ON_ERROR(esp_media_dummy_service_create(&sink_cfg, &dummy_sink), cleanup, TAG, "dummy sink");

    /* Dummy can generate OPUS, which SIP can negotiate. The pattern is only
     * used to advertise the codec at start; a real peer still has to answer. */
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
    ESP_GOTO_ON_ERROR(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &audio),
                      cleanup, TAG, "add opus");

    /* One SRC_SINK instance carries both directions: the uplink consumes the
     * dummy source, the downlink feeds the dummy sink. */
    ESP_GOTO_ON_ERROR(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                             ESP_SERVICE_BASE(call.sip), ESP_SIP_SERVICE_STREAM_UPLINK),
                      cleanup, TAG, "link uplink");
    linked_up = true;
    ESP_GOTO_ON_ERROR(esp_media_service_link(ESP_SERVICE_BASE(call.sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                             ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM),
                      cleanup, TAG, "link downlink");
    linked_down = true;

    ESP_GOTO_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(call.sip)), cleanup, TAG, "start sip");
    ESP_GOTO_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(dummy_sink)), cleanup, TAG, "start sink");
    ESP_GOTO_ON_ERROR(esp_service_start(ESP_SERVICE_BASE(dummy_src)), cleanup, TAG, "start src");

    ESP_LOGI(TAG, "Calling %s at %s:%d", SIP_PEER_USER, SIP_PEER_IP, SIP_PEER_PORT);
    ESP_GOTO_ON_ERROR(esp_sip_service_call(call.sip, SIP_PEER_USER), cleanup, TAG, "call");

    for (uint32_t waited = 0; waited < SIP_CALL_SETTLE_MS && !call.answered && !call.hangup; waited += 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (call.answered) {
        vTaskDelay(pdMS_TO_TICKS(run_ms));
        esp_media_dummy_stream_stats_t stats = {0};
        esp_media_dummy_service_get_stats(dummy_sink, ESP_MEDIA_DEFAULT_STREAM, &stats);
        ESP_LOGI(TAG, "Downlink received %d audio frames", (int)stats.audio_frame_count);
    } else {
        ESP_LOGW(TAG, "Nobody answered at %s:%d, set SIP_PEER_IP to a real endpoint for media",
                 SIP_PEER_IP, SIP_PEER_PORT);
    }

    esp_sip_service_bye(call.sip);
    vTaskDelay(pdMS_TO_TICKS(500));

    /* The INVITE leaving the device is what this demo can prove on its own. */
    ret = call.calling ? ESP_OK : ESP_FAIL;

cleanup:
    if (dummy_src != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(dummy_src));
    }
    if (dummy_sink != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(dummy_sink));
    }
    if (call.sip != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(call.sip));
    }
    if (linked_up) {
        esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                 ESP_SERVICE_BASE(call.sip), ESP_SIP_SERVICE_STREAM_UPLINK);
    }
    if (linked_down) {
        esp_media_service_unlink(ESP_SERVICE_BASE(call.sip), ESP_SIP_SERVICE_STREAM_DOWNLINK,
                                 ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (call.sip != NULL) {
        esp_media_service_deinit(ESP_SERVICE_BASE(call.sip));
        free(call.sip);
    }
    if (dummy_src != NULL) {
        esp_media_dummy_service_destroy(dummy_src);
    }
    if (dummy_sink != NULL) {
        esp_media_dummy_service_destroy(dummy_sink);
    }
    return ret;
}
