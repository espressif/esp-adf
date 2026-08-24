/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_capture_sink.h"
#if CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY
#include "esp_capture_text_overlay.h"
#endif  /* CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY */
#include "esp_log.h"
#include "esp_service.h"
#include "esp_service_scheduler.h"

#include "capture_service_err.h"
#include "esp_video_capture_service_priv.h"

static const char *TAG = "VIDEO_CAPTURE_OVERLAY";

#define OVERLAY_REDRAW_INTERVAL_MS   900
#define OVERLAY_REDRAW_STOP_WAIT_MS  1000

typedef struct video_capture_overlay_node {
    esp_capture_service_t                   *capture;
    esp_capture_overlay_if_t                *overlay;
    esp_video_capture_service_overlay_cfg_t  cfg;
    esp_capture_rgn_t                        rgn;
    uint16_t                                 font_size;
    QueueHandle_t                            ctrl_queue;
    TaskHandle_t                             task;
    volatile bool                            task_running;
    struct video_capture_overlay_node       *next;
} video_capture_overlay_node_t;

static video_capture_overlay_node_t *s_overlay_list;

#if CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY
static uint16_t prefer_font_size(uint16_t width, uint16_t height)
{
    /* Sized for timestamp like "YYYY-MM-DD HH:MM:SS" (~19 glyphs) on real capture. */
    uint16_t short_edge = width < height ? width : height;
    if (short_edge >= 720) {
        return 32;
    }
    if (short_edge >= 480) {
        return 24;
    }
    if (short_edge >= 360) {
        return 24;
    }
    return 16;
}

static bool font_size_exists(uint16_t font_size)
{
    switch (font_size) {
#if CONFIG_ESP_PAINTER_BASIC_FONT_12
        case 12:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_12 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_16
        case 16:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_16 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_20
        case 20:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_20 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_24
        case 24:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_24 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_28
        case 28:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_28 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_32
        case 32:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_32 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_36
        case 36:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_36 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_40
        case 40:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_40 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_44
        case 44:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_44 */
#if CONFIG_ESP_PAINTER_BASIC_FONT_48
        case 48:
            return true;
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_48 */
        default:
            return false;
    }
}

static esp_err_t get_default_font_size(uint16_t *font_size)
{
    /* Prefer 24 for timestamp readability; then nearest compiled-in sizes. */
    static const uint16_t candidates[] = {24, 28, 20, 32, 16, 36, 12, 40, 44, 48};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (font_size_exists(candidates[i])) {
            *font_size = candidates[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t resolve_font_size(uint16_t preferred, uint16_t *font_size)
{
    if (font_size_exists(preferred)) {
        *font_size = preferred;
        return ESP_OK;
    }
    esp_err_t ret = get_default_font_size(font_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No text overlay font size is enabled in menuconfig");
        return ret;
    }
    ESP_LOGW(TAG, "Font size %u not compiled in, fallback to %u", preferred, *font_size);
    return ESP_OK;
}

static uint16_t text_width_limit(uint16_t width, uint16_t font_size)
{
    uint16_t max_width = width > 32 ? width - 32 : width;
    uint16_t preferred = font_size * 24;
    return preferred < max_width ? preferred : max_width;
}

static esp_err_t calc_overlay_region(const esp_media_video_info_t *video,
                                     uint8_t line_num, esp_capture_rgn_t *rgn, uint16_t *font_size)
{
    esp_err_t ret = resolve_font_size(prefer_font_size(video->width, video->height), font_size);
    if (ret != ESP_OK) {
        return ret;
    }
    uint16_t pad = *font_size / 2;
    if (pad < 4) {
        pad = 4;
    }
    rgn->x = pad;
    rgn->y = pad;
    rgn->width = text_width_limit(video->width, *font_size);
    rgn->height = (uint16_t)(pad * 2 + *font_size * line_num);
    if (rgn->width > video->width) {
        rgn->width = video->width;
    }
    if (rgn->height > video->height) {
        rgn->height = video->height;
    }
    return ESP_OK;
}

static const esp_video_capture_service_stream_cfg_t *find_overlay_stream(const esp_video_capture_service_setup_t *cfg,
                                                                         uint16_t *out_stream_idx)
{
    for (uint16_t i = 0; i < cfg->stream_num; i++) {
        const esp_video_capture_service_stream_cfg_t *stream = &cfg->streams[i];
        if (stream->enabled && stream->video_info.codec != 0 &&
            stream->video_info.width > 0 && stream->video_info.height > 0) {
            *out_stream_idx = i;
            return stream;
        }
    }
    return NULL;
}

static uint8_t overlay_line_num(const esp_video_capture_service_overlay_cfg_t *cfg)
{
    uint8_t lines = 0;
    if (cfg->show_camera_type) {
        lines++;
    }
    if (cfg->show_datetime) {
        lines++;
    }
    return lines;
}

static esp_err_t draw_overlay_text(esp_capture_overlay_if_t *overlay,
                                   const esp_video_capture_service_overlay_cfg_t *cfg,
                                   const esp_capture_rgn_t *rgn,
                                   uint16_t font_size)
{
    esp_capture_rgn_t clear_rgn = {
        .x = 0,
        .y = 0,
        .width = rgn->width,
        .height = rgn->height,
    };
    const uint8_t trans_rgb[] = {0, 255, 0};
    esp_err_t ret = capture_err_to_esp(overlay->set_trans_color(overlay, trans_rgb));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = capture_err_to_esp(esp_capture_text_overlay_draw_start(overlay));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = capture_err_to_esp(esp_capture_text_overlay_clear(overlay, &clear_rgn, COLOR_RGB565_GREEN));
    if (ret == ESP_OK) {
        esp_capture_text_overlay_draw_info_t font_info = {
            .color = COLOR_RGB565_WHITE,
            .font_size = font_size,
            .x = 0,
            .y = 0,
        };
        const char *camera_type = cfg->camera_type ? cfg->camera_type : "Espressif";
        if (cfg->show_camera_type) {
            ret = capture_err_to_esp(esp_capture_text_overlay_draw_text_fmt(overlay, &font_info,
                                                                            "%s", camera_type));
            font_info.y += font_size;
        }
        if (ret == ESP_OK && cfg->show_datetime) {
            struct timeval tv = {0};
            struct tm timeinfo = {0};
            gettimeofday(&tv, NULL);
            localtime_r(&tv.tv_sec, &timeinfo);
            ret = capture_err_to_esp(esp_capture_text_overlay_draw_text_fmt(overlay, &font_info,
                                                                            "%04d-%02d-%02d %02d:%02d:%02d",
                                                                            timeinfo.tm_year + 1900,
                                                                            timeinfo.tm_mon + 1,
                                                                            timeinfo.tm_mday,
                                                                            timeinfo.tm_hour,
                                                                            timeinfo.tm_min,
                                                                            timeinfo.tm_sec));
        }
    }
    esp_err_t finish_ret = capture_err_to_esp(esp_capture_text_overlay_draw_finished(overlay));
    return ret == ESP_OK ? finish_ret : ret;
}

static bool overlay_capture_running(esp_capture_service_t *capture)
{
    esp_service_state_t state = ESP_SERVICE_STATE_UNINITIALIZED;
    return capture != NULL &&
           esp_service_get_state(ESP_SERVICE_BASE(capture), &state) == ESP_OK &&
           state == ESP_SERVICE_STATE_RUNNING;
}

static void overlay_redraw_task(void *arg)
{
    video_capture_overlay_node_t *node = (video_capture_overlay_node_t *)arg;
    node->task_running = true;
    uint8_t cmd = 0;
    while (xQueueReceive(node->ctrl_queue, &cmd, pdMS_TO_TICKS(OVERLAY_REDRAW_INTERVAL_MS)) != pdTRUE) {
        if (!overlay_capture_running(node->capture)) {
            continue;
        }
        esp_err_t ret = draw_overlay_text(node->overlay, &node->cfg, &node->rgn, node->font_size);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Overlay redraw failed: %s", esp_err_to_name(ret));
        }
    }
    node->task_running = false;
    vTaskDelete(NULL);
}

static esp_err_t start_overlay_redraw(video_capture_overlay_node_t *node)
{
    if (node == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!node->cfg.show_datetime || node->task != NULL) {
        return ESP_OK;
    }
    node->ctrl_queue = xQueueCreate(1, sizeof(uint8_t));
    if (node->ctrl_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    const char *service_name = NULL;
    (void)esp_service_get_name(ESP_SERVICE_BASE(node->capture), &service_name);
    esp_service_thread_cfg_t default_cfg = {
        .stack_size = 4 * 1024,
        .priority = 5,
        .core_id = ESP_SERVICE_THREAD_CORE_NO_AFFINITY,
    };
    esp_service_thread_cfg_t task_cfg = default_cfg;
    esp_service_thread_request_t request = {
        .service_name = service_name ? service_name : ESP_VIDEO_CAPTURE_SERVICE_NAME,
        .thread_name = ESP_VIDEO_CAPTURE_TASK_OVL_REDRAW,
    };
    esp_err_t ret = esp_service_scheduler_get_thread_cfg(&request, &default_cfg, &task_cfg);
    if (ret != ESP_OK) {
        vQueueDelete(node->ctrl_queue);
        node->ctrl_queue = NULL;
        return ret;
    }
    node->task_running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(overlay_redraw_task,
                                            ESP_VIDEO_CAPTURE_TASK_OVL_REDRAW,
                                            task_cfg.stack_size,
                                            node,
                                            task_cfg.priority,
                                            &node->task,
                                            task_cfg.core_id < 0 ? tskNO_AFFINITY : task_cfg.core_id);
    if (ok != pdPASS) {
        node->task_running = false;
        vQueueDelete(node->ctrl_queue);
        node->ctrl_queue = NULL;
        node->task = NULL;
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t stop_overlay_redraw(video_capture_overlay_node_t *node)
{
    if (node == NULL || node->task == NULL) {
        return ESP_OK;
    }
    uint8_t cmd = 1;
    xQueueOverwrite(node->ctrl_queue, &cmd);
    uint16_t wait_ms = 0;
    while (node->task_running && wait_ms < OVERLAY_REDRAW_STOP_WAIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_ms += 10;
    }
    if (node->task_running) {
        ESP_LOGE(TAG, "Overlay stop timeout");
        return ESP_ERR_TIMEOUT;
    }
    node->task = NULL;
    if (node->ctrl_queue != NULL) {
        vQueueDelete(node->ctrl_queue);
        node->ctrl_queue = NULL;
    }
    return ESP_OK;
}

static esp_err_t register_overlay(esp_capture_service_t *capture,
                                  esp_capture_overlay_if_t *overlay,
                                  const esp_video_capture_service_overlay_cfg_t *cfg,
                                  const esp_capture_rgn_t *rgn,
                                  uint16_t font_size)
{
    video_capture_overlay_node_t *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return ESP_ERR_NO_MEM;
    }
    node->capture = capture;
    node->overlay = overlay;
    node->cfg = *cfg;
    node->rgn = *rgn;
    node->font_size = font_size;
    node->next = s_overlay_list;
    s_overlay_list = node;
    return ESP_OK;
}

static esp_err_t overlay_only_deinit_cb(esp_capture_service_t *capture, void *user_data)
{
    (void)user_data;
    return esp_video_capture_service_cleanup_overlays(capture);
}

esp_err_t esp_video_capture_service_apply_overlay(esp_capture_service_t *capture,
                                                  const esp_video_capture_service_setup_t *cfg)
{
    if (capture == NULL || cfg == NULL || !cfg->overlay.enabled) {
        return ESP_OK;
    }
    uint8_t line_num = overlay_line_num(&cfg->overlay);
    if (line_num == 0) {
        return ESP_OK;
    }

    uint16_t stream_idx = 0;
    const esp_video_capture_service_stream_cfg_t *stream_cfg = find_overlay_stream(cfg, &stream_idx);
    if (stream_cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_capture_rgn_t rgn = {0};
    uint16_t font_size = 0;
    esp_err_t ret = calc_overlay_region(&stream_cfg->video_info, line_num, &rgn, &font_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to select overlay font ret=%s", esp_err_to_name(ret));
        return ret;
    }
    esp_capture_overlay_if_t *overlay = esp_capture_new_text_overlay(&rgn);
    if (overlay == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ret = capture_err_to_esp(overlay->open(overlay));
    if (ret == ESP_OK) {
        ret = draw_overlay_text(overlay, &cfg->overlay, &rgn, font_size);
    }
    if (ret == ESP_OK) {
        esp_capture_sink_handle_t sink = NULL;
        ret = esp_capture_service_get_sink_handle(capture, stream_idx, &sink);
        if (ret == ESP_OK) {
            ret = capture_err_to_esp(esp_capture_sink_add_overlay(sink, overlay));
        }
        if (ret == ESP_OK) {
            ret = capture_err_to_esp(esp_capture_sink_enable_overlay(sink, true));
        }
    }
    if (ret == ESP_OK) {
        ret = register_overlay(capture, overlay, &cfg->overlay, &rgn, font_size);
    }
    if (ret == ESP_OK && esp_video_capture_service_ctx_find(capture) == NULL) {
        /* Raw esp_capture_service_create path has no video deinit_cb; ensure destroy frees overlays. */
        (void)esp_capture_service_set_deinit_cb(capture, overlay_only_deinit_cb, NULL);
    }
    if (ret != ESP_OK) {
        if (overlay->close != NULL) {
            overlay->close(overlay);
        }
        free(overlay);
        ESP_LOGE(TAG, "Failed to apply overlay ret=%s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t esp_video_capture_service_overlay_enable_redraw(esp_capture_service_t *capture, bool enable)
{
    if (capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid capture");
    }
    bool found = false;
    esp_err_t ret = ESP_OK;
    for (video_capture_overlay_node_t *node = s_overlay_list; node != NULL; node = node->next) {
        if (node->capture != capture) {
            continue;
        }
        found = true;
        esp_err_t op_ret = enable ? start_overlay_redraw(node) : stop_overlay_redraw(node);
        if (op_ret != ESP_OK && ret == ESP_OK) {
            ret = op_ret;
        }
    }
    if (!found && enable) {
        RET_FOR(ESP_ERR_NOT_FOUND, "Overlay not found");
    }
    if (ret != ESP_OK) {
        RET_FOR(ret, "Failed to %s redraw", enable ? "enable" : "disable");
    }
    return ESP_OK;
}
#else
esp_err_t esp_video_capture_service_apply_overlay(esp_capture_service_t *capture,
                                                  const esp_video_capture_service_setup_t *cfg)
{
    (void)capture;
    return (cfg != NULL && cfg->overlay.enabled) ? ESP_ERR_NOT_SUPPORTED : ESP_OK;
}

esp_err_t esp_video_capture_service_overlay_enable_redraw(esp_capture_service_t *capture, bool enable)
{
    if (capture == NULL) {
        RET_FOR(ESP_ERR_INVALID_ARG, "Invalid capture");
    }
    if (enable) {
        RET_FOR(ESP_ERR_NOT_SUPPORTED, "Overlay support disabled");
    }
    return ESP_OK;
}
#endif  /* CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY */

esp_err_t esp_video_capture_service_cleanup_overlays(esp_capture_service_t *capture)
{
    if (capture == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)esp_video_capture_service_overlay_enable_redraw(capture, false);
    video_capture_overlay_node_t **cur = &s_overlay_list;
    while (*cur != NULL) {
        video_capture_overlay_node_t *node = *cur;
        if (node->capture != capture) {
            cur = &node->next;
            continue;
        }
        *cur = node->next;
        if (node->overlay != NULL) {
            if (node->overlay->close != NULL) {
                node->overlay->close(node->overlay);
            }
            free(node->overlay);
        }
        free(node);
    }
    return ESP_OK;
}
