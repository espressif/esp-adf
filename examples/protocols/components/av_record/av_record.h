/* AV Record module to record video and audio data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef _AV_RECORD_H
#define _AV_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include "record_src.h"
#ifdef __cplusplus
extern "C" {
#endif

#define AV_RECORD_AUDIO_FLAG (1)
#define AV_RECORD_VIDEO_FLAG (2)

typedef enum {
    AV_RECORD_STREAM_TYPE_NONE,
    AV_RECORD_STREAM_TYPE_AUDIO,
    AV_RECORD_STREAM_TYPE_VIDEO,
} av_record_stream_type_t;

typedef struct {
    av_record_stream_type_t stream_type; /*!< Stream type */
    uint32_t                pts;         /*!< PTS of current stream */
    uint8_t                *data;        /*!< Stream data */
    uint32_t                size;        /*!< Stream data size */
} av_record_data_t;

/**
 * @brief Record output data callback
 */
typedef int (*_av_record_data_cb)(av_record_data_t *data, void *ctx);

/**
 * @brief Record video quality setting
 */
typedef enum {
    AV_RECORD_VIDEO_QUALITY_QVGA = 0, /*!< 320x240 */
    AV_RECORD_VIDEO_QUALITY_HVGA,     /*!< 480x320 */
    AV_RECORD_VIDEO_QUALITY_VGA,      /*!< 640x480 */
    AV_RECORD_VIDEO_QUALITY_800X480,  /*!< 800x480 */
    AV_RECORD_VIDEO_QUALITY_XVGA,     /*!< 1024x768 */
    AV_RECORD_VIDEO_QUALITY_HD,       /*!< 1280x720 */
    AV_RECORD_VIDEO_QUALITY_FHD,      /*!< 1920x1080 */
} av_record_video_quality_t;

typedef enum {
    AV_RECORD_VIDEO_FMT_NONE,
    AV_RECORD_VIDEO_FMT_MJPEG,
    AV_RECORD_VIDEO_FMT_H264,
} av_record_video_fmt_t;

typedef enum {
    AV_RECORD_AUDIO_FMT_NONE,
    AV_RECORD_AUDIO_FMT_PCM,
    AV_RECORD_AUDIO_FMT_G711A,
    AV_RECORD_AUDIO_FMT_G711U,
    AV_RECORD_AUDIO_FMT_AAC,
} av_record_audio_fmt_t;

/**
 * @brief AV Record configuration
 */
typedef struct {
    record_src_type_t         audio_src;         /*!< Audio source type */
    record_src_type_t         video_src;         /*!< Video source type */
    av_record_video_fmt_t     video_fmt;         /*!< Video codec format */
    av_record_audio_fmt_t     audio_fmt;         /*!< Audio codec format */
    bool                      sw_jpeg_enc;       /*!< Use software jpeg encoder */
    av_record_video_quality_t video_quality;     /*!< Video quality */
    uint8_t                   video_fps;         /*!< Video frames per second */
    uint16_t                  audio_sample_rate; /*!< Audio sample rate */
    uint8_t                   audio_channel;     /*!< Audio channel */
    _av_record_data_cb        data_cb;           /*!< Record data callback */
    void                     *ctx;               /*!< Record input context */
} av_record_cfg_t;

/**
 * @brief         Start record process
 *
 * @param         cfg: Recorder configuration
 * @return        - 0: On Record
 *                - Others: Fail to start
 */
int av_record_start(av_record_cfg_t *cfg);

/**
 * @brief         Stop record process
 * @return        - 0: On success
 *                - Others: Fail to stop
 */
int av_record_stop();

/**
 * @brief         Check whether record is running or not
 * @return        - true: Still running
 *                - false: Not running
 */
bool av_record_running();

/**
 * @brief         Get current recording PTS
 * @return        Maximum PTS of video and audio
 */
uint32_t av_record_get_pts();

/**
 * @brief         Get actual width and height from enum value
 * @param         quality: Video quality setting
 * @param[out]    width: Video width
 * @param[out]    height: Video height
 */
void av_record_get_video_size(av_record_video_quality_t quality, uint16_t *width, uint16_t *height);

/**
 * @brief         Get audio format string
 * @param         afmt: Audio format
 * @return        Audio format string
 */
const char* av_record_get_afmt_str(av_record_audio_fmt_t afmt);

/**
 * @brief         Get video format string
 * @param         vfmt: Audio format
 * @return        Audio format string
 */
const char* av_record_get_vfmt_str(av_record_video_fmt_t vfmt);

#ifdef __cplusplus
}
#endif

#endif
