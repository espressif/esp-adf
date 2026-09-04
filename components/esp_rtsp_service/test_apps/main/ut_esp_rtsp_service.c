/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"

#include "esp_fourcc.h"
#include "esp_media_dummy_service.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"


#define RTSP_SERVER_URL  "rtsp://127.0.0.1:8554/live"
#define RTSP_CLIENT_URL  "rtsp://127.0.0.1:8554/live"
#define TEST_RUN_MS      2000

void esp_rtsp_service_ut_force_link(void) {}

static void rtsp_service_delete(esp_rtsp_service_t *service)
{
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(service)));
    free(service);
}

static void expect_rtsp_media_role(esp_rtsp_service_t *service, esp_media_role_t expected)
{
    esp_media_role_t role = ESP_MEDIA_ROLE_NONE;
    TEST_ESP_OK(esp_media_service_get_role(ESP_SERVICE_BASE(service), &role));
    TEST_ASSERT_EQUAL(expected, role);
}

static esp_err_t test_scheduler_cb(const esp_service_thread_request_t *request,
                                   esp_service_thread_cfg_t *cfg,
                                   void *ctx)
{
    (void)ctx;
    if (request == NULL || cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    cfg->stack_size = 8192;
    cfg->priority = 8;
    cfg->core_id = 0;
    return ESP_OK;
}

static void rtsp_loopback_aac_with_video(esp_media_codec_fourcc_t video_codec)
{
    TEST_ESP_OK(esp_service_scheduler_set_cb(test_scheduler_cb, NULL));

    esp_rtsp_service_cfg_t server_cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SERVER);
    server_cfg.name = "rtsp_server";
    esp_rtsp_service_t *server = NULL;
    TEST_ESP_OK(esp_rtsp_service_create(&server_cfg, &server));
    esp_rtsp_service_setup_t server_setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
    server_setup.local_port = 8554;
    server_setup.transport = RTSP_TRANSPORT_UDP;
    TEST_ESP_OK(esp_rtsp_service_setup(server, &server_setup));
    TEST_ESP_OK(esp_rtsp_service_set_url(server, RTSP_SERVER_URL));

    esp_media_dummy_service_cfg_t dummy_src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_src_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_src_cfg.max_stream_num = 1;
    dummy_src_cfg.name = "dummy_src";
    esp_media_dummy_service_t *dummy_src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_src_cfg, &dummy_src));

    esp_rtsp_service_cfg_t src_cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SRC);
    src_cfg.name = "rtsp_src";
    esp_rtsp_service_t *src = NULL;
    TEST_ESP_OK(esp_rtsp_service_create(&src_cfg, &src));
    esp_rtsp_service_setup_t src_setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
    src_setup.transport = RTSP_TRANSPORT_UDP;
    TEST_ESP_OK(esp_rtsp_service_setup(src, &src_setup));
    TEST_ESP_OK(esp_rtsp_service_set_url(src, RTSP_CLIENT_URL));

    esp_media_dummy_service_cfg_t dummy_sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    dummy_sink_cfg.max_stream_num = 1;
    dummy_sink_cfg.name = "dummy_sink";
    esp_media_dummy_service_t *dummy_sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_sink_cfg, &dummy_sink));

    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = video_codec,
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &audio));
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &video));

    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(server), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(server)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_src)));
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    vTaskDelay(pdMS_TO_TICKS(TEST_RUN_MS));

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(server)));

    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(dummy_sink, ESP_MEDIA_DEFAULT_STREAM, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count);
    TEST_ASSERT_GREATER_THAN(0, stats.video_frame_count);

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(server), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    rtsp_service_delete(src);
    rtsp_service_delete(server);
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_sink));
    esp_service_scheduler_set_cb(NULL, NULL);
}

TEST_CASE("rtsp create deinit by role", "[esp_rtsp_service]")
{
    esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SERVER);
    esp_rtsp_service_t *server = NULL;
    TEST_ESP_OK(esp_rtsp_service_create(&cfg, &server));
    expect_rtsp_media_role(server, ESP_MEDIA_ROLE_SINK);
    rtsp_service_delete(server);

    cfg.role = ESP_RTSP_SERVICE_ROLE_SINK;
    esp_rtsp_service_t *sink = NULL;
    TEST_ESP_OK(esp_rtsp_service_create(&cfg, &sink));
    expect_rtsp_media_role(sink, ESP_MEDIA_ROLE_SINK);
    rtsp_service_delete(sink);

    cfg.role = ESP_RTSP_SERVICE_ROLE_SRC;
    esp_rtsp_service_t *src = NULL;
    TEST_ESP_OK(esp_rtsp_service_create(&cfg, &src));
    expect_rtsp_media_role(src, ESP_MEDIA_ROLE_SRC);
    rtsp_service_delete(src);
}

TEST_CASE("rtsp loopback MJPEG + AAC", "[esp_rtsp_service]")
{
    rtsp_loopback_aac_with_video(ESP_FOURCC_MJPG);
}

TEST_CASE("rtsp loopback H264 + AAC", "[esp_rtsp_service]")
{
    rtsp_loopback_aac_with_video(ESP_FOURCC_H264);
}
