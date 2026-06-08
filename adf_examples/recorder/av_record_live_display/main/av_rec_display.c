/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_gmf_err.h"
#include "av_rec_config.h"

static const char *TAG = "AV_REC_DISPLAY";

typedef struct {
    av_record_live_display_sys_t *sys;
    int                           duration_ms;
    TaskHandle_t                  done_task;
    esp_err_t                     result;
} display_task_arg_t;

static esp_err_t run_display_loop(av_record_live_display_sys_t *sys, int duration_ms)
{
    int64_t start_ms = esp_timer_get_time() / 1000;
    int64_t now_ms = start_ms;
    int64_t last_report_ms = start_ms;
    uint32_t frame_count = 0;
    uint32_t last_report_frame_count = 0;
    uint32_t expected_frame_size = sys->display_info.width * sys->display_info.height * 2;
    esp_capture_stream_frame_t frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
    };

    ESP_LOGI(TAG, "Live display loop started");
    while (now_ms < start_ms + duration_ms) {
        esp_capture_err_t ret = esp_capture_sink_acquire_frame(sys->display_sink, &frame, false);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGW(TAG, "Failed to acquire display frame, ret=%d", ret);
            now_ms = esp_timer_get_time() / 1000;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        ESP_GMF_CHECK(TAG, (uint32_t)frame.size == expected_frame_size, {
            esp_capture_sink_release_frame(sys->display_sink, &frame);
            return ESP_FAIL;
        }, "Unexpected display frame size");
        esp_err_t draw_ret = esp_lcd_panel_draw_bitmap(sys->lcd_handles->panel_handle, 0, 0,
                                                       sys->display_info.width, sys->display_info.height,
                                                       frame.data);
        if (draw_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to draw display frame");
        } else if (sys->display_done_sem) {
            /* Only wait for completion semaphore when the draw actually succeeded */
            xSemaphoreTake(sys->display_done_sem, pdMS_TO_TICKS(1000));
        }
        uint32_t saved_pts = frame.pts;
        esp_capture_sink_release_frame(sys->display_sink, &frame);
        frame_count++;

        now_ms = esp_timer_get_time() / 1000;
        if (now_ms > last_report_ms + 1000) {
            int64_t interval_ms = now_ms - last_report_ms;
            uint32_t interval_frames = frame_count - last_report_frame_count;
            float fps = interval_ms ? (interval_frames * 1000.0f / interval_ms) : 0.0f;
            ESP_LOGI(TAG, "Display fps=%.2f, frames=%" PRIu32 ", pts=%" PRIu32,
                     fps, frame_count, saved_pts);
            last_report_ms = now_ms;
            last_report_frame_count = frame_count;
        }
    }
    ESP_LOGI(TAG, "Display loop finished, frames=%" PRIu32 ", bad_frames=0", frame_count);
    ESP_GMF_CHECK(TAG, frame_count > 0, return ESP_FAIL, "Invalid display result, frames=0");
    return ESP_OK;
}

static void display_task(void *arg)
{
    display_task_arg_t *task_arg = (display_task_arg_t *)arg;
    task_arg->result = run_display_loop(task_arg->sys, task_arg->duration_ms);
    xTaskNotifyGive(task_arg->done_task);
    vTaskDelete(NULL);
}

esp_err_t av_rec_run_display(av_record_live_display_sys_t *sys, int duration_ms)
{
    ESP_GMF_CHECK(TAG, duration_ms > 0, return ESP_ERR_INVALID_ARG, "Invalid display duration");
    display_task_arg_t display_arg = {
        .sys = sys,
        .duration_ms = duration_ms,
        .done_task = xTaskGetCurrentTaskHandle(),
        .result = ESP_FAIL,
    };
    BaseType_t task_ret = xTaskCreatePinnedToCore(display_task, "display", 6 * 1024,
                                                  &display_arg, 5, NULL, DISPLAY_CORE_ID);
    ESP_GMF_CHECK(TAG, task_ret == pdPASS, return ESP_FAIL, "Failed to create display task");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return display_arg.result;
}
