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
#ifndef _AV_STREAM_H
#define _AV_STREAM_H

#include "esp_peripherals.h"
#include "av_stream_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The AEC internal buffering mechanism requires that the recording signal
   is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal. */
#define DEFAULT_REF_DELAY_MS        0

/* Debug original input/output data for AEC feature*/
#define DEBUG_AEC_INPUT             false
#define DEBUG_AEC_OUTPUT            false

#define AUDIO_CODEC_SAMPLE_RATE     8000
#define PCM_FRAME_SIZE              320     // 20ms 8K 1channel
#define AAC_FRAME_SIZE              2*1024  // 16K 1channel with 1024 sample

typedef struct _av_stream_handle *av_stream_handle_t;

/**
 * @brief ESP RTC video info
 */
typedef enum {
    AV_ACODEC_NULL,
    AV_ACODEC_PCM,
    AV_ACODEC_G711A,
    AV_ACODEC_G711U,
    AV_ACODEC_AAC_LC,
    AV_ACODEC_OPUS,
} av_stream_acodec_t;

/**
 * @brief ESP RTC video info
 */
typedef enum {
    AV_VCODEC_NULL,
    AV_VCODEC_MJPEG,
    AV_VCODEC_H264,
} av_stream_vcodec_t;

/**
 * @brief AV stream frame type
 */
typedef struct {
    uint8_t    *data;
    uint32_t    len;
    uint64_t    pts;
} av_stream_frame_t;

/**
 * @brief AV stream configurations
 */
typedef struct {
    void                        *ctx;               /*!< AV stream user context */
    int8_t                      algo_mask;          /*!< Choose the algorithm to be used */
    av_stream_acodec_t          acodec_type;        /*!< Audio codec type */
    uint32_t                    acodec_samplerate;  /*!< If the sample rate is different with HAL, av_stream will resample to match it */
    av_stream_vcodec_t          vcodec_type;        /*!< Video codec type */
    av_stream_hal_config_t      hal;                /*!< Audio Video hal config */
} av_stream_config_t;

/**
 * @brief      Intialize av stream
 *
 * @param[in]  config    The av stream configuration
 *
 * @return
 *     - The av_stream handle if successfully created, NULL on error
 */
av_stream_handle_t av_stream_init(av_stream_config_t *config);

/**
 * @brief      Deintialize av stream
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_stream_deinit(av_stream_handle_t av_stream);

/**
 * @brief      Start audio encoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_audio_enc_start(av_stream_handle_t av_stream);

/**
 * @brief      Stop audio encoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_audio_enc_stop(av_stream_handle_t av_stream);

/**
 * @brief      Start audio decoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_audio_dec_start(av_stream_handle_t av_stream);

/**
 * @brief      Stop audio decoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_audio_dec_stop(av_stream_handle_t av_stream);

/**
 * @brief      Start video encoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_video_enc_start(av_stream_handle_t av_stream);

/**
 * @brief      Stop video encoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_video_enc_stop(av_stream_handle_t av_stream);

/**
 * @brief      Start video decoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_video_dec_start(av_stream_handle_t av_stream);

/**
 * @brief      Stop video decoder
 *
 * @param[in]  av_stream   The av_stream handle
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_video_dec_stop(av_stream_handle_t av_stream);

/**
 * @brief      Read audio encoded data
 *
 * @param[in]  frame    The frame handle
 * @param[in]  ctx      The user context
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL on read timeout
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_audio_enc_read(av_stream_frame_t *frame, void *ctx);

/**
 * @brief      Write audio encoded data to decode and play
 *
 * @param[in]  frame    The frame handle
 * @param[in]  ctx      The user context
 *
 * @return
 *     - ESP_OK on success
 *     - other on decoder error
 */
int av_audio_dec_write(av_stream_frame_t *frame, void *ctx);

/**
 * @brief      Read video encoded data
 *
 * @param[in]  frame    The frame handle
 * @param[in]  ctx      The user context
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL on read timeout
 *     - ESP_ERR_INVALID_ARG on wrong handle
 */
int av_video_enc_read(av_stream_frame_t *frame, void *ctx);

/**
 * @brief      Write video encoded data to decode and render
 *
 * @param[in]  frame    The frame handle
 * @param[in]  ctx      The user context
 *
 * @return
 *     - ESP_OK on success
 *     - other on decoder error
 */
int av_video_dec_write(av_stream_frame_t *frame, void *ctx);

/**
 * @brief      Set audio volume
 *
 * @param[in]  av_stream    The av_stream handle
 * @param[in]  vol          Value of volume in percent(%)
 *
 * @return
 *     - ESP_OK on success
 *     - other on decoder error
 */
int av_audio_set_vol(av_stream_handle_t av_stream, int vol);

/**
 * @brief      Get audio volume
 *
 * @param[in]   av_stream   The av_stream handle
 * @param[out]  vol         Value of volume in percent returned(%)
 *
 * @return
 *     - ESP_OK on success
 *     - other on decoder error
 */
int av_audio_get_vol(av_stream_handle_t av_stream, int *vol);

#ifdef __cplusplus
}
#endif

#endif