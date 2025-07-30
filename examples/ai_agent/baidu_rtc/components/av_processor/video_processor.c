/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_oal_mem.h"
#include "esp_video_dec.h"
#include "esp_video_dec_reg.h"
#include "esp_video_codec_utils.h"
#include "esp_video_dec_default.h"
#include "esp_video_codec_version.h"
#include "esp_video_codec_types.h"
#include "esp_capture.h"
#include "esp_capture_defaults.h"

#include "video_processor.h"

static const char *TAG = "VIDEO_PROCESSOR";

#define VIDEO_RENDER_QUEUE_DEFAULT_SIZE (5)
#define _EVENT_GROUP_STOP_BIT           (1 << 0)

typedef enum {
    VID_PROCESSOR_CMD_STOP,
    VID_PROCESSOR_CMD_DATA,
    VID_PROCESSOR_CMD_MAX,
} video_processor_cmd_t;

typedef struct {
    video_processor_cmd_t  cmd;
    video_frame_t          frame;
} video_processor_event_t;

/**
 * @brief  Video render internal structure
 */
typedef struct {
    video_render_config_t   config;       /*!< Render configuration */
    esp_gmf_oal_thread_t    vdec_thread;  /*!< Decoder thread handle */
    QueueHandle_t           frame_queue;  /*!< Frame queue handle */
    EventGroupHandle_t      event_group;  /*!< Event group handle */
    video_frame_t           out_frame;    /*!< Output frame buffer */
    void                   *ctx;          /*!< User context */
    esp_video_dec_handle_t  dec_handle;   /*!< Decoder handle */
    bool                    is_running;   /*!< Running state flag */
} video_render_t;

/**
 * @brief  Video capture internal structure
 */
typedef struct {
    video_capture_config_t       config;                      /*!< Capture configuration */
    esp_capture_handle_t         capture;                     /*!< Capture handle */
    esp_capture_audio_src_if_t  *aud_src;                     /*!< Audio source interface */
    esp_capture_video_src_if_t  *vid_src;                     /*!< Video source interface */
    esp_capture_sink_handle_t    sink[DEFAULT_VID_MAX_PATH];  /*!< Sink handles */
    esp_gmf_oal_thread_t         capture_thread;              /*!< Capture thread handle */
    EventGroupHandle_t           event_group;                 /*!< Event group handle */
    QueueHandle_t                frame_queue;                 /*!< Frame queue handle */
    bool                         is_running;                  /*!< Running state flag */
} video_capture_t;

static esp_capture_video_src_if_t *create_video_source(void *cam_config)
{
#if CONFIG_IDF_TARGET_ESP32S3
    return esp_capture_new_video_dvp_src((esp_capture_video_dvp_src_cfg_t *)cam_config);
#else
    ESP_LOGE(TAG, "Video source not supported for this target");
    return NULL;
#endif  /* CONFIG_IDF_TARGET_ESP32S3 */
}

static void decoder_worker_thread(void *arg)
{
    video_render_t *render = (video_render_t *)arg;
    if (!render) {
        ESP_LOGE(TAG, "Invalid render handle");
        esp_gmf_oal_thread_delete(NULL);
    }

    if (!render->out_frame.data) {
        ESP_LOGE(TAG, "Failed to allocate memory for output frame");
        esp_gmf_oal_thread_delete(NULL);
    }

    render->is_running = true;
    video_processor_event_t evt_frame = {0};

    while (render->is_running) {
        if (xQueueReceive(render->frame_queue, &evt_frame, portMAX_DELAY) == pdPASS) {
            ESP_LOGD(TAG, "vdec_task: %d", evt_frame.cmd);

            switch (evt_frame.cmd) {
                case VID_PROCESSOR_CMD_DATA:
                    ESP_LOGD(TAG, "Processing video frame, size: %zu", evt_frame.frame.size);
                    esp_video_dec_in_frame_t in_frame = {
                        .pts = 0,
                        .data = evt_frame.frame.data,
                        .size = evt_frame.frame.size,
                    };

                    esp_video_dec_out_frame_t out_frame = {
                        .data = render->out_frame.data,
                        .size = render->out_frame.size,
                    };

                    int ret = esp_video_dec_process(render->dec_handle, &in_frame, &out_frame);
                    if (ret != ESP_VC_ERR_OK) {
                        ESP_LOGE(TAG, "Failed to decode frame. ret: %d", ret);
                        break;
                    }

                    if (render->config.decode_cb) {
                        render->config.decode_cb(render->config.decode_cb_ctx, out_frame.data, out_frame.size);
                    }
                    break;

                case VID_PROCESSOR_CMD_STOP:
                    ESP_LOGI(TAG, "Received stop command, exiting decoder thread");
                    render->is_running = false;
                    break;

                default:
                    ESP_LOGE(TAG, "Unknown command: %d", evt_frame.cmd);
                    break;
            }

            if (evt_frame.frame.data) {
                esp_gmf_oal_free(evt_frame.frame.data);
                evt_frame.frame.data = NULL;
            }
        }
    }
    xEventGroupClearBits(render->event_group, _EVENT_GROUP_STOP_BIT);
    ESP_LOGI(TAG, "Decoder thread exiting");
    vTaskDelete(NULL);
}

static void capture_worker_thread(void *arg)
{
    video_capture_t *capture = (video_capture_t *)arg;
    if (!capture) {
        ESP_LOGE(TAG, "Invalid capture handle");
        esp_gmf_oal_thread_delete(NULL);
    }

    for (int i = 0; i < capture->config.sink_num; i++) {
        esp_capture_sink_enable(capture->sink[i], ESP_CAPTURE_RUN_MODE_ALWAYS);
    }

    capture->is_running = true;
    ESP_LOGI(TAG, "Capture thread started");

    while (capture->is_running) {
        for (int i = 0; i < capture->config.sink_num; i++) {
            esp_capture_stream_frame_t frame = {0};
            frame.stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO;
            // Acquire frame from sink
            esp_err_t ret = esp_capture_sink_acquire_frame(capture->sink[i], &frame, true);
            if (ret != ESP_CAPTURE_ERR_OK) {
                ESP_LOGW(TAG, "Failed to acquire frame from sink %d", i);
                vTaskDelay(pdMS_TO_TICKS(1000 / capture->config.sink_cfg[i].video_info.fps));
                continue;
            }

            if (frame.stream_type == ESP_CAPTURE_STREAM_TYPE_VIDEO && frame.data && frame.size > 0) {
                if (capture->config.capture_frame_cb) {
                    video_frame_t vid_frame = {
                        .data = frame.data,
                        .size = frame.size,
                    };
                    capture->config.capture_frame_cb(capture->config.capture_frame_cb_ctx, i, &vid_frame);
                }
            }

            esp_capture_sink_release_frame(capture->sink[i], &frame);
        }
    }

    ESP_LOGI(TAG, "Capture thread exiting");
    xEventGroupSetBits(capture->event_group, _EVENT_GROUP_STOP_BIT);
    esp_gmf_oal_thread_delete(NULL);
}

video_render_handle_t video_render_open(video_render_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return NULL;
    }

    esp_video_dec_register_default();

    video_render_t *render = esp_gmf_oal_calloc(1, sizeof(video_render_t));
    if (!render) {
        ESP_LOGE(TAG, "Failed to allocate memory for render");
        return NULL;
    }

    render->config.decode_cb_ctx = config->decode_cb_ctx;
    render->config.decode_cb = config->decode_cb;
    render->config.decode_cfg = config->decode_cfg;
    render->config.resolution.width = config->resolution.width;
    render->config.resolution.height = config->resolution.height;

    render->frame_queue = xQueueCreate(VIDEO_RENDER_QUEUE_DEFAULT_SIZE, sizeof(video_processor_event_t));
    if (!render->frame_queue) {
        goto __exit;
    }
    render->event_group = xEventGroupCreate();
    if (!render->event_group) {
        goto __exit;
    }

    ESP_LOGI(TAG, "Video render opened successfully");
    return render;

__exit:
    if (render->frame_queue) {
        vQueueDelete(render->frame_queue);
        render->frame_queue = NULL;
    }
    if (render->event_group) {
        vEventGroupDelete(render->event_group);
        render->event_group = NULL;
    }
    if (render) {
        esp_gmf_oal_free(render);
        render = NULL;
    }
    return NULL;
}

esp_err_t video_render_start(video_render_handle_t handle)
{
    video_render_t *render = (video_render_t *)handle;
    if (!render) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t actual_size = 0;
    render->out_frame.size = esp_video_codec_get_image_size(render->config.decode_cfg.out_fmt, &render->config.resolution);
    render->out_frame.data = esp_video_codec_align_alloc(128, render->out_frame.size, &actual_size);
    if (!render->out_frame.data) {
        ESP_LOGE(TAG, "Failed to allocate memory for output frame");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Output frame buffer allocated, size: %zu", render->out_frame.size);

    int ret = esp_video_dec_open(&render->config.decode_cfg, &render->dec_handle);
    if (!render->dec_handle || ret != ESP_VC_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open video decoder, error: %d", ret);
        esp_gmf_oal_free(render->out_frame.data);
        render->out_frame.data = NULL;
        return ESP_ERR_NO_MEM;
    }

    esp_gmf_oal_thread_create(&render->vdec_thread, "decoder_worker",
                              decoder_worker_thread, (void *)render,
                              4096, 12, true, 0);

    ESP_LOGI(TAG, "Video render started successfully");
    return ESP_OK;
}

esp_err_t video_render_frame_feed(video_render_handle_t handle, video_frame_t *vid_frame)
{
    video_render_t *render = (video_render_t *)handle;
    if (!render) {
        ESP_LOGE(TAG, "Invalid render handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (!vid_frame || !vid_frame->data || vid_frame->size == 0) {
        ESP_LOGE(TAG, "Invalid frame data");
        return ESP_ERR_INVALID_ARG;
    }

    video_processor_event_t evt_frame = {0};
    evt_frame.cmd = VID_PROCESSOR_CMD_DATA;
    evt_frame.frame.data = esp_gmf_oal_malloc(vid_frame->size);
    if (!evt_frame.frame.data) {
        ESP_LOGE(TAG, "Failed to allocate memory for frame data");
        return ESP_ERR_NO_MEM;
    }

    memcpy(evt_frame.frame.data, vid_frame->data, vid_frame->size);
    evt_frame.frame.size = vid_frame->size;

    if (xQueueSend(render->frame_queue, &evt_frame, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send frame to queue");
        esp_gmf_oal_free(evt_frame.frame.data);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t video_render_stop(video_render_handle_t handle)
{
    video_render_t *render = (video_render_t *)handle;
    if (!render) {
        ESP_LOGE(TAG, "Invalid render handle");
        return ESP_ERR_INVALID_ARG;
    }

    if (!render->is_running) {
        ESP_LOGW(TAG, "Video render is not running");
        return ESP_OK;
    }

    video_processor_event_t evt_frame = {0};
    evt_frame.cmd = VID_PROCESSOR_CMD_STOP;

    if (xQueueSend(render->frame_queue, &evt_frame, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send stop command to queue");
        return ESP_FAIL;
    }

    xEventGroupWaitBits(render->event_group, _EVENT_GROUP_STOP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "Video render stopped successfully");
    return ESP_OK;
}

void video_render_close(video_render_handle_t handle)
{
    video_render_t *render = (video_render_t *)handle;
    if (!render) {
        ESP_LOGE(TAG, "Invalid render handle");
        return;
    }

    if (render->is_running) {
        video_render_stop(handle);
    }

    if (render->dec_handle) {
        esp_video_dec_close(render->dec_handle);
        render->dec_handle = NULL;
    }

    if (render->out_frame.data) {
        esp_gmf_oal_free(render->out_frame.data);
        render->out_frame.data = NULL;
    }

    if (render->frame_queue) {
        vQueueDelete(render->frame_queue);
        render->frame_queue = NULL;
    }

    esp_gmf_oal_free(render);
    ESP_LOGI(TAG, "Video render closed successfully");
}

video_capture_handle_t video_capture_open(video_capture_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return NULL;
    }

    //  capture structure
    video_capture_t *capture = esp_gmf_oal_calloc(1, sizeof(video_capture_t));
    if (!capture) {
        ESP_LOGE(TAG, "Failed to allocate memory for capture");
        return NULL;
    }
    capture->config = *config;
    capture->frame_queue = xQueueCreate(2, sizeof(video_processor_event_t));
    if (!capture->frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        goto __exit;
    }
    capture->event_group = xEventGroupCreate();
    if (!capture->event_group) {
        goto __exit;
    }
    return capture;

__exit:
    if (capture->frame_queue) {
        vQueueDelete(capture->frame_queue);
        capture->frame_queue = NULL;
    }
    if (capture->event_group) {
        vEventGroupDelete(capture->event_group);
        capture->event_group = NULL;
    }
    if (capture) {
        esp_gmf_oal_free(capture);
        capture = NULL;
    }
    return NULL;
}

esp_err_t video_capture_start(video_capture_handle_t handle)
{
    video_capture_t *capture = (video_capture_t *)handle;
    if (!capture) {
        ESP_LOGE(TAG, "Invalid capture handle");
        return ESP_ERR_INVALID_ARG;
    }
    capture->vid_src = create_video_source(capture->config.camera_config);
    if (!capture->vid_src) {
        ESP_LOGE(TAG, "Failed to create video source or not support");
        return ESP_ERR_NO_MEM;
    }

    esp_capture_cfg_t capture_cfg = {
        .video_src = capture->vid_src,
    };
    esp_capture_open(&capture_cfg, &capture->capture);
    if (capture->capture == NULL) {
        ESP_LOGE(TAG, "Fail to create capture");
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < capture->config.sink_num; i++) {
        esp_capture_sink_cfg_t sink_cfg = capture->config.sink_cfg[i];
        esp_capture_sink_setup(capture->capture, i, &sink_cfg, &capture->sink[i]);
        esp_capture_sink_enable(capture->sink[i], ESP_CAPTURE_RUN_MODE_ALWAYS);
    }
    esp_capture_start(capture->capture);

    esp_gmf_oal_thread_create(&capture->capture_thread, "capture_worker_thread", capture_worker_thread, (void *)capture, 4096, 12, true, 0);

    return ESP_OK;
}

void video_capture_close(video_capture_handle_t handle)
{
    video_capture_t *capture = (video_capture_t *)handle;
    if (!capture) {
        ESP_LOGE(TAG, "Invalid capture handle");
        return;
    }

    if (capture->capture) {
        esp_capture_stop(capture->capture);
        esp_capture_close(capture->capture);
    }

    capture->is_running = false;
    xEventGroupWaitBits(capture->event_group, _EVENT_GROUP_STOP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (capture->vid_src) {
        capture->vid_src = NULL;
    }

    if (capture->frame_queue) {
        vQueueDelete(capture->frame_queue);
    }

    free(capture);
}
