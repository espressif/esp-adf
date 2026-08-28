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
#include "esp_rtmp_service.h"
#include "esp_rtmp_service_ops.h"
#include "esp_rtmp_scheduler.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"

#define RTMP_SERVER_URL   "rtmp://127.0.0.1:1935/live"
#define RTMP_STREAM_URL   "rtmp://127.0.0.1:1935/live/stream0"
#define TEST_RUN_MS       1500

void esp_rtmp_service_ut_force_link(void) {}

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

static void rtmp_service_delete(esp_rtmp_service_t *service)
{
    TEST_ESP_OK(esp_media_service_deinit(ESP_SERVICE_BASE(service)));
    free(service);
}

TEST_CASE("rtmp create deinit by role", "[esp_rtmp_service]")
{
    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SERVER);
    esp_rtmp_service_t *server = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&cfg, &server));
    esp_rtmp_service_setup_t server_setup = ESP_RTMP_SERVICE_SERVER_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(server, &server_setup));
    rtmp_service_delete(server);

    cfg.role = ESP_RTMP_SERVICE_ROLE_SINK;
    esp_rtmp_service_t *sink = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&cfg, &sink));
    esp_rtmp_service_setup_t sink_setup = ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(sink, &sink_setup));
    rtmp_service_delete(sink);

    cfg.role = ESP_RTMP_SERVICE_ROLE_SRC;
    esp_rtmp_service_t *src = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&cfg, &src));
    esp_rtmp_service_setup_t src_setup = ESP_RTMP_SERVICE_SRC_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(src, &src_setup));
    rtmp_service_delete(src);
}

esp_err_t rtmp_server_query(esp_rtmp_service_t *service);

TEST_CASE("rtmp loopback dummy_src to sink to src to dummy_sink", "[esp_rtmp_service]")
{
    TEST_ESP_OK(esp_service_scheduler_set_cb(test_scheduler_cb, NULL));

    esp_rtmp_service_cfg_t server_cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SERVER);
    server_cfg.name = "rtmp_server";
    esp_rtmp_service_t *server = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&server_cfg, &server));
    esp_rtmp_service_setup_t server_setup = ESP_RTMP_SERVICE_SERVER_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(server, &server_setup));
    TEST_ESP_OK(esp_rtmp_service_set_url(server, RTMP_SERVER_URL));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(server)));
    vTaskDelay(pdMS_TO_TICKS(200));

    esp_media_dummy_service_cfg_t dummy_src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_src_cfg.role = ESP_MEDIA_ROLE_SRC;
    dummy_src_cfg.max_stream_num = 1;
    dummy_src_cfg.name = "dummy_src";
    esp_media_dummy_service_t *dummy_src = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_src_cfg, &dummy_src));

    esp_rtmp_service_cfg_t sink_cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SINK);
    sink_cfg.name = "rtmp_sink";
    esp_rtmp_service_t *sink = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&sink_cfg, &sink));
    esp_rtmp_service_setup_t sink_setup = ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(sink, &sink_setup));
    TEST_ESP_OK(esp_rtmp_service_set_url(sink, RTMP_STREAM_URL));

    esp_rtmp_service_cfg_t src_cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SRC);
    src_cfg.name = "rtmp_src";
    esp_rtmp_service_t *src = NULL;
    TEST_ESP_OK(esp_rtmp_service_create(&src_cfg, &src));
    esp_rtmp_service_setup_t src_setup = ESP_RTMP_SERVICE_SRC_SETUP_DEFAULT();
    TEST_ESP_OK(esp_rtmp_service_setup(src, &src_setup));
    TEST_ESP_OK(esp_rtmp_service_set_url(src, RTMP_STREAM_URL));

    esp_media_dummy_service_cfg_t dummy_sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    dummy_sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    dummy_sink_cfg.max_stream_num = 1;
    dummy_sink_cfg.name = "dummy_sink";
    esp_media_dummy_service_t *dummy_sink = NULL;
    TEST_ESP_OK(esp_media_dummy_service_create(&dummy_sink_cfg, &dummy_sink));


    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_MJPG,
    };
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &audio));
    TEST_ESP_OK(esp_media_dummy_service_add_track(dummy_src, ESP_MEDIA_DEFAULT_STREAM, &video));

    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(dummy_src)));
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ESP_OK(esp_service_start(ESP_SERVICE_BASE(src)));
    printf("go %d.....\n", __LINE__);
    int timeout =  5000;
    while (timeout > 0) {
        printf("\n\n");
        rtmp_server_query(server);
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout -= 1000;

    }

    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_sink)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(dummy_src)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(sink)));
    TEST_ESP_OK(esp_service_stop(ESP_SERVICE_BASE(server)));

    esp_media_dummy_stream_stats_t stats = {0};
    TEST_ESP_OK(esp_media_dummy_service_get_stats(dummy_sink, ESP_MEDIA_DEFAULT_STREAM, &stats));
    TEST_ASSERT_GREATER_THAN(0, stats.audio_frame_count + stats.video_frame_count);

    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(dummy_src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM));
    TEST_ESP_OK(esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                         ESP_SERVICE_BASE(dummy_sink), ESP_MEDIA_DEFAULT_STREAM));

    rtmp_service_delete(src);
    rtmp_service_delete(sink);
    rtmp_service_delete(server);
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_src));
    TEST_ESP_OK(esp_media_dummy_service_destroy(dummy_sink));
    esp_service_scheduler_set_cb(NULL, NULL);
}
