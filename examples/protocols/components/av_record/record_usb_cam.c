/* SPI Camera source to get video data from SPI Camera

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "record_src.h"
#include "record_src_cfg.h"
#include "media_lib_err.h"
#include "media_lib_os.h"
#include "usb_stream.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG                           "USB_Cam"

#define USB_CAM_DATA_ARRIVE_BIT       (1)
#define USB_CAM_DATA_CONSUMED_BIT     (1 << 1)

#define _SET_BITS(group, bit)          media_lib_event_group_set_bits(group, bit)
// Need manual clear bits
#define _WAIT_BITS(group, bit)                                            \
    media_lib_event_group_wait_bits(group, bit, MEDIA_LIB_MAX_LOCK_TIME); \
    media_lib_event_group_clr_bits(group, bit)

typedef struct {
    media_lib_event_grp_handle_t event;
    uint8_t                     *frame_buffer;
    uint8_t                     *transfer_buffer[2];
    uvc_frame_t                 *pic_frame;
    bool                         started;
} record_src_usb_cam_t;

static int power_on_usb_cam(bool on)
{
    if (USB_CAM_PIN_PWDN == -1) {
        return 0;
    }
    gpio_config_t usb_camera_pins_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (BIT64(USB_CAM_PIN_PWDN)),
        .pull_up_en = 0,
        .pull_down_en = 0,
    };
    gpio_config(&usb_camera_pins_cfg);
    gpio_set_level(USB_CAM_PIN_PWDN, 0);
    return 0;
}

void close_usb_cam(record_src_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    record_src_usb_cam_t *usb_cam_src = (record_src_usb_cam_t *) handle;
    if (usb_cam_src->started) {
        usb_cam_src->pic_frame = NULL;
        _SET_BITS(usb_cam_src->event, USB_CAM_DATA_CONSUMED_BIT);
        _SET_BITS(usb_cam_src->event, USB_CAM_DATA_ARRIVE_BIT);
        usb_cam_src->started = false;
        usb_streaming_stop();
    }
    power_on_usb_cam(false);
    if (usb_cam_src->event) {
        media_lib_event_group_destroy(usb_cam_src->event);
    }
    if (usb_cam_src->transfer_buffer[0]) {
        media_lib_free(usb_cam_src->transfer_buffer[0]);
    }
    if (usb_cam_src->transfer_buffer[1]) {
        media_lib_free(usb_cam_src->transfer_buffer[1]);
    }
    if (usb_cam_src->frame_buffer) {
        media_lib_free(usb_cam_src->frame_buffer);
    }
    free(usb_cam_src);
}

static void usb_cam_frame_cb(uvc_frame_t *frame, void *ptr)
{
    record_src_usb_cam_t *usb_cam_src = (record_src_usb_cam_t *) ptr;
    if (usb_cam_src->started == false) {
        return;
    }
    usb_cam_src->pic_frame = frame;
    _SET_BITS(usb_cam_src->event, USB_CAM_DATA_ARRIVE_BIT);
    _WAIT_BITS(usb_cam_src->event, USB_CAM_DATA_CONSUMED_BIT);
}

record_src_handle_t open_usb_cam(void *cfg, int cfg_size)
{
    if (cfg == NULL || cfg_size != sizeof(record_src_video_cfg_t)) {
        return NULL;
    }
    record_src_usb_cam_t *usb_cam_src = (record_src_usb_cam_t *) calloc(sizeof(record_src_usb_cam_t), 1);
    if (usb_cam_src == NULL) {
        ESP_LOGE(TAG, "No memory for resource");
        return NULL;
    }
    record_src_video_cfg_t *vid_cfg = (record_src_video_cfg_t *) cfg;
    do {
        if (vid_cfg->video_fmt != RECORD_SRC_VIDEO_FMT_JPEG) {
            ESP_LOGE(TAG, "Usb camera only support MJPEG");
            break;
        }
        media_lib_event_group_create(&usb_cam_src->event);
        if (usb_cam_src->event == NULL) {
            ESP_LOGE(TAG, "No memory for resource");
            break;
        }
        usb_cam_src->transfer_buffer[0] = media_lib_malloc(CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
        if (usb_cam_src->transfer_buffer[0] == NULL) {
            ESP_LOGE(TAG, "Fail to Allocate frame buffer size %d", CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
            break;
        }
        usb_cam_src->transfer_buffer[1] = media_lib_malloc(CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
        if (usb_cam_src->transfer_buffer[1] == NULL) {
            ESP_LOGE(TAG, "Fail to Allocate frame buffer size %d", CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
            break;
        }
        usb_cam_src->frame_buffer = media_lib_malloc(CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
        if (usb_cam_src->frame_buffer == NULL) {
            ESP_LOGE(TAG, "Fail to Allocate frame buffer size %d", CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE);
            break;
        }
        power_on_usb_cam(true);
        uvc_config_t uvc_config = {
            .frame_width = vid_cfg->width,
            .frame_height = vid_cfg->height,
            .frame_interval = FPS2INTERVAL(20),
            .xfer_buffer_size = CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE,
            .xfer_buffer_a = usb_cam_src->transfer_buffer[0],
            .xfer_buffer_b = usb_cam_src->transfer_buffer[1],
            .frame_buffer_size = CONFIG_USB_CAMERA_FRAME_BUFFER_SIZE,
            .frame_buffer = usb_cam_src->frame_buffer,
            .frame_cb = usb_cam_frame_cb,
            .frame_cb_arg = usb_cam_src,
        };
        ESP_LOGI(TAG, "Camera width %d height %d", vid_cfg->width, vid_cfg->height);
        esp_err_t ret = uvc_streaming_config(&uvc_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "UVC streaming config failed");
            break;
        }
        ret = usb_streaming_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Fail to start UVC stream");
            break;
        }
        usb_cam_src->started = true;
        return (record_src_handle_t) usb_cam_src;
    } while (0);
    close_usb_cam((record_src_handle_t) usb_cam_src);
    return NULL;
}

int read_usb_cam(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout)
{
    if (handle == NULL || frame_data == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_usb_cam_t *usb_cam_src = (record_src_usb_cam_t *) handle;
    int ret = _WAIT_BITS(usb_cam_src->event, USB_CAM_DATA_ARRIVE_BIT);
    if (ret <= 0 || usb_cam_src->pic_frame == NULL) {
        ESP_LOGE(TAG, "Wait for frame fail %d", ret);
        return ESP_MEDIA_ERR_READ_DATA;
    }
    frame_data->data = usb_cam_src->pic_frame->data;
    frame_data->size = usb_cam_src->pic_frame->data_bytes;
    return ESP_MEDIA_ERR_OK;
}

int unlock_usb_cam(record_src_handle_t handle)
{
    if (handle == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_usb_cam_t *usb_cam_src = (record_src_usb_cam_t *) handle;
    if (usb_cam_src->pic_frame) {
        _SET_BITS(usb_cam_src->event, USB_CAM_DATA_CONSUMED_BIT);
        usb_cam_src->pic_frame = NULL;
    }
    return ESP_MEDIA_ERR_OK;
}

int record_src_usb_cam_register()
{
    record_src_api_t usb_cam_record = {
        .open_record = open_usb_cam,
        .read_frame = read_usb_cam,
        .unlock_frame = unlock_usb_cam,
        .close_record = close_usb_cam,
    };
    return record_src_register(RECORD_SRC_TYPE_USB_CAM, &usb_cam_record);
}
