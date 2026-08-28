/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_rtmp_service_priv.h"

static int server_new_connect_cb(void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    return rtmp_service_publish_event(service, ESP_RTMP_SERVICE_EVENT_SERVER_CLIENT_CONNECTED, NULL) == ESP_OK ? 0 : -1;
}

static int server_puller_active_cb(char *stream_name, bool active, void *ctx)
{
    esp_rtmp_service_t *service = (esp_rtmp_service_t *)ctx;
    esp_rtmp_service_event_t event_id = active ? ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STARTED :
                                                ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STOPPED;
    return rtmp_service_publish_event(service, event_id, stream_name) == ESP_OK ? 0 : -1;
}

esp_err_t rtmp_server_on_start(esp_rtmp_service_t *service)
{
    media_lib_thread_cfg_t thread_cfg = {0};
    rtmp_get_media_thread_cfg(service, ESP_RTMP_SERVICE_SERVER_TASK_NAME, &thread_cfg);
    rtmp_server_cfg_t server_cfg = {
        .chunk_size = service->chunk_size == 0 ? RTMP_DEFAULT_CHUNK_SIZE : service->chunk_size,
        .app_name = service->server.app_name ? service->server.app_name : ESP_RTMP_SERVICE_DEFAULT_APP_NAME,
        .port = service->server.port == 0 ? ESP_RTMP_SERVICE_DEFAULT_PORT : service->server.port,
        .max_clients = service->server.max_clients,
        .thread_cfg = thread_cfg,
        .ssl_cfg = rtmp_url_is_secure(service->url) ? &service->ssl_cfg.server : NULL,
        .ctx = service,
    };
    service->server.handle = esp_rtmp_server_open(&server_cfg);
    if (service->server.handle == NULL) {
        return ESP_FAIL;
    }
    esp_media_err_t media_ret = esp_rtmp_server_monitor_connect_in(service->server.handle, server_new_connect_cb);
    if (media_ret == ESP_MEDIA_ERR_OK) {
        media_ret = esp_rtmp_server_monitor_puller(service->server.handle, server_puller_active_cb);
    }
    if (media_ret != ESP_MEDIA_ERR_OK) {
        esp_rtmp_server_close(service->server.handle);
        service->server.handle = NULL;
        return rtmp_media_err_to_esp(media_ret);
    }
    esp_err_t ret = rtmp_media_err_to_esp(esp_rtmp_server_setup(service->server.handle));
    if (ret != ESP_OK) {
        esp_rtmp_server_close(service->server.handle);
        service->server.handle = NULL;
    }
    return ret;
}

esp_err_t rtmp_server_query(esp_rtmp_service_t *service)
{
    if (service->server.handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_rtmp_server_query(service->server.handle);
}

esp_err_t rtmp_server_on_stop(esp_rtmp_service_t *service)
{
    if (service->server.handle != NULL) {
        esp_rtmp_server_close(service->server.handle);
        service->server.handle = NULL;
    }
    return ESP_OK;
}
