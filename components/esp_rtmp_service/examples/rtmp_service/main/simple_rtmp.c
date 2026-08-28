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
#include "esp_rtmp_service.h"
#include "esp_rtmp_service_ops.h"
#include "esp_service.h"

#include "settings.h"
#include "simple_rtmp.h"

static const char *TAG = "SIMPLE_RTMP";

#define SIMPLE_RUN_DONE_BIT  BIT0

typedef struct {
    EventGroupHandle_t group;
    bool               abort_run;
} simple_run_t;

static void rtmp_event_handler(const adf_event_t *event, void *ctx)
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
    if (event->event_id >= ESP_RTMP_SERVICE_EVENT_PEER_CLOSED &&
        event->event_id <= ESP_RTMP_SERVICE_EVENT_SERVER_PULLER_STOPPED &&
        event->payload != NULL && event->payload_len >= sizeof(esp_rtmp_service_event_payload_t)) {
        const esp_rtmp_service_event_payload_t *payload = event->payload;
        ESP_LOGI(TAG, "event %s role:%d stream:%s", event_name, (int)payload->role,
                 payload->stream_name[0] ? payload->stream_name : "-");
    } else {
        ESP_LOGI(TAG, "event %s id:%u", event_name, (unsigned)event->event_id);
    }

    bool abort_run = false;
    if (event->event_id == ESP_RTMP_SERVICE_EVENT_PEER_CLOSED) {
        abort_run = true;
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

static esp_err_t subscribe_rtmp_events(esp_rtmp_service_t *service, simple_run_t *run)
{
    esp_service_t *base = ESP_SERVICE_BASE(service);
    esp_err_t ret = esp_service_set_user_data(base, run);
    if (ret != ESP_OK) {
        return ret;
    }
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = rtmp_event_handler;
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

static esp_err_t wait_run(simple_run_t *run, uint32_t duration_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(duration_ms);
    if (run->group == NULL) {
        vTaskDelay(ticks);
        return ESP_OK;
    }
    EventBits_t bits = xEventGroupWaitBits(run->group, SIMPLE_RUN_DONE_BIT, pdTRUE, pdFALSE, ticks);
    if ((bits & SIMPLE_RUN_DONE_BIT) != 0 || run->abort_run) {
        ESP_LOGW(TAG, "session ended early (peer closed or error)");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void rtmp_delete(esp_rtmp_service_t *service)
{
    if (service == NULL) {
        return;
    }
    esp_service_t *base = ESP_SERVICE_BASE(service);
    (void)esp_service_set_user_data(base, NULL);
    (void)esp_media_service_deinit(base);
    free(service);
}

static esp_err_t add_dummy_av(esp_media_dummy_service_t *src)
{
    esp_media_track_info_t audio = {
        .id = 1,
        .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        .info.audio.codec = ESP_FOURCC_AAC,
    };
    esp_media_track_info_t video = {
        .id = 2,
        .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        .info.video.codec = ESP_FOURCC_H264,
    };
    esp_err_t ret = esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &audio);
    if (ret != ESP_OK) {
        return ret;
    }
    return esp_media_dummy_service_add_track(src, ESP_MEDIA_DEFAULT_STREAM, &video);
}

static const char *resolve_url(const char *url, const char *fallback)
{
    return (url != NULL && url[0] != '\0') ? url : fallback;
}

static void fill_sta_ip(char *buf, size_t buflen)
{
    snprintf(buf, buflen, "DEVICE_IP");
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        return;
    }
    esp_netif_ip_info_t ip = {0};
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr != 0) {
        snprintf(buf, buflen, IPSTR, IP2STR(&ip.ip));
    }
}

esp_err_t simple_rtmp_pusher(uint32_t duration_ms, const char *url)
{
    const char *push_url = resolve_url(url, RTMP_PUSH_URL);
    /* 1. Dummy source (tracks added after link so RTMP can enable global cache). */
    esp_media_dummy_service_cfg_t src_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    src_cfg.role = ESP_MEDIA_ROLE_SRC;
    src_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *src = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&src_cfg, &src);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. RTMP pusher sink. Configure the server URL in settings.h / menuconfig. */
    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtmp_service_t *rtmp = NULL;
    if (ret == ESP_OK) {
        esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SINK);
        ret = esp_rtmp_service_create(&cfg, &rtmp);
    }
    if (ret == ESP_OK) {
        ret = subscribe_rtmp_events(rtmp, &run);
    }
    if (ret == ESP_OK) {
        esp_rtmp_service_setup_t setup = ESP_RTMP_SERVICE_SINK_SETUP_DEFAULT();
        ret = esp_rtmp_service_setup(rtmp, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_rtmp_service_set_url(rtmp, push_url);
    }
    /* Link before tracks: sink requests global cache, which cannot be set after add_track. */
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(rtmp), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (ret == ESP_OK) {
        ret = add_dummy_av(src);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(rtmp));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(src));
    }
    ESP_LOGI(TAG, "pusher url:%s duration_ms:%" PRIu32, push_url, duration_ms);
    if (ret == ESP_OK) {
        ret = wait_run(&run, duration_ms);
    }

    if (src != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(src));
    }
    if (rtmp != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(rtmp));
        if (src != NULL) {
            (void)esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                           ESP_SERVICE_BASE(rtmp), ESP_MEDIA_DEFAULT_STREAM);
        }
    }
    rtmp_delete(rtmp);
    if (src != NULL) {
        (void)esp_media_dummy_service_destroy(src);
    }
    simple_run_deinit(&run);
    return ret;
}

esp_err_t simple_rtmp_puller(uint32_t duration_ms, const char *url)
{
    const char *pull_url = resolve_url(url, RTMP_PULL_URL);
    /* 1. Dummy sink to consume pulled frames. */
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    esp_media_dummy_service_t *sink = NULL;
    esp_err_t ret = esp_media_dummy_service_create(&sink_cfg, &sink);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. RTMP puller source. */
    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SRC);
    esp_rtmp_service_t *rtmp = NULL;
    ret = esp_rtmp_service_create(&cfg, &rtmp);
    if (ret == ESP_OK) {
        ret = subscribe_rtmp_events(rtmp, &run);
    }
    if (ret == ESP_OK) {
        esp_rtmp_service_setup_t setup = ESP_RTMP_SERVICE_SRC_SETUP_DEFAULT();
        ret = esp_rtmp_service_setup(rtmp, &setup);
    }
    if (ret == ESP_OK) {
        ret = esp_rtmp_service_set_url(rtmp, pull_url);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(rtmp), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(rtmp));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(sink));
    }
    ESP_LOGI(TAG, "puller url:%s duration_ms:%" PRIu32, pull_url, duration_ms);
    if (ret == ESP_OK) {
        ret = wait_run(&run, duration_ms);
    }

    if (rtmp != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(rtmp));
    }
    if (sink != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(sink));
        esp_media_dummy_stream_stats_t stats = {0};
        (void)esp_media_dummy_service_get_stats(sink, ESP_MEDIA_DEFAULT_STREAM, &stats);
        ESP_LOGI(TAG, "puller stats: audio %" PRIu32 " video %" PRIu32,
                 stats.audio_frame_count, stats.video_frame_count);
        if (rtmp != NULL) {
            (void)esp_media_service_unlink(ESP_SERVICE_BASE(rtmp), ESP_MEDIA_DEFAULT_STREAM,
                                           ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
        }
        (void)esp_media_dummy_service_destroy(sink);
    }
    rtmp_delete(rtmp);
    simple_run_deinit(&run);
    return ret;
}

esp_err_t simple_rtmp_server(uint32_t duration_ms, const char *url)
{
    const char *server_url = resolve_url(url, RTMP_SERVER_URL);
    /* Local RTMP server. Push/pull from a PC with the printed ffmpeg commands. */
    esp_rtmp_service_cfg_t cfg = ESP_RTMP_SERVICE_CFG_DEFAULT(ESP_RTMP_SERVICE_ROLE_SERVER);
    simple_run_t run = {0};
    simple_run_init(&run);
    esp_rtmp_service_t *server = NULL;
    esp_err_t ret = esp_rtmp_service_create(&cfg, &server);
    if (ret != ESP_OK) {
        simple_run_deinit(&run);
        return ret;
    }
    ret = subscribe_rtmp_events(server, &run);
    if (ret != ESP_OK) {
        rtmp_delete(server);
        simple_run_deinit(&run);
        return ret;
    }
    esp_rtmp_service_setup_t setup = ESP_RTMP_SERVICE_SERVER_SETUP_DEFAULT();
    ret = esp_rtmp_service_setup(server, &setup);
    if (ret == ESP_OK) {
        ret = esp_rtmp_service_set_url(server, server_url);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(server));
    }

    char ip[16];
    fill_sta_ip(ip, sizeof(ip));
    ESP_LOGI(TAG, "server url:%s", server_url);
    ESP_LOGI(TAG, "ffmpeg push: ffmpeg -re -f lavfi -i testsrc=size=320x240:rate=15 -f lavfi -i sine -c:v libx264 -preset ultrafast -tune zerolatency -c:a aac -f flv rtmp://%s:1935/live/%s",
             ip, RTMP_SERVER_STREAM);
    ESP_LOGI(TAG, "ffmpeg pull: ffplay rtmp://%s:1935/live/%s", ip, RTMP_SERVER_STREAM);

    if (ret == ESP_OK) {
        TickType_t start = xTaskGetTickCount();
        TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
        while ((xTaskGetTickCount() - start) < duration_ticks && !run.abort_run) {
            ESP_LOGI(TAG, "\n");
            (void)esp_rtmp_service_query(server);
            TickType_t remain = duration_ticks - (xTaskGetTickCount() - start);
            TickType_t slice = pdMS_TO_TICKS(5000);
            if (remain < slice) {
                slice = remain;
            }
            if (run.group != NULL) {
                EventBits_t bits = xEventGroupWaitBits(run.group, SIMPLE_RUN_DONE_BIT, pdTRUE, pdFALSE, slice);
                if ((bits & SIMPLE_RUN_DONE_BIT) != 0) {
                    break;
                }
            } else {
                vTaskDelay(slice);
            }
        }
        if (run.abort_run) {
            ESP_LOGW(TAG, "server session ended early");
            ret = ESP_FAIL;
        }
    }

    (void)esp_service_stop(ESP_SERVICE_BASE(server));
    rtmp_delete(server);
    simple_run_deinit(&run);
    return ret;
}
