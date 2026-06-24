/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_media_service.h"
#include "media_service_err.h"

static const char *TAG = "MEDIA_SERVICE";

esp_err_t esp_media_service_init(esp_media_service_t *service, const esp_media_service_config_t *config)
{
    if (service == NULL || config == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p config:%p",
                service, config);
    }

    memset(service, 0, sizeof(*service));
    service->media_ops = config->media_ops;

    esp_service_config_t base_cfg = {
        .name = config->name,
        .user_data = config->user_data,
    };
    RET_CHK(esp_service_init(&service->base, &base_cfg, config->service_ops),
            "Failed to init base service");
    return ESP_OK;
}

esp_err_t esp_media_service_deinit(esp_service_t *service)
{
    if (service == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p", service);
    }
    RET_CHK(esp_service_deinit(service), "Failed to deinit base service");
    return ESP_OK;
}

esp_err_t esp_media_service_get_role(esp_service_t *service, esp_media_role_t *out_role)
{
    if (service == NULL || out_role == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p out_role:%p",
                service, out_role);
    }
    esp_media_service_t *media_service = (esp_media_service_t *)service;
    if (media_service->media_ops == NULL || media_service->media_ops->get_role == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Get role op not supported inst");
    }
    RET_CHK(media_service->media_ops->get_role(service, out_role),
            "Failed to get role");
    return ESP_OK;
}

esp_err_t esp_media_service_get_provider(esp_service_t *service, esp_media_stream_id_t stream, esp_media_provider_t *out_provider)
{
    if (service == NULL || out_provider == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p out_provider:%p",
                service, out_provider);
    }
    esp_media_service_t *media_service = (esp_media_service_t *)service;
    if (media_service->media_ops == NULL || media_service->media_ops->get_provider == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Get provider op not supported stream:%u", stream);
    }
    RET_CHK(media_service->media_ops->get_provider(service, stream, out_provider),
            "Failed to get provider stream:%u", stream);
    return ESP_OK;
}

esp_err_t esp_media_service_set_provider(esp_service_t *service, esp_media_stream_id_t stream, const esp_media_provider_t *provider)
{
    if (service == NULL || (provider != NULL && provider->ops == NULL)) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p provider:%p",
                service, provider);
    }
    esp_media_service_t *media_service = (esp_media_service_t *)service;
    if (media_service->media_ops == NULL || media_service->media_ops->set_provider == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Set provider op not supported stream:%u", stream);
    }
    RET_CHK(media_service->media_ops->set_provider(service, stream, provider),
            "Failed to set provider stream:%u", stream);
    return ESP_OK;
}

esp_err_t esp_media_service_get_request(esp_service_t *service, esp_media_stream_id_t stream,
                                        esp_media_service_request_t *out_request)
{
    if (service == NULL || out_request == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p out_request:%p",
                service, out_request);
    }
    memset(out_request, 0, sizeof(*out_request));
    esp_media_service_t *media_service = (esp_media_service_t *)service;
    if (media_service->media_ops == NULL || media_service->media_ops->get_request == NULL) {
        // No error log
        return ESP_ERR_NOT_SUPPORTED;
    }
    RET_CHK(media_service->media_ops->get_request(service, stream, out_request),
            "Failed to get request stream:%u", stream);
    return ESP_OK;
}

esp_err_t esp_media_service_set_request(esp_service_t *service, esp_media_stream_id_t stream,
                                        const esp_media_service_request_t *request)
{
    if (service == NULL || request == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args service:%p request:%p",
                service, request);
    }
    esp_media_service_t *media_service = (esp_media_service_t *)service;
    if (media_service->media_ops == NULL || media_service->media_ops->set_request == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    RET_CHK(media_service->media_ops->set_request(service, stream, request),
            "Failed to set request stream:%u", stream);
    return ESP_OK;
}

esp_err_t esp_media_service_link(esp_service_t *src_service, esp_media_stream_id_t src_stream,
                                 esp_service_t *sink_service, esp_media_stream_id_t sink_stream)
{
    if (src_service == NULL || sink_service == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args src:%p sink:%p", src_service, sink_service);
    }
    esp_media_service_t *media_src = (esp_media_service_t *)src_service;
    esp_media_service_t *media_sink = (esp_media_service_t *)sink_service;
    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    esp_err_t ret = ESP_OK;
    if (media_src->media_ops != NULL && media_src->media_ops->get_role != NULL) {
        ret = media_src->media_ops->get_role(src_service, &role);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Failed to get source role stream:%u ret:%s",
                    src_stream, esp_err_to_name(ret));
        }
        if ((role & ESP_MEDIA_ROLE_SRC) == 0) {
            RET_FOR(ESP_ERR_NOT_SUPPORTED, "Source role not supported stream:%u role:%d",
                    src_stream, role);
        }
    }
    if (media_sink->media_ops != NULL && media_sink->media_ops->get_role != NULL) {
        role = ESP_MEDIA_ROLE_NONE;
        ret = media_sink->media_ops->get_role(sink_service, &role);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Failed to get sink role stream:%u ret:%s",
                    sink_stream, esp_err_to_name(ret));
        }
        if ((role & ESP_MEDIA_ROLE_SINK) == 0) {
            RET_FOR(ESP_ERR_NOT_SUPPORTED, "Sink role not supported stream:%u role:%d",
                    sink_stream, role);
        }
    }
    esp_media_service_request_t request = {0};
    esp_err_t request_ret = esp_media_service_get_request(sink_service, sink_stream, &request);
    if (request_ret == ESP_OK) {
        esp_err_t set_ret = esp_media_service_set_request(src_service, src_stream, &request);
        if (set_ret != ESP_OK && set_ret != ESP_ERR_NOT_SUPPORTED) {
            RET_FOR(set_ret, "Failed to apply sink request to source ret:%s",
                    esp_err_to_name(set_ret));
        }
    } else if (request_ret != ESP_ERR_NOT_SUPPORTED) {
        RET_FOR(request_ret, "Failed to get sink request ret:%s", esp_err_to_name(request_ret));
    }

    esp_media_provider_t provider = {0};
    ret = esp_media_service_get_provider(src_service, src_stream, &provider);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Failed to get source provider ret:%s", esp_err_to_name(ret));
    }
    RET_CHK(esp_media_service_set_provider(sink_service, sink_stream, &provider),
            "Failed to set provider to sink");
    return ESP_OK;
}

esp_err_t esp_media_service_unlink(esp_service_t *src_service, esp_media_stream_id_t src_stream,
                                   esp_service_t *sink_service, esp_media_stream_id_t sink_stream)
{
    if (src_service == NULL || sink_service == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args src:%p sink:%p",
                src_service, sink_service);
    }
    esp_media_service_t *media_src = (esp_media_service_t *)src_service;
    esp_media_service_t *media_sink = (esp_media_service_t *)sink_service;
    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    esp_err_t ret = ESP_OK;
    if (media_src->media_ops != NULL && media_src->media_ops->get_role != NULL) {
        ret = media_src->media_ops->get_role(src_service, &role);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Failed to get source role stream:%u ret:%s",
                    src_stream, esp_err_to_name(ret));
        }
        if ((role & ESP_MEDIA_ROLE_SRC) == 0) {
            RET_FOR(ESP_ERR_NOT_SUPPORTED, "Source role not supported stream:%u role:%d",
                    src_stream, role);
        }
    }
    if (media_sink->media_ops != NULL && media_sink->media_ops->get_role != NULL) {
        role = ESP_MEDIA_ROLE_NONE;
        ret = media_sink->media_ops->get_role(sink_service, &role);
        if (ret != ESP_OK) {
            RET_FOR(ret, "Failed to get sink role stream:%u ret:%s",
                    sink_stream, esp_err_to_name(ret));
        }
        if ((role & ESP_MEDIA_ROLE_SINK) == 0) {
            RET_FOR(ESP_ERR_NOT_SUPPORTED, "Sink role not supported stream:%u role:%d",
                    sink_stream, role);
        }
    }
    RET_CHK(esp_media_service_set_provider(sink_service, sink_stream, NULL),
            "Failed to clear sink provider");
    return ESP_OK;
}
