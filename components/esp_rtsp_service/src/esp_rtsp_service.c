/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rtsp_service_err.h"
#include "esp_rtsp_service_ops.h"
#include "esp_rtsp_service_priv.h"

static const char *TAG = "RTSP_SVC";

char *rtsp_strdup_or_null(const char *str)
{
    return str == NULL ? NULL : strdup(str);
}

static esp_rtsp_service_event_t rtsp_state_to_service_event(esp_rtsp_state_t state)
{
    switch (state) {
    case RTSP_STATE_OPTIONS:
        return ESP_RTSP_SERVICE_EVENT_OPTIONS;
    case RTSP_STATE_ANNOUNCE:
        return ESP_RTSP_SERVICE_EVENT_ANNOUNCE;
    case RTSP_STATE_DESCRIBING:
        return ESP_RTSP_SERVICE_EVENT_DESCRIBING;
    case RTSP_STATE_DESCRIBE:
        return ESP_RTSP_SERVICE_EVENT_DESCRIBE;
    case RTSP_STATE_SETUP:
        return ESP_RTSP_SERVICE_EVENT_SETUP;
    case RTSP_STATE_PLAY:
        return ESP_RTSP_SERVICE_EVENT_PLAY;
    case RTSP_STATE_RECORD:
        return ESP_RTSP_SERVICE_EVENT_RECORD;
    case RTSP_STATE_TEARDOWN:
        return ESP_RTSP_SERVICE_EVENT_TEARDOWN;
    default:
        return 0;
    }
}

esp_err_t rtsp_service_publish_state(esp_rtsp_service_t *service, esp_rtsp_state_t state)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "publish state: service is NULL");
    esp_rtsp_service_event_t event_id = rtsp_state_to_service_event(state);
    if (event_id == 0) {
        return ESP_OK;
    }
    esp_rtsp_service_event_payload_t payload = {
        .role = service->role,
        .state = state,
    };
    esp_err_t ret = esp_service_publish_event(ESP_SERVICE_BASE(service), (uint16_t)event_id,
                                              &payload, sizeof(payload), NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Publish event %u failed: %s", (unsigned)event_id, esp_err_to_name(ret));
    }
    return ret;
}

int rtsp_service_state_handler(esp_rtsp_state_t state, void *ctx)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)ctx;
    if (service == NULL) {
        return -1;
    }
    /* esp_rtsp reports PLAY after a successful RECORD request in push mode. */
    if (service->role == ESP_RTSP_SERVICE_ROLE_SINK && state == RTSP_STATE_PLAY) {
        state = RTSP_STATE_RECORD;
    }
    return rtsp_service_publish_state(service, state) == ESP_OK ? 0 : -1;
}

void rtsp_get_media_thread_cfg(const esp_rtsp_service_t *service, const char *thread_name,
                               media_lib_thread_cfg_t *out_cfg)
{
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = 4096,
        .priority = 10,
        .core_id = 0,
    };
    esp_service_thread_cfg_t cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name ? service->media.base.name : ESP_RTSP_SERVICE_NAME,
        .thread_name = thread_name,
    };
    esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &cfg);
    out_cfg->stack_size = cfg.stack_size;
    out_cfg->priority = (uint8_t)cfg.priority;
    out_cfg->core_id = (uint8_t)(cfg.core_id < 0 ? 0 : cfg.core_id);
}

rtsp_payload_codec_t rtsp_to_audio_codec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_AAC) {
        return RTSP_ACODEC_AAC;
    }
    if (codec == ESP_MEDIA_CODEC_G711A) {
        return RTSP_ACODEC_G711A;
    }
    if (codec == ESP_MEDIA_CODEC_G711U) {
        return RTSP_ACODEC_G711U;
    }
    return RTSP_INVALID_CODEC;
}

rtsp_payload_codec_t rtsp_to_video_codec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_H264) {
        return RTSP_VCODEC_H264;
    }
    if (codec == ESP_MEDIA_CODEC_MJPEG) {
        return RTSP_VCODEC_MJPEG;
    }
    return RTSP_INVALID_CODEC;
}

esp_media_codec_fourcc_t rtsp_from_codec(rtsp_payload_codec_t codec)
{
    switch (codec) {
    case RTSP_ACODEC_AAC:
        return ESP_MEDIA_CODEC_AAC;
    case RTSP_ACODEC_G711A:
        return ESP_MEDIA_CODEC_G711A;
    case RTSP_ACODEC_G711U:
        return ESP_MEDIA_CODEC_G711U;
    case RTSP_VCODEC_H264:
        return ESP_MEDIA_CODEC_H264;
    case RTSP_VCODEC_MJPEG:
        return ESP_MEDIA_CODEC_MJPEG;
    default:
        return 0;
    }
}

static bool role_enabled(esp_rtsp_service_role_t role)
{
    switch (role) {
#ifdef CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SERVER:
        return true;
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SRC:
        return true;
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SINK:
        return true;
#endif
    default:
        return false;
    }
}

static esp_err_t ensure_stopped(esp_rtsp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_INITIALIZED,
                        ESP_ERR_INVALID_STATE, TAG, "bad state");
    return ESP_OK;
}

static esp_err_t rtsp_on_start(esp_service_t *base)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    switch (service->role) {
#ifdef CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SERVER:
        return rtsp_server_on_start(service);
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SINK:
        return rtsp_sink_on_start(service);
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SRC:
        return rtsp_src_on_start(service);
#endif
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t rtsp_on_stop(esp_service_t *base)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    switch (service->role) {
#ifdef CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SERVER:
        return rtsp_server_on_stop(service);
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SINK:
        return rtsp_sink_on_stop(service);
#endif
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    case ESP_RTSP_SERVICE_ROLE_SRC:
        return rtsp_src_on_stop(service);
#endif
    default:
        return ESP_OK;
    }
}

static esp_err_t rtsp_on_deinit(esp_service_t *base)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    (void)rtsp_on_stop(base);
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    if (service->role == ESP_RTSP_SERVICE_ROLE_SRC && service->src.mngr != NULL) {
        esp_media_track_mngr_destroy(service->src.mngr);
        service->src.mngr = NULL;
    }
#endif
    free(service->url);
    service->url = NULL;
    free(service->local_addr);
    service->local_addr = NULL;
    return ESP_OK;
}

static esp_err_t rtsp_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (service->role) {
    case ESP_RTSP_SERVICE_ROLE_SRC:
        *out_role = ESP_MEDIA_ROLE_SRC;
        break;
    case ESP_RTSP_SERVICE_ROLE_SINK:
    case ESP_RTSP_SERVICE_ROLE_SERVER:
        *out_role = ESP_MEDIA_ROLE_SINK;
        break;
    default:
        *out_role = ESP_MEDIA_ROLE_NONE;
        break;
    }
    return ESP_OK;
}

static esp_err_t rtsp_get_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                   esp_media_provider_t *out_provider)
{
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    if (service->role == ESP_RTSP_SERVICE_ROLE_SRC) {
        return rtsp_src_get_provider(service, stream, out_provider);
    }
#else
    (void)base;
    (void)stream;
    (void)out_provider;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtsp_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                   const esp_media_provider_t *provider)
{
#if defined(CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT) || defined(CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT)
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    if (service->role == ESP_RTSP_SERVICE_ROLE_SINK || service->role == ESP_RTSP_SERVICE_ROLE_SERVER) {
        return rtsp_sender_set_provider(service, stream, provider);
    }
#else
    (void)base;
    (void)stream;
    (void)provider;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtsp_get_request(esp_service_t *base, esp_media_stream_id_t stream,
                                  esp_media_service_request_t *request)
{
#if defined(CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT) || defined(CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT)
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    if (service->role == ESP_RTSP_SERVICE_ROLE_SINK || service->role == ESP_RTSP_SERVICE_ROLE_SERVER) {
        return rtsp_sender_get_request(service, stream, request);
    }
#else
    (void)base;
    (void)stream;
    (void)request;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtsp_set_request(esp_service_t *base, esp_media_stream_id_t stream,
                                  const esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    esp_rtsp_service_t *service = (esp_rtsp_service_t *)base;
    if (service->role == ESP_RTSP_SERVICE_ROLE_SRC) {
        return rtsp_src_set_request(service, stream, request);
    }
#else
    (void)base;
    (void)stream;
    (void)request;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static const char *rtsp_service_event_to_name(uint16_t event_id)
{
    switch (event_id) {
    case ESP_RTSP_SERVICE_EVENT_OPTIONS:
        return "OPTIONS";
    case ESP_RTSP_SERVICE_EVENT_ANNOUNCE:
        return "ANNOUNCE";
    case ESP_RTSP_SERVICE_EVENT_DESCRIBING:
        return "DESCRIBING";
    case ESP_RTSP_SERVICE_EVENT_DESCRIBE:
        return "DESCRIBE";
    case ESP_RTSP_SERVICE_EVENT_SETUP:
        return "SETUP";
    case ESP_RTSP_SERVICE_EVENT_PLAY:
        return "PLAY";
    case ESP_RTSP_SERVICE_EVENT_RECORD:
        return "RECORD";
    case ESP_RTSP_SERVICE_EVENT_TEARDOWN:
        return "TEARDOWN";
    default:
        return NULL;
    }
}

static const esp_service_ops_t s_rtsp_service_ops = {
    .on_start = rtsp_on_start,
    .on_stop = rtsp_on_stop,
    .on_deinit = rtsp_on_deinit,
    .event_to_name = rtsp_service_event_to_name,
};

static const esp_media_service_ops_t s_rtsp_media_ops = {
    .get_role = rtsp_get_role,
    .get_provider = rtsp_get_provider,
    .set_provider = rtsp_set_provider,
    .get_request = rtsp_get_request,
    .set_request = rtsp_set_request,
};

esp_err_t esp_rtsp_service_create(const esp_rtsp_service_cfg_t *cfg, esp_rtsp_service_t **out_service)
{
    ESP_RETURN_ON_FALSE(out_service != NULL && cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(role_enabled(cfg->role), ESP_ERR_NOT_SUPPORTED, TAG, "role disabled");

    esp_rtsp_service_t *service = calloc(1, sizeof(*service));
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    service->role = cfg->role;
    service->setup = (esp_rtsp_service_setup_t)ESP_RTSP_SERVICE_SETUP_DEFAULT();

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = cfg->name ? cfg->name : ESP_RTSP_SERVICE_NAME;
    media_cfg.service_ops = &s_rtsp_service_ops;
    media_cfg.media_ops = &s_rtsp_media_ops;
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        free(service);
        RET_FOR(ret, "media init failed");
    }

#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    if (service->role == ESP_RTSP_SERVICE_ROLE_SRC) {
        service->src.audio_cache_size = RTSP_DEFAULT_AUDIO_CACHE_SIZE;
        service->src.video_cache_size = RTSP_DEFAULT_CACHE_SIZE;
        ret = rtsp_src_ensure_mngr(service);
    }
#endif
    if (ret != ESP_OK) {
        esp_media_service_deinit(ESP_SERVICE_BASE(service));
        free(service);
        RET_FOR(ret, "src manager init failed");
    }

    *out_service = service;
    return ESP_OK;
}

esp_err_t esp_rtsp_service_setup(esp_rtsp_service_t *service, const esp_rtsp_service_setup_t *setup)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_stopped(service), TAG, "service not stopped");

    esp_rtsp_service_setup_t cfg = setup == NULL ? (esp_rtsp_service_setup_t)ESP_RTSP_SERVICE_SETUP_DEFAULT() : *setup;
    service->setup.local_port = cfg.local_port == 0 ? ESP_RTSP_SERVICE_DEFAULT_PORT : cfg.local_port;
    service->setup.transport = cfg.transport;
    service->setup.audio_enable = cfg.audio_enable;
    service->setup.video_enable = cfg.video_enable;
    service->setup.audio_cache_size = cfg.audio_cache_size;
    service->setup.video_cache_size = cfg.video_cache_size;
    service->setup.aud_frame_size = cfg.aud_frame_size;
    service->setup.vid_frame_size = cfg.vid_frame_size;
#ifdef CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT
    if (service->role == ESP_RTSP_SERVICE_ROLE_SRC) {
        if (cfg.audio_cache_size != 0) {
            service->src.audio_cache_size = cfg.audio_cache_size;
        }
        if (cfg.video_cache_size != 0) {
            service->src.video_cache_size = cfg.video_cache_size;
        }
    }
#endif
    return ESP_OK;
}

esp_err_t esp_rtsp_service_set_url(esp_rtsp_service_t *service, const char *url)
{
    ESP_RETURN_ON_FALSE(service != NULL && url != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_stopped(service), TAG, "service not stopped");

    char *copy = strdup(url);
    ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
    free(service->url);
    service->url = copy;
    return ESP_OK;
}

esp_err_t esp_rtsp_service_set_ip(esp_rtsp_service_t *service, const char *ip)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_stopped(service), TAG, "service not stopped");

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
