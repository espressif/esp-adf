/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_capture.h"
#include "esp_capture_sink.h"
#include "esp_capture_overlay_if.h"
#include "esp_lcd_touch.h"
#include "dev_display_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DEFAULT_SLICE_DURATION_MS   60000

#define RECORD_CORE_ID         (0)
#define DISPLAY_CORE_ID        (1)
#define REC_AUDIO_FMT          (ESP_CAPTURE_FMT_ID_AAC)
#define REC_AUDIO_SAMPLE_RATE  (48000)
#define REC_AUDIO_CHANNEL      (2)
#define REC_AUDIO_BITS         (16)
#define FILE_RAM_CACHE_SIZE    (8 * 1024)

#if CONFIG_IDF_TARGET_ESP32P4
#define RECORD_FORMAT_ID  (ESP_CAPTURE_FMT_ID_H264)
#define RECORD_WIDTH      (1024)
#define RECORD_HEIGHT     (600)
#define RECORD_FPS        (30)
#define DISPLAY_FPS       (30)
#define RECORD_BITRATE    (4 * 1000 * 1000)
#elif CONFIG_IDF_TARGET_ESP32S31
#define RECORD_FORMAT_ID  (ESP_CAPTURE_FMT_ID_MJPEG)
#define RECORD_WIDTH      (640)
#define RECORD_HEIGHT     (480)
#define RECORD_FPS        (25)
#define DISPLAY_FPS       (25)
#define RECORD_BITRATE    (1500 * 1000)
#else
#define RECORD_FORMAT_ID  (ESP_CAPTURE_FMT_ID_MJPEG)
#define RECORD_WIDTH      (320)
#define RECORD_HEIGHT     (240)
#define RECORD_FPS        (25)
#define DISPLAY_FPS       (25)
#define RECORD_BITRATE    (1500 * 1000)
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

/**
 * @brief  Runtime resources shared by board, capture, display, and storage modules
 */
typedef struct {
    esp_capture_handle_t        capture;           /*!< ESP Capture instance handle */
    esp_capture_audio_src_if_t *audio_src;         /*!< Audio source interface */
    esp_capture_video_src_if_t *video_src;         /*!< Video source interface */
    esp_capture_sink_handle_t   record_sink;       /*!< Sink used for MP4 recording */
    esp_capture_sink_handle_t   display_sink;      /*!< Sink used for live LCD display */
    dev_display_lcd_handles_t  *lcd_handles;       /*!< Board LCD device handles */
    dev_display_lcd_config_t   *lcd_cfg;           /*!< Board LCD configuration */
    esp_lcd_touch_handle_t      touch_handle;      /*!< Optional LCD touch device handle */
    esp_capture_video_info_t    display_info;      /*!< Video information for display frames */
    esp_capture_overlay_if_t   *timer_overlay;     /*!< Timer badge canvas, blended by video_render only */
    esp_capture_overlay_if_t   *fps_overlay;       /*!< FPS badge canvas, blended by video_render only */
    uint32_t                    next_record_id;    /*!< Next recording session identifier */
    uint32_t                    active_record_id;  /*!< Recording session identifier in progress */
    uint32_t                    last_timer_sec;    /*!< Last rendered recording duration in seconds */
    uint32_t                    last_fps;          /*!< Last rendered FPS value */
    int64_t                     record_start_ms;   /*!< Recording start time in milliseconds */
    bool                        recording;         /*!< True when record sink is actively writing slices */
    bool                        last_recording;    /*!< Last rendered record state */
} av_record_live_display_sys_t;

/**
 * @brief  Initialize board devices required by the example
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to get or validate a required device handle
 *       - Others    Error codes from board manager, codec device, or board peripheral initialization
 */
esp_err_t av_rec_init_devices(av_record_live_display_sys_t *sys);

/**
 * @brief  Deinitialize board devices initialized by av_rec_init_devices()
 *
 * @return
 *       - ESP_OK  On success
 *       - Others  Error code from board manager or board peripheral deinitialization
 */
esp_err_t av_rec_deinit_devices(void);

/**
 * @brief  Create the esp_capture instance and attach audio/video sources
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to create sources or open esp_capture
 */
esp_err_t av_rec_build_capture(av_record_live_display_sys_t *sys);

/**
 * @brief  Configure the recording sink and MP4 muxer
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to set up the record sink or add the MP4 muxer
 */
esp_err_t av_rec_setup_record_sink(av_record_live_display_sys_t *sys);

/**
 * @brief  Configure the live display sink
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to set up the display sink
 */
esp_err_t av_rec_setup_display_sink(av_record_live_display_sys_t *sys);

/**
 * @brief  Release capture, sinks, and source resources
 *
 * @param[in,out]  sys  Shared runtime resource context
 */
void av_rec_release_capture(av_record_live_display_sys_t *sys);

/**
 * @brief  Run capture and the interactive live display session
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to start or stop capture, or display task failed
 */
esp_err_t av_rec_run_live_session(av_record_live_display_sys_t *sys);

/**
 * @brief  Pull frames from the display sink, draw UI, and refresh the LCD
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to create the display task or display frames
 */
esp_err_t av_rec_run_display(av_record_live_display_sys_t *sys);

/**
 * @brief  Create the timer and FPS badge canvases used by the live preview
 *
 * @note  The canvases are never attached to a capture sink, which keeps their pixels out of the
 *        recorded stream. Badge content is drawn once the video render widgets are created.
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to create one of the badge canvases
 */
esp_err_t av_rec_display_create_overlays(av_record_live_display_sys_t *sys);

/**
 * @brief  Destroy the badge canvases created by av_rec_display_create_overlays()
 *
 * @note  Must be called after the video render widgets referencing the canvases are destroyed
 *
 * @param[in,out]  sys  Shared runtime resource context
 */
void av_rec_display_destroy_overlays(av_record_live_display_sys_t *sys);

/**
 * @brief  Start writing MP4 slices for the next recording session
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to enable recording
 */
esp_err_t av_rec_start_record(av_record_live_display_sys_t *sys);

/**
 * @brief  Stop writing MP4 slices for the current recording session
 *
 * @param[in,out]  sys  Shared runtime resource context
 *
 * @return
 *       - ESP_OK    On success
 *       - ESP_FAIL  Failed to disable recording
 */
esp_err_t av_rec_stop_record(av_record_live_display_sys_t *sys);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
