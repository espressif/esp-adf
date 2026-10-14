/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"

#include "esp_llm_baidu_internal.h"

static const char *TAG = "ESP_LLM_BAIDU_INTERCOM";

static esp_err_t intercom_mode_stop(esp_llm_baidu_ctx_t *ctx);

static void fill_intercom_defaults(esp_llm_baidu_ctx_t *ctx)
{
    esp_llm_baidu_intercom_cfg_t *ic = &ctx->cfg.intercom;
    ic->display_name = baidu_str_or(ic->display_name, "espcall");
    ic->token = baidu_str_or(ic->token, "no_token");

    if (ic->user_id != NULL && ic->user_id[0] != '\0') {
        snprintf(ctx->intercom_user_id, sizeof(ctx->intercom_user_id), "%s", ic->user_id);
    } else {
        snprintf(ctx->intercom_user_id, sizeof(ctx->intercom_user_id), "600%04d",
                 (int)(esp_random() % 10000 + 3));
    }
    ic->user_id = ctx->intercom_user_id;
}

static void intercom_auto_reconnect(void *h)
{
    (void)h;
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL || ctx->rtc_client == NULL) {
        if (ctx != NULL) {
            baidu_callback_exit(ctx);
        }
        return;
    }
    ctx->connected = false;
    ctx->stream_up = false;
    (void)brtc_logout_room(ctx->rtc_client);
    if (brtc_login_room(ctx->rtc_client,
                        ctx->cfg.intercom.room_name,
                        ctx->intercom_user_id,
                        ctx->cfg.intercom.display_name,
                        ctx->cfg.intercom.token) == 1) {
        ESP_LOGI(TAG, "Intercom re-login ok");
    }
    baidu_callback_exit(ctx);
}

static void on_rtc_message(RtcMessage *msg)
{
    if (msg == NULL) {
        return;
    }
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    switch (msg->msgType) {
        case RTC_MESSAGE_ROOM_EVENT_LOGIN_OK:
            if (ctx != NULL) {
                ctx->connected = true;
                if (ctx->evt_group != NULL) {
                    xEventGroupSetBits(ctx->evt_group, BAIDU_JOIN_EVENT_BIT);
                }
                (void)baidu_emit(ctx,  ESP_LLM_EVENT_CONNECTED,
                                 "INTERCOM_LOGIN_OK", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                 ESP_LLM_ROLE_NONE, NULL, 0);
            }
            break;
        case RTC_MESSAGE_ROOM_EVENT_LOGIN_ERROR:
            if (ctx != NULL) {
                ctx->connected = false;
                (void)baidu_emit(ctx,  ESP_LLM_EVENT_ERROR,
                                 "INTERCOM_LOGIN_ERROR", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                 ESP_LLM_ROLE_NONE, NULL, 0);
            }
            break;
        case RTC_MESSAGE_ROOM_EVENT_CONNECTION_LOST:
            brtc_sdk_do_async(intercom_auto_reconnect, NULL, 50);
            break;
        case RTC_MESSAGE_STATE_STREAM_UP:
            if (ctx != NULL) {
                ctx->stream_up = true;
                (void)baidu_emit(ctx,  ESP_LLM_EVENT_MEDIA_READY,
                                 "INTERCOM_STREAM_UP", ESP_LLM_EVENT_PAYLOAD_TYPE_TEXT,
                                 ESP_LLM_ROLE_NONE, NULL, 0);
            }
            break;
        default:
            break;
    }
    if (ctx != NULL) {
        baidu_callback_exit(ctx);
    }
}

static void on_intercom_audio(int64_t feedid, const char *audio, int len, int samplerate, int channels)
{
    (void)feedid;
    (void)channels;
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    if (ctx == NULL || audio == NULL || len <= 0) {
        if (ctx != NULL) {
            baidu_callback_exit(ctx);
        }
        return;
    }
    if (samplerate > 0) {
        ctx->downlink_rate = (uint32_t)samplerate;
    }
    esp_err_t ret = baidu_feed_tts(ctx, (const uint8_t *)audio, (size_t)len, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Intercom audio output failed: %s", esp_err_to_name(ret));
    }
    baidu_callback_exit(ctx);
}

static void on_intercom_video(int64_t feedid, const char *img, int len, RtcImageType imgtype, int width, int height)
{
    (void)feedid;
    esp_llm_baidu_ctx_t *ctx = baidu_callback_enter();
    baidu_on_video_data(ctx, (const uint8_t *)img, (size_t)len, (int)imgtype, width, height);
    if (ctx != NULL) {
        baidu_callback_exit(ctx);
    }
}

static esp_err_t intercom_mode_start(esp_llm_baidu_ctx_t *ctx)
{
    fill_intercom_defaults(ctx);
    baidu_apply_downlink_audio_profile(ctx);
    brtc_sdk_enable_inner_av_devices(0);
    brtc_sdk_enable_log(0);
    brtc_sdk_enable_nack(1);
    (void)brtc_sdk_init();
    ctx->sdk_inited = true;

    ctx->rtc_client = brtc_create_client();
    if (ctx->rtc_client == NULL) {
        intercom_mode_stop(ctx);
        return ESP_FAIL;
    }

    brtc_register_message_listener(ctx->rtc_client, on_rtc_message);
    brtc_register_audio_frame_observer(ctx->rtc_client, on_intercom_audio);
    brtc_register_video_frame_observer(ctx->rtc_client, on_intercom_video);
    brtc_set_cer(ctx->rtc_client, ctx->cfg.intercom.cer);
    brtc_set_server_url(ctx->rtc_client, ctx->cfg.intercom.server_url);
    brtc_set_appid(ctx->rtc_client, ctx->cfg.intercom.app_id);
    brtc_set_auto_publish(ctx->rtc_client, 1);
    brtc_set_auto_subscribe(ctx->rtc_client, 1);
    brtc_set_audiocodec(ctx->rtc_client, BAIDU_INTERCOM_AUDIO_CODEC);

    if (baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP) ||
        baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN)) {
        brtc_set_videocodec(ctx->rtc_client, baidu_sdk_video_codec(&ctx->cfg));
        brtc_set_usingvideo(ctx->rtc_client,
                            baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_UP) ? 1 : 0);
        brtc_set_receivingvideo(ctx->rtc_client,
                                baidu_stream_on(&ctx->cfg, ESP_LLM_MEDIA_STREAM_VIDEO_DN) ? 1 : 0);
    } else {
        brtc_set_usingvideo(ctx->rtc_client, 0);
        brtc_set_receivingvideo(ctx->rtc_client, 0);
    }

    if (brtc_login_room(ctx->rtc_client,
                        ctx->cfg.intercom.room_name,
                        ctx->intercom_user_id,
                        ctx->cfg.intercom.display_name,
                        ctx->cfg.intercom.token) != 1) {
        ESP_LOGE(TAG, "Intercom login failed");
        intercom_mode_stop(ctx);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Intercom client started room=%s user=%s", ctx->cfg.intercom.room_name,
             ctx->intercom_user_id);
    return ESP_OK;
}

static esp_err_t intercom_mode_stop(esp_llm_baidu_ctx_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ctx->connected = false;
    ctx->stream_up = false;
    if (ctx->rtc_client != NULL) {
        (void)brtc_logout_room(ctx->rtc_client);
        brtc_destroy_client(ctx->rtc_client);
        ctx->rtc_client = NULL;
    }
    if (ctx->sdk_inited) {
        (void)brtc_sdk_deinit();
        ctx->sdk_inited = false;
    }
    return ESP_OK;
}

static esp_err_t intercom_mode_send_text(esp_llm_baidu_ctx_t *ctx, const char *text)
{
    (void)ctx;
    (void)text;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t intercom_mode_send_frame(esp_llm_baidu_ctx_t *ctx, const esp_media_frame_t *frame)
{
    if (ctx == NULL || frame == NULL || frame->data == NULL || frame->size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctx->connected || ctx->rtc_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
        brtc_send_audio(ctx->rtc_client, frame->data, (int)frame->size);
        return ESP_OK;
    }
    if (frame->type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
        brtc_send_image(ctx->rtc_client, frame->data, (int)frame->size);
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t intercom_mode_send_custom_data(esp_llm_baidu_ctx_t *ctx, const char *data)
{
    if (ctx == NULL || data == NULL || data[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ctx->connected || ctx->rtc_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Broadcast to the room; BRTC treats user id "0" as everyone. */
    brtc_send_message_to_user(ctx->rtc_client, data, BAIDU_INTERCOM_BROADCAST_ID);
    return ESP_OK;
}

static esp_err_t intercom_mode_cancel_response(esp_llm_baidu_ctx_t *ctx)
{
    (void)ctx;
    return ESP_ERR_NOT_SUPPORTED;
}

const esp_llm_baidu_mode_ops_t baidu_intercom_mode_ops = {
    .start            = intercom_mode_start,
    .stop             = intercom_mode_stop,
    .send_text        = intercom_mode_send_text,
    .send_frame       = intercom_mode_send_frame,
    .send_custom_data = intercom_mode_send_custom_data,
    .cancel_response  = intercom_mode_cancel_response,
};
