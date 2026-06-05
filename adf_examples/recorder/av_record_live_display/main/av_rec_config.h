/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_capture.h"
#include "esp_capture_sink.h"
#include "dev_display_lcd.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define DEFAULT_RECORD_DURATION_MS  15000
#define DEFAULT_SLICE_DURATION_MS   60000

#define RECORD_CORE_ID         (0)
#define DISPLAY_CORE_ID        (1)
#define REC_AUDIO_FMT          (ESP_CAPTURE_FMT_ID_AAC)
#define REC_AUDIO_SAMPLE_RATE  (48000)
#define REC_AUDIO_CHANNEL      (2)
#define REC_AUDIO_BITS         (16)

#if CONFIG_IDF_TARGET_ESP32P4
#define RECORD_FORMAT_ID  (ESP_CAPTURE_FMT_ID_MJPEG)
#define RECORD_WIDTH      (1024)
#define RECORD_HEIGHT     (600)
#define RECORD_FPS        (30)
#define DISPLAY_FPS       (30)
#define RECORD_BITRATE    (4 * 1000 * 1000)
#else
#define RECORD_FORMAT_ID  (ESP_CAPTURE_FMT_ID_MJPEG)
#define RECORD_WIDTH      (640)
#define RECORD_HEIGHT     (480)
#define RECORD_FPS        (5)
#define DISPLAY_FPS       (5)
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
    esp_capture_video_info_t    display_info;      /*!< Video information for display frames */
    SemaphoreHandle_t           display_done_sem;  /*!< P4 display completion semaphore */
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
 * @brief  Run capture, live display, and recording for the specified duration
 *
 * @param[in,out]  sys          Shared runtime resource context
 * @param[in]      duration_ms  Session duration, in milliseconds
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid display duration
 *       - ESP_FAIL             Failed to start or stop capture, or display task failed
 */
esp_err_t av_rec_run_live_session(av_record_live_display_sys_t *sys, int duration_ms);

/**
 * @brief  Pull frames from the display sink and draw them to the LCD
 *
 * @param[in,out]  sys          Shared runtime resource context
 * @param[in]      duration_ms  Display duration, in milliseconds
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid display duration
 *       - ESP_FAIL             Failed to create the display task or display frames
 */
esp_err_t av_rec_run_display(av_record_live_display_sys_t *sys, int duration_ms);

/**
 * @brief  Check whether the first generated MP4 file exists and has a valid size
 *
 * @param[out]  file_size  File size in bytes on success, or -1 if record file was not found
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If file_size is NULL
 *       - ESP_ERR_NOT_FOUND    If storage mount point or record file was not found
 */
esp_err_t av_rec_check_record_file(int *file_size);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
