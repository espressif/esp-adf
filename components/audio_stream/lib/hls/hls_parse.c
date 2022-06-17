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

#include "esp_log.h"
#include "hls_parse.h"

#define TAG "HLS_PARSER"

#define MEM_SAME(a, b) (memcmp(a, b, sizeof(b)-1) == 0)
#define STR_SAME(a, b) (strcmp((char*)a, b) == 0)

static hls_playlist_type_t hls_get_playlist_type(char* attr)
{
    if (STR_SAME(attr, HLS_STR_EVENT)) {
        return HLS_PLAYLIST_TYPE_EVENT;
    }
    if (STR_SAME(attr, HLS_STR_VOD)) {
        return HLS_PLAYLIST_TYPE_VOD;
    }
    return HLS_PLAYLIST_TYPE_EVENT;
}

static hls_type_t hls_get_type(char* attr)
{
    if (STR_SAME(attr, HLS_STR_AUDIO)) {
        return HLS_TYPE_AUDIO;
    }
    if (STR_SAME(attr, HLS_STR_VIDEO)) {
        return HLS_TYPE_VIDEO;
    }
    if (STR_SAME(attr, HLS_STR_SUBTITLES)) {
        return HLS_TYPE_SUBTITLES;
    }
    if (STR_SAME(attr, HLS_STR_CLOSED_CAPTIONS)) {
        return HLS_TYPE_CLOSED_CAPTION;
    }
    return HLS_TYPE_AUDIO;
}

static int hls_get_bool_value(char* attr)
{
    if (STR_SAME(attr, HLS_STR_YES)) {
        return 1;
    }
    return 0;
}

static uint64_t hls_get_int_value(char* attr)
{
    return (uint64_t)atoll(attr);
}

static int hls_get_float_value(char* attr)
{
    return atof(attr);
}

static hls_encrypt_method_t hls_get_method(char* attr)
{
    if (STR_SAME(attr, HLS_STR_AES_128)) {
        return HLS_ENCRYPT_METHOD_AES128;
    }
    if (STR_SAME(attr, HLS_STR_SAMPLE_AES)) {
        return HLS_ENCRYPT_METHOD_SAMPLE_AES;
    }
    return HLS_ENCRYPT_METHOD_NONE;
}

static hls_attr_t hls_get_attr(char* attr)
{
    switch (*attr) {
        case 'A':
            if (STR_SAME(attr, HLS_STR_AUTOSELECT)) {
                return HLS_ATTR_AUTO_SELECT;
            }
            if (STR_SAME(attr, HLS_STR_AUDIO)) {
                return HLS_ATTR_AUDIO;
            }
            break;
        case 'B':
            if (STR_SAME(attr, HLS_STR_BANDWIDTH)) {
                return HLS_ATTR_BANDWIDTH;
            }
            break;
        case 'C':
            if (STR_SAME(attr, HLS_STR_CODECS)) {
                return HLS_ATTR_CODECS;
            }
            break;
        case 'D':
            if (STR_SAME(attr, HLS_STR_DEFAULT)) {
                return HLS_ATTR_DEFAULT;
            }
            break;
        case 'F':
            if (STR_SAME(attr, HLS_STR_FORCED)) {
                return HLS_ATTR_FORCED;
            }
            break;
        case 'G':
            if (STR_SAME(attr, HLS_STR_GROUP_ID)) {
                return HLS_ATTR_GROUP_ID;
            }
            break;
        case 'I':
            if (STR_SAME(attr, HLS_STR_IV)) {
                return HLS_ATTR_IV;
            }
            break;
        case 'K':
            if (STR_SAME(attr, HLS_STR_KEYFORMAT)) {
                return HLS_ATTR_KEYFORMAT;
            }
            if (STR_SAME(attr, HLS_STR_KEYFORMATVERSION)) {
                return HLS_ATTR_KEYFORMAT_VERSION;
            }
            break;
        case 'L':
            if (STR_SAME(attr, HLS_STR_LANGUAGE)) {
                return HLS_ATTR_LANGUAGE;
            }
            break;
            case 'M':
            if (STR_SAME(attr, HLS_STR_METHOD)) {
                return HLS_ATTR_METHOD;
            }
            break;
        case 'N':
            if (STR_SAME(attr, HLS_STR_NAME)) {
                return HLS_ATTR_NAME;
            }
            break;
        case 'P':
            if (STR_SAME(attr, HLS_STR_PROGRAM_ID)) {
                return HLS_ATTR_PROGRAM_ID;
            }
            break;
        case 'R':
            if (STR_SAME(attr, HLS_STR_RESOLUTION)) {
                return HLS_ATTR_RESOLUTION;
            }
            break;
        case 'S':
            if (STR_SAME(attr, HLS_STR_SUBTITLES)) {
                return HLS_ATTR_SUBTITLES;
            }
            break;
        case 'T':
            if (STR_SAME(attr, HLS_STR_TYPE)) {
                return HLS_ATTR_TYPE;
            }
            break;
        case 'U':
            if (STR_SAME(attr, HLS_STR_URI)) {
                return HLS_ATTR_URI;
            }
            break;
        default:
            break;
    }
    return HLS_ATTR_IGNORE;
}

static hls_tag_t hls_get_tag(char* tag)
{
    if (MEM_SAME(tag, HLS_STR_EXT_X_)) {
        tag += sizeof(HLS_STR_EXT_X_) - 1;
    } else if (MEM_SAME(tag, HLS_STR_EXT)) {
        tag += sizeof(HLS_STR_EXT) -1;
    } else {
        return HLS_TAG_IGNORE;
    }
    switch (*tag) {
        case 'B':
            if (STR_SAME(tag, HLS_STR_BYTERANGE)) {
                return HLS_TAG_BYTE_RANGE;
            }
            break;
        case 'D':
            if (STR_SAME(tag, HLS_STR_DISCONTINUITY)) {
                return HLS_TAG_DISCONTINUITY;
            }
            break;
        case 'E':
            if (STR_SAME(tag, HLS_STR_ENDLIST)) {
                return HLS_TAG_ENDLIST;
            }
            break;
        case 'I':
            if (STR_SAME(tag, HLS_STR_INF)) {
                return HLS_TAG_INF;
            }
            if (STR_SAME(tag, HLS_STR_I_FRAME_STREAM_INF)) {
                return HLS_TAG_I_FRAME_STREAM_INF;
            }
            if (STR_SAME(tag, HLS_STR_INDEPENDENT_SEGMENTS)) {
                return HLS_TAG_INDEPENDENT_SEGMENTS;
            }
            break;
        case 'K':
            if (STR_SAME(tag, HLS_STR_KEY)) {
                return HLS_TAG_KEY;
            }
            break;
        case 'M':
            if (STR_SAME(tag, HLS_STR_MEDIA)) {
                return HLS_TAG_MEDIA;
            }
            if (STR_SAME(tag, HLS_STR_MEDIA_SEQUENCE)) {
                return HLS_TAG_MEDIA_SEQUENCE;
            }
            if (STR_SAME(tag, HLS_STR_MAP)) {
                return HLS_TAG_MAP;
            }
            break;
        case 'P':
            if (STR_SAME(tag, HLS_STR_PLAYLIST_TYPE)) {
                return HLS_TAG_PLAYLIST_TYPE;
            }
            break;
        case 'S':
            if (STR_SAME(tag, HLS_STR_STREAM_INF)) {
                return HLS_TAG_STREAM_INF;
            }
            if (STR_SAME(tag, HLS_STR_SESSION_KEY)) {
                return HLS_TAG_SESSION_KEY;
            }
            break;
        case 'T':
            if (STR_SAME(tag, HLS_STR_TARGETDURATION)) {
                return HLS_TAG_TARGET_DURATION;
            }
            break;
        case 'V':
            if (STR_SAME(tag, HLS_STR_VERSION)) {
                return HLS_TAG_VERSION;
            }
            break;
        default:
            break;
    }
    return HLS_TAG_IGNORE;
}

static char* hls_get_tag_sep(char* s)
{
    if (*s == '#') {
        s++;
        while (*s) {
            if ((*s >= 'A' && *s <= 'Z') || *s == '-') {

            } else if (*s == ':') {
                return s;
            } else {
                return NULL;
            }
            s++;
        }
    }
    return NULL;
}

static int hls_parse_attr(hls_parse_t* parser, char* s, int attr_num) {
    bool in_string = false;
    bool is_slash  = false;
    while (*s) {
        if (in_string && *s == '\\') {
            is_slash = true;
        } else {
            if (*s == '"') {
                if (is_slash == false) {
                    in_string = !in_string;
                }
            } else if (*s == ',') {
                if (in_string == false) {
                    if (attr_num < HLS_MAX_ATTR_NUM) {
                        *(s++) = 0;
                        if (*s == 0) {
                            break;
                        }
                        parser->attr[attr_num++] = s;
                    } else {
                        ESP_LOGE(TAG, "Too many hls attributes try to enlarge HLS_MAX_ATTR_NUM");
                    }
                }
            }
            if (is_slash) {
                is_slash = false;
            }
        }
        s++;
    }
    return attr_num;
}

static void hls_parse_key(hls_parse_t* parser, hls_tag_t tag, int attr_num) {
    for (int i = 0; i < attr_num; i++) {
        char* k = parser->attr[i];
        char* sep = strchr(k, '=');
        if (sep) {
            *(sep++) = 0;
            parser->k[i] = hls_get_attr(k);
            parser->v[i].s = sep;
        } else {
            parser->k[i] = HLS_ATTR_IGNORE;
            if (i == 0) {
                switch (tag) {
                    case HLS_TAG_INF:
                    case HLS_TAG_TARGET_DURATION:
                        parser->k[i] = HLS_ATTR_DURATION;
                        break;
                    case HLS_TAG_MEDIA_SEQUENCE:
                    case HLS_TAG_VERSION:
                        parser->k[i] = HLS_ATTR_INT;
                        break;
                    case HLS_TAG_PLAYLIST_TYPE:
                        parser->k[i] = HLS_ATTR_INT;
                        parser->v[i].v = (uint64_t)hls_get_playlist_type(k);
                        continue;
                    default:
                        break;
                }
            }
            parser->v[i].s = k;
        }
    }
}

static void hls_parse_value(hls_parse_t* parser, int attr_num) {
    for (int i = 0; i < attr_num; i++) {
        switch (parser->k[i]) {
            case HLS_ATTR_DURATION:
                parser->v[i].f = hls_get_float_value(parser->v[i].s);
                break;
            case HLS_ATTR_TYPE:
                parser->v[i].v = (uint64_t)hls_get_type(parser->v[i].s);
                break;
            case HLS_ATTR_BANDWIDTH:
            case HLS_ATTR_PROGRAM_ID:
            case HLS_ATTR_INT:
                parser->v[i].v = hls_get_int_value(parser->v[i].s);
             break;
            case HLS_ATTR_DEFAULT:
            case HLS_ATTR_AUTO_SELECT:
            case HLS_ATTR_FORCED:
                parser->v[i].v = hls_get_bool_value(parser->v[i].s);
                break;
            case HLS_ATTR_METHOD:
                parser->v[i].v = (uint64_t)hls_get_method(parser->v[i].s);
                break;
            default: {
                // remove start and end "
                char* v = parser->v[i].s;
                if (v[0] == '"') {
                    int len = strlen(v);
                    if (v[--len] == '"') {
                        v[len] = 0;
                    }
                    parser->v[i].s = v+1;
                }
                break;
            }
        }
    }
}

int hls_parse_init(hls_parse_t* parser)
{
    memset(parser, 0, sizeof(hls_parse_t));
    parser->reader = line_reader_init(HLS_MAX_LINE_CHAR);
    if (parser->reader == NULL) {
        return -1;
    }
    return 0;
}

void hls_parse_deinit(hls_parse_t* parser)
{
    if (parser->reader) {
        line_reader_deinit(parser->reader);
        parser->reader = NULL;
    }
}

bool hls_matched(uint8_t* b, int len)
{
    if (STR_SAME(b, HLS_STR_EXTM3U) == 0) {
        return true;
    }
    return false;
}

hls_file_type_t hls_get_file_type(uint8_t* b, int len)
{
    while (len > sizeof(HLS_STR_EXT_X_STREAM_INF)) {
        if (MEM_SAME(b, HLS_STR_EXT_X_STREAM_INF) || MEM_SAME(b, HLS_STR_EXT_X_MEDIA)) {
            return HLS_FILE_TYPE_MASTER_PLAYLIST;
        }
        if (MEM_SAME(b, HLS_STR_EXTINF)) {
            return HLS_FILE_TYPE_MEDIA_PLAYLIST;
        }
        len--;
        b++;
    }
    return HLS_FILE_TYPE_NONE;
}

int hls_parse_add_buffer(hls_parse_t* parser, uint8_t* buffer, int size, bool eos)
{
    if (parser->reader == NULL) {
        return -1;
    }
    line_reader_add_buffer(parser->reader, buffer, size, eos);
    return 0;
}

int hls_parse(hls_parse_t* parser, hls_tag_callback cb, void* ctx)
{
    while (1) {
        char* line = line_reader_get_line(parser->reader);
        if (line == NULL) {
            break;
        }
        hls_tag_t tag;
        int attr_num = 0;
        char* sep = hls_get_tag_sep(line);
        if (sep == NULL) {
            tag = hls_get_tag(line);
            if (tag == HLS_TAG_IGNORE) {
                // append tag attribute to previous tag
                if (parser->tag == HLS_TAG_STREAM_INF || parser->tag == HLS_TAG_INF) {
                    tag = (parser->tag == HLS_TAG_STREAM_INF)? HLS_TAG_STREAM_INF_APPEND : HLS_TAG_INF_APPEND;
                    parser->k[attr_num] = HLS_ATTR_URI;
                    parser->v[attr_num++].s = line;
                }
                parser->tag = HLS_TAG_IGNORE;
                if (attr_num == 0) {
                    continue;
                }
            }
            parser->tag = tag;
        } else {
            char* tag_str = line;
            *(sep++) = 0;
            char* attr = sep;
            tag = hls_get_tag(tag_str);
            // set previous tag
            parser->tag = tag;
            if (tag == HLS_TAG_IGNORE) {
                continue;
            }
            parser->attr[attr_num++] = attr;
            attr_num = hls_parse_attr(parser, sep, attr_num);
            hls_parse_key(parser, tag, attr_num);
            hls_parse_value(parser, attr_num);
        }
        if (cb) {
            hls_tag_info_t tag_info = {
                .tag = tag,
                .attr_num = attr_num,
                .k = parser->k,
                .v = parser->v,
            };
            cb(&tag_info, ctx);
        }
    }
    return 0;
}

const char* hls_tag2str(hls_tag_t tag)
{
    switch (tag) {
        default:
        case HLS_TAG_IGNORE: return HLS_STR_IGNORE;
        case HLS_TAG_VERSION: return HLS_STR_VERSION;
        case HLS_TAG_MEDIA_SEQUENCE: return HLS_STR_MEDIA_SEQUENCE;
        case HLS_TAG_TARGET_DURATION: return HLS_STR_TARGETDURATION;
        case HLS_TAG_INF: return HLS_STR_INF;
        case HLS_TAG_MEDIA: return HLS_STR_MEDIA;
        case HLS_TAG_STREAM_INF: return HLS_STR_STREAM_INF;
        case HLS_TAG_INDEPENDENT_SEGMENTS: return HLS_STR_INDEPENDENT_SEGMENTS;
        case HLS_TAG_I_FRAME_STREAM_INF: return HLS_STR_I_FRAME_STREAM_INF;
        case HLS_TAG_KEY: return HLS_STR_KEY;
        case HLS_TAG_SESSION_KEY: return HLS_STR_SESSION_KEY;
        case HLS_TAG_BYTE_RANGE: return HLS_STR_BYTERANGE;
        case HLS_TAG_PLAYLIST_TYPE: return HLS_STR_PLAYLIST_TYPE;
        case HLS_TAG_MAP: return HLS_STR_MAP;
        case HLS_TAG_DISCONTINUITY: return HLS_STR_DISCONTINUITY;
        case HLS_TAG_ENDLIST: return HLS_STR_ENDLIST;
    }
}

const char* hls_attr2str(hls_attr_t attr)
{
    switch (attr) {
        default:
        case HLS_ATTR_IGNORE: return HLS_STR_IGNORE;
        case HLS_ATTR_VERSION: return HLS_STR_VERSION;
        case HLS_ATTR_DURATION: return HLS_STR_DURATION;
        case HLS_ATTR_TITLE: return HLS_STR_TITLE;
        case HLS_ATTR_TYPE: return HLS_STR_TYPE;
        case HLS_ATTR_GROUP_ID: return HLS_STR_GROUP_ID;
        case HLS_ATTR_NAME: return HLS_STR_NAME;
        case HLS_ATTR_LANGUAGE: return HLS_STR_LANGUAGE;
        case HLS_ATTR_AUTO_SELECT: return HLS_STR_AUTO_SELECT;
        case HLS_ATTR_URI: return HLS_STR_URI;
        case HLS_ATTR_BANDWIDTH: return HLS_STR_BANDWIDTH;
        case HLS_ATTR_CODECS: return HLS_STR_CODECS;
        case HLS_ATTR_RESOLUTION: return HLS_STR_RESOLUTION;
        case HLS_ATTR_DEFAULT: return HLS_STR_DEFAULT;
        case HLS_ATTR_FORCED: return HLS_STR_FORCED;
        case HLS_ATTR_AUDIO: return HLS_STR_AUDIO;
        case HLS_ATTR_SUBTITLES: return HLS_STR_SUBTITLES;
        case HLS_ATTR_PROGRAM_ID: return HLS_STR_PROGRAM_ID;
        case HLS_ATTR_INT: return HLS_STR_INT;
        case HLS_ATTR_METHOD: return HLS_STR_METHOD;
        case HLS_ATTR_IV: return HLS_STR_IV;
        case HLS_ATTR_KEYFORMAT: return HLS_STR_KEYFORMAT;
        case HLS_ATTR_KEYFORMAT_VERSION: return HLS_STR_KEYFORMATVERSION;
    }
}
