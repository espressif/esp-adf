/* Record input source to get audio and video data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _RECORD_SRC_H
#define _RECORD_SRC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *record_src_handle_t;

/**
 * @brief Record source type
 */
typedef enum {
    RECORD_SRC_TYPE_NONE,
    RECORD_SRC_TYPE_I2S_AUD,
    RECORD_SRC_TYPE_SPI_CAM,
    RECORD_SRC_TYPE_USB_CAM
} record_src_type_t;

/**
 * @brief Video format of video source
 */
typedef enum {
    RECORD_SRC_VIDEO_FMT_NONE,
    RECORD_SRC_VIDEO_FMT_YUV422,
    RECORD_SRC_VIDEO_FMT_YUV420,
    RECORD_SRC_VIDEO_FMT_JPEG,
} record_src_video_fmt_t;

/**
 * @brief Video configuration for general video source
 */
typedef struct {
    record_src_video_fmt_t video_fmt; /*!< Video codec format */
    uint16_t               width;     /*!< Video width */
    uint16_t               height;    /*!< Video height */
    uint8_t                fps;       /*!< Video frames per second */
    bool                   reuse_i2c; /*!< Reuse I2C settings when use spi camera */
} record_src_video_cfg_t;

/**
 * @brief Audio configuration for general audio source
 */
typedef struct {
    uint16_t sample_rate;     /*!< Audio sample rate */
    uint8_t  channel;         /*!< Audio channel */
    uint8_t  bits_per_sample; /*!< Audio channel */
} record_src_audio_cfg_t;

/**
 * @brief Frame data of audio and video
 */
typedef struct {
    uint8_t *data;
    int      size;
} record_src_frame_data_t;

/**
 * @brief Record source API
 */
typedef struct {
    record_src_handle_t (*open_record)(void *cfg, int cfg_size);
    int (*read_frame)(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout);
    int (*unlock_frame)(record_src_handle_t handle);
    void (*close_record)(record_src_handle_t handle);
} record_src_api_t;


/**
 * @brief         Register record source according configuration
 * @return        - ESP_MEDIA_ERR_OK: Register success
 *                - Others: Fail to register
 */
int record_src_register_default();

/**
 * @brief         Register of record source
 *
 * @param         type: Record source type
 * @param         record_api: Record source API
 * @return        - ESP_MEDIA_ERR_OK: Register success
 *                - Others: Fail to register
 */
int record_src_register(record_src_type_t type, record_src_api_t *record_api);

/**
 * @brief         Register of I2S audio source
 *
 * @return        - ESP_MEDIA_ERR_OK: Register success
 *                - Others: Fail to register
 */
int record_src_i2s_aud_register();

/**
 * @brief         Register of SPI Camera source
 *
 * @return        - ESP_MEDIA_ERR_OK: Register success
 *                - Others: Fail to register
 */
int record_src_dvp_cam_register();

/**
 * @brief         Register of USB Camera source
 *
 * @return        - ESP_MEDIA_ERR_OK: Register success
 *                - Others: Fail to register
 */
int record_src_usb_cam_register();

/**
 * @brief         Unregister of all record source
 */
void record_src_unregister_all();

/**
 * @brief         Open driver for record source
 *
 * @param         type: Record source type
 * @param         cfg: Configuration of record source either using `record_src_video_cfg_t` or `record_src_audio_cfg_t`
 * @param         cfg_size: Size of configuration struct
 * @return        - NULL: Fail to open
 *                - Others: Handle for record source
 */
record_src_handle_t record_src_open(record_src_type_t type, void *cfg, int cfg_size);

/**
 * @brief         Read frame data from record source
 *
 * @param         handle: Record source handle
 * @param         frame_data: Frame data to be read
 * @param         timeout: Read timeout setting (unit ms)
 * @return        - ESP_MEDIA_ERR_OK: Read Frame success
 *                - Others: Fail to read frame
 */
int record_src_read_frame(record_src_handle_t handle, record_src_frame_data_t *frame_data, int timeout);

/**
 * @brief         Unlock frame data
 *                It's special design for video record for frame size not fixed
 * @param         handle: Record source handle
 * @return        - ESP_MEDIA_ERR_OK: Unlock video frame success
 *                - Others: Fail to read frame
 */
int record_src_unlock_frame(record_src_handle_t handle);

/**
 * @brief         Close record source
 * @param         handle: Record source handle
 */
void record_src_close(record_src_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif
