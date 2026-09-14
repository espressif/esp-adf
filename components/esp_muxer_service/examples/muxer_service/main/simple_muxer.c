/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_muxer_service.h"
#include "esp_muxer_service_ops.h"
#include "esp_service.h"

#include "settings.h"
#include "simple_muxer.h"

static const char *TAG = "SIMPLE_MUXER";

static bool muxer_mode_has_streaming(esp_muxer_service_mode_t mode)
{
    return mode == ESP_MUXER_SERVICE_MODE_STREAMING_ONLY ||
           mode == ESP_MUXER_SERVICE_MODE_BOTH;
}

static bool muxer_mode_has_storage(esp_muxer_service_mode_t mode)
{
    return mode == ESP_MUXER_SERVICE_MODE_STORAGE_ONLY ||
           mode == ESP_MUXER_SERVICE_MODE_BOTH;
}

static const char *muxer_mode_name(esp_muxer_service_mode_t mode)
{
    switch (mode) {
        case ESP_MUXER_SERVICE_MODE_STORAGE_ONLY:
            return "storage";
        case ESP_MUXER_SERVICE_MODE_STREAMING_ONLY:
            return "streaming";
        case ESP_MUXER_SERVICE_MODE_BOTH:
            return "both";
        default:
            return "unknown";
    }
}

static void muxer_delete(esp_muxer_service_t *muxer)
{
    if (muxer == NULL) {
        return;
    }
    (void)esp_media_service_deinit(ESP_SERVICE_BASE(muxer));
    free(muxer);
}

static esp_err_t add_dummy_audio(esp_media_dummy_service_t *src)
{
    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    return esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &audio);
}

static esp_err_t add_dummy_video(esp_media_dummy_service_t *src)
{
    esp_media_track_info_t video = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_H264,
    };
    return esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &video);
}

static esp_err_t drain_streaming(esp_muxer_service_t *muxer, uint32_t duration_ms,
                                 uint32_t *pkt_count, uint64_t *byte_count)
{
    static uint8_t buffer[4096];
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    *pkt_count = 0;
    *byte_count = 0;

    while ((xTaskGetTickCount() - start) < duration_ticks) {
        size_t size = sizeof(buffer);
        esp_err_t ret = esp_muxer_service_read_streaming_data(muxer, buffer, &size,
                                                              MUXER_EXAMPLE_STREAM_TIMEOUT);
        if (ret == ESP_ERR_TIMEOUT || ret == ESP_ERR_INVALID_STATE) {
            continue;
        }
        if (ret != ESP_OK) {
            return ret;
        }
        (*pkt_count)++;
        *byte_count += size;
    }
    return ESP_OK;
}

static esp_err_t run_linked_muxer(bool with_video, uint32_t duration_ms, esp_muxer_service_mode_t mode)
{
    /* 1. Dummy source that generates encoded pattern frames. */
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    src_cfg.name = "muxer_dummy_src";
    esp_media_dummy_service_t *src = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&src_cfg, &src);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = add_dummy_audio(src);
    if (ret == ESP_OK && with_video) {
        ret = add_dummy_video(src);
    }

    /* 2. Muxer sink. Storage writes under /sdcard/muxed (board-manager SD). */
    esp_muxer_service_t *muxer = NULL;
    if (ret == ESP_OK) {
        esp_muxer_service_cfg_t muxer_cfg = ESP_MUXER_SERVICE_CFG_DEFAULT();
        ret = esp_muxer_service_create(&muxer_cfg, &muxer);
    }
    if (ret == ESP_OK) {
        esp_muxer_service_setup_t setup = ESP_MUXER_SERVICE_SETUP_DEFAULT();
        setup.muxer_type = MUXER_EXAMPLE_MUXER_TYPE;
        setup.mode = mode;
        setup.ram_cache_size = MUXER_EXAMPLE_RAM_CACHE;
        if (muxer_mode_has_storage(mode)) {
            setup.storage_dir = MUXER_EXAMPLE_FAKE_STORAGE_DIR;
        }
        ret = esp_muxer_service_setup(muxer, &setup);
    }

    /* 3. Link dummy SRC -> muxer, start muxer first, then source. */
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(muxer), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(muxer));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(src));
    }

    uint32_t pkt_count = 0;
    uint64_t byte_count = 0;
    if (ret == ESP_OK) {
        if (muxer_mode_has_streaming(mode)) {
            ret = drain_streaming(muxer, duration_ms, &pkt_count, &byte_count);
        } else {
            vTaskDelay(pdMS_TO_TICKS(duration_ms));
        }
    }

    if (src != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(src));
    }
    if (muxer != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(muxer));
    }
    if (src != NULL && muxer != NULL) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(muxer), ESP_MEDIA_DEFAULT_STREAM);
    }
    ESP_LOGI(TAG, "%s %s: %" PRIu32 " packets, %" PRIu64 " bytes",
             with_video ? "av" : "audio", muxer_mode_name(mode), pkt_count, byte_count);

    muxer_delete(muxer);
    if (src != NULL) {
        (void)esp_media_dummy_service_destroy(src);
    }
    return ret;
}

esp_err_t simple_muxer_audio(uint32_t duration_ms, esp_muxer_service_mode_t mode)
{
    return run_linked_muxer(false, duration_ms, mode);
}

esp_err_t simple_muxer_av(uint32_t duration_ms, esp_muxer_service_mode_t mode)
{
    return run_linked_muxer(true, duration_ms, mode);
}
