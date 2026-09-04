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
#include "esp_rtmp_service_err.h"
#include "esp_rtmp_service_ops.h"
#include "esp_rtmp_service_priv.h"

static const char *TAG = "RTMP_SVC";
#define RTMP_DEFAULT_STACK_SIZE 4096
#define RTMP_DEFAULT_PRIORITY   10
#define RTMP_DEFAULT_CORE_ID    0

esp_err_t rtmp_media_err_to_esp(esp_media_err_t err)
{
    return (esp_err_t)err;
}

char *rtmp_strdup_or_null(const char *str)
{
    return str == NULL ? NULL : strdup(str);
}

bool rtmp_url_is_secure(const char *url)
{
    return url != NULL && strncmp(url, "rtmps://", 8) == 0;
}

esp_err_t rtmp_service_publish_event(esp_rtmp_service_t *service, esp_rtmp_service_event_t event_id,
                                     const char *stream_name)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "publish event: service is NULL");
    esp_rtmp_service_event_payload_t payload = {
        .role = service->role,
    };
    if (stream_name != NULL) {
        strncpy(payload.stream_name, stream_name, sizeof(payload.stream_name) - 1);
    }
    esp_err_t ret = esp_service_publish_event(ESP_SERVICE_BASE(service), (uint16_t)event_id,
                                              &payload, sizeof(payload), NULL, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Publish event %u failed: %s", (unsigned)event_id, esp_err_to_name(ret));
    }
    return ret;
}

int rtmp_service_protocol_event(esp_rtmp_event_t event, void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    if (service == NULL) {
        return -1;
    }
    if (event == RTMP_EVENT_CLOSED_BY_SERVER) {
        (void)rtmp_service_publish_event(service, ESP_RTMP_SERVICE_EVENT_PEER_CLOSED, NULL);
    }
    return 0;
}

void rtmp_get_media_thread_cfg(const esp_rtmp_service_t *service, const char *thread_name,
                               media_lib_thread_cfg_t *out_cfg)
{
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = RTMP_DEFAULT_STACK_SIZE,
        .priority = RTMP_DEFAULT_PRIORITY,
        .core_id = RTMP_DEFAULT_CORE_ID,
    };
    esp_service_thread_cfg_t cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service->media.base.name ? service->media.base.name : ESP_RTMP_SERVICE_NAME,
        .thread_name = thread_name,
    };
    esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &cfg);
    out_cfg->stack_size = cfg.stack_size;
    out_cfg->priority = (uint8_t)cfg.priority;
    out_cfg->core_id = (uint8_t)(cfg.core_id < 0 ? 0 : cfg.core_id);
}

esp_rtmp_audio_codec_t rtmp_to_audio_codec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_AAC || codec == RTMP_AUDIO_CODEC_AAC) {
        return RTMP_AUDIO_CODEC_AAC;
    }
    if (codec == ESP_MEDIA_CODEC_MP3 || codec == RTMP_AUDIO_CODEC_MP3) {
        return RTMP_AUDIO_CODEC_MP3;
    }
    if (codec == ESP_MEDIA_CODEC_PCM || codec == RTMP_AUDIO_CODEC_PCM) {
        return RTMP_AUDIO_CODEC_PCM;
    }
    if (codec == ESP_MEDIA_CODEC_G711A || codec == ESP_FOURCC_ALAW || codec == RTMP_AUDIO_CODEC_G711A) {
        return RTMP_AUDIO_CODEC_G711A;
    }
    if (codec == ESP_MEDIA_CODEC_G711U || codec == ESP_FOURCC_ULAW || codec == RTMP_AUDIO_CODEC_G711U) {
        return RTMP_AUDIO_CODEC_G711U;
    }
    return RTMP_AUDIO_CODEC_NONE;
}

esp_rtmp_video_codec_t rtmp_to_video_codec(esp_media_codec_fourcc_t codec)
{
    if (codec == ESP_MEDIA_CODEC_H264 || codec == RTMP_VIDEO_CODEC_H264) {
        return RTMP_VIDEO_CODEC_H264;
    }
    if (codec == ESP_MEDIA_CODEC_MJPEG || codec == RTMP_VIDEO_CODEC_MJPEG) {
        return RTMP_VIDEO_CODEC_MJPEG;
    }
    return RTMP_VIDEO_CODEC_NONE;
}

esp_media_codec_fourcc_t rtmp_from_audio_codec(esp_rtmp_audio_codec_t codec)
{
    switch (codec) {
    case RTMP_AUDIO_CODEC_AAC:
        return ESP_MEDIA_CODEC_AAC;
    case RTMP_AUDIO_CODEC_MP3:
        return ESP_MEDIA_CODEC_MP3;
    case RTMP_AUDIO_CODEC_PCM:
        return ESP_MEDIA_CODEC_PCM;
    case RTMP_AUDIO_CODEC_G711A:
        return ESP_MEDIA_CODEC_G711A;
    case RTMP_AUDIO_CODEC_G711U:
        return ESP_MEDIA_CODEC_G711U;
    default:
        return 0;
    }
}

esp_media_codec_fourcc_t rtmp_from_video_codec(esp_rtmp_video_codec_t codec)
{
    switch (codec) {
    case RTMP_VIDEO_CODEC_H264:
        return ESP_MEDIA_CODEC_H264;
    case RTMP_VIDEO_CODEC_MJPEG:
        return ESP_MEDIA_CODEC_MJPEG;
    default:
        return 0;
    }
}

static bool role_enabled(esp_rtmp_service_role_t role)
{
    switch (role) {
#ifdef CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SERVER:
        return true;
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SRC:
        return true;
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SINK:
        return true;
#endif
    default:
        return false;
    }
}

static esp_err_t ensure_stopped(esp_rtmp_service_t *service)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    ESP_RETURN_ON_FALSE(esp_service_get_state(ESP_SERVICE_BASE(service), &state) == ESP_OK &&
                            state == ESP_SERVICE_STATE_INITIALIZED,
                        ESP_ERR_INVALID_STATE, TAG, "bad state");
    return ESP_OK;
}

static esp_err_t parse_server_url(const char *url, uint16_t *out_port, char **out_app_name)
{
    const char *host = NULL;
    if (strncmp(url, "rtmp://", 7) == 0) {
        host = url + 7;
    } else if (strncmp(url, "rtmps://", 8) == 0) {
        host = url + 8;
    } else {
        return ESP_ERR_INVALID_ARG;
    }
    const char *slash = strchr(host, '/');
    const char *host_end = slash ? slash : host + strlen(host);
    ESP_RETURN_ON_FALSE(host_end > host, ESP_ERR_INVALID_ARG, TAG, "bad url");

    const char *colon = memchr(host, ':', host_end - host);
    *out_port = 0;
    *out_app_name = NULL;
    if (colon != NULL) {
        ESP_RETURN_ON_FALSE(colon > host && colon + 1 < host_end, ESP_ERR_INVALID_ARG, TAG, "bad port");
        for (const char *p = colon + 1; p < host_end; p++) {
            ESP_RETURN_ON_FALSE(*p >= '0' && *p <= '9', ESP_ERR_INVALID_ARG, TAG, "bad port");
        }
        int parsed = atoi(colon + 1);
        ESP_RETURN_ON_FALSE(parsed > 0 && parsed <= UINT16_MAX, ESP_ERR_INVALID_ARG, TAG, "bad port");
        *out_port = (uint16_t)parsed;
    }
    if (slash != NULL && slash[1] != '\0') {
        const char *app_end = strchr(slash + 1, '/');
        size_t app_len = app_end ? (size_t)(app_end - slash - 1) : strlen(slash + 1);
        ESP_RETURN_ON_FALSE(app_len > 0, ESP_ERR_INVALID_ARG, TAG, "bad app");
        char *copy = calloc(1, app_len + 1);
        ESP_RETURN_ON_FALSE(copy != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
        memcpy(copy, slash + 1, app_len);
        *out_app_name = copy;
    }
    return ESP_OK;
}

static esp_err_t rtmp_on_start(esp_service_t *base)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    switch (service->role) {
#ifdef CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SERVER:
        return rtmp_server_on_start(service);
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SINK:
        return rtmp_sink_on_start(service);
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SRC:
        return rtmp_src_on_start(service);
#endif
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t rtmp_on_stop(esp_service_t *base)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    switch (service->role) {
#ifdef CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SERVER:
        return rtmp_server_on_stop(service);
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SINK:
        return rtmp_sink_on_stop(service);
#endif
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    case ESP_RTMP_SERVICE_ROLE_SRC:
        return rtmp_src_on_stop(service);
#endif
    default:
        return ESP_OK;
    }
}

static esp_err_t rtmp_on_deinit(esp_service_t *base)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    (void)rtmp_on_stop(base);
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    if (service->role == ESP_RTMP_SERVICE_ROLE_SRC && service->src.mngr != NULL) {
        esp_media_track_mngr_destroy(service->src.mngr);
        service->src.mngr = NULL;
    }
#endif
    if (service->role == ESP_RTMP_SERVICE_ROLE_SERVER) {
        free(service->server.app_name);
        service->server.app_name = NULL;
    }
    free(service->url);
    service->url = NULL;
    return ESP_OK;
}

static esp_err_t rtmp_get_role(esp_service_t *base, esp_media_role_t *out_role)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    if (out_role == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (service->role) {
    case ESP_RTMP_SERVICE_ROLE_SRC:
        *out_role = ESP_MEDIA_ROLE_SRC;
        break;
    case ESP_RTMP_SERVICE_ROLE_SINK:
        *out_role = ESP_MEDIA_ROLE_SINK;
        break;
    default:
        *out_role = ESP_MEDIA_ROLE_NONE;
        break;
    }
    return ESP_OK;
}

static esp_err_t rtmp_get_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                   esp_media_provider_t *out_provider)
{
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    if (service->role == ESP_RTMP_SERVICE_ROLE_SRC) {
        return rtmp_src_get_provider(service, stream, out_provider);
    }
#else
    (void)base;
    (void)stream;
    (void)out_provider;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtmp_set_provider(esp_service_t *base, esp_media_stream_id_t stream,
                                   const esp_media_provider_t *provider)
{
#ifdef CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    if (service->role == ESP_RTMP_SERVICE_ROLE_SINK) {
        return rtmp_sink_set_provider(service, stream, provider);
    }
#else
    (void)base;
    (void)stream;
    (void)provider;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtmp_get_request(esp_service_t *base, esp_media_stream_id_t stream,
                                  esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    if (service->role == ESP_RTMP_SERVICE_ROLE_SINK) {
        return rtmp_sink_get_request(service, stream, request);
    }
#else
    (void)base;
    (void)stream;
    (void)request;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t rtmp_set_request(esp_service_t *base, esp_media_stream_id_t stream,
                                  const esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)base;
    if (service->role == ESP_RTMP_SERVICE_ROLE_SRC) {
        return rtmp_src_set_request(service, stream, request);
    }
#else
    (void)base;
    (void)stream;
    (void)request;
#endif
    return ESP_ERR_NOT_SUPPORTED;
}

static const char *rtmp_service_event_to_name(uint16_t event_id)
{
    switch (event_id) {
    case ESP_RTMP_SERVICE_EVENT_PEER_CLOSED:
        return "PEER_CLOSED";
    case ESP_RTMP_SERVICE_EVENT_SERVER_CLIENT_CONNECTED:
        return "SERVER_CLIENT_CONNECTED";
    case ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STARTED:
        return "SERVER_PULLER_STARTED";
    case ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STOPPED:
        return "SERVER_PULLER_STOPPED";
    default:
        return NULL;
    }
}

static const esp_service_ops_t s_rtmp_service_ops = {
    .on_start = rtmp_on_start,
    .on_stop = rtmp_on_stop,
    .on_deinit = rtmp_on_deinit,
    .event_to_name = rtmp_service_event_to_name,
};

static const esp_media_service_ops_t s_rtmp_media_ops = {
    .get_role = rtmp_get_role,
    .get_provider = rtmp_get_provider,
    .set_provider = rtmp_set_provider,
    .get_request = rtmp_get_request,
    .set_request = rtmp_set_request,
};

esp_err_t esp_rtmp_service_create(const esp_rtmp_service_cfg_t *cfg, esp_rtmp_service_t **out_service)
{
    ESP_RETURN_ON_FALSE(out_service != NULL && cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(role_enabled(cfg->role), ESP_ERR_NOT_SUPPORTED, TAG, "role disabled");

    esp_rtmp_service_t *service = calloc(1, sizeof(*service));
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_NO_MEM, TAG, "no mem");

    service->role = cfg->role;
    service->chunk_size = 0;
    switch (service->role) {
    case ESP_RTMP_SERVICE_ROLE_SERVER:
        service->server.port = ESP_RTMP_SERVICE_DEFAULT_PORT;
        service->server.max_clients = 4;
        service->server.app_name = rtmp_strdup_or_null(ESP_RTMP_SERVICE_DEFAULT_APP_NAME);
        if (service->server.app_name == NULL) {
            free(service);
            RET_FOR(ESP_ERR_NO_MEM, "no mem");
        }
        break;
    case ESP_RTMP_SERVICE_ROLE_SRC:
        service->src.need_global_cache = false;
        service->src.cache_size = RTMP_DEFAULT_CACHE_SIZE;
        service->src.audio_cache_size = RTMP_DEFAULT_AUDIO_CACHE_SIZE;
        service->src.video_cache_size = RTMP_DEFAULT_VIDEO_CACHE_SIZE;
        break;
    case ESP_RTMP_SERVICE_ROLE_SINK:
        break;
    default:
        free(service);
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "unsupported role");
    }

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = cfg->name ? cfg->name : ESP_RTMP_SERVICE_NAME;
    media_cfg.service_ops = &s_rtmp_service_ops;
    media_cfg.media_ops = &s_rtmp_media_ops;
    esp_err_t ret = esp_media_service_init(&service->media, &media_cfg);
    if (ret != ESP_OK) {
        free(service->server.app_name);
        free(service);
        RET_FOR(ret, "media init failed");
    }

    *out_service = service;
    return ESP_OK;
}

esp_err_t esp_rtmp_service_setup(esp_rtmp_service_t *service, const esp_rtmp_service_setup_t *setup)
{
    ESP_RETURN_ON_FALSE(service != NULL && setup != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_stopped(service), TAG, "service not stopped");

    if (setup->chunk_size != 0) {
        service->chunk_size = setup->chunk_size;
    }
    memcpy(&service->ssl_cfg, &setup->ssl_cfg, sizeof(service->ssl_cfg));

    switch (service->role) {
    case ESP_RTMP_SERVICE_ROLE_SERVER:
        if (setup->server.port != 0) {
            service->server.port = setup->server.port;
        }
        if (setup->server.app_name != NULL) {
            char *app_name = strdup(setup->server.app_name);
            ESP_RETURN_ON_FALSE(app_name != NULL, ESP_ERR_NO_MEM, TAG, "no mem");
            free(service->server.app_name);
            service->server.app_name = app_name;
        }
        if (setup->server.max_clients != 0) {
            service->server.max_clients = setup->server.max_clients;
        }
        break;
    case ESP_RTMP_SERVICE_ROLE_SRC:
#ifdef CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT
        if (setup->src.cache_size != 0) {
            service->src.cache_size = setup->src.cache_size;
        }
        if (setup->src.audio_cache_size != 0) {
            service->src.audio_cache_size = setup->src.audio_cache_size;
        }
        if (setup->src.video_cache_size != 0) {
            service->src.video_cache_size = setup->src.video_cache_size;
        }
        if (service->src.mngr != NULL) {
            esp_err_t ret = esp_media_track_mngr_set_global_cache(service->src.mngr,
                                                                  service->src.need_global_cache,
                                                                  service->src.cache_size);
            if (ret != ESP_OK) {
                RET_FOR(ret, "set global cache failed");
            }
        }
#endif
        break;
    case ESP_RTMP_SERVICE_ROLE_SINK:
        break;
    default:
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "unsupported role");
    }
    return ESP_OK;
}

esp_err_t esp_rtmp_service_set_url(esp_rtmp_service_t *service, const char *url)
{
    ESP_RETURN_ON_FALSE(service != NULL && url != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_ERROR(ensure_stopped(service), TAG, "service not stopped");

    uint16_t port = 0;
    char *app_name = NULL;
    if (service->role == ESP_RTMP_SERVICE_ROLE_SERVER) {
        ESP_RETURN_ON_ERROR(parse_server_url(url, &port, &app_name), TAG, "parse url failed");
    }

    char *copy = strdup(url);
    if (copy == NULL) {
        free(app_name);
        RET_FOR(ESP_ERR_NO_MEM, "no mem");
    }
    free(service->url);
    service->url = copy;
    if (port != 0) {
        service->server.port = port;
    }
    if (app_name != NULL) {
        free(service->server.app_name);
        service->server.app_name = app_name;
    }
    return ESP_OK;
}

esp_err_t esp_rtmp_service_query(esp_rtmp_service_t *service)
{
    ESP_RETURN_ON_FALSE(service != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid arg");
    ESP_RETURN_ON_FALSE(service->role == ESP_RTMP_SERVICE_ROLE_SERVER, ESP_ERR_NOT_SUPPORTED,
                        TAG, "query is server only");
#ifdef CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT
    return rtmp_server_query(service);
#else
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "RTMP server support is disabled");
#endif
}