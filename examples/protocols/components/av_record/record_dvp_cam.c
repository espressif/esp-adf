/* SPI Camera source to get video data from SPI Camera

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "record_src.h"
#include "esp_camera.h"
#include "media_lib_err.h"
#include "board.h"
#include "esp_log.h"

#define TAG           "DVP_Cam"

#define CAM_PIN_PWDN  -1 // Power down is not used
#define CAM_PIN_RESET -1 // Software reset will be performed

// When the camera is not enabled, use invalid pin setting
#if !defined(FUNC_LCD_SCREEN_EN) && !defined(FUNC_CAMERA_EN)
#define CAM_PIN_XCLK  -1
#define CAM_PIN_SIOD  -1
#define CAM_PIN_SIOC  -1
#define CAM_PIN_D7    -1
#define CAM_PIN_D6    -1
#define CAM_PIN_D5    -1
#define CAM_PIN_D4    -1
#define CAM_PIN_D3    -1
#define CAM_PIN_D2    -1
#define CAM_PIN_D1    -1
#define CAM_PIN_D0    -1
#define CAM_PIN_VSYNC -1
#define CAM_PIN_HREF  -1
#define CAM_PIN_PCLK  -1
#endif

typedef struct {
    camera_fb_t *pic;
} record_src_dvp_cam_t;

framesize_t get_video_quality(int width, int height)
{
    if (width == 320 && height == 240) {
        return FRAMESIZE_QVGA;
    }
    if (width == 480 && height == 320) {
        return FRAMESIZE_HVGA;
    }
    if (width == 640 && height == 480) {
        return FRAMESIZE_VGA;
    }
    if (width == 1024 && height == 768) {
        return FRAMESIZE_XGA;
    }
    if (width == 1280 && height == 720) {
        return FRAMESIZE_HD;
    }
    if (width == 1920 && height == 1080) {
        return FRAMESIZE_FHD;
    }
    return FRAMESIZE_QVGA;
}

record_src_handle_t open_dvp_cam(void *cfg, int cfg_size)
{
    if (cfg == NULL || cfg_size != sizeof(record_src_video_cfg_t)) {
        return NULL;
    }
    record_src_dvp_cam_t *dvp_cam_src = (record_src_dvp_cam_t *) calloc(sizeof(record_src_dvp_cam_t), 1);
    if (dvp_cam_src == NULL) {
        return NULL;
    }
    record_src_video_cfg_t *vid_cfg = (record_src_video_cfg_t *) cfg;
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
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
        .jpeg_quality = 12, // 0-63 lower number means higher quality
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    if (vid_cfg->reuse_i2c) {
        camera_config.pin_sccb_sda = -1;
        camera_config.pin_sccb_scl = -1;
    }
    if (vid_cfg->video_fmt == RECORD_SRC_VIDEO_FMT_YUV422) {
        camera_config.pixel_format = PIXFORMAT_YUV422;
        camera_config.xclk_freq_hz = 40000000;
    }
    do {
        if (vid_cfg->video_fmt == RECORD_SRC_VIDEO_FMT_YUV420) {
            camera_config.pixel_format = PIXFORMAT_YUV422;
            camera_config.xclk_freq_hz = 40000000;
    #if CONFIG_CAMERA_CONVERTER_ENABLED
            camera_config.conv_mode = YUV422_TO_YUV420;
    #else
            ESP_LOGE(TAG, "Please enable CONFIG_CAMERA_CONVERTER_ENABLED");
            break;
    #endif
        }
        if (CAM_PIN_XCLK == -1) {
            ESP_LOGE(TAG, "Camera pin not configured on this platform yet");
            break;
        }
        camera_config.frame_size = get_video_quality(vid_cfg->width, vid_cfg->height);
        esp_err_t err = esp_camera_init(&camera_config);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Camera Init Failed");
            break;
        }
        return (record_src_handle_t) dvp_cam_src;
    } while (0);
    free(dvp_cam_src);
    return NULL;
}

int read_dvp_cam(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout)
{
    if (handle == NULL || frame_data == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_dvp_cam_t *dvp_cam_src = (record_src_dvp_cam_t *) handle;
    camera_fb_t *pic = esp_camera_fb_get();
    if (pic == NULL) {
        return ESP_MEDIA_ERR_NOT_FOUND;
    }
    frame_data->data = pic->buf;
    frame_data->size = pic->len;
    dvp_cam_src->pic = pic;
    return ESP_MEDIA_ERR_OK;
}

int unlock_dvp_cam(record_src_handle_t handle)
{
    if (handle == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_dvp_cam_t *dvp_cam_src = (record_src_dvp_cam_t *) handle;
    if (dvp_cam_src->pic) {
        esp_camera_fb_return(dvp_cam_src->pic);
        dvp_cam_src->pic = NULL;
    }
    return ESP_MEDIA_ERR_OK;
}

void close_dvp_cam(record_src_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    record_src_dvp_cam_t *dvp_cam_src = (record_src_dvp_cam_t *) handle;
    esp_camera_deinit();
    free(dvp_cam_src);
}

int record_src_dvp_cam_register()
{
    record_src_api_t dvp_cam_record = {
        .open_record = open_dvp_cam,
        .read_frame = read_dvp_cam,
        .unlock_frame = unlock_dvp_cam,
        .close_record = close_dvp_cam,
    };
    return record_src_register(RECORD_SRC_TYPE_SPI_CAM, &dvp_cam_record);
}
