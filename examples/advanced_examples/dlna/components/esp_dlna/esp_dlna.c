/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_dlna.h"
#include "esp_dlna_avt.h"
#include "esp_dlna_cmr.h"
#include "esp_dlna_rcs.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "audio_mem.h"

typedef struct esp_dlna {
    EventGroupHandle_t      state_event;
    upnp_handle_t           upnp;
    renderer_request_t      renderer_req;
    void                    *user_ctx;
} esp_dlna_t;

static const char *TAG = "ESP_DLNA";

esp_dlna_handle_t esp_dlna_start(dlna_config_t *config)
{
    esp_dlna_handle_t dlna = audio_calloc(1, sizeof(esp_dlna_t));
    if (dlna == NULL) {
        return NULL;
    }

    upnp_config_t upnp_config = {
        .httpd              = config->httpd,
        .port               = config->httpd_port,
        .uuid               = config->uuid,
        .serial             = config->uuid,
        .friendly_name      = config->friendly_name,
        .root_path          = config->root_path,
        .device_list        = config->device_list,
        .device_type        = "MediaRenderer",
        .version            = "1",
        .manufacturer       = "ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD",
        .manufacturer_url   = "https://espressif.com",
        .model_description  = "Media Renderer Device",
        .model_name         = "AV Renderer",
        .model_number       = "ESP32_Audio_Board",
        .model_url          = "https://www.espressif.com/en/products/socs",
        .logo               = {
            .mime_type  = config->logo.mime_type,
            .path       = config->logo.path,
            .data       = config->logo.data,
            .size       = config->logo.size,
        },
        .user_ctx = dlna, // VERY IMPORTANT FOR CALLBACK
    };
    dlna->upnp = esp_upnp_init(&upnp_config);
    if (dlna->upnp == NULL) {
        ESP_LOGE(TAG, "error create UPnP client");
        return NULL;
    }
    dlna->renderer_req = (renderer_request_t)config->renderer_req;
    dlna->user_ctx = config->user_ctx;

    ESP_LOGD(TAG, "DLNA starting up...at port %d", config->httpd_port);

    esp_dlna_cmr_register_service(dlna->upnp);
    esp_dlna_avt_register_service(dlna->upnp);
    esp_dlna_rcs_register_service(dlna->upnp);

    return dlna;
}

int esp_dlna_upnp_attr_cb(void *user_ctx, const upnp_attr_t *attr, int attr_num, char *buffer, int max_buffer_len)
{
    esp_dlna_handle_t dlna = (esp_dlna_handle_t)user_ctx;
    if (dlna == NULL) {
        return -1;
    }
    if (dlna->renderer_req == NULL) {
        return 0;
    }
    return dlna->renderer_req(dlna, attr, attr_num, buffer, max_buffer_len);
}

esp_err_t esp_dlna_notify(esp_dlna_handle_t dlna, const char *service_name)
{
    if (dlna == NULL || service_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_upnp_send_notify(dlna->upnp, service_name);
}

esp_err_t esp_dlna_notify_avt_by_action(esp_dlna_handle_t dlna, const char *action_name)
{
    if (dlna == NULL || action_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_upnp_send_avt_notify(dlna->upnp, action_name);
}

void *esp_dlna_get_user_ctx(esp_dlna_handle_t dlna)
{
    if (dlna) {
        return dlna->user_ctx;
    }
    return NULL;
}

esp_err_t esp_dlna_destroy(esp_dlna_handle_t dlna)
{
    esp_err_t err;

    if (dlna == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((err = esp_upnp_destroy(dlna->upnp)) != ESP_OK) {
        return err;
    }
    free(dlna);

    return ESP_OK;
}
