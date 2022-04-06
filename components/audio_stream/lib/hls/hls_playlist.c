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

#include <string.h>
#include <audio_mem.h>
#include <audio_error.h>
#include "esp_log.h"
#include "hls_playlist.h"
#include "join_path.h"
#include "hls_parse.h"

#define TAG "HLS_PLAYLIST"

#define MEDIA_FLAG_AUTO_SELECT (1)
#define MEDIA_FLAG_DEFAULT     (2)
#define MEDIA_FLAG_FORCED      (4)

#define HLS_MALLOC(type) (type*)audio_calloc(1, sizeof(type))
#define HLS_FREE(b)      if (b) {audio_free(b); b = NULL;}

/**
 * @brief Url tag information
 */
typedef struct {
    float duration;
    char* uri;
} hls_url_t;

/**
 * @brief Key tag information
 */
typedef struct {
    hls_encrypt_method_t method;
    char*                uri;
    uint8_t              k[16];
    uint8_t              iv[16];
    uint16_t             range[2];  /*!< Indicate key applied url range */
} hls_key_t;

/**
 * @brief HLS media information
 */
typedef struct {
    uint8_t    media_flag;
    hls_type_t type;
    char*      group_id;
    char*      name;
    char*      lang;
    char*      uri;
} hls_media_t;

/**
 * @brief HLS stream information
 */
typedef struct {
    uint16_t program_id;
    uint32_t bandwidth;
    char*    codec;
    char*    audio;
    char*    subtitle;
    char*    resolution;
    char*    uri;
} hls_stream_t;

/**
 * @brief HLS master playlist information
 */
typedef struct {
    uint8_t       ver;
    uint16_t      media_num;
    uint16_t      stream_num;
    hls_media_t*  media;
    hls_stream_t* stream;
    char*         uri;
} hls_master_playlist_t;

/**
 * @brief HLS media playlist information
 */
typedef struct {
    uint8_t              ver;             /*!< Version */
    hls_stream_t*        variant_stream;  /*!< Stream list of different bitrate */
    bool                 active;          /*!< Stream is used or not */
    hls_type_t           stream_type;     /*!< Stream type */
    hls_playlist_type_t  type;            /*!< Playlist type change over time */
    uint32_t             target_duration; /*!< Maximum segment duration */
    uint16_t             url_num;         /*!< Record of url number */
    uint16_t             url_alloc;       /*!< Allocated number of record */
    uint16_t             key_num;         /*!< Key number */
    uint16_t             current_url;     /*!< Current used url index in url_num */
    float                current_time;    /*!< Current time to determine when to reload playlist file */
    uint64_t             media_sequence;  /*!< Media sequence */
    hls_url_t*           url_items;       /*!< Record of url */
    hls_key_t*           key;             /*!< Record of key */
    char*                uri;             /*!< Base url of media playlist */
} hls_media_playlist_t;

/**
 * @brief Struct for HLS instance
 */
typedef struct {
    hls_playlist_cfg_t      cfg;              /*!< HLS playlist configuration */
    hls_parse_t             parser;           /*!< HLS parser */
    hls_file_type_t         type;             /*!< Master playlist or media playlist */
    uint8_t                 playlist_num;     /*!< Media playlist number */
    hls_master_playlist_t*  master_playlist;  /*!< Master playlist information */
    hls_media_playlist_t*   media_playlist;   /*!< Media playlist information */
} hls_t;

static int hls_fill_media_attr(hls_media_t* m, hls_tag_info_t* tag_info)
{
    for (int i = 0; i < tag_info->attr_num; i++) {
        switch (tag_info->k[i]) {
            case HLS_ATTR_AUTO_SELECT:
                m->media_flag |= tag_info->v[i].v ? MEDIA_FLAG_AUTO_SELECT : 0;
                break;
            case HLS_ATTR_DEFAULT:
                m->media_flag |= tag_info->v[i].v ? MEDIA_FLAG_DEFAULT : 0;
                break;
            case HLS_ATTR_FORCED:
                m->media_flag |= tag_info->v[i].v ? MEDIA_FLAG_FORCED : 0;
                break;
            case HLS_ATTR_TYPE:
                m->type = (hls_type_t)tag_info->v[i].v;
                break;
            case HLS_ATTR_GROUP_ID:
                m->group_id = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_NAME:
                m->name = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_LANGUAGE:
                m->lang = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_URI:
                m->uri = audio_strdup(tag_info->v[i].s);
                break;
            default:
                break;
        }
    }
    return 0;
}

static int hls_fill_stream_attr(hls_stream_t* s, hls_tag_info_t* tag_info)
{
    for (int i = 0; i < tag_info->attr_num; i++) {
        switch (tag_info->k[i]) {
            case HLS_ATTR_AUTO_SELECT:
                s->program_id = (uint16_t)tag_info->v[i].v;
                break;
            case HLS_ATTR_BANDWIDTH:
                s->bandwidth = (uint32_t)tag_info->v[i].v;
                break;
            case HLS_ATTR_CODECS:
                s->codec = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_AUDIO:
                s->audio = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_SUBTITLES:
                s->subtitle = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_RESOLUTION:
                s->resolution = audio_strdup(tag_info->v[i].s);
                break;
            case HLS_ATTR_URI:
                s->uri = audio_strdup(tag_info->v[i].s);
                break;
            default:
                break;
        }
    }
    return 0;
}

static int hls_main_tag_cb(hls_tag_info_t* tag_info, void* ctx)
{
    hls_master_playlist_t* master_playlist = (hls_master_playlist_t*)ctx;
    switch (tag_info->tag) {
        case HLS_TAG_VERSION:
            if (tag_info->attr_num) {
                master_playlist->ver = (uint8_t)tag_info->v[0].v;
            }
            break;
        case HLS_TAG_MEDIA:
            if (tag_info->attr_num) {
                hls_media_t* new_media = audio_realloc(master_playlist->media, sizeof(hls_media_t) * (master_playlist->media_num + 1));
                AUDIO_MEM_CHECK(TAG, new_media, break);
                if (new_media) {
                    master_playlist->media = new_media;
                    memset(&master_playlist->media[master_playlist->media_num], 0, sizeof(hls_media_t));
                    hls_fill_media_attr(&master_playlist->media[master_playlist->media_num], tag_info);
                    master_playlist->media_num++;
                }
            }
            break;
        case HLS_TAG_STREAM_INF:
            if (tag_info->attr_num) {
                hls_stream_t* new_stream = audio_realloc(master_playlist->stream, sizeof(hls_stream_t) * (master_playlist->stream_num + 1));
                AUDIO_MEM_CHECK(TAG, new_stream, break);
                if (new_stream) {
                    master_playlist->stream = new_stream;
                    memset(&master_playlist->stream[master_playlist->stream_num], 0, sizeof(hls_stream_t));
                    hls_fill_stream_attr(&master_playlist->stream[master_playlist->stream_num], tag_info);
                    master_playlist->stream_num++;
                }
            }
            break;
        case HLS_TAG_STREAM_INF_APPEND:
            if (master_playlist->stream && master_playlist->stream_num) {
                hls_fill_stream_attr(&master_playlist->stream[master_playlist->stream_num-1], tag_info);
            }
            break;
        default:
            break;
    }
    return 0;
}

static int hls_media_tag_cb(hls_tag_info_t* tag_info, void* ctx)
{
    hls_t* hls = (hls_t*)ctx;
    if (hls == NULL || hls->media_playlist == NULL) {
        return -1;
    }
    hls_media_playlist_t* media = hls->media_playlist;
    switch (tag_info->tag) {
        case HLS_TAG_ENDLIST:
            media->type = HLS_PLAYLIST_TYPE_VOD;
            break;
        case HLS_TAG_VERSION:
            if (tag_info->attr_num) {
                media->ver = (uint8_t)tag_info->v[0].v;
            }
            break;
        case HLS_TAG_PLAYLIST_TYPE:
            if (tag_info->attr_num) {
                media->type = (hls_playlist_type_t)tag_info->v[0].v;
            }
            break;
        case HLS_TAG_TARGET_DURATION:
            if (tag_info->attr_num) {
                media->target_duration = (uint32_t)tag_info->v[0].v;
            }
            break;
        case HLS_TAG_MEDIA_SEQUENCE:
            if (tag_info->attr_num) {
                media->media_sequence = tag_info->v[0].v;
            }
            break;
        case HLS_TAG_KEY:
            if (tag_info->attr_num) {
                //TODO how to process key
                ESP_LOGW(TAG, "key not support currently");
            }
            break;

        case HLS_TAG_INF_APPEND:
            for (int i = 0; i < tag_info->attr_num; i++) {
                switch (tag_info->k[i]) {
                    case HLS_ATTR_URI: {
                        char* url = join_url(media->uri, tag_info->v[i].s);
                        if (url) {
                            hls->cfg.cb(url, hls->cfg.ctx);
                            audio_free(url);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }
            break;

        default:
            break;
    }
    return 0;
}

static hls_stream_t* hls_filter_stream(hls_master_playlist_t* main, uint32_t bitrate)
{
    if (main->stream_num == 0) {
        return NULL;
    }
    int max_bitrate = 0;
    hls_stream_t* sel_stream = NULL;
    for (int i = 0; i < main->stream_num; i++) {
        hls_stream_t* stream = &main->stream[i];
        if (stream->bandwidth <= bitrate) {
            if (stream->bandwidth > max_bitrate) {
                max_bitrate = stream->bandwidth;
                sel_stream = stream;
            }
        }
    }
    if (sel_stream == NULL) {
        sel_stream = &main->stream[0];
    }
    return sel_stream;
}

static hls_media_t* hls_filter_media(hls_master_playlist_t* main, char* group_id, hls_type_t type)
{
    hls_media_t* sel = NULL;
    if (group_id == NULL) {
        return NULL;
    }
    for (int i = 0; i < main->media_num; i++) {
        hls_media_t* m = &main->media[i];
        if (m->uri && strcmp(m->group_id, group_id) == 0 && m->type == type) {
            if (sel == NULL) {
                sel = m;
            }
            else if ((m->media_flag & MEDIA_FLAG_AUTO_SELECT)) {
                return m;
            }
        }
    }
    return sel;
}

static int hls_close_master_playlist(hls_t* hls, hls_master_playlist_t* m)
{
    //TODO add lock to wait parse exit
    int i;
    for (i = 0; i < m->media_num; i++) {
        hls_media_t* media = &m->media[i];
        HLS_FREE(media->group_id);
        HLS_FREE(media->name);
        HLS_FREE(media->lang);
        HLS_FREE(media->uri);
    }
    HLS_FREE(m->media);
    m->media_num = 0;
    for (i = 0; i < m->stream_num; i++) {
        hls_stream_t* stream = &m->stream[i];
        HLS_FREE(stream->codec);
        HLS_FREE(stream->audio);
        HLS_FREE(stream->subtitle);
        HLS_FREE(stream->resolution);
        HLS_FREE(stream->uri);
    }
    HLS_FREE(m->stream);
    m->stream_num = 0;
    HLS_FREE(m->uri);
    return 0;
}

static int hls_close_media_playlist(hls_t* hls, hls_media_playlist_t* media)
{
    int i;
    // release key
    for (i = 0; i < media->key_num; i++) {
        hls_key_t* key = &media->key[i];
        HLS_FREE(key->uri);
    }
    media->key_num = 0;
    HLS_FREE(media->key);
    // release url
    for (i = 0; i < media->url_num; i++) {
        hls_url_t* url = &media->url_items[i];
        HLS_FREE(url->uri);
    }
    HLS_FREE(media->url_items);
    HLS_FREE(media->uri);
    return 0;
}

hls_handle_t hls_playlist_open(hls_playlist_cfg_t* cfg)
{
    hls_t* hls = HLS_MALLOC(hls_t);
    if (hls == NULL) {
        return NULL;
    }
    memcpy(&hls->cfg, cfg, sizeof(hls_playlist_cfg_t));
    if (hls_parse_init(&hls->parser) != 0) {
        HLS_FREE(hls);
        return NULL;
    }
    hls->cfg.uri = strdup(cfg->uri);
    return (hls_handle_t)hls;
}

bool hls_playlist_is_media_end(hls_handle_t h)
{
    hls_t* hls = (hls_t*)h;
    if (hls && hls->media_playlist) {
        return (hls->media_playlist->type == HLS_PLAYLIST_TYPE_VOD);
    }
    return false;
}

char* hls_playlist_get_prefer_url(hls_handle_t h, hls_stream_type_t type)
{
    hls_t* hls = (hls_t*) h;
    if (hls == NULL || hls->master_playlist == NULL) {
        return NULL;
    }
    hls_master_playlist_t* master_playlist = hls->master_playlist;
    hls_stream_t* stream = hls_filter_stream(master_playlist, hls->cfg.prefer_bitrate);
    if (stream == NULL || stream->uri == NULL) {
        return NULL;
    }
    hls_media_t* audio = hls_filter_media(master_playlist, stream->audio, HLS_TYPE_AUDIO);
    bool has_audio = (audio && audio->uri);
    char* uri = NULL;
    switch (type) {
        case HLS_STREAM_TYPE_VIDEO:
        case HLS_STREAM_TYPE_AV:
            uri = stream->uri;
            break;
        case HLS_STREAM_TYPE_AUDIO:
            uri = has_audio ? audio->uri : stream->uri;
        break;
        case HLS_STREAM_TYPE_SUBTITLE: {
            hls_media_t* subtitle = hls_filter_media(master_playlist, stream->subtitle, HLS_TYPE_SUBTITLES);
            if (subtitle) {
                uri = subtitle->uri;
            }
            break;
        }
        default:
            return NULL;
    }
    return uri ? join_url(hls->master_playlist->uri, uri) : NULL;
}

int hls_playlist_parse_data(hls_handle_t h, uint8_t* buffer, int size, bool eos)
{
    hls_t* hls = (hls_t*)h;
    if (hls == NULL) {
        return -1;
    }
    if (hls->type == HLS_FILE_TYPE_NONE) {
        hls->type = hls_get_file_type(buffer, size);
        if (hls->type == HLS_FILE_TYPE_NONE) {
            return -1;
        }
        if (hls->type == HLS_FILE_TYPE_MEDIA_PLAYLIST) {
            hls->media_playlist = HLS_MALLOC(hls_media_playlist_t);
            if (hls->media_playlist == NULL) {
                return -1;
            }
            hls->playlist_num = 1;
            hls->media_playlist->uri = hls->cfg.uri;
            hls->media_playlist->active = true;
            hls->media_playlist->stream_type = HLS_TYPE_AV;
            hls->cfg.uri = NULL;
        } else if (hls->type == HLS_FILE_TYPE_MASTER_PLAYLIST) {
            hls->master_playlist = HLS_MALLOC(hls_master_playlist_t);
            if (hls->master_playlist == NULL) {
                return -1;
            }
            hls->master_playlist->uri = hls->cfg.uri;
            hls->cfg.uri = NULL;
        }
    }
    hls_parse_add_buffer(&hls->parser, buffer, size, eos);
    if (hls->type == HLS_FILE_TYPE_MASTER_PLAYLIST) {
        hls_parse(&hls->parser, hls_main_tag_cb, hls->master_playlist);
    }
    if (hls->type == HLS_FILE_TYPE_MEDIA_PLAYLIST && hls->media_playlist) {
        hls_parse(&hls->parser, hls_media_tag_cb, hls);
    }
    return 0;
}

bool hls_playlist_is_master(hls_handle_t h)
{
    hls_t* hls = (hls_t*)h;
    return (hls && hls->type == HLS_FILE_TYPE_MASTER_PLAYLIST);
}

int hls_playlist_close(hls_handle_t h)
{
    hls_t* hls = (hls_t*)h;
    if (hls == NULL) {
        return -1;
    }
    if (hls->master_playlist) {
        hls_close_master_playlist(hls, hls->master_playlist);
        HLS_FREE(hls->master_playlist);
    }
    for (int i = 0; i < hls->playlist_num; i++) {
        hls_close_media_playlist(hls, &hls->media_playlist[i]);
    }
    hls_parse_deinit(&hls->parser);
    HLS_FREE(hls->media_playlist);
    HLS_FREE(hls->cfg.uri);
    HLS_FREE(hls);
    return 0;
}
