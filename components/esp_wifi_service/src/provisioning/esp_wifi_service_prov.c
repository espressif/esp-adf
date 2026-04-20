/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_check.h"
#include "esp_heap_caps.h"

#include "esp_wifi_service_prov.h"

static const char *TAG = "WIFI_PROV";

esp_err_t esp_wifi_service_prov_init(esp_wifi_service_prov_t prov, const esp_wifi_service_prov_config_t *config)
{
    ESP_RETURN_ON_FALSE(prov && config && config->name, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning base init failed: invalid arguments");
    ESP_RETURN_ON_FALSE(config->ops && config->ops->start && config->ops->stop && config->ops->send, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning base init failed: missing vtable entry");
    prov->name = config->name;
    prov->event_cb = config->event_cb;
    prov->event_ctx = config->event_ctx;
    prov->default_priority = config->default_priority;
    prov->ops = config->ops;
    return ESP_OK;
}

esp_err_t esp_wifi_service_prov_set_cb(esp_wifi_service_prov_t prov, esp_wifi_service_prov_event_cb_t cb, void *event_ctx)
{
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_INVALID_ARG, TAG, "Provisioning set callback failed: invalid arguments");
    prov->event_cb = cb;
    prov->event_ctx = event_ctx;
    return ESP_OK;
}

esp_err_t esp_wifi_service_prov_start(esp_wifi_service_prov_t prov)
{
    ESP_RETURN_ON_FALSE(prov && prov->ops && prov->ops->start, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning start failed: invalid arguments");
    return prov->ops->start(prov);
}

esp_err_t esp_wifi_service_prov_stop(esp_wifi_service_prov_t prov)
{
    ESP_RETURN_ON_FALSE(prov && prov->ops && prov->ops->stop, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning stop failed: invalid arguments");
    return prov->ops->stop(prov);
}

esp_err_t esp_wifi_service_prov_send(esp_wifi_service_prov_t prov, const uint8_t *data, uint32_t data_len)
{
    ESP_RETURN_ON_FALSE(prov && data && data_len > 0 && prov->ops && prov->ops->send, ESP_ERR_INVALID_ARG, TAG,
                        "Provisioning send failed: invalid arguments");
    return prov->ops->send(prov, data, data_len);
}

esp_err_t esp_wifi_service_prov_deinit(esp_wifi_service_prov_t prov)
{
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_INVALID_ARG, TAG, "Provisioning deinit failed: invalid arguments");
    heap_caps_free(prov);
    return ESP_OK;
}

esp_err_t esp_wifi_service_prov_dispatch_event(esp_wifi_service_prov_t prov,
                                               esp_wifi_service_prov_event_id_t event_id,
                                               const esp_wifi_service_prov_event_t *payload)
{
    ESP_RETURN_ON_FALSE(prov, ESP_ERR_INVALID_ARG, TAG, "Provisioning event dispatch failed: invalid arguments");
    if (!prov->event_cb) {
        return ESP_OK;
    }
    return prov->event_cb(event_id, payload, prov->event_ctx);
}

esp_err_t esp_wifi_service_prov_get_name(esp_wifi_service_prov_t prov, const char **name)
{
    ESP_RETURN_ON_FALSE(prov && name, ESP_ERR_INVALID_ARG, TAG, "Provisioning get name failed: invalid arguments");
    *name = prov->name;
    return ESP_OK;
}
