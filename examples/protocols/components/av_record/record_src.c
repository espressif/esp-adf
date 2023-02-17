/* Record input source to get audio and video data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "record_src.h"
#include "media_lib_err.h"

static record_src_api_t *i2s_aud_api;
static record_src_api_t *dvp_cam_api;
static record_src_api_t *usb_cam_api;

typedef struct {
    record_src_api_t   *record_api;
    record_src_handle_t handle;
} record_src_t;

record_src_api_t **get_record_api(record_src_type_t type)
{
    record_src_api_t **src_api = NULL;
    switch (type) {
        default:
            return NULL;
        case RECORD_SRC_TYPE_I2S_AUD:
            src_api = &i2s_aud_api;
            break;
        case RECORD_SRC_TYPE_SPI_CAM:
            src_api = &dvp_cam_api;
            break;
        case RECORD_SRC_TYPE_USB_CAM:
            src_api = &usb_cam_api;
            break;
    }
    return src_api;
}

void record_src_unregister_all()
{
    if (i2s_aud_api) {
        free(i2s_aud_api);
        i2s_aud_api = NULL;
    }
    if (dvp_cam_api) {
        free(dvp_cam_api);
        dvp_cam_api = NULL;
    }
    if (usb_cam_api) {
        free(usb_cam_api);
        usb_cam_api = NULL;
    }
}

int record_src_register(record_src_type_t type, record_src_api_t *record_api)
{
    if (record_api == NULL || record_api->open_record == NULL || record_api->read_frame == NULL ||
        record_api->close_record == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_api_t **src_api = get_record_api(type);
    if (src_api == NULL) {
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    if (*src_api) {
        return ESP_MEDIA_ERR_OK;
    }
    *src_api = (record_src_api_t *) calloc(sizeof(record_src_api_t), 1);
    if (*src_api == NULL) {
        return ESP_MEDIA_ERR_NO_MEM;
    }
    memcpy(*src_api, record_api, sizeof(record_src_api_t));
    return ESP_MEDIA_ERR_OK;
}

record_src_handle_t record_src_open(record_src_type_t type, void *cfg, int cfg_size)
{
    record_src_api_t **src_api = get_record_api(type);
    if (src_api == NULL || *src_api == NULL) {
        return NULL;
    }
    record_src_t *record_src = calloc(sizeof(record_src_t), 1);
    if (record_src == NULL) {
        return NULL;
    }
    record_src->record_api = *src_api;
    record_src->handle = record_src->record_api->open_record(cfg, cfg_size);
    if (record_src->handle == NULL) {
        free(record_src);
        return NULL;
    }
    return (record_src_handle_t) record_src;
}

int record_src_read_frame(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout)
{
    record_src_t *record_src = (record_src_t *) handle;
    if (record_src == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    return record_src->record_api->read_frame(record_src->handle, frame_data, timeout);
}

int record_src_unlock_frame(record_src_handle_t handle)
{
    record_src_t *record_src = (record_src_t *) handle;
    if (record_src == NULL) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    if (record_src->record_api->unlock_frame == NULL) {
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    return record_src->record_api->unlock_frame(record_src->handle);
}

void record_src_close(record_src_handle_t handle)
{
    record_src_t *record_src = (record_src_t *) handle;
    if (record_src == NULL) {
        return;
    }
    record_src->record_api->close_record(record_src->handle);
    free(record_src);
}

int record_src_register_default()
{
    int ret = record_src_i2s_aud_register();
    if (ret != 0) {
        return ret;
    }
#ifdef CONFIG_DVP_CAMERA_SUPPORT
    ret = record_src_dvp_cam_register();
    if (ret != 0) {
        return ret;
    }
#endif
#ifdef CONFIG_USB_CAMERA_SUPPORT
    ret = record_src_usb_cam_register();
    if (ret != 0) {
        return ret;
    }
#endif
    return ret;
}
