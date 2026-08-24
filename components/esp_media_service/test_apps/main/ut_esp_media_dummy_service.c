/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_service.h"

TEST_CASE("dummy source and sink link PCM stream", "[esp_media_service]")
{
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;

    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;

    esp_media_dummy_service_t *src = NULL;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&src_cfg, &src));
    TEST_ESP_OK(esp_media_dummy_service_create(&sink_cfg, &sink));

    esp_media_track_info_t audio = {
        .id = 0,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_PCM,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        },
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(src, 0, &audio));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0,
                                       ESP_SERVICE_BASE(sink), 0));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));

    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ASSERT_GREATER_THAN(0, stats.audio_byte_count);

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0,
                                         ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}

TEST_CASE("dummy source encoded AAC and H264 to sink", "[esp_media_service]")
{
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;

    esp_media_dummy_service_t *src = NULL;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&src_cfg, &src));
    TEST_ESP_OK(esp_media_dummy_service_create(&sink_cfg, &sink));

    esp_media_track_info_t audio = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_H264,
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(src, 0, &audio));
    TEST_ESP_OK(esp_media_dummy_service_add_track(src, 0, &video));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0,
                                       ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));

    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ASSERT_GREATER_THAN(0, stats.video_frame_count);

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0,
                                         ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
}

TEST_CASE("dummy source start stop can repeat", "[esp_media_service]")
{
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;

    esp_media_dummy_service_t *src = NULL;
    esp_media_dummy_service_t *sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&src_cfg, &src));
    TEST_ESP_OK(esp_media_dummy_service_create(&sink_cfg, &sink));

    esp_media_track_info_t audio = {
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio = {
            .codec = ESP_FOURCC_PCM,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .channel = 1,
        },
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(src, 0, &audio));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), 0,
                                       ESP_SERVICE_BASE(sink), 0));

    for (int round = 0; round < 2; round++) {
        TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
        TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
        vTaskDelay(pdMS_TO_TICKS(100));
        TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
        TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));

        esp_media_dummy_stream_stats_t stats = {0};
        TEST_ESP_OK(esp_media_dummy_service_get_stats(sink, 0, &stats));
        TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    }

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), 0,
                                         ESP_SERVICE_BASE(sink), 0));
    TEST_ESP_OK(esp_media_dummy_service_destroy(src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(sink));
}
