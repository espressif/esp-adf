/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_extractor_service.h"
#include "esp_extractor_service_ops.h"
#include "esp_log.h"
#include "esp_media_dummy_service.h"
#include "esp_media_provider.h"
#include "esp_media_service.h"
#include "esp_media_service_types.h"
#include "esp_service.h"
#include "esp_timer.h"

#include "extractor_demo.h"
#include "settings.h"

static const char *TAG = "EXTRACTOR_DEMO";

#define TRACK_WAIT_FILE_MS     8000
#define TRACK_WAIT_NETWORK_MS  60000
#define IDLE_MS                10
#define MAX_TRACKS             3

#define BIT_FAILED       BIT0
#define BIT_SEEK_OK      BIT1
#define BIT_SEEK_ERR     BIT2
#define BIT_EOS          BIT3
#define BIT_TRACK_READY  BIT4

/**
 * Tiny shared context for the event handler.
 * repeat_n == 0 : play once, with a one-shot seek demo
 * repeat_n  > 0 : auto_loop on; app stops after N service EOS events
 */
typedef struct {
    EventGroupHandle_t  ev;
    bool                failed;
    uint32_t            eos_n;
    uint32_t            repeat_n;
} demo_t;

typedef struct {
    uint16_t                id;
    esp_media_track_type_t  type;
    bool                    eos;
} track_t;

typedef struct {
    SemaphoreHandle_t   lock;
    EventGroupHandle_t  ready;
    track_t             items[MAX_TRACKS];
    uint16_t            num;
    bool                aborted;
} tracks_t;

static bool url_needs_network(const char *url)
{
    return (strncmp(url, "http://", 7) == 0 ||
            strncmp(url, "https://", 8) == 0 ||
            strstr(url, ".m3u8") != NULL ||
            strstr(url, ".hls") != NULL);
}

static void log_frame(const esp_media_frame_t *frame, uint32_t audio_n, uint32_t video_n)
{
    const uint8_t *p = (const uint8_t *)frame->data;
    char hex[24] = {0};
    size_t n = frame->size < 8 ? frame->size : 8;
    size_t off = 0;
    for (size_t i = 0; i < n; i++) {
        off += (size_t)snprintf(hex + off, sizeof(hex) - off, "%02x ", p[i]);
    }
    if (off > 0) {
        hex[off - 1] = '\0';
    }
    ESP_LOGI(TAG, "frame type:%d pts:%lld size:%u a:%" PRIu32 " v:%" PRIu32 " | %s",
             (int)frame->type, (long long)frame->pts, (unsigned)frame->size, audio_n, video_n, hex);
}

static void on_event(const adf_event_t *event, void *ctx)
{
    esp_service_t *svc = (esp_service_t *)ctx;
    demo_t *demo = NULL;
    if (svc != NULL) {
        (void)esp_service_get_user_data(svc, (void **)&demo);
    }

    const char *name = "UNKNOWN";
    const char *resolved = NULL;
    if (svc != NULL && svc->ops != NULL &&
        esp_service_get_event_name(svc, event->event_id, &resolved) == ESP_OK && resolved != NULL) {
        name = resolved;
    }

    if (event->event_id == ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE ||
        event->event_id == ESP_EXTRACTOR_SERVICE_EVENT_SEEK_ERROR ||
        event->event_id == ESP_EXTRACTOR_SERVICE_EVENT_EOS) {
        const esp_extractor_service_event_payload_t *p = NULL;
        if (event->payload != NULL && event->payload_len >= sizeof(*p)) {
            p = event->payload;
        }
        ESP_LOGI(TAG, "event %s pos:%u loop:%u err:%s", name,
                 p ? (unsigned)p->position_ms : 0,
                 p ? (unsigned)p->loop_count : 0,
                 p ? esp_err_to_name(p->err) : "-");
        if (demo == NULL || demo->ev == NULL) {
            return;
        }
        if (event->event_id == ESP_EXTRACTOR_SERVICE_EVENT_SEEK_DONE) {
            xEventGroupSetBits(demo->ev, BIT_SEEK_OK);
        } else if (event->event_id == ESP_EXTRACTOR_SERVICE_EVENT_SEEK_ERROR) {
            demo->failed = true;
            xEventGroupSetBits(demo->ev, BIT_SEEK_ERR | BIT_FAILED);
        } else {
            demo->eos_n++;
            xEventGroupSetBits(demo->ev, BIT_EOS);
        }
        return;
    }

    if (event->event_id == ESP_SERVICE_EVENT_STATE_CHANGED &&
        event->payload != NULL &&
        event->payload_len >= sizeof(esp_service_state_changed_payload_t)) {
        const esp_service_state_changed_payload_t *st = event->payload;
        ESP_LOGI(TAG, "event %s state:%d", name, (int)st->new_state);
        if (st->new_state == ESP_SERVICE_STATE_ERROR && demo != NULL && demo->ev != NULL) {
            demo->failed = true;
            xEventGroupSetBits(demo->ev, BIT_FAILED);
        }
        return;
    }

    ESP_LOGI(TAG, "event %s id:%u", name, (unsigned)event->event_id);
}

static esp_err_t demo_subscribe(esp_service_t *svc, demo_t *demo)
{
    ESP_RETURN_ON_ERROR(esp_service_set_user_data(svc, demo), TAG, "user_data");
    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_id = ADF_EVENT_ANY_ID;
    info.handler = on_event;
    info.handler_ctx = svc;
    return esp_service_event_subscribe(svc, &info);
}

static void demo_init(demo_t *demo, uint32_t repeat_n)
{
    memset(demo, 0, sizeof(*demo));
    demo->repeat_n = repeat_n;
    demo->ev = xEventGroupCreate();
}

static void demo_deinit(demo_t *demo)
{
    if (demo->ev != NULL) {
        vEventGroupDelete(demo->ev);
        demo->ev = NULL;
    }
}

static esp_err_t demo_wait(demo_t *demo, uint32_t duration_ms)
{
    if (demo->ev == NULL) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return ESP_OK;
    }
    const int64_t deadline = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    while (esp_timer_get_time() < deadline) {
        int64_t left_ms = (deadline - esp_timer_get_time()) / 1000;
        if (left_ms <= 0) {
            break;
        }
        EventBits_t want = BIT_FAILED | ((demo->repeat_n > 0) ? BIT_EOS : 0);
        (void)xEventGroupWaitBits(demo->ev, want, pdTRUE, pdFALSE, pdMS_TO_TICKS((uint32_t)left_ms));
        if (demo->failed) {
            ESP_LOGW(TAG, "session ended early (error)");
            return ESP_FAIL;
        }
        if (demo->repeat_n > 0 && demo->eos_n >= demo->repeat_n) {
            ESP_LOGI(TAG, "repeat done eos:%u", (unsigned)demo->eos_n);
            return ESP_OK;
        }
    }
    return demo->failed ? ESP_FAIL : ESP_OK;
}

static void tracks_add(tracks_t *tk, uint16_t id, esp_media_track_type_t type)
{
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    for (uint16_t i = 0; i < tk->num; i++) {
        if (tk->items[i].id == id) {
            tk->items[i].type = type;
            xSemaphoreGive(tk->lock);
            return;
        }
    }
    if (tk->num < MAX_TRACKS) {
        tk->items[tk->num] = (track_t) {.id = id, .type = type, .eos = false};
        tk->num++;
        xEventGroupSetBits(tk->ready, BIT_TRACK_READY);
    }
    xSemaphoreGive(tk->lock);
}

static void tracks_remove(tracks_t *tk, uint16_t id)
{
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    for (uint16_t i = 0; i < tk->num; i++) {
        if (tk->items[i].id != id) {
            continue;
        }
        for (uint16_t j = i + 1; j < tk->num; j++) {
            tk->items[j - 1] = tk->items[j];
        }
        tk->num--;
        break;
    }
    xSemaphoreGive(tk->lock);
}

static void tracks_mark_eos(tracks_t *tk, uint16_t id)
{
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    for (uint16_t i = 0; i < tk->num; i++) {
        if (tk->items[i].id == id) {
            tk->items[i].eos = true;
            break;
        }
    }
    xSemaphoreGive(tk->lock);
}

static void tracks_clear_eos(tracks_t *tk)
{
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    for (uint16_t i = 0; i < tk->num; i++) {
        tk->items[i].eos = false;
    }
    tk->aborted = false;
    xSemaphoreGive(tk->lock);
}

static bool tracks_all_eos(tracks_t *tk)
{
    bool all = false;
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    all = (tk->num > 0);
    for (uint16_t i = 0; all && i < tk->num; i++) {
        all = tk->items[i].eos;
    }
    xSemaphoreGive(tk->lock);
    return all;
}

static void on_track_event(esp_media_provider_event_t event, const esp_media_track_info_t *info, void *ctx)
{
    tracks_t *tk = (tracks_t *)ctx;
    if (tk == NULL) {
        return;
    }
    switch (event) {
        case ESP_MEDIA_PROVIDER_EVENT_TRACK_ADDED:
        case ESP_MEDIA_PROVIDER_EVENT_TRACK_UPDATED:
            if (info != NULL) {
                tracks_add(tk, info->id, info->type);
            }
            break;
        case ESP_MEDIA_PROVIDER_EVENT_TRACK_REMOVED:
            if (info != NULL) {
                tracks_remove(tk, info->id);
            }
            break;
        case ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT:
            tk->aborted = true;
            break;
        default:
            break;
    }
}

static void tracks_sync(tracks_t *tk, esp_media_provider_t *provider)
{
    uint16_t num = 0;
    if (esp_media_provider_get_track_num(provider, &num) != ESP_OK) {
        return;
    }
    for (uint16_t i = 0; i < num; i++) {
        esp_media_track_info_t info = {0};
        if (esp_media_provider_get_track_info(provider, i, &info) == ESP_OK) {
            tracks_add(tk, info.id, info.type);
        }
    }
}

/** Drain ready frames. Returns true if any frame was acquired. */
static bool pull_frames(esp_media_provider_t *provider, tracks_t *tk, bool during_seek,
                        uint32_t *audio_n, uint32_t *video_n)
{
    track_t local[MAX_TRACKS];
    uint16_t num = 0;
    xSemaphoreTake(tk->lock, portMAX_DELAY);
    num = tk->num;
    memcpy(local, tk->items, num * sizeof(local[0]));
    xSemaphoreGive(tk->lock);

    bool got = false;
    for (uint16_t i = 0; i < num; i++) {
        if (local[i].eos) {
            continue;
        }
        while (true) {
            esp_media_frame_t frame = {
                .type = local[i].type,
                .track_id = local[i].id,
            };
            esp_err_t acq = esp_media_provider_acquire_frame(provider, &frame, 0);
            if (acq == ESP_ERR_TIMEOUT || acq == ESP_ERR_NOT_FOUND) {
                break;
            }
            if (acq != ESP_OK) {
                /* write_abort during seek is transient */
                if (!during_seek) {
                    ESP_LOGW(TAG, "acquire id:%u ret:%s", (unsigned)local[i].id, esp_err_to_name(acq));
                    tracks_mark_eos(tk, local[i].id);
                }
                break;
            }

            got = true;
            if (frame.type == ESP_MEDIA_TRACK_TYPE_AUDIO) {
                (*audio_n)++;
            } else if (frame.type == ESP_MEDIA_TRACK_TYPE_VIDEO) {
                (*video_n)++;
            }
            const bool eos = (frame.flags & ESP_MEDIA_FRAME_FLAG_EOS) != 0;
            if (eos) {
                tracks_mark_eos(tk, local[i].id);
            }
            log_frame(&frame, *audio_n, *video_n);
            esp_media_provider_release_frame(provider, &frame);
            if (eos) {
                break;
            }
        }
    }
    return got;
}

/**
 * @brief  Provider consume loop — two clear modes:
 *           - repeat_n == 0 : read, seek once after SEEK_AFTER_MS, continue to EOS/duration
 *           - repeat_n  > 0 : auto_loop; on each service EOS clear track EOS; stop at N
 */
static esp_err_t consume_provider(esp_extractor_service_t *src, esp_media_provider_t *provider,
                                  demo_t *demo, uint32_t duration_ms, uint32_t track_wait_ms)
{
    esp_err_t ret = ESP_OK;
    tracks_t tk = {0};
    tk.lock = xSemaphoreCreateMutex();
    tk.ready = xEventGroupCreate();
    if (!tk.lock || !tk.ready) {
        ret = ESP_ERR_NO_MEM;
        goto _exit;
    }
    ret = esp_media_provider_set_event_cb(provider, on_track_event, &tk);
    if (ret != ESP_OK) {
        goto _exit;
    }
    // Sync for tracks added after provider is created
    tracks_sync(&tk, provider);
    if (tk.num == 0) {
        // Wait for tracks to be added
        EventBits_t bits = xEventGroupWaitBits(tk.ready, BIT_TRACK_READY, pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(track_wait_ms));
        if ((bits & BIT_TRACK_READY) == 0 && tk.num == 0) {
            ESP_LOGE(TAG, "wait tracks timeout");
            (void)esp_media_provider_set_event_cb(provider, NULL, NULL);
            ret = ESP_ERR_TIMEOUT;
            goto _exit;
        }
    }

    const bool do_seek = (demo->repeat_n == 0);
    ESP_LOGI(TAG, "tracks:%u duration_ms:%u %s=%u",
             (unsigned)tk.num, (unsigned)duration_ms,
             do_seek ? "seek_pos_ms" : "repeat",
             do_seek ? (unsigned)EXTRACTOR_SEEK_POSITION_MS : (unsigned)demo->repeat_n);

    const int64_t start_us = esp_timer_get_time();
    const int64_t deadline_us = start_us + (int64_t)duration_ms * 1000;
    const int64_t seek_at_us = start_us + (int64_t)EXTRACTOR_SEEK_AFTER_MS * 1000;
    bool seeking = false;
    bool seeked = false;
    uint32_t audio_n = 0;
    uint32_t video_n = 0;

    while (!demo->failed && esp_timer_get_time() < deadline_us) {
        /* ---- stop conditions ---- */
        if (demo->repeat_n > 0 && demo->eos_n >= demo->repeat_n) {
            break;
        }
        if (demo->repeat_n == 0 && !seeking && tracks_all_eos(&tk)) {
            break;
        }
        if (tk.aborted && !seeking) {
            ret = ESP_FAIL;
            break;
        }

        // Handle repeat received event
        if (demo->repeat_n > 0) {
            EventBits_t eb = xEventGroupWaitBits(demo->ev, BIT_EOS, pdTRUE, pdFALSE, 0);
            if ((eb & BIT_EOS) && demo->eos_n < demo->repeat_n) {
                ESP_LOGI(TAG, "EOS %u/%u — clear track eos, continue",
                         (unsigned)demo->eos_n, (unsigned)demo->repeat_n);
                tracks_clear_eos(&tk);
            }
        }

        // Do seek
        if (do_seek && !seeking && !seeked && esp_timer_get_time() >= seek_at_us) {
            ESP_LOGI(TAG, "seek -> %u ms", (unsigned)EXTRACTOR_SEEK_POSITION_MS);
            xEventGroupClearBits(demo->ev, BIT_SEEK_OK | BIT_SEEK_ERR);
            ret = esp_extractor_service_seek(src, EXTRACTOR_SEEK_POSITION_MS);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "seek request: %s", esp_err_to_name(ret));
                break;
            }
            seeking = true;
        }
        if (seeking) {
            EventBits_t sb = xEventGroupWaitBits(demo->ev, BIT_SEEK_OK | BIT_SEEK_ERR, pdTRUE, pdFALSE, 0);
            if (sb & BIT_SEEK_ERR) {
                ret = ESP_FAIL;
                break;
            }
            if (sb & BIT_SEEK_OK) {
                ESP_LOGI(TAG, "seek done");
                tracks_clear_eos(&tk);
                seeking = false;
                seeked = true;
            }
        }

        // Get all frames in IDLE interval
        if (!pull_frames(provider, &tk, seeking, &audio_n, &video_n)) {
            vTaskDelay(pdMS_TO_TICKS(IDLE_MS));
        }
    }

    ESP_LOGI(TAG, "done a/v:%" PRIu32 "/%" PRIu32 " eos:%u/%u failed:%d",
             audio_n, video_n, (unsigned)demo->eos_n, (unsigned)demo->repeat_n, (int)demo->failed);

    (void)esp_media_provider_set_event_cb(provider, NULL, NULL);
_exit:
    if (tk.lock) {
        vSemaphoreDelete(tk.lock);
        tk.lock = NULL;
    }
    if (tk.ready) {
        vEventGroupDelete(tk.ready);
        tk.ready = NULL;
    }
    return demo->failed ? ESP_FAIL : ret;
}

static esp_err_t create_extractor(esp_extractor_service_t **out, const char *url)
{
    /* Separate name from MCP extractor so event hubs are not shared. */
    esp_extractor_service_cfg_t cfg = ESP_EXTRACTOR_SERVICE_CFG_DEFAULT();
    cfg.name = "extractor_demo";
    ESP_RETURN_ON_ERROR(esp_extractor_service_create(&cfg, out), TAG, "create");
    ESP_RETURN_ON_ERROR(esp_extractor_service_set_extract_mask(*out, ESP_EXTRACT_MASK_AV), TAG, "mask");
    ESP_RETURN_ON_ERROR(esp_extractor_service_set_out_pool_size(*out, EXTRACTOR_OUT_POOL_SIZE), TAG, "pool");
    ESP_RETURN_ON_ERROR(esp_extractor_service_set_url(*out, url), TAG, "url");
    return ESP_OK;
}

static void destroy_extractor(esp_extractor_service_t *svc)
{
    if (svc == NULL) {
        return;
    }
    esp_service_t *base = ESP_SERVICE_BASE(svc);
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    if (esp_service_get_state(base, &state) == ESP_OK && state == ESP_SERVICE_STATE_RUNNING) {
        (void)esp_service_stop(base);
    }
    (void)esp_service_event_unsubscribe(base, NULL, ADF_EVENT_ANY_ID);
    (void)esp_service_set_user_data(base, NULL);
    (void)esp_media_service_deinit(base);
    free(svc);
}

static esp_err_t run_link(esp_extractor_service_t *src, uint32_t duration_ms, uint32_t repeat_n)
{
    esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
    sink_cfg.role = ESP_MEDIA_ROLE_SINK;
    sink_cfg.max_stream_num = 1;
    sink_cfg.name = "extractor_demo_sink";

    esp_media_dummy_service_t *sink = NULL;
    demo_t demo;
    demo_init(&demo, repeat_n);

    esp_err_t ret = esp_media_dummy_service_create(&sink_cfg, &sink);
    if (ret == ESP_OK) {
        ret = demo_subscribe(ESP_SERVICE_BASE(src), &demo);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                     ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(sink));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(src));
    }
    if (ret == ESP_OK) {
        ret = demo_wait(&demo, duration_ms);
    }

    (void)esp_service_stop(ESP_SERVICE_BASE(src));
    if (sink != NULL) {
        (void)esp_service_stop(ESP_SERVICE_BASE(sink));
        esp_media_dummy_stream_stats_t stats = {0};
        (void)esp_media_dummy_service_get_stats(sink, ESP_MEDIA_DEFAULT_STREAM, &stats);
        ESP_LOGI(TAG, "link stats a:%" PRIu32 "/%" PRIu32 " v:%" PRIu32 "/%" PRIu32,
                 stats.audio_frame_count, stats.audio_byte_count,
                 stats.video_frame_count, stats.video_byte_count);
        (void)esp_media_service_unlink(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM,
                                       ESP_SERVICE_BASE(sink), ESP_MEDIA_DEFAULT_STREAM);
        (void)esp_media_dummy_service_destroy(sink);
    }
    (void)esp_service_event_unsubscribe(ESP_SERVICE_BASE(src), NULL, ADF_EVENT_ANY_ID);
    (void)esp_service_set_user_data(ESP_SERVICE_BASE(src), NULL);
    demo_deinit(&demo);
    return ret;
}

static esp_err_t run_provider(esp_extractor_service_t *src, const char *url,
                              uint32_t duration_ms, uint32_t repeat_n)
{
    uint32_t wait_ms = url_needs_network(url) ? TRACK_WAIT_NETWORK_MS : TRACK_WAIT_FILE_MS;
    demo_t demo;
    demo_init(&demo, repeat_n);

    esp_err_t ret = demo_subscribe(ESP_SERVICE_BASE(src), &demo);
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(src));
    }
    if (ret == ESP_OK) {
        esp_media_provider_t provider = {0};
        ret = esp_media_service_get_provider(ESP_SERVICE_BASE(src), ESP_MEDIA_DEFAULT_STREAM, &provider);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "get_provider: %s", esp_err_to_name(ret));
        } else {
            ret = consume_provider(src, &provider, &demo, duration_ms, wait_ms);
        }
    }

    (void)esp_service_stop(ESP_SERVICE_BASE(src));
    (void)esp_service_event_unsubscribe(ESP_SERVICE_BASE(src), NULL, ADF_EVENT_ANY_ID);
    (void)esp_service_set_user_data(ESP_SERVICE_BASE(src), NULL);
    demo_deinit(&demo);
    return ret;
}

esp_err_t extractor_demo_run(const char *url, uint32_t duration_ms, extractor_demo_mode_t mode)
{
    ESP_RETURN_ON_FALSE(url && duration_ms > 0, ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_LOGI(TAG, "run mode:%d url:%s duration_ms:%u", (int)mode, url, (unsigned)duration_ms);

    esp_extractor_service_t *src = NULL;
    esp_err_t ret = create_extractor(&src, url);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = (mode == EXTRACTOR_DEMO_MODE_LINK_DUMMY_SINK)
              ? run_link(src, duration_ms, 0)
              : run_provider(src, url, duration_ms, 0);
    destroy_extractor(src);
    return ret;
}

esp_err_t extractor_demo_run_repeat(const char *url, uint32_t duration_ms,
                                    extractor_demo_mode_t mode, uint32_t repeat_count)
{
    ESP_RETURN_ON_FALSE(url && duration_ms > 0 && repeat_count > 0,
                        ESP_ERR_INVALID_ARG, TAG, "bad args");
    ESP_LOGI(TAG, "run_repeat mode:%d url:%s duration_ms:%u repeat:%u",
             (int)mode, url, (unsigned)duration_ms, (unsigned)repeat_count);

    esp_extractor_service_t *src = NULL;
    esp_err_t ret = create_extractor(&src, url);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_extractor_service_set_auto_loop(src, true);
    if (ret != ESP_OK) {
        destroy_extractor(src);
        return ret;
    }
    ret = (mode == EXTRACTOR_DEMO_MODE_LINK_DUMMY_SINK)
              ? run_link(src, duration_ms, repeat_count)
              : run_provider(src, url, duration_ms, repeat_count);
    destroy_extractor(src);
    return ret;
}
