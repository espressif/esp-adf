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

#ifndef HLS_PARSE_H
#define HLS_PARSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "line_reader.h"

#define HLS_MAX_ATTR_NUM             (16)
#define HLS_MAX_LINE_CHAR            (512)

#define HLS_STR_AES_128              "AES-128"
#define HLS_STR_AUDIO                "AUDIO"
#define HLS_STR_AUTOSELECT           "AUTOSELECT"
#define HLS_STR_AUTO_SELECT          "AUTO-SELECT"
#define HLS_STR_BANDWIDTH            "BANDWIDTH"
#define HLS_STR_BYTERANGE            "BYTERANGE"
#define HLS_STR_CLOSED_CAPTIONS      "CLOSED-CAPTIONS"
#define HLS_STR_CODECS               "CODECS"
#define HLS_STR_DEFAULT              "DEFAULT"
#define HLS_STR_DISCONTINUITY        "DISCONTINUITY"
#define HLS_STR_DURATION             "DURATION"
#define HLS_STR_ENDLIST              "ENDLIST"
#define HLS_STR_EVENT                "EVENT"
#define HLS_STR_EXT                  "#EXT"
#define HLS_STR_EXTINF               "#EXTINF:"
#define HLS_STR_EXTM3U               "#EXTM3U"
#define HLS_STR_EXT_X_               "#EXT-X-"
#define HLS_STR_EXT_X_MEDIA          "#EXT-X-MEDIA:"
#define HLS_STR_EXT_X_STREAM_INF     "#EXT-X-STREAM-INF:"
#define HLS_STR_FORCED               "FORCED"
#define HLS_STR_GROUP_ID             "GROUP-ID"
#define HLS_STR_IGNORE               "IGNORE"
#define HLS_STR_INDEPENDENT_SEGMENTS "INDEPENDENT-SEGMENTS"
#define HLS_STR_INF                  "INF"
#define HLS_STR_INT                  "INT"
#define HLS_STR_IV                   "IV"
#define HLS_STR_I_FRAME_STREAM_INF   "I-FRAME-STREAM-INF"
#define HLS_STR_KEY                  "KEY"
#define HLS_STR_KEYFORMAT            "KEYFORMAT"
#define HLS_STR_KEYFORMATVERSION     "KEYFORMATVERSION"
#define HLS_STR_LANGUAGE             "LANGUAGE"
#define HLS_STR_MAP                  "MAP"
#define HLS_STR_MEDIA                "MEDIA"
#define HLS_STR_MEDIA_SEQUENCE       "MEDIA-SEQUENCE"
#define HLS_STR_METHOD               "METHOD"
#define HLS_STR_NAME                 "NAME"
#define HLS_STR_PLAYLIST_TYPE        "PLAYLIST-TYPE"
#define HLS_STR_PROGRAM_ID           "PROGRAM-ID"
#define HLS_STR_RESOLUTION           "RESOLUTION"
#define HLS_STR_SAMPLE_AES           "SAMPLE-AES"
#define HLS_STR_SESSION_KEY          "SESSION-KEY"
#define HLS_STR_STREAM_INF           "STREAM-INF"
#define HLS_STR_SUBTITLES            "SUBTITLES"
#define HLS_STR_TARGETDURATION       "TARGETDURATION"
#define HLS_STR_TITLE                "TITLE"
#define HLS_STR_TYPE                 "TYPE"
#define HLS_STR_URI                  "URI"
#define HLS_STR_VERSION              "VERSION"
#define HLS_STR_VIDEO                "VIDEO"
#define HLS_STR_VOD                  "VOD"
#define HLS_STR_YES                  "YES"

/**
 * @brief HLS tag type
 */
typedef enum {
    HLS_TAG_IGNORE,
    HLS_TAG_VERSION,
    HLS_TAG_MEDIA_SEQUENCE,
    HLS_TAG_TARGET_DURATION,
    HLS_TAG_INF,
    HLS_TAG_MEDIA,
    HLS_TAG_STREAM_INF,
    HLS_TAG_I_FRAME_STREAM_INF,
    HLS_TAG_KEY,
    HLS_TAG_SESSION_KEY,
    HLS_TAG_BYTE_RANGE,
    HLS_TAG_PLAYLIST_TYPE,
    HLS_TAG_MAP,
    HLS_TAG_DISCONTINUITY,
    HLS_TAG_INDEPENDENT_SEGMENTS,
    HLS_TAG_INF_APPEND,       // Fake tag to avoid strdup
    HLS_TAG_STREAM_INF_APPEND,
    HLS_TAG_ENDLIST,
} hls_tag_t;

/**
 * @brief Attribute type of HLS tag
 */
typedef enum {
    HLS_ATTR_IGNORE,
    HLS_ATTR_VERSION,
    HLS_ATTR_DURATION,
    HLS_ATTR_TITLE,
    HLS_ATTR_TYPE,
    HLS_ATTR_GROUP_ID,
    HLS_ATTR_NAME,
    HLS_ATTR_LANGUAGE,
    HLS_ATTR_AUTO_SELECT,
    HLS_ATTR_URI,
    HLS_ATTR_BANDWIDTH,
    HLS_ATTR_CODECS,
    HLS_ATTR_RESOLUTION,
    HLS_ATTR_DEFAULT,
    HLS_ATTR_FORCED,
    HLS_ATTR_AUDIO,
    HLS_ATTR_SUBTITLES,
    HLS_ATTR_PROGRAM_ID,
    HLS_ATTR_INT,
    HLS_ATTR_METHOD,
    HLS_ATTR_IV,
    HLS_ATTR_KEYFORMAT,
    HLS_ATTR_KEYFORMAT_VERSION,
} hls_attr_t;

/**
 * @brief HLS stream type
 */
typedef enum {
    HLS_TYPE_AUDIO,          /*!< Contain audio */
    HLS_TYPE_VIDEO,          /*!< Contain video */
    HLS_TYPE_SUBTITLES,      /*!< Contain subtitle */
    HLS_TYPE_CLOSED_CAPTION, /*!< Contain close caption */
    HLS_TYPE_AV,             /*!< Contain both audio and video in one url */
} hls_type_t;

/**
 * @brief HLS encrypt type
 */
typedef enum {
    HLS_ENCRYPT_METHOD_NONE,
    HLS_ENCRYPT_METHOD_AES128,
    HLS_ENCRYPT_METHOD_SAMPLE_AES,
} hls_encrypt_method_t;

/**
 * @brief File type of HLS playlist
 */
typedef enum {
    HLS_FILE_TYPE_NONE,
    HLS_FILE_TYPE_MASTER_PLAYLIST,
    HLS_FILE_TYPE_MEDIA_PLAYLIST
} hls_file_type_t;

/**
 * @brief Type of HLS playlist
 */
typedef enum {
   HLS_PLAYLIST_TYPE_EVENT,  /*!< Playlist file is dynamic. Need reload. */
   HLS_PLAYLIST_TYPE_VOD,    /*!< Playlist file is constant. Need load only once. */
} hls_playlist_type_t;

/**
 * @brief Attribute union of HLS tag
 */
typedef union {
    uint64_t   v;  /*!< Decimal integer */
    float      f;  /*!< Float */
    char*      s;  /*!< String */
} hls_attr_value_t;

/**
 * @brief Struct for HLS parser
 */
typedef struct {
    line_reader_t*   reader;                   /*!< Line reader instance */
    hls_tag_t        tag;                      /*!< Type of HLS tag */
    int              attr_num;                 /*!< Attribute number of HLS tag */
    char*            attr[HLS_MAX_ATTR_NUM];   /*!< Attribute string of HLS tag */
    hls_attr_t       k[HLS_MAX_ATTR_NUM];      /*!< Attribute type of HLS tag */
    hls_attr_value_t v[HLS_MAX_ATTR_NUM];      /*!< Attribute value of HLS tag */
} hls_parse_t;

/**
 * @brief HLS tag information
 */
typedef struct {
    hls_tag_t         tag;                     /*!< Type of HLS tag */
    int               attr_num;                /*!< Attribute number of HLS tag */
    hls_attr_t*       k;                       /*!< Attribute type of HLS tag */
    hls_attr_value_t* v;                       /*!< Attribute value of HLS tag */
} hls_tag_info_t;

typedef int (*hls_tag_callback) (hls_tag_info_t* tag_info, void* ctx);

/**
 * @brief       Check whether input buffer match an Extended M3U Playlist file
 *
 * @param       b: Input Data
 * @param       len: Input Data length
 * @return      -true: Matched ok
 *              -false: Not matched
 */
bool hls_matched(uint8_t* b, int len);

/**
 * @brief       Check HLS file type
 *
 * @param       b: Input Data
 * @param       len: Input Data length
 * @return      Recognize HLS file type media playlist file or master playlist file
 *              -HLS_FILE_TYPE_NONE on fail
 */
hls_file_type_t hls_get_file_type(uint8_t* b, int len);

/**
 * @brief       Initialize HLS parser
 *
 * @param       parser: HLS parser struct
 * @return      -0: On success otherwise fail
 */
int hls_parse_init(hls_parse_t* parser);

/**
 * @brief       Add buffer for HLS parser
 *
 * @param       parser: HLS parser instance
 * @param       buffer: Buffer to be parsed
 * @param       size: Buffer size
 * @param       eos: Whether it is last buffer or not
 * @return      -0: On success otherwise parser not initialized
 */
int hls_parse_add_buffer(hls_parse_t* parser, uint8_t* buffer, int size, bool eos);

/**
 * @brief       Start parsing of input data
 *
 * @param       parser: HLS parser instance
 * @param       cb: HLS tag callback
 * @param       ctx: Input context
 * @return      -0: On success otherwise parse fail
 */
int hls_parse(hls_parse_t* parser, hls_tag_callback cb, void* ctx);

/**
 * @brief       Deinitialize of HLS parser
 *
 * @param       parser HLS parser instance
 */
void hls_parse_deinit(hls_parse_t* parser);

/**
 * @brief       Convert HLS tag to string 
 *
 * @param       tag: HLS tag
 * @return      String representation of tag
 */
const char* hls_tag2str(hls_tag_t tag);

/**
 * @brief       Convert HLS tag attribute to string 
 *
 * @param       attr: HLS tag attribute
 * @return      String representation of tag attribute
 */
const char* hls_attr2str(hls_attr_t attr);

#ifdef __cplusplus
}
#endif

#endif
