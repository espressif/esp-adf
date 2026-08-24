/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "dev_audio_codec.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_capture_defaults.h"

#include "capture_service_err.h"
#include "internal/esp_audio_capture_service_priv.h"
#include "esp_audio_capture_service_setup.h"
#include "esp_audio_capture_scheduler.h"
#include "esp_service.h"
#include "esp_codec_dev.h"
#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED
#include "ai_audio/esp_ai_audio_src.h"
#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED */

static const char *TAG = "AUDIO_CAPTURE";
static esp_audio_capture_service_ctx_t *s_ctx_list;

static char label_to_afe_channel(const char *label)
{
    if (label == NULL || label[0] == '\0') {
        return 'N';
    }
    if (strncmp(label, "RE", 2) == 0) {
        return 'R';
    }
    if (strncmp(label, "NA", 2) == 0) {
        return 'N';
    }
    /* FC/FL/FR/SL/SR/BL/BR and other mic positions map to 'M'. */
    return 'M';
}

static int get_channel_from_label(const char *label)
{
    int n = 0;
    const char *p = label;
    if (label[0] == '\0') {
        return n;
    }
    n = 1;
    while (*p) {
        if (*p == ',' && *(p + 1) != '\0') {
            n++;
        }
        p++;
    }
    return n;
}

static void discover_mic_layout(esp_audio_capture_service_ctx_t *rec)
{
    char label[20];
    label[0] = 0;
    const char *dev_name = rec->cfg.dev_name ? rec->cfg.dev_name : ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    dev_audio_codec_config_t *adc_cfg = NULL;
    if (esp_board_manager_get_device_config(dev_name, (void **)&adc_cfg) != ESP_OK ||
        adc_cfg == NULL ||
        adc_cfg->codec_adc_cfg.label == NULL) {
        strlcpy(rec->mic_layout, "MR", sizeof(rec->mic_layout));
        return;
    }
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = get_channel_from_label(adc_cfg->codec_adc_cfg.label),
        .sample_rate = 16000,
    };
    int ret = esp_codec_dev_open(rec->record_handle, &fs);
    if (ret == ESP_OK) {
        esp_codec_dev_get_data_layout_label(rec->record_handle, label, sizeof(label));
        esp_codec_dev_close(rec->record_handle);
    }
    // Fallback
    if (label[0] == 0) {
        const char *ch_start[4];
        int ch_num = 1;
        ch_start[0] = adc_cfg->codec_adc_cfg.label;
        const char *p = ch_start[0];
        while (*p) {
            if (*p == ',' && *(p + 1) != '\0') {
                if (ch_num >= 4) {
                    break;
                }
                ch_start[ch_num++] = p + 1;
            }
            p++;
        }
        if (ch_num == 4) {
            // 1,3,2,4
            int n = 0;
            uint8_t order[4] = {0, 2, 1, 3};
            for (int i = 0; i < ch_num && n + 1 < (int)sizeof(label); i++) {
                const char *ch_label = ch_start[order[i]];
                while (*ch_label != ' ' && *ch_label != ',' && *ch_label != '\0' &&
                       n + 1 < (int)sizeof(label)) {
                    label[n++] = *ch_label++;
                }
                label[n++] = i < ch_num - 1 ? ',' : '\0';
            }
        }
    }

    int total_channel = 0;
    const char *p = label;
    while (*p != '\0' && total_channel < sizeof(rec->mic_layout)) {
         while (*p != '\0' && total_channel + 1 < sizeof(rec->mic_layout)) {
            while (*p == ' ' || *p == ',') {
                p++;
            }
            if (*p == '\0') {
                break;
            }
            char token[4] = {0};
            size_t n = 0;
            while (*p != '\0' && *p != ',' && *p != ' ' && n + 1 < sizeof(token)) {
                token[n++] = *p++;
            }
            rec->mic_layout[total_channel++] = label_to_afe_channel(token);
        }
    }
    ESP_LOGI(TAG, "mic_layout: %s ch: %d", rec->mic_layout, total_channel);
}

static esp_err_t init_record_handle(esp_audio_capture_service_ctx_t *rec)
{
    dev_audio_codec_handles_t *codec_handle = NULL;
    const char *dev_name = rec->cfg.dev_name ? rec->cfg.dev_name : ESP_BOARD_DEVICE_NAME_AUDIO_ADC;
    esp_err_t ret = esp_board_manager_get_device_handle(dev_name, (void **)&codec_handle);
    if (ret != ESP_OK || codec_handle == NULL || codec_handle->codec_dev == NULL) {
        /* Normalize board-manager missing-device codes to ESP_ERR_NOT_FOUND. */
        return ESP_ERR_NOT_FOUND;
    }
    rec->record_handle = codec_handle->codec_dev;
    discover_mic_layout(rec);
    return ESP_OK;
}

static esp_err_t create_sources(esp_audio_capture_service_ctx_t *rec)
{
    esp_err_t ret = init_record_handle(rec);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_capture_audio_dev_src_cfg_t codec_cfg = {
        .record_handle = rec->record_handle,
    };
    rec->codec_src = esp_capture_new_audio_dev_src(&codec_cfg);
    if (rec->codec_src == NULL) {
        return ESP_ERR_NO_MEM;
    }
#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED
    const char *service_name = rec->cfg.service_name;
    if (service_name == NULL && rec->capture != NULL) {
        (void)esp_service_get_name(ESP_SERVICE_BASE(rec->capture), &service_name);
    }
    if (service_name == NULL) {
        service_name = ESP_AUDIO_CAPTURE_SERVICE_NAME;
    }
    esp_ai_audio_src_cfg_t ai_cfg = {
        .record_handle = rec->record_handle,
        .mic_layout = rec->mic_layout[0] ? rec->mic_layout : NULL,
        .pool = rec->cfg.pool,
        .service_name = service_name,
    };
    rec->ai_aud_src = esp_ai_audio_new_src(&ai_cfg);
    if (rec->ai_aud_src == NULL) {
        return ESP_ERR_NO_MEM;
    }
#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED */
    return ESP_OK;
}

static void register_ctx(esp_audio_capture_service_ctx_t *rec)
{
    rec->next = s_ctx_list;
    s_ctx_list = rec;
}

static void unregister_ctx(esp_audio_capture_service_ctx_t *rec)
{
    esp_audio_capture_service_ctx_t **cur = &s_ctx_list;
    while (*cur != NULL) {
        if (*cur == rec) {
            *cur = rec->next;
            rec->next = NULL;
            return;
        }
        cur = &(*cur)->next;
    }
}

static esp_audio_capture_service_ctx_t *find_ctx(esp_capture_service_t *capture)
{
    for (esp_audio_capture_service_ctx_t *cur = s_ctx_list; cur != NULL; cur = cur->next) {
        if (cur->capture == capture) {
            return cur;
        }
    }
    return NULL;
}

static esp_err_t create_ctx(esp_capture_service_t *capture,
                            const esp_audio_capture_service_cfg_t *cfg,
                            esp_audio_capture_service_ctx_t **out_ctx)
{
    if (out_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_capture_service_ctx_t *rec = calloc(1, sizeof(*rec));
    if (rec == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (cfg != NULL) {
        rec->cfg = *cfg;
    }
    rec->capture = capture;

    esp_err_t ret = create_sources(rec);
    if (ret != ESP_OK) {
        free(rec->codec_src);
        free(rec->ai_aud_src);
        free(rec);
        return ret;
    }
    *out_ctx = rec;
    return ESP_OK;
}

static void destroy_ctx(esp_audio_capture_service_ctx_t *rec)
{
    if (rec == NULL) {
        return;
    }
    unregister_ctx(rec);
    free(rec->codec_src);
    /* Capture owns open/close; free the src object only. */
    free(rec->ai_aud_src);
    free(rec);
}

esp_audio_capture_service_ctx_t *esp_audio_capture_service_ctx_find(esp_capture_service_t *capture)
{
    return find_ctx(capture);
}

esp_err_t esp_audio_capture_service_attach(esp_capture_service_t *capture,
                                           const esp_audio_capture_service_cfg_t *cfg,
                                           esp_capture_audio_src_if_t **out_src)
{
    if (capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid capture");
    }
    if (find_ctx(capture) != NULL) {
        RET_FOR(ESP_ERR_INVALID_STATE, "Already attached");
    }

    esp_audio_capture_service_ctx_t *rec = NULL;
    esp_err_t ret = create_ctx(capture, cfg, &rec);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Create context failed");
    }
    register_ctx(rec);
    if (out_src != NULL) {
        *out_src = esp_audio_capture_service_select_src(capture);
    }
    return ESP_OK;
}

esp_err_t esp_audio_capture_service_detach(esp_capture_service_t *capture)
{
    if (capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid capture");
    }
    esp_audio_capture_service_ctx_t *rec = find_ctx(capture);
    if (rec == NULL) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Context not found");
    }
    destroy_ctx(rec);
    return ESP_OK;
}

esp_capture_audio_src_if_t *esp_audio_capture_service_select_src(esp_capture_service_t *capture)
{
    esp_audio_capture_service_ctx_t *rec = find_ctx(capture);
    if (rec == NULL) {
        return NULL;
    }
    if (rec->ai_features != 0) {
        return rec->ai_aud_src;
    }
    return rec->codec_src;
}

static esp_err_t audio_capture_service_deinit_cb(esp_capture_service_t *capture, void *user_data)
{
    (void)user_data;
    return esp_audio_capture_service_detach(capture);
}

esp_err_t esp_audio_capture_service_create(const esp_audio_capture_service_cfg_t *cfg,
                                           esp_capture_service_t **out_capture)
{
    if (out_capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid output handle");
    }
    esp_capture_service_cfg_t capture_cfg = {
        .name = (cfg != NULL && cfg->service_name != NULL) ? cfg->service_name : ESP_AUDIO_CAPTURE_SERVICE_NAME,
        .max_stream_num = (cfg != NULL && cfg->max_stream_num) ? cfg->max_stream_num : 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_capture_service_create(&capture_cfg, &capture);
    if (ret != ESP_OK) {
        RET_FOR(ret, "Create capture failed");
    }
    ret = esp_audio_capture_service_attach(capture, cfg, NULL);
    if (ret != ESP_OK) {
        esp_capture_service_destroy(capture);
        RET_FOR(ret, "Attach capture failed");
    }
    ret = esp_capture_service_set_deinit_cb(capture, audio_capture_service_deinit_cb, NULL);
    if (ret != ESP_OK) {
        esp_audio_capture_service_detach(capture);
        esp_capture_service_destroy(capture);
        RET_FOR(ret, "Set deinit callback failed");
    }
    *out_capture = capture;
    return ESP_OK;
}
