/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _HLS_PLAYLIST_H
#define _HLS_PLAYLIST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* hls_handle_t;

/**
 * @brief HLS stream type
 */
typedef enum {
   HLS_STREAM_TYPE_AUDIO,    /*!< Audio stream type */
   HLS_STREAM_TYPE_VIDEO,    /*!< Video stream type */
   HLS_STREAM_TYPE_AV,       /*!< Audio and Video stream type */
   HLS_STREAM_TYPE_SUBTITLE, /*!< Subtitle stream type */
} hls_stream_type_t;

/**
 * @brief Callback for HLS media uri
 */
typedef int (*hls_uri_callback) (char* uri, void* tag);

/**
 * @brief HLS playlist parser configuration
 */
typedef struct {
    uint32_t         prefer_bitrate;   /*!< Prefer bitrate used to filter media playlist */
    hls_uri_callback cb;               /*!< HLS media stream uri callback */
    void*            ctx;              /*!< Input context */
    char*            uri;              /*!< M3U8 host url */
} hls_playlist_cfg_t;

/**
 * @brief         Open HLS playlist parser
 *
 * @param         cfg: HLS filter configuration
 * @return        HLS parse handle
 */
hls_handle_t hls_playlist_open(hls_playlist_cfg_t* cfg);

/**
 * @brief         Check whether playlist is master playlist
 *
 * @param         h: HLS handle
 * @return        -true: Master playlist
 *                -false: Not master playlist
 */
bool hls_playlist_is_master(hls_handle_t h);

/**
 * @brief         Check whether media playlist contain #END-PLAYLIST tag
 *
 * @param         h: HLS handle
 * @return        -true: Contain end tag
 *                -false: Not contain
 */
bool hls_playlist_is_media_end(hls_handle_t h);

/**
 * @brief         Filter url by stream type and given bitrate
 *
 * @param         h: HLS handle
 * @param         type: HLS stream type
 * @return        Filtered stream url
 */
char* hls_playlist_get_prefer_url(hls_handle_t h, hls_stream_type_t type);

/**
 * @brief           Parse data of HLS playlist
 *
 * @param           h: HLS handle
 * @param           data: Data of m3u8 playlist
 * @param           size: Input data size
 * @param           eos: Whether input data reach file end or not
 * @return          -0: On sucess
 *                  other: Parse fail
 */
int hls_playlist_parse_data(hls_handle_t h, uint8_t* data, int size, bool eos);

/**
 * @brief         Close parse for HLS playlist
 *
 * @param           h: HLS handle
 * @return          -0: On sucess
 *                  other: Invalid input
 */
int hls_playlist_close(hls_handle_t h);

#ifdef __cplusplus
}
#endif

#endif
