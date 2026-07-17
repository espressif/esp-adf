/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "esp_media_service_types.h"
#include "esp_media_provider.h"
#include "media_service_err.h"

static const char *TAG = "MEDIA_PROVIDER";

esp_err_t esp_media_provider_get_track_num(const esp_media_provider_t *provider, uint16_t *out_num)
{
    if (provider == NULL || provider->ops == NULL || out_num == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p out_num:%p",
                provider, out_num);
    }
    if (provider->ops->get_track_num == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider get track num op not supported");
    }
    RET_CHK(provider->ops->get_track_num(provider->ctx, out_num),
            "Failed to get provider track num");
    return ESP_OK;
}

esp_err_t esp_media_provider_get_track_info(const esp_media_provider_t *provider, uint16_t index,
                                            esp_media_track_info_t *out_info)
{
    if (provider == NULL || provider->ops == NULL || out_info == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p out_info:%p",
                provider, out_info);
    }
    if (provider->ops->get_track_info == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op found get track info");
    }
    RET_CHK(provider->ops->get_track_info(provider->ctx, index, out_info),
            "Failed to get provider track info index:%u", index);
    return ESP_OK;
}

esp_err_t esp_media_provider_set_event_cb(const esp_media_provider_t *provider, esp_media_provider_event_cb_t cb,
                                          void *event_ctx)
{
    if (provider == NULL || provider->ops == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p", provider);
    }
    if (provider->ops->set_event_cb == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op for set event cb");
    }
    RET_CHK(provider->ops->set_event_cb(provider->ctx, cb, event_ctx),
            "Failed to set provider event cb");
    return ESP_OK;
}

esp_err_t esp_media_provider_abort(const esp_media_provider_t *provider)
{
    if (provider == NULL || provider->ops == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p", provider);
    }
    if (provider->ops->abort == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op for read abort");
    }
    RET_CHK(provider->ops->abort(provider->ctx), "Failed to abort provider");
    return ESP_OK;
}

esp_err_t esp_media_provider_acquire_frame(const esp_media_provider_t *provider, esp_media_frame_t *out_frame,
                                           uint32_t timeout_ms)
{
    if (provider == NULL || provider->ops == NULL || out_frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p out_frame:%p",
                provider, out_frame);
    }
    if (provider->ops->acquire_frame == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op for acquire frame");
    }
    esp_err_t ret = provider->ops->acquire_frame(provider->ctx, out_frame, timeout_ms);
    if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
        RET_FOR(ret, "Failed to acquire frame ret:%d", ret);
    }
    return ret;
}

esp_err_t esp_media_provider_read_frame(const esp_media_provider_t *provider, esp_media_frame_t *out_frame,
                                        uint32_t timeout_ms)
{
    if (provider == NULL || provider->ops == NULL || out_frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p out_frame:%p",
                provider, out_frame);
    }
    if (provider->ops->read_frame == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op for read frame");
    }
    RET_CHK(provider->ops->read_frame(provider->ctx, out_frame, timeout_ms),
            "Failed to read provider frame timeout:%u", timeout_ms);
    return ESP_OK;
}

esp_err_t esp_media_provider_release_frame(const esp_media_provider_t *provider, esp_media_frame_t *frame)
{
    if (provider == NULL || provider->ops == NULL || frame == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid args provider:%p frame:%p",
                provider, frame);
    }
    if (provider->ops->release_frame == NULL) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Provider no op for release frame");
    }
    RET_CHK(provider->ops->release_frame(provider->ctx, frame),
            "Failed to release provider frame");
    return ESP_OK;
}
