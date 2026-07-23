/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "capture_service_err.h"
#include "esp_audio_capture_ai_src.h"
#include "internal/esp_audio_capture_service_priv.h"

static const char *TAG = "AUDIO_CAPTURE_AI";

#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED

#include "ai_audio/esp_ai_audio_src.h"
#include "esp_service.h"



static bool audio_capture_service_is_running(esp_capture_service_t *capture)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    return capture != NULL &&
           esp_service_get_state(ESP_SERVICE_BASE(capture), &state) == ESP_OK &&
           state == ESP_SERVICE_STATE_RUNNING;
}

esp_err_t esp_capture_service_ai_audio_src_set_feature(esp_capture_service_t *capture,
                                                       uint32_t feature_mask,
                                                       const esp_capture_service_ai_audio_src_feature_cfg_t *feature_cfg)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_capture_service_ai_audio_src_feature_cfg_t local_cfg = {0};
    if (feature_cfg != NULL) {
        local_cfg = *feature_cfg;
    }
    if (local_cfg.afe.pool == NULL) {
        local_cfg.afe.pool = rec->cfg.pool;
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_feature(rec->ai_aud_src,
                                                                (esp_ai_audio_feature_t)feature_mask,
                                                                &local_cfg));
    if (ret == ESP_OK) {
        rec->ai_features = feature_mask;
        rec->ai_feature_cfg = local_cfg;
    }
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set AI feature failed");
    }
    return ret;
}

esp_err_t esp_capture_service_ai_audio_src_set_vad_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_vad_cb_t cb,
                                                      void *ctx)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_vad_cb(rec->ai_aud_src, cb, ctx));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set VAD callback failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_ai_audio_src_set_wn_cb(esp_capture_service_t *capture,
                                                     esp_capture_service_ai_audio_src_wn_cb_t cb,
                                                     void *ctx)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_wn_cb(rec->ai_aud_src, cb, ctx));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set wake word callback failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_ai_audio_src_set_doa_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_doa_cb_t cb,
                                                      void *ctx)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_doa_cb(rec->ai_aud_src, cb, ctx));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set DOA callback failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_ai_audio_src_set_read_cb(esp_capture_service_t *capture,
                                                       esp_capture_service_ai_audio_src_read_cb_t cb,
                                                       void *ctx)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_read_cb(rec->ai_aud_src, cb, ctx));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set read callback failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_ai_audio_src_enable_dump(esp_capture_service_t *capture,
                                                       const char *dir)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_enable_dump(rec->ai_aud_src, dir));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Enable dump failed");
    }
    return ESP_OK;
}

esp_err_t esp_capture_service_ai_audio_src_set_alc_gain(esp_capture_service_t *capture,
                                                        int channel,
                                                        float gain)
{
    if (audio_capture_service_is_running(capture)) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Capture is running");
    }
    esp_audio_capture_service_ctx_t *rec = esp_audio_capture_service_ctx_find(capture);
    if (rec == NULL || rec->ai_aud_src == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "AI source not found");
    }
    esp_err_t ret = capture_err_to_esp(esp_ai_audio_set_alc_gain(rec->ai_aud_src, channel, gain));
    if (ret != ESP_OK) {
        RET_FOR(ret, "Set ALC gain failed");
    }
    return ESP_OK;
}

#else  /* !CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED */

esp_err_t esp_capture_service_ai_audio_src_set_feature(esp_capture_service_t *capture,
                                                       uint32_t feature_mask,
                                                       const esp_capture_service_ai_audio_src_feature_cfg_t *feature_cfg)
{
    (void)capture;
    (void)feature_mask;
    (void)feature_cfg;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_set_vad_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_vad_cb_t cb,
                                                      void *ctx)
{
    (void)capture;
    (void)cb;
    (void)ctx;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_set_wn_cb(esp_capture_service_t *capture,
                                                     esp_capture_service_ai_audio_src_wn_cb_t cb,
                                                     void *ctx)
{
    (void)capture;
    (void)cb;
    (void)ctx;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_set_doa_cb(esp_capture_service_t *capture,
                                                      esp_capture_service_ai_audio_src_doa_cb_t cb,
                                                      void *ctx)
{
    (void)capture;
    (void)cb;
    (void)ctx;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_set_read_cb(esp_capture_service_t *capture,
                                                       esp_capture_service_ai_audio_src_read_cb_t cb,
                                                       void *ctx)
{
    (void)capture;
    (void)cb;
    (void)ctx;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_enable_dump(esp_capture_service_t *capture,
                                                       const char *dir)
{
    (void)capture;
    (void)dir;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

esp_err_t esp_capture_service_ai_audio_src_set_alc_gain(esp_capture_service_t *capture,
                                                        int channel,
                                                        float gain)
{
    (void)capture;
    (void)channel;
    (void)gain;
    RET_FOR(ESP_ERR_NOT_SUPPORTED, "AI source disabled");
}

#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED */
