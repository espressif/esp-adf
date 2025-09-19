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

#include "esp_log.h"
#include "audio_mem.h"
#include "av_stream_hal.h"

static const char *TAG = "AV_STREAM_HAL";

void av_stream_get_video_framesize(framesize_t framesize, uint16_t *width, uint16_t *height)
{
    switch (framesize) {
        case FRAMESIZE_96X96:
            *width = 96;
            *height = 96;
            break;
        case FRAMESIZE_QQVGA:
            *width = 160;
            *height = 120;
            break;
        case FRAMESIZE_128X128:
            *width = 128;
            *height = 128;
            break;
        case FRAMESIZE_QCIF:
            *width = 176;
            *height = 144;
            break;
        case FRAMESIZE_HQVGA:
            *width = 240;
            *height = 176;
            break;
        case FRAMESIZE_240X240:
            *width = 240;
            *height = 240;
            break;
        case FRAMESIZE_QVGA:
        default:
            *width = 320;
            *height = 240;
            break;
        case FRAMESIZE_320X320:
            *width = 320;
            *height = 320;
            break;
        case FRAMESIZE_CIF:
            *width = 400;
            *height = 296;
            break;
        case FRAMESIZE_HVGA:
            *width = 480;
            *height = 320;
            break;
        case FRAMESIZE_VGA:
            *width = 640;
            *height = 480;
            break;
        case FRAMESIZE_SVGA:    
            *width = 800;
            *height = 600;
            break;
        case FRAMESIZE_XGA:
            *width = 1024;
            *height = 768;
            break;
        case FRAMESIZE_HD:
            *width = 1280;
            *height = 720;
            break;
        case FRAMESIZE_SXGA:
            *width = 1280;
            *height = 1024;
            break;
        case FRAMESIZE_UXGA:
            *width = 1600;
            *height = 1200;
            break;
        case FRAMESIZE_FHD:
            *width = 1920;
            *height = 1080;
            break;
        case FRAMESIZE_P_HD:
            *width = 720;
            *height = 1280;
            break;
        case FRAMESIZE_P_3MP:
            *width = 864;
            *height = 1536;
            break;
        case FRAMESIZE_QXGA:
            *width = 2048;
            *height = 1536;
            break;
        case FRAMESIZE_QHD:
            *width = 2560;
            *height = 1440;
            break;
        case FRAMESIZE_WQXGA:
            *width = 2560;
            *height = 1600;
            break;
        case FRAMESIZE_P_FHD:
            *width = 1080;
            *height = 1920;
            break;
        case FRAMESIZE_QSXGA:
            *width = 2560;
            *height = 1920;
            break;
        case FRAMESIZE_5MP:
            *width = 2592;
            *height = 1944;
            break;
    }
}

#if CONFIG_IDF_TARGET_ESP32
int av_stream_camera_init(av_stream_hal_config_t *config, void *cb, void *arg)
{
    AUDIO_NULL_CHECK(TAG, config, return ESP_ERR_INVALID_ARG);
    /* Not Support */
    return ESP_FAIL;
}
int av_stream_camera_deinit(bool uvc_en)
{
    /* Not Support */
    return ESP_FAIL;
}
#else
#include "usb_stream.h"
#include "esp_camera.h"
static uint8_t *xfer_buffer_a = NULL;
static uint8_t *xfer_buffer_b = NULL;
static uint8_t *frame_buffer = NULL;
static esp_err_t usb_camera_init(av_stream_hal_config_t *config, void *camera_cb, void *ctx)
{
    AUDIO_NULL_CHECK(TAG, camera_cb, return ESP_ERR_INVALID_ARG);

#if CONFIG_ESP32_S3_KORVO2L_V1_BOARD
    /* power on usb camera */
    audio_board_usb_cam_init();
#endif
    /* malloc double buffer for usb payload, xfer_buffer_size >= frame_buffer_size*/
    xfer_buffer_a = (uint8_t *)audio_malloc(VIDEO_MAX_SIZE);
    assert(xfer_buffer_a != NULL);
    xfer_buffer_b = (uint8_t *)audio_malloc(VIDEO_MAX_SIZE);
    assert(xfer_buffer_b != NULL);
    /* malloc frame buffer for a jpeg frame*/
    frame_buffer = (uint8_t *)audio_malloc(VIDEO_MAX_SIZE);
    assert(frame_buffer != NULL);

    /* the quick demo skip the standred get descriptors process,
    users need to get params from camera descriptors from demo print */
    uint16_t width = 0, height = 0;
    av_stream_get_video_framesize(config->video_framesize, &width, &height);
    ESP_LOGI(TAG, "camera frame size: %dx%d", width, height);
    uvc_config_t uvc_config = {
        .frame_width = width,
        .frame_height = height,
        .frame_interval = USB_CAMERA_FRAME_INTERVAL(VIDEO_FPS),
        .xfer_buffer_size = VIDEO_MAX_SIZE,
        .xfer_buffer_a = xfer_buffer_a,
        .xfer_buffer_b = xfer_buffer_b,
        .frame_buffer_size = VIDEO_MAX_SIZE,
        .frame_buffer = frame_buffer,
        .frame_cb = camera_cb,
        .frame_cb_arg = ctx,
    };

    /* pre-config UVC driver with params from known USB Camera Descriptors*/
    esp_err_t ret = uvc_streaming_config(&uvc_config);

    /* Start camera IN stream with pre-configs, uvc driver will create multi-tasks internal
    to handle usb data from different pipes, and user's callback will be called after new frame ready. */
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uvc streaming config failed");
    }

    usb_streaming_start();
    return ret;
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = -1,
    .pin_sccb_scl = -1,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,

    .jpeg_quality = 12,     //0-63 lower number means higher quality
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

int av_stream_camera_init(av_stream_hal_config_t *config, void *cb, void *arg)
{
    AUDIO_NULL_CHECK(TAG, config, return ESP_FAIL);

    if (config->uvc_en) {
        if (usb_camera_init(config, cb, arg) != ESP_OK) {
            return ESP_FAIL;
        }
    } else {
        camera_config.frame_size = config->video_framesize;
        if (config->video_soft_enc) {
            camera_config.pixel_format = PIXFORMAT_YUV422;
        }
        if (config->uac_en) {
            camera_config.pin_sccb_scl = CAM_PIN_SIOC;
            camera_config.pin_sccb_sda = CAM_PIN_SIOD;
        }
        if (esp_camera_init(&camera_config) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

int av_stream_camera_deinit(bool uvc_en)
{
    int ret = ESP_OK;
    if (uvc_en) {
        usb_streaming_stop();
        if (xfer_buffer_a) {
            free(xfer_buffer_a);
            xfer_buffer_a = NULL;
        }
        if (xfer_buffer_b) {
            free(xfer_buffer_b);
            xfer_buffer_b = NULL;
        }
        if (frame_buffer) {
            free(frame_buffer);
            frame_buffer = NULL;
        }
    } else {
        ret = esp_camera_deinit();
    }

    return ret;
}
#endif