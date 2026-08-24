/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "media_dummy_priv.h"

static const char *TAG = "DUMMY_SVC";

static esp_err_t get_role(esp_service_t *service, esp_media_role_t *role)
{
    esp_media_dummy_service_t *svc = (esp_media_dummy_service_t *)service;
    *role = svc->role;
    return ESP_OK;
}

static esp_err_t on_start(esp_service_t *service)
{
#if defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT) || defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT)
    esp_media_dummy_service_t *svc = (esp_media_dummy_service_t *)service;
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SRC) {
        svc->running = true;
        esp_err_t ret = media_dummy_src_on_start(svc);
        if (ret != ESP_OK) {
            svc->running = false;
            (void)media_dummy_src_on_stop(svc);
        }
        return ret;
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SINK) {
        return media_dummy_sink_on_start(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
#else
    (void)service;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT || CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t on_stop(esp_service_t *service)
{
#if defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT) || defined(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT)
    esp_media_dummy_service_t *svc = (esp_media_dummy_service_t *)service;
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SRC) {
        svc->running = false;
        return media_dummy_src_on_stop(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SINK) {
        return media_dummy_sink_on_stop(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
#else
    (void)service;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT || CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t get_provider(esp_service_t *service, esp_media_stream_id_t stream,
                              esp_media_provider_t *out_provider)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    return media_dummy_src_get_provider((esp_media_dummy_service_t *)service, stream, out_provider);
#else
    (void)service;
    (void)stream;
    (void)out_provider;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
}

static esp_err_t set_request(esp_service_t *service, esp_media_stream_id_t stream,
                             const esp_media_service_request_t *request)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    return media_dummy_src_set_request((esp_media_dummy_service_t *)service, stream, request);
#else
    (void)service;
    (void)stream;
    (void)request;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
}

static esp_err_t set_provider(esp_service_t *service, esp_media_stream_id_t stream,
                              const esp_media_provider_t *provider)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    return media_dummy_sink_set_provider((esp_media_dummy_service_t *)service, stream, provider);
#else
    (void)service;
    (void)stream;
    (void)provider;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
}

static const esp_service_ops_t s_service_ops = {
    .on_start = on_start,
    .on_stop  = on_stop,
};

static const esp_media_service_ops_t s_src_media_ops = {
    .get_role     = get_role,
    .get_provider = get_provider,
    .set_request  = set_request,
};

static const esp_media_service_ops_t s_sink_media_ops = {
    .get_role     = get_role,
    .set_provider = set_provider,
};

esp_err_t esp_media_dummy_service_create(const esp_media_dummy_service_cfg_t *cfg,
                                         esp_media_dummy_service_t **out)
{
    if (cfg == NULL || out == NULL || cfg->max_stream_num == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cfg->role != ESP_MEDIA_ROLE_SRC && cfg->role != ESP_MEDIA_ROLE_SINK) {
        return ESP_ERR_INVALID_ARG;
    }
#ifndef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (cfg->role == ESP_MEDIA_ROLE_SRC) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifndef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (cfg->role == ESP_MEDIA_ROLE_SINK) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */

    *out = NULL;
    esp_media_dummy_service_t *svc = calloc(1, sizeof(*svc));
    if (svc == NULL) {
        return ESP_ERR_NO_MEM;
    }
    svc->role = cfg->role;
    svc->max_stream_num = cfg->max_stream_num;

    const char *default_name = (cfg->role == ESP_MEDIA_ROLE_SRC) ? ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SRC_NAME : ESP_MEDIA_DUMMY_SERVICE_DEFAULT_SINK_NAME;
    const char *name = cfg->name ? cfg->name : default_name;
    svc->name_storage = strdup(name);
    if (svc->name_storage == NULL) {
        free(svc);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (cfg->role == ESP_MEDIA_ROLE_SRC) {
        ret = media_dummy_src_init(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (cfg->role == ESP_MEDIA_ROLE_SINK) {
        ret = media_dummy_sink_init(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
    if (ret != ESP_OK) {
        free(svc->name_storage);
        free(svc);
        return ret;
    }

    esp_media_service_config_t media_cfg = ESP_MEDIA_SERVICE_CONFIG_DEFAULT();
    media_cfg.name = svc->name_storage;
    media_cfg.service_ops = &s_service_ops;
    if (cfg->role == ESP_MEDIA_ROLE_SRC) {
        media_cfg.media_ops = &s_src_media_ops;
    } else {
        media_cfg.media_ops = &s_sink_media_ops;
    }
    ret = esp_media_service_init(&svc->media, &media_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "media init failed: %s", esp_err_to_name(ret));
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
        if (cfg->role == ESP_MEDIA_ROLE_SRC) {
            media_dummy_src_deinit(svc);
        }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
        if (cfg->role == ESP_MEDIA_ROLE_SINK) {
            media_dummy_sink_deinit(svc);
        }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
        free(svc->name_storage);
        free(svc);
        return ret;
    }
    *out = svc;
    return ESP_OK;
}

esp_err_t esp_media_dummy_service_destroy(esp_media_dummy_service_t *svc)
{
    if (svc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)esp_service_stop(ESP_SERVICE_BASE(&svc->media));
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SRC) {
        media_dummy_src_deinit(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (svc->role == ESP_MEDIA_ROLE_SINK) {
        media_dummy_sink_deinit(svc);
    }
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
    esp_err_t ret = esp_media_service_deinit(ESP_SERVICE_BASE(&svc->media));
    free(svc->name_storage);
    free(svc);
    return ret;
}

esp_err_t esp_media_dummy_service_add_track(esp_media_dummy_service_t *svc,
                                            esp_media_stream_id_t stream,
                                            const esp_media_track_info_t *info)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc == NULL || svc->role != ESP_MEDIA_ROLE_SRC) {
        return ESP_ERR_INVALID_ARG;
    }
    return media_dummy_src_add_track(svc, stream, info);
#else
    (void)svc;
    (void)stream;
    (void)info;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
}

esp_err_t esp_media_dummy_service_reset_tracks(esp_media_dummy_service_t *svc,
                                               esp_media_stream_id_t stream)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc == NULL || svc->role != ESP_MEDIA_ROLE_SRC) {
        return ESP_ERR_INVALID_ARG;
    }
    return media_dummy_src_reset_tracks(svc, stream);
#else
    (void)svc;
    (void)stream;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
}

esp_err_t esp_media_dummy_service_sync_record(esp_media_dummy_service_t *svc,
                                              esp_media_stream_id_t stream)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT
    if (svc == NULL || svc->role != ESP_MEDIA_ROLE_SRC) {
        return ESP_ERR_INVALID_ARG;
    }
    return media_dummy_src_sync_record(svc, stream);
#else
    (void)svc;
    (void)stream;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT */
}

esp_err_t esp_media_dummy_service_get_stats(esp_media_dummy_service_t *svc,
                                            esp_media_stream_id_t stream,
                                            esp_media_dummy_stream_stats_t *stats)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (svc == NULL || svc->role != ESP_MEDIA_ROLE_SINK) {
        return ESP_ERR_INVALID_ARG;
    }
    return media_dummy_sink_get_stats(svc, stream, stats);
#else
    (void)svc;
    (void)stream;
    (void)stats;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
}

esp_err_t esp_media_dummy_service_reset_stats(esp_media_dummy_service_t *svc)
{
#ifdef CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT
    if (svc == NULL || svc->role != ESP_MEDIA_ROLE_SINK) {
        return ESP_ERR_INVALID_ARG;
    }
    media_dummy_sink_reset_stats(svc);
    return ESP_OK;
#else
    (void)svc;
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT */
}
