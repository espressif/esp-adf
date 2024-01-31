/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef _AV_STREAM_HAL_H
#define _AV_STREAM_HAL_H

#include "esp_log.h"
#include "driver/i2s.h"
#include "board.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_FPS                   15
#define VIDEO_DEC_SIZE              320*240*2
#define VIDEO_MAX_SIZE              100*1024
#define AUDIO_MAX_SIZE              2*1024
#define AUDIO_HAL_SAMPLE_RATE       8000

#define I2S_DEFAULT_PORT            I2S_NUM_0
#define I2S_DEFAULT_BITS            CODEC_ADC_BITS_PER_SAMPLE

#if CONFIG_ESP32_S3_KORVO2L_V1_BOARD // ES8311 need ADCL + DACR for AEC feature
#define I2S_CHANNELS                I2S_CHANNEL_FMT_RIGHT_LEFT
#else
#define I2S_CHANNELS                I2S_CHANNEL_FMT_ONLY_LEFT
#endif

#define USB_CAMERA_FRAME_INTERVAL(fps) ((10000000) / fps)

typedef enum {
    AV_FRAMESIZE_96X96,    // 96x96
    AV_FRAMESIZE_QQVGA,    // 160x120
    AV_FRAMESIZE_QCIF,     // 176x144
    AV_FRAMESIZE_HQVGA,    // 240x176
    AV_FRAMESIZE_240X240,  // 240x240
    AV_FRAMESIZE_QVGA,     // 320x240
    AV_FRAMESIZE_CIF,      // 400x296
    AV_FRAMESIZE_HVGA,     // 480x320
    AV_FRAMESIZE_VGA,      // 640x480
    AV_FRAMESIZE_SVGA,     // 800x600
    AV_FRAMESIZE_XGA,      // 1024x768
    AV_FRAMESIZE_HD,       // 1280x720
    AV_FRAMESIZE_SXGA,     // 1280x1024
    AV_FRAMESIZE_UXGA,     // 1600x1200
    // 3MP Sensors
    AV_FRAMESIZE_FHD,      // 1920x1080
    AV_FRAMESIZE_P_HD,     //  720x1280
    AV_FRAMESIZE_P_3MP,    //  864x1536
    AV_FRAMESIZE_QXGA,     // 2048x1536
    // 5MP Sensors
    AV_FRAMESIZE_QHD,      // 2560x1440
    AV_FRAMESIZE_WQXGA,    // 2560x1600
    AV_FRAMESIZE_P_FHD,    // 1080x1920
    AV_FRAMESIZE_QSXGA,    // 2560x1920
    AV_FRAMESIZE_INVALID
} av_framesize_t;

typedef struct {
    const uint16_t width;
    const uint16_t height;
} av_resolution_info_t;

extern const av_resolution_info_t av_resolution[];

/**
 * @brief AV stream hal configurations
 */
typedef struct {
    bool                        uvc_en;             /*!< Use USB video camera, but user need check uvc MACROS */
    bool                        uac_en;             /*!< Use USB audio device, but user need check uac MACROS */
    bool                        lcd_en;             /*!< Have LCD to render */
    esp_periph_set_handle_t     set;                /*!< The handle of esp_periph */
    bool                        video_soft_enc;     /*!< Use software JPEG / H264 encoder */
    uint32_t                    audio_samplerate;   /*!< Audio sample rate */
    uint32_t                    audio_framesize;    /*!< Audio frame size */
    av_framesize_t              video_framesize;    /*!< Video frame size */
} av_stream_hal_config_t;

/**
 * @brief      Intialize audio hal
 *
 * @param[in]  ctx          The user context
 * @param[in]  arg          The user argument
 * @param[in]  config       The av stream hal configuration
 *
 * @return
 *     - The audio_board handle if successfully created
 */
audio_board_handle_t av_stream_audio_init(void *ctx, void *arg, av_stream_hal_config_t *config);

/**
 * @brief      Deintialize audio hal
 *
 * @param[in]  ctx      The user context
 * @param[in]  uac_en   Use USB audio device
 *
 * @return
 *     - ESP_OK on success
 *     - other on error
 */
int av_stream_audio_deinit(void *ctx, bool uac_en);

/**
 * @brief      Read audio hal PCM data
 *
 * @param[in]  buf          The frame buffer
 * @param[in]  len          Length of data in bytes
 * @param[in]  wait_time    Wait timeout in RTOS ticks
 * @param[in]  uac_en       Use USB audio device
 *
 * @return
 *     - Number of bytes read
 */
int av_stream_audio_read(char *buf, int len, TickType_t wait_time, bool uac_en);

/**
 * @brief      Write audio hal with PCM data
 *
 * @param[in]  buf          The frame buffer
 * @param[in]  len          Length of data in bytes
 * @param[in]  wait_time    Wait timeout in RTOS ticks
 * @param[in]  uac_en       Use USB audio device
 *
 * @return
 *     - Number of bytes written
 */
int av_stream_audio_write(char *buf, int len, TickType_t wait_time, bool uac_en);

/**
 * @brief      Intialize camera hal
 *
 * @param[in]  config       The av stream hal configuration
 * @param[in]  cb           The UVC frame callback
 * @param[in]  arg          The user argument
 *
 * @return
 *     - ESP_OK on success
 *     - other on error
 */
int av_stream_camera_init(av_stream_hal_config_t *config, void *cb, void *arg);

/**
 * @brief      Deintialize camera hal
 *
 * @param[in]  uvc_en   Use USB video camera device
 *
 * @return
 *     - ESP_OK on success
 *     - other on error
 */
int av_stream_camera_deinit(bool uvc_en);

/**
 * @brief      Intialize lcd hal
 *
 * @param[in]  set      The av stream hal configuration
 *
 * @return
 *     - The handle of esp_lcd_panel
 *     - NULL on error
 */
esp_lcd_panel_handle_t av_stream_lcd_init(esp_periph_set_handle_t set);

/**
 * @brief      Deintialize lcd hal
 *
 * @param[in]  panel_handle     The handle of esp_lcd_panel
 *
 * @return
 *     - ESP_OK on success
 *     - other on error
 */
int av_stream_lcd_deinit(esp_lcd_panel_handle_t panel_handle);

/**
 * @brief      Write render device with RGB video data
 *
 * @param[in]  data             The RGB frame data
 * @param[in]  panel_handle     The handle of esp_lcd_panel
 *
 * @return
 *     - ESP_OK on success
 *     - other on error
 */
int av_stream_lcd_render(esp_lcd_panel_handle_t panel_handle, unsigned char *data);

#ifdef __cplusplus
}
#endif

#endif