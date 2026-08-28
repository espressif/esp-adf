/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_fourcc.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_service.h"
#include "esp_netif.h"
#include "esp_rtsp_service.h"
#include "esp_rtsp_service_ops.h"
#include "esp_service.h"

#include "settings.h"
#include "simple_rtsp.h"

static const char *TAG = "SIMPLE_RTSP";

#define SIMPLE_RUN_DONE_BIT  BIT0

typedef struct {
    EventGroupHandle_t group;
    bool               abort_run;
} simple_run_t;

static void rtsp_event_handler(const adf_event_t *event, void *ctx)
{
    /* handler_ctx is the publisher service — do not recover it from simple_run_t. */
    esp_service_t *service = (esp_service_t *)ctx;
    simple_run_t *run = NULL;
    if (service != NULL) {
        (void)esp_service_get_user_data(service, (void **)&run);
    }

    const char *event_name = "UNKNOWN";
    const char *resolved = NULL;
    if (service != NULL &&
        esp_service_get_event_name(service, event->event_id, &resolved) == ESP_OK && resolved != NULL) {
        event_name = resolved;
    }
    if (event->event_id >= ESP_RTSP_SERVICE_EVENT_OPTIONS &&
        event->event_id <= ESP_RTSP_SERVICE_EVENT_TEARDOWN &&
        event->payload != NULL && event->payload_len >= sizeof(esp_rtsp_service_event_payload_t)) {
        const esp_rtsp_service_event_payload_t *payload = event->payload;
        ESP_LOGI(TAG, "event %s role:%d state:%d", event_name, (int)payload->role, (int)payload->state);
    } else {
        ESP_LOGI(TAG, "event %s id:%u", event_name, (unsigned)event->event_id);
    }

    bool abort_run = false;
    if (event->event_id == ESP_RTSP_SERVICE_EVENT_TEARDOWN) {
        const esp_rtsp_service_event_payload_t *payload = event->payload;
        /* Local server keeps running after a remote client tears down. */
        abort_run = (payload == NULL || payload->role != ESP_RTSP_SERVICE_ROLE_SERVER);
    } else if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED &&
               event->payload != NULL &&
               event->payload_len >= sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *st = event->payload;
        abort_run = (st->new_state == ESP_SERVICE_STATE_ERROR);
    }
    if (abort_run && run != NULL && run->group != NULL) {
        run->abort_run = true;
        xEventGroupSetBits(run->group, SIMPLE_RUN_DONE_BIT);
    }
}

static esp_err_t subscribe_rtsp_events(esp_rtsp_service_t *service, simple_run_t *run)
{
    esp_service_t *base = ESP_SERVICE_BASE(service);
    esp_err_t ret = esp_service_set_user_data(base, run);
    if (ret != ESP_OK) {
        return ret;
    }
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = rtsp_event_handler;
    info.handler_ctx = base;
    return esp_service_event_subscribe(base, &info);
}

static void simple_run_init(simple_run_t *run)
{
    run->abort_run = false;
    run->group = xEventGroupCreate();
}

static void simple_run_deinit(simple_run_t *run)
{
    if (run->group != NULL) {
        vEventGroupDelete(run->group);
        run->group = NULL;
    }
}

static void simple_run_begin(simple_run_t *run)
{
    /* Drop stale TEARDOWN/DONE from the previous loop or stop path. */
    run->abort_run = false;
    if (run->group != NULL) {
        xEventGroupClearBits(run->group, SIMPLE_RUN_DONE_BIT);
    }
}

static esp_err_t wait_run(simple_run_t *run, uint32_t duration_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(duration_ms);
    if (run->group == NULL) {
        vTaskDelay(ticks);
        return ESP_OK;
    }
    EventBits_t bits = xEventGroupWaitBits(run->group, SIMPLE_RUN_DONE_BIT, pdTRUE, pdFALSE, ticks);
    if ((bits & SIMPLE_RUN_DONE_BIT) != 0 || run->abort_run) {
        ESP_LOGW(TAG, "session ended early (teardown or error)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void rtsp_delete(esp_rtsp_service_t *service)
{
    if (service == NULL) {
        return;
    }
    esp_service_t *base = ESP_SERVICE_BASE(service);
    (void)esp_service_set_user_data(base, NULL);
    (void)esp_media_service_deinit(base);
    free(service);
}

static esp_err_t add_dummy_av(esp_media_dummy_service_t *src, int stream_mask)
{
    esp_err_t ret = ESP_OK;
    if (stream_mask & SIMPLE_RTSP_PUSH_MASK_AUDIO) {
        esp_media_track_info_t audio = {
            .id = 1,
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
            .info.audio.codec = ESP_FOURCC_AAC,
        };

        ret = esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &audio);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (stream_mask & SIMPLE_RTSP_PUSH_MASK_VIDEO) {
        esp_media_track_info_t video = {
            .id = 2,
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
            .info.video.codec = ESP_FOURCC_H264,
        };
        ret = esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &video);
    }
    return ret;
}

static const char *resolve_url(const char *url, const char *fallback)
{
    return (url != NULL && url[0] != '\0') ? url : fallback;
}

static void fill_sta_ip(char *buf, size_t buflen)
{
    buf[0] = 0;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return;
    }
    esp_netif_ip_info_t ip = {0};
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        snprintf(buf, buflen, IPSTR, IP2STR(&ip.ip));
    }
}

esp_err_t simple_rtsp_pusher(uint32_t duration_ms, const char *url, int stream_mask)
{
    const char *push_url = resolve_url(url, RTSP_PUSH_URL);
    /* Dummy H264 and/or AAC -> RTSP client push. */
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *src = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&src_cfg, &src);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = add_dummy_av(src, stream_mask);

    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtsp_service_t *rtsp = NULL;
    if (ret == ESP_OK) {
        esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SINK);
        ret = esp_rtsp_service_create(&cfg, &rtsp);
    }
    if (ret == ESP_OK) {
        ret = subscribe_rtsp_events(rtsp, &run);
    }
    if (ret == ESP_OK) {
        esp_rtsp_service_setup_t setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
        ret = esp_rtsp_service_setup(rtsp, &setup);
    }
    char ip[16];
    fill_sta_ip(ip, sizeof(ip));
    if (ip[0]) {
        ret = esp_rtsp_service_set_ip(rtsp, ip);
    }
    if (ret == ESP_OK) {
        ret = esp_rtsp_service_set_url(rtsp, push_url);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM);
    }
    for (int i = 0; i < 2; i++) {
        simple_run_begin(&run);
        if (ret == ESP_OK) {
            ret = esp_service_start(ESP_SERVICE_BASE(rtsp));
        }
        if (ret == ESP_OK) {
            ret = esp_service_start(ESP_SERVICE_BASE(src));
        }
        ESP_LOGI(TAG, "pusher url:%s duration_ms:%" PRIu32 " audio:%d video:%d",
                 push_url, duration_ms,
                 !!(stream_mask & SIMPLE_RTSP_PUSH_MASK_AUDIO),
                 !!(stream_mask & SIMPLE_RTSP_PUSH_MASK_VIDEO));
        if (ret == ESP_OK) {
            ret = wait_run(&run, duration_ms);
        }

        if (src != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(src));
        }
        if (rtsp != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(rtsp));
        }
    }
    if (src != NULL) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM);
    }
    rtsp_delete(rtsp);
    if (src != NULL) {
        (void)esp_media_dummy_service_destroy(src);
    }
    simple_run_deinit(&run);
    return ret;
}

esp_err_t simple_rtsp_puller(uint32_t duration_ms, const char *url)
{
    const char *pull_url = resolve_url(url, RTSP_PULL_URL);
    /* RTSP client play -> dummy sink. */
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *sink = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&sink_cfg, &sink);
    if (ret != ESP_OK) {
        return ret;
    }

    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SRC);
    esp_rtsp_service_t *rtsp = NULL;
    ret = esp_rtsp_service_create(&cfg, &rtsp);
    if (ret == ESP_OK) {
        ret = subscribe_rtsp_events(rtsp, &run);
    }
    if (ret == ESP_OK) {
        esp_rtsp_service_setup_t setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
        ret = esp_rtsp_service_setup(rtsp, &setup);
    }
    char ip[16];
    fill_sta_ip(ip, sizeof(ip));
    if (ip[0]) {
        ret = esp_rtsp_service_set_ip(rtsp, ip);
    }
    if (ret == ESP_OK) {
        ret = esp_rtsp_service_set_url(rtsp, pull_url);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
    }

    for (int i = 0; i < 2; i++) {
        simple_run_begin(&run);
        if (ret == ESP_OK) {
            ret = esp_service_start(ESP_SERVICE_BASE(rtsp));
        }
        if (ret == ESP_OK) {
            ret = esp_service_start(ESP_SERVICE_BASE(sink));
        }
        ESP_LOGI(TAG, "puller url:%s duration_ms:%" PRIu32, pull_url, duration_ms);
        if (ret == ESP_OK) {
            ret = wait_run(&run, duration_ms);
        }

        if (rtsp != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(rtsp));
        }
        if (sink != NULL) {
            (void)esp_service_stop(ESP_SERVICE_BASE(sink));
            esp_media_dummy_stream_stats_t stats = {0};
            (void)esp_media_dummy_service_get_stats(sink, ESP_MEDIA_DEFAULT_STREAM, &stats);
            ESP_LOGI(TAG, "puller stats: audio %" PRIu32 " video %" PRIu32,
                     stats.audio_frame_count, stats.video_frame_count);
        }
    }
    if (rtsp != NULL) {
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM,
                                    ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
    }
    (void)esp_media_dummy_service_destroy(sink);
    rtsp_delete(rtsp);
    simple_run_deinit(&run);
    return ret;
}

esp_err_t simple_rtsp_server(uint32_t duration_ms, const char *url, int stream_mask)
{
    const char *server_url = resolve_url(url, RTSP_SERVER_URL);
    /* Dummy local push into ROLE_SERVER; remote players use the printed ffplay URL. */
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *src = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&src_cfg, &src);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = add_dummy_av(src, stream_mask);

    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtsp_service_t *rtsp = NULL;
    if (ret == ESP_OK) {
        esp_rtsp_service_cfg_t cfg = ESP_RTSP_SERVICE_CFG_DEFAULT(ESP_RTSP_SERVICE_ROLE_SERVER);
        ret = esp_rtsp_service_create(&cfg, &rtsp);
    }
    if (ret == ESP_OK) {
        ret = subscribe_rtsp_events(rtsp, &run);
    }
    if (ret == ESP_OK) {
        esp_rtsp_service_setup_t setup = ESP_RTSP_SERVICE_SETUP_DEFAULT();
        ret = esp_rtsp_service_setup(rtsp, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_rtsp_service_set_url(rtsp, server_url);
    }
    char ip[16];
    fill_sta_ip(ip, sizeof(ip));
    if (ip[0]) {
        ret = esp_rtsp_service_set_ip(rtsp, ip);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(rtsp));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(src));
    }

    ESP_LOGI(TAG, "server url:%s local_ip:%s audio:%d video:%d",
             server_url, ip,
             !!(stream_mask & SIMPLE_RTSP_PUSH_MASK_AUDIO),
             !!(stream_mask & SIMPLE_RTSP_PUSH_MASK_VIDEO));
    ESP_LOGI(TAG, "remote pull: ffplay rtsp://%s:554/live", ip);

    if (ret == ESP_OK) {
        ret = wait_run(&run, duration_ms);
    }

    if (rtsp != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(rtsp));
        if (src != NULL) {
            (void)esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                           ESP_SERVICE_BASE(rtsp), ESP_MEDIA_DEFAULT_STREAM);
        }
    }
    if (src != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(src));
    }
    rtsp_delete(rtsp);
    if (src != NULL) {
        (void)esp_media_dummy_service_destroy(src);
    }
    simple_run_deinit(&run);
    return ret;
}
