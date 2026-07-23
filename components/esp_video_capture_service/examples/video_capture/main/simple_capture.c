/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_audio_capture_ai_src.h"
#include "esp_board_manager_defs.h"
#include "esp_capture.h"
#include "esp_capture_service_ops.h"
#include "esp_capture_sink.h"
#include "esp_capture_types.h"
#include "esp_capture_video_v4l2_src.h"
#include "esp_log.h"
#include "esp_media_service.h"
#include "esp_service.h"
#include "esp_video_capture_service.h"
#include "esp_video_capture_service_setup.h"
#if defined(CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE)
#include "esp_video_init.h"
#endif  /* defined(CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE) */
#include "settings.h"
#include "simple_capture.h"
#include "esp_media_dummy_service.h"
#include "video_capture_utils.h"

static const char *TAG = "SIMPLE_CAPTURE";

static void simple_vad_cb(int state, void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "VAD state: %d", state);
}

static esp_media_video_info_t make_stream0_video(void)
{
    return (esp_media_video_info_t) {
        .codec = VIDEO_CAPTURE_STREAM0_CODEC,
        .width = VIDEO_CAPTURE_STREAM0_WIDTH,
        .height = VIDEO_CAPTURE_STREAM0_HEIGHT,
        .fps = VIDEO_CAPTURE_STREAM0_FPS,
    };
}

static esp_media_audio_info_t make_stream0_audio(void)
{
    return (esp_media_audio_info_t) {
        .codec = VIDEO_CAPTURE_AUDIO_CODEC,
        .sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .bits_per_sample = VIDEO_CAPTURE_AUDIO_BITS_PER_SAMPLE,
        .channel = VIDEO_CAPTURE_AUDIO_CHANNELS,
        .bitrate = VIDEO_CAPTURE_AAC_BITRATE,
    };
}

esp_err_t simple_capture_video_only(uint32_t duration_ms)
{
    /* 1. Create the video capture service. Camera/V4L2 source is created during apply_setup. */
    esp_video_capture_service_cfg_t service_cfg = {
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_video_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. One H264/MJPEG video stream, no audio track. */
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .streams[0] = {
            .enabled = true,
            .video_info = make_stream0_video(),
        },
    };
    ret = esp_video_capture_service_apply_setup(capture, &setup);

    /* 3. Pull frames directly with acquire/release. */
    bool capture_started = false;
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        capture_started = (ret == ESP_OK);
    }

    uint32_t frame_count = 0;
    uint64_t byte_count = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_media_frame_t frame = {
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        };
        ret = esp_capture_service_acquire_frame(capture, 0, &frame, VIDEO_CAPTURE_FRAME_TIMEOUT_MS);
        if (ret == ESP_ERR_TIMEOUT) {
            ret = ESP_OK;
            continue;
        }
        if (ret != ESP_OK) {
            break;
        }
        frame_count++;
        byte_count += frame.size;
        ret = esp_capture_service_release_frame(capture, 0, &frame);
    }

    if (capture_started) {
        esp_err_t stop_ret = esp_service_stop(ESP_SERVICE_BASE(capture));
        if (ret == ESP_OK) {
            ret = stop_ret;
        }
    }
    ESP_LOGI(TAG, "video_only: %" PRIu32 " frames, %" PRIu64 " bytes", frame_count, byte_count);
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_capture_av_stream(uint32_t duration_ms)
{
    /* 1. Create AV capture service (audio context is attached automatically). */
    esp_video_capture_service_cfg_t service_cfg = {
        .audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_video_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. One AV stream: encoded video + AAC. */
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .video_info = make_stream0_video(),
            .audio_info = make_stream0_audio(),
        },
    };
    ret = esp_video_capture_service_apply_setup(capture, &setup);

    bool capture_started = false;
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        capture_started = (ret == ESP_OK);
    }

    uint32_t video_frames = 0;
    uint32_t audio_frames = 0;
    uint64_t video_bytes = 0;
    uint64_t audio_bytes = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_media_frame_t vframe = {
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        };
        esp_err_t vret = esp_capture_service_acquire_frame(capture, 0, &vframe,
                                                           VIDEO_CAPTURE_FRAME_TIMEOUT_MS);
        if (vret == ESP_OK) {
            video_frames++;
            video_bytes += vframe.size;
            ret = esp_capture_service_release_frame(capture, 0, &vframe);
        } else if (vret != ESP_ERR_TIMEOUT) {
            ret = vret;
            break;
        }

        esp_media_frame_t aframe = {
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        };
        esp_err_t aret = esp_capture_service_acquire_frame(capture, 0, &aframe, 0);
        if (aret == ESP_OK) {
            audio_frames++;
            audio_bytes += aframe.size;
            ret = esp_capture_service_release_frame(capture, 0, &aframe);
        } else if (aret != ESP_ERR_TIMEOUT && aret != ESP_ERR_INVALID_STATE) {
            ret = aret;
            break;
        }
    }

    if (capture_started) {
        esp_err_t stop_ret = esp_service_stop(ESP_SERVICE_BASE(capture));
        if (ret == ESP_OK) {
            ret = stop_ret;
        }
    }
    ESP_LOGI(TAG, "av_stream: video %" PRIu32 "/%" PRIu64 " audio %" PRIu32 "/%" PRIu64,
             video_frames, video_bytes, audio_frames, audio_bytes);
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_capture_av_storage(uint32_t duration_ms)
{
    /* 1. Create AV capture service. */
    esp_video_capture_service_cfg_t service_cfg = {
        .audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_video_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Configure AV stream with MP4 muxer. */
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .video_info = make_stream0_video(),
            .audio_info = make_stream0_audio(),
            .muxer_info = {
                .muxer_type = ESP_MUXER_TYPE_MP4,
            },
        },
    };
    ret = esp_video_capture_service_apply_setup(capture, &setup);
    if (ret == ESP_OK) {
        /* Storage-only: disable provider tracks so unread frames cannot back-pressure recording. */
        ret = esp_capture_service_enable_track(capture, 0, ESP_MEDIA_TRACK_TYPE_VIDEO, false);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_enable_track(capture, 0, ESP_MEDIA_TRACK_TYPE_AUDIO, false);
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_set_storage_url(capture, 0, VIDEO_CAPTURE_SIMPLE_AV_MP4);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
    }
    if (ret == ESP_OK) {
        ret = esp_capture_service_start_record(capture, 0);
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        ret = esp_capture_service_stop_record(capture, 0);
    }
    esp_service_stop(ESP_SERVICE_BASE(capture));
    esp_capture_service_destroy(capture);
    if (ret == ESP_OK) {
        ret = video_capture_check_recorded_file(TAG, VIDEO_CAPTURE_SIMPLE_AV_MP4);
    }
    return ret;
}

esp_err_t simple_capture_av_link(uint32_t duration_ms)
{
    /* 1. Create AV capture service. */
    esp_video_capture_service_cfg_t service_cfg = {
        .audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_video_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. One AV stream. */
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .video_info = make_stream0_video(),
            .audio_info = make_stream0_audio(),
        },
    };
    ret = esp_video_capture_service_apply_setup(capture, &setup);

    /* 3. Link capture output to a sink that owns frame consumption. */
    esp_media_dummy_service_t *sink = NULL;
    if (ret == ESP_OK) {
        esp_media_dummy_service_cfg_t sink_cfg = ESP_MEDIA_DUMMY_SERVICE_CONFIG_DEFAULT();
        sink_cfg.role = ESP_MEDIA_ROLE_SINK;
        sink_cfg.max_stream_num = 1;
        ret = esp_media_dummy_service_create(&sink_cfg, &sink);
    }
    if (ret == ESP_OK) {
        ret = esp_media_service_link(ESP_SERVICE_BASE(capture), 0,
                                     ESP_SERVICE_BASE(sink), 0);
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(sink));
    }
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
    }
    if (ret == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
    }

    if (sink != NULL) {
        esp_service_stop(ESP_SERVICE_BASE(sink));
    }
    esp_service_stop(ESP_SERVICE_BASE(capture));
    if (sink != NULL) {
        esp_media_dummy_stream_stats_t stats = {0};
        esp_media_dummy_service_get_stats(sink, 0, &stats);
        ESP_LOGI(TAG, "av_link: video %" PRIu32 "/%" PRIu32 " audio %" PRIu32 "/%" PRIu32,
                 stats.video_frame_count, stats.video_byte_count,
                 stats.audio_frame_count, stats.audio_byte_count);
        esp_media_service_unlink(ESP_SERVICE_BASE(capture), 0,
                                 ESP_SERVICE_BASE(sink), 0);
        esp_media_dummy_service_destroy(sink);
    }
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_capture_av_ai(uint32_t duration_ms)
{
    /* 1. Create AV capture. AI audio is selected later via audio-capture APIs on
       the same handle (see esp_audio_capture_ai_src / esp_capture_service_ai_*). */
    esp_video_capture_service_cfg_t service_cfg = {
        .audio_dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_ADC,
        .video_dev_name = ESP_BOARD_DEVICE_NAME_CAMERA,
        .max_stream_num = 1,
    };
    esp_capture_service_t *capture = NULL;
    esp_err_t ret = esp_video_capture_service_create(&service_cfg, &capture);
    if (ret != ESP_OK) {
        return ret;
    }

    /* 2. Configure AI audio BEFORE apply_setup so setup picks the AI source
       instead of the plain codec ADC source. Requires
       CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED (+ AEC/VAD Kconfig). */
    const uint32_t features = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_AEC | ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_VAD;
    esp_capture_service_ai_audio_src_feature_cfg_t feature_cfg = {0};
    ret = esp_capture_service_ai_audio_src_set_feature(capture, features, &feature_cfg);
    if (ret == ESP_OK) {
        ret = esp_capture_service_ai_audio_src_set_vad_cb(capture, simple_vad_cb, NULL);
    }

    /* 3. Same AAC stream as normal AV; AI source feeds mono PCM into the encoder. */
    esp_video_capture_service_setup_t setup = {
        .stream_num = 1,
        .fixed_src_sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
        .streams[0] = {
            .enabled = true,
            .video_info = make_stream0_video(),
            .audio_info = {
                .codec = VIDEO_CAPTURE_AUDIO_CODEC,
                .sample_rate = VIDEO_CAPTURE_AUDIO_SAMPLE_RATE,
                .bits_per_sample = VIDEO_CAPTURE_AUDIO_BITS_PER_SAMPLE,
                .channel = 1,
                .bitrate = VIDEO_CAPTURE_AAC_BITRATE,
            },
        },
    };
    if (ret == ESP_OK) {
        ret = esp_video_capture_service_apply_setup(capture, &setup);
    }

    bool capture_started = false;
    if (ret == ESP_OK) {
        ret = esp_service_start(ESP_SERVICE_BASE(capture));
        capture_started = (ret == ESP_OK);
    }

    uint32_t video_frames = 0;
    uint32_t audio_frames = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_media_frame_t vframe = {
            .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
        };
        esp_err_t vret = esp_capture_service_acquire_frame(capture, 0, &vframe,
                                                           VIDEO_CAPTURE_FRAME_TIMEOUT_MS);
        if (vret == ESP_OK) {
            video_frames++;
            ret = esp_capture_service_release_frame(capture, 0, &vframe);
        } else if (vret != ESP_ERR_TIMEOUT) {
            ret = vret;
            break;
        }

        esp_media_frame_t aframe = {
            .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
        };
        esp_err_t aret = esp_capture_service_acquire_frame(capture, 0, &aframe, 0);
        if (aret == ESP_OK) {
            audio_frames++;
            ret = esp_capture_service_release_frame(capture, 0, &aframe);
        } else if (aret != ESP_ERR_TIMEOUT && aret != ESP_ERR_INVALID_STATE) {
            ret = aret;
            break;
        }
    }

    if (capture_started) {
        esp_err_t stop_ret = esp_service_stop(ESP_SERVICE_BASE(capture));
        if (ret == ESP_OK) {
            ret = stop_ret;
        }
    }
    ESP_LOGI(TAG, "av_ai: video %" PRIu32 " audio %" PRIu32 " frames", video_frames, audio_frames);
    esp_capture_service_destroy(capture);
    return ret;
}

esp_err_t simple_capture_fullspeed_uvc(uint32_t duration_ms)
{
#if !defined(CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE)
    ESP_LOGW(TAG, "UVC is disabled; enable CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE for fullspeed_uvc");
    return ESP_ERR_NOT_SUPPORTED;
#else
    /* Native esp_capture path: decode runs in VID_SRC, encode in venc_0. */
    static const esp_video_init_usb_uvc_config_t s_usb_uvc_config = {
        .uvc = {
            .uvc_dev_num   = 1,
            .task_stack    = 8192,
            .task_priority = 15,
            .task_affinity = 0,
        },
        .usb = {
            .init_usb_host_lib = true,
            .peripheral_map    = 0,
            .task_stack        = 8192,
            .task_priority     = 15,
            .task_affinity     = 0,
        },
    };
    /* Initialize only the UVC device. ESP_VIDEO_INIT_FLAGS_ALL would also
       recreate board-manager-owned devices such as ISP /dev/video20. */
    esp_video_init_config_t cam_config = {
        .usb_uvc = &s_usb_uvc_config,
    };
    esp_err_t ret = esp_video_init_with_flags(&cam_config, ESP_VIDEO_INIT_FLAGS_USB_UVC);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UVC camera init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_capture_video_v4l2_src_cfg_t v4l2_cfg = {
        .buf_count = 3,
    };
    strncpy(v4l2_cfg.dev_name, "/dev/video40", sizeof(v4l2_cfg.dev_name) - 1);
    esp_capture_video_src_if_t *vid_src = esp_capture_new_video_v4l2_src(&v4l2_cfg);
    if (vid_src == NULL) {
        ESP_LOGE(TAG, "Failed to create UVC V4L2 source");
        esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_USB_UVC);
        return ESP_ERR_NO_MEM;
    }

    esp_capture_handle_t capture = NULL;
    esp_capture_cfg_t cfg = {
        .sync_mode         = ESP_CAPTURE_SYNC_MODE_NONE,
        .video_src         = vid_src,
        .full_speed_decode = true,
    };
    ret = (esp_capture_open(&cfg, &capture) == ESP_CAPTURE_ERR_OK) ? ESP_OK : ESP_FAIL;
    if (ret != ESP_OK) {
        free(vid_src);
        esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_USB_UVC);
        return ret;
    }

    esp_capture_sink_cfg_t sink_cfg = {
        .video_info = {
            .format_id = VIDEO_CAPTURE_STREAM0_CODEC,
            .width     = VIDEO_CAPTURE_STREAM0_WIDTH,
            .height    = VIDEO_CAPTURE_STREAM0_HEIGHT,
            .fps       = 25,
        },
    };
    esp_capture_sink_handle_t sink = NULL;
    if (esp_capture_sink_setup(capture, 0, &sink_cfg, &sink) != ESP_CAPTURE_ERR_OK) {
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && esp_capture_sink_enable(sink, ESP_CAPTURE_RUN_MODE_ALWAYS) != ESP_CAPTURE_ERR_OK) {
        ret = ESP_FAIL;
    }
    if (ret == ESP_OK && esp_capture_start(capture) != ESP_CAPTURE_ERR_OK) {
        ret = ESP_FAIL;
    }

    uint32_t frame_count = 0;
    uint64_t byte_count = 0;
    TickType_t start = xTaskGetTickCount();
    TickType_t duration_ticks = pdMS_TO_TICKS(duration_ms);
    while (ret == ESP_OK && (xTaskGetTickCount() - start) < duration_ticks) {
        esp_capture_stream_frame_t frame = {
            .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
        };
        esp_capture_err_t capt_ret = esp_capture_sink_acquire_frame(sink, &frame, true);
        if (capt_ret == ESP_CAPTURE_ERR_OK) {
            frame_count++;
            byte_count += frame.size;
            esp_capture_sink_release_frame(sink, &frame);
        } else if (capt_ret == ESP_CAPTURE_ERR_TIMEOUT || capt_ret == ESP_CAPTURE_ERR_NOT_FOUND) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ret = ESP_FAIL;
            break;
        }
    }

    esp_capture_stop(capture);
    esp_capture_close(capture);
    free(vid_src);
    esp_err_t deinit_ret = esp_video_deinit_with_flags(ESP_VIDEO_INIT_FLAGS_USB_UVC);
    if (deinit_ret != ESP_OK) {
        ESP_LOGE(TAG, "UVC camera deinit failed: %s", esp_err_to_name(deinit_ret));
        if (ret == ESP_OK) {
            ret = deinit_ret;
        }
    }
    ESP_LOGI(TAG, "fullspeed_uvc: %" PRIu32 " frames, %" PRIu64 " bytes", frame_count, byte_count);
    return ret;
#endif  /* !defined(CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE) */
}
