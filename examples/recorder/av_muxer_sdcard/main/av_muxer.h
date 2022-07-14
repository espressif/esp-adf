/* AV muxer module to record video and audio data and mux into media file

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _AV_MUXER_H
#define _AV_MUXER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AV_MUXER_AUDIO_FLAG (1)
#define AV_MUXER_VIDEO_FLAG (2)

/**
 * @brief Muxer output data callback
 */
typedef int (*_av_muxer_data_cb)(uint8_t* data, uint32_t size, void* ctx);

/**
 * @brief Muxer video quality setting
 */
typedef enum {
    MUXER_VIDEO_QUALITY_QVGA = 0, /*!< 320x240 */
    MUXER_VIDEO_QUALITY_HVGA,     /*!< 480x320 */
    MUXER_VIDEO_QUALITY_VGA,      /*!< 640x480 */
    MUXER_VIDEO_QUALITY_XVGA,     /*!< 1024x768 */
    MUXER_VIDEO_QUALITY_HD,       /*!< 1280x720 */
} muxer_video_quality_t;

/**
 * @brief AV Muxer configuration
 */
typedef struct {
    char                  fmt[4];            /*!< Container format */
    muxer_video_quality_t quality;           /*!< Video quality */
    uint8_t               muxer_flag;        /*!< Muxer flag, muxer audio or video or both */
    uint16_t              audio_sample_rate; /*!< Audio sample rate */
    _av_muxer_data_cb     data_cb;           /*!< Muxer data callback */
    void*                 ctx;               /*!< Muxer input context */
    bool                  save_to_file;      /*!< Save to file or not */
} av_muxer_cfg_t;

/**
 * @brief         Start muxer process
 *
 * @param         cfg: Muxer configuration
 * @return        - 0: On sucess
 *                - Others: Fail to start
 */
int av_muxer_start(av_muxer_cfg_t* cfg);

/**
 * @brief         Stop muxer process
 * @return        - 0: On sucess
 *                - Others: Fail to start
 */
int av_muxer_stop();

/**
 * @brief         Check whether muxer is running or not
 * @return        - true: Still running
 *                - false: Not running
 */
bool av_muxer_running();

/**
 * @brief         Get current mux recording PTS
 * @return        Maximum PTS of video and audio
 */
uint32_t av_muxer_get_pts();

#ifdef __cplusplus
}
#endif

#endif
