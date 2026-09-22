/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_sip_service_err.h"
#include "esp_sip_service_ops.h"
#include "esp_sip_service_priv.h"

#define SIP_CALL_TARGET_MAX  128
#define SIP_DTMF_EVENT_MAX   15

static const char *TAG = "SIP_SIG";

esp_err_t esp_sip_service_call(esp_sip_service_t *service, const char *remote_user)
{
    ESP_RETURN_ON_FALSE(service != NULL && remote_user != NULL && remote_user[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    /* Without a registrar the stack cannot resolve a bare user and insists on
     * "user@host:port", so the peer of the configured uri is filled in. Only
     * this path needs it, hence the parse here. */
    char target[SIP_CALL_TARGET_MAX];
    if (service->setup.p2p_mode && strchr(remote_user, '@') == NULL) {
        char peer_host[SIP_PEER_HOST_MAX] = {0};
        uint16_t peer_port = 0;
        ESP_RETURN_ON_FALSE(service->uri != NULL, ESP_ERR_INVALID_STATE, TAG, "uri not set");
        ESP_RETURN_ON_ERROR(sip_parse_uri(service->uri, peer_host, sizeof(peer_host), &peer_port), TAG, "bad uri");
        int len = snprintf(target, sizeof(target), "%s@%s:%u", remote_user, peer_host, (unsigned)peer_port);
        ESP_RETURN_ON_FALSE(len > 0 && len < (int)sizeof(target), ESP_ERR_INVALID_ARG, TAG,
                            "p2p call target too long");
        remote_user = target;
    }

    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    esp_rtc_handle_t handle = service->handle;
    if (handle == NULL) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "service not running");
    }
    if (service->call_state != SIP_CALL_IDLE) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "call already in progress");
    }
    /* Mark the attempt before inviting: the protocol may report CALLING from
     * its own thread before esp_rtc_call() returns here. */
    service->call_state = SIP_CALL_OUTGOING;
    sip_service_unlock(service);

    int ret = esp_rtc_call(handle, remote_user);
    if (ret != ESP_OK) {
        if (sip_service_lock(service) == ESP_OK) {
            if (service->call_state == SIP_CALL_OUTGOING) {
                service->call_state = SIP_CALL_IDLE;
            }
            sip_service_unlock(service);
        }
        RET_FOR((esp_err_t)ret, "call failed: %s", esp_err_to_name((esp_err_t)ret));
    }
    return ESP_OK;
}

esp_err_t esp_sip_service_answer(esp_sip_service_t *service)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    esp_rtc_handle_t handle = service->handle;
    sip_call_state_t state = service->call_state;
    sip_service_unlock(service);
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_STATE, TAG, "service not running");
    ESP_RETURN_ON_FALSE(state == SIP_CALL_INCOMING, ESP_ERR_INVALID_STATE, TAG, "no ringing call");

    int ret = esp_rtc_answer(handle);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, (esp_err_t)ret, TAG, "answer failed");
    return ESP_OK;
}

esp_err_t esp_sip_service_bye(esp_sip_service_t *service)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    esp_rtc_handle_t handle = service->handle;
    sip_call_state_t state = service->call_state;
    sip_service_unlock(service);
    ESP_RETURN_ON_FALSE(handle != NULL, ESP_ERR_INVALID_STATE, TAG, "service not running");
    if (state == SIP_CALL_IDLE) {
        return ESP_OK;
    }

    int ret = esp_rtc_bye(handle);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, (esp_err_t)ret, TAG, "bye failed");
    return ESP_OK;
}

esp_err_t esp_sip_service_send_dtmf(esp_sip_service_t *service, const esp_sip_service_dtmf_t *dtmf)
{
    ESP_RETURN_ON_FALSE(service != NULL && dtmf != NULL && dtmf->event <= SIP_DTMF_EVENT_MAX, ESP_ERR_INVALID_ARG,
                        TAG, "invalid arg");
    esp_rtc_handle_t handle = NULL;
    ESP_RETURN_ON_ERROR(sip_service_get_handle(service, &handle), TAG, "service not running");

    int ret = esp_rtc_send_dtmf(handle, dtmf->event, dtmf->volume, dtmf->duration_ms);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, (esp_err_t)ret, TAG, "send dtmf failed");
    return ESP_OK;
}

esp_err_t esp_sip_service_send_message(esp_sip_service_t *service, const esp_sip_service_msg_t *msg)
{
    ESP_RETURN_ON_FALSE(service != NULL && msg != NULL && msg->body != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    /* The protocol stack allows only one MESSAGE in flight and is not
     * reentrant, so hold the slot until MESSAGE_SENT clears it. */
    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    esp_rtc_handle_t handle = service->handle;
    if (handle == NULL) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "service not running");
    }
    if (service->msg_pending) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "a MESSAGE is still pending");
    }
    service->msg_pending = true;
    sip_service_unlock(service);

    esp_rtc_msg_data_t data = {
        .content_type = msg->content_type != NULL ? msg->content_type : "text/plain",
        .body = msg->body,
        .body_len = msg->body_len,
        .peer_uri = msg->peer_uri,
    };
    int ret = esp_rtc_send_message(handle, &data);
    if (ret != ESP_OK) {
        if (sip_service_lock(service) == ESP_OK) {
            service->msg_pending = false;
            sip_service_unlock(service);
        }
        RET_FOR((esp_err_t)ret, "send message failed: %s", esp_err_to_name((esp_err_t)ret));
    }
    return ESP_OK;
}

esp_err_t esp_sip_service_is_srtp_active(esp_sip_service_t *service, bool *out_active)
{
    ESP_RETURN_ON_FALSE(service != NULL && out_active != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");

    /* Sampled when the call was answered so the answer stays stable even after
     * the session ends. */
    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    *out_active = service->srtp_active;
    sip_service_unlock(service);
    return ESP_OK;
}

esp_err_t esp_sip_service_get_peer(esp_sip_service_t *service, char *buf, size_t buf_size)
{
    ESP_RETURN_ON_FALSE(service != NULL && buf != NULL && buf_size > 0, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    if (service->handle == NULL) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "service not running");
    }
    const char *peer = esp_rtc_get_peer(service->handle);
    esp_err_t ret = ESP_OK;
    if (peer == NULL || peer[0] == '\0') {
        ret = ESP_ERR_NOT_FOUND;
    } else {
        size_t len = strlen(peer);
        if (len >= buf_size) {
            ret = ESP_ERR_INVALID_SIZE;
        } else {
            memcpy(buf, peer, len + 1);
        }
    }
    sip_service_unlock(service);
    return ret;
}

esp_err_t esp_sip_service_read_raw_headers(esp_sip_service_t *service, char *buf, size_t buf_size, int *out_len)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(buf == NULL || buf_size > 0, ESP_ERR_INVALID_ARG, TAG, "invalid buf size");
    ESP_RETURN_ON_ERROR(sip_service_check_running(service), TAG, "service not running");

    ESP_RETURN_ON_ERROR(sip_service_lock(service), TAG, "lock failed");
    if (service->handle == NULL) {
        sip_service_unlock(service);
        RET_FOR(ESP_ERR_INVALID_STATE, "service not running");
    }
    int len = esp_rtc_read_raw_headers(service->handle, buf, buf == NULL ? 0 : (int)buf_size);
    sip_service_unlock(service);

    if (len < 0) {
        RET_FOR(ESP_FAIL, "no headers available or buffer too small");
    }
    if (out_len != NULL) {
        *out_len = len;
    }
    return ESP_OK;
}

esp_err_t esp_sip_service_set_invite_info(esp_sip_service_t *service, const esp_sip_service_headers_t *headers)
{
    ESP_RETURN_ON_FALSE(service != NULL && headers != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    esp_rtc_handle_t handle = NULL;
    ESP_RETURN_ON_ERROR(sip_service_get_handle(service, &handle), TAG, "service not running");

    /* The stack copies the fields, so the caller strings need only live for
     * the duration of this call. */
    esp_rtc_sip_message_info_t info = {
        .via = (char *)headers->via,
        .from = (char *)headers->from,
        .to = (char *)headers->to,
        .contact = (char *)headers->contact,
        .special = (char *)headers->special,
    };
    int ret = esp_rtc_set_invite_info(handle, &info);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, (esp_err_t)ret, TAG, "set invite info failed");
    return ESP_OK;
}

esp_err_t esp_sip_service_set_private_header(esp_sip_service_t *service, const char *header)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    esp_rtc_handle_t handle = NULL;
    ESP_RETURN_ON_ERROR(sip_service_get_handle(service, &handle), TAG, "service not running");

    int ret = esp_rtc_set_private_header(handle, header);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, (esp_err_t)ret, TAG, "set private header failed");
    return ESP_OK;
}
