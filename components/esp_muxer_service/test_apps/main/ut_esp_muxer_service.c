/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include <stdlib.h>

#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_muxer_service.h"
#include "esp_muxer_service_ops.h"
#include "esp_service.h"
#include "ts_muxer.h"

void esp_muxer_service_ut_force_link(void)  { }

TEST_CASE("muxer service create deinit is sink", "[esp_muxer_service]")
{
    esp_muxer_service_cfg_t cfg = ESP_MUXER_SERVICE_CFG_DEFAULT();
    esp_muxer_service_t *muxer = NULL;
    TEST_ESP_OK(esp_muxer_service_create(&cfg, &muxer));

    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    TEST_ESP_OK(esp_media_service_get_role(ESP_SERVICE_BASE(muxer), &role));
    TEST_ASSERT_EQUAL(ESP_MEDIA_ROLE_SINK, role);
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(muxer)));
    free(muxer);
}

TEST_CASE("muxer service setup and storage url require stopped state", "[esp_muxer_service]")
{
    esp_muxer_service_cfg_t cfg = ESP_MUXER_SERVICE_CFG_DEFAULT();
    esp_muxer_service_t *muxer = NULL;
    TEST_ESP_OK(esp_muxer_service_create(&cfg, &muxer));

    TEST_ESP_OK(esp_muxer_service_set_storage_url(muxer, "/fake/default.ts"));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(muxer)));

    esp_muxer_service_setup_t setup = ESP_MUXER_SERVICE_SETUP_DEFAULT();
    setup.mode = ESP_MUXER_SERVICE_MODE_STREAMING_ONLY;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_muxer_service_setup(muxer, &setup));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_muxer_service_set_storage_url(muxer, "/fake/next.mp4"));

    const uint8_t *data = NULL;
    size_t size = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      esp_muxer_service_acquire_streaming_data(muxer, &data, &size, 0));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(muxer)));
    TEST_ESP_OK(esp_muxer_service_setup(muxer, &setup));
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(muxer)));
    free(muxer);
}

TEST_CASE("muxer service streams TS bytes from dummy source", "[esp_muxer_service]")
{
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    src_cfg.name = "muxer_dummy_src";
    esp_media_dummy_service_t *src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&src_cfg, &src));
    ts_muxer_register();

    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_AAC,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        },
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &audio));

    esp_muxer_service_cfg_t muxer_cfg = ESP_MUXER_SERVICE_CFG_DEFAULT();
    muxer_cfg.name = "muxer_sink";
    esp_muxer_service_t *muxer = NULL;
    TEST_ESP_OK(esp_muxer_service_create(&muxer_cfg, &muxer));
    esp_muxer_service_setup_t muxer_setup = ESP_MUXER_SERVICE_SETUP_DEFAULT();
    muxer_setup.mode = ESP_MUXER_SERVICE_MODE_STREAMING_ONLY;
    muxer_setup.ram_cache_size = 256;
    TEST_ESP_OK(esp_muxer_service_setup(muxer, &muxer_setup));

    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(muxer), ESP_MEDIA_DEFAULT_STREAM));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(muxer)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));

    static uint8_t buffer[4096];
    size_t size = sizeof(buffer);
    TEST_ESP_OK(esp_muxer_service_read_streaming_data(muxer, buffer, &size, 2000));
    TEST_ASSERT_GREATER_THAN(0, size);

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(muxer)));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(muxer), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(muxer)));
    free(muxer);
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
    esp_muxer_unreg_all();
}
