/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_log.h"
#include "esp_service_mcp_trans.h"

static const char *TAG = "MCP_TRANSPORT";

esp_err_t esp_service_mcp_trans_init(esp_service_mcp_trans_t *transport, esp_service_mcp_trans_type_t type,
                                     const char *tag, const esp_service_mcp_trans_ops_t *ops)
{
    if (!transport || !ops || !tag) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!ops->start || !ops->stop || !ops->destroy) {
        ESP_LOGE(TAG, "Transport ops must provide start/stop/destroy");
        return ESP_ERR_INVALID_ARG;
    }

    memset(transport, 0, sizeof(esp_service_mcp_trans_t));
    transport->type = type;
    transport->tag = tag;
    transport->ops = ops;
    transport->on_request = NULL;
    transport->user_data = NULL;

    return ESP_OK;
}

esp_err_t esp_service_mcp_trans_set_request_handler(esp_service_mcp_trans_t *transport,
                                                    esp_service_mcp_trans_on_request_t handler, void *user_data)
{
    if (!transport) {
        return ESP_ERR_INVALID_ARG;
    }

    transport->on_request = handler;
    transport->user_data = user_data;

    ESP_LOGD(TAG, "[%s] Request handler set", transport->tag);
    return ESP_OK;
}

esp_err_t esp_service_mcp_trans_start(esp_service_mcp_trans_t *transport)
{
    if (!transport || !transport->ops) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!transport->on_request) {
        ESP_LOGW(TAG, "[%s] Starting without a request handler", transport->tag);
    }

    ESP_LOGI(TAG, "[%s] Starting transport", transport->tag);
    return transport->ops->start(transport);
}

esp_err_t esp_service_mcp_trans_stop(esp_service_mcp_trans_t *transport)
{
    if (!transport || !transport->ops) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "[%s] Stopping transport", transport->tag);
    return transport->ops->stop(transport);
}

esp_err_t esp_service_mcp_trans_destroy(esp_service_mcp_trans_t *transport)
{
    if (!transport || !transport->ops) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "[%s] Destroying transport", transport->tag);
    return transport->ops->destroy(transport);
}

esp_err_t esp_service_mcp_trans_broadcast(esp_service_mcp_trans_t *transport, const char *data, size_t len)
{
    if (!transport || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!transport->ops || !transport->ops->broadcast) {
        ESP_LOGD(TAG, "[%s] Broadcast not supported", transport->tag);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return transport->ops->broadcast(transport, data, len);
}

esp_service_mcp_trans_type_t esp_service_mcp_trans_get_type(esp_service_mcp_trans_t *transport)
{
    if (!transport) {
        return ESP_SERVICE_MCP_TRANS_CUSTOM;
    }
    return transport->type;
}

esp_err_t esp_service_mcp_trans_dispatch_request(esp_service_mcp_trans_t *transport, const char *request, char **response)
{
    if (!transport || !request || !response) {
        return ESP_ERR_INVALID_ARG;
    }

    *response = NULL;

    if (!transport->on_request) {
        ESP_LOGE(TAG, "[%s] No request handler registered", transport->tag);
        return ESP_ERR_INVALID_STATE;
    }

    return transport->on_request(request, response, transport->user_data);
}
