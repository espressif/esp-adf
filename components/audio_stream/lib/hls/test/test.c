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

#include <stdio.h>
#include <stdint.h>
#include "hls_parse.h"
#include "hls_playlist.h"

uint8_t* read_file(char* f, int* size)
{
    FILE* fp = fopen(f, "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        *size = ftell(fp);
        void* b = malloc(*size);
        if (b) {
            fseek(fp, 0, SEEK_SET);
            fread(b, 1, *size, fp);
            fclose(fp);
        }
        return (uint8_t*) b;
    }
    return NULL;
}

int tag_callback(hls_tag_info_t* tag_info, void* ctx)
{
    if (tag_info->tag != HLS_TAG_INF_APPEND && tag_info->tag != HLS_TAG_STREAM_INF_APPEND) {
        printf("Tag:%s(%d)\n", hls_tag2str(tag_info->tag), tag_info->tag);
    }
    for (int i = 0; i < tag_info->attr_num; i++) {
        printf("    %s(%d) = ", hls_attr2str(tag_info->k[i]), tag_info->k[i]);
        switch (tag_info->k[i]) {
            case HLS_ATTR_DURATION:
                printf("%f\n", tag_info->v[i].f);
                break;
            case HLS_ATTR_TYPE:
            case HLS_ATTR_BANDWIDTH:
            case HLS_ATTR_PROGRAM_ID:
            case HLS_ATTR_INT:
            case HLS_ATTR_DEFAULT:
            case HLS_ATTR_AUTO_SELECT:
            case HLS_ATTR_FORCED:
                printf("%d\n", tag_info->v[i].v);
                break;
            default:
                printf("%s\n", tag_info->v[i].s);
                break;
        }
    }
    return 0;
}

int test_parser(char* file_name)
{
    int      size = 0;
    uint8_t* buffer = read_file(file_name, &size);
    if (buffer == NULL) {
        printf("File %s not exists\n", file_name);
        return 0;
    }
    hls_parse_t parser;
    do {
        if (hls_parse_init(&parser) != 0) {
            break;
        }
        if (hls_matched(buffer, size) == false) {
            printf("file is not hls\n");
            break;
        }
        hls_file_type_t type = hls_get_file_type(buffer, size);
        if (type == HLS_FILE_TYPE_NONE) {
            printf("Not a playlist file\n");
            break;
        }
        printf("filetype:%s playlist\n", type == HLS_FILE_TYPE_MASTER_PLAYLIST ? "master" : "media");
        uint8_t* t = buffer;
        while (size > 0) {
            int  s = size;
            bool eos = false;
            if (s > 16) {
                s = 16;
            } else {
                eos = true;
            }
            hls_parse_add_buffer(&parser, t, s, eos);
            hls_parse(&parser, tag_callback, NULL);
            size -= s;
            t += s;
        }
        hls_parse(&parser, tag_callback, NULL);
    } while (0);
    hls_parse_deinit(&parser);
    free(buffer);
    return 0;
}

int read_local(char* uri, uint8_t* data, int size)
{
    static FILE* fp = NULL;
    if (fp == NULL) {
        fp = fopen(uri, "r");
        if (fp == NULL) {
            return -1;
        }
    }
    if (fp) {
        int s = fread(data, 1, size, fp);
        printf("Read got size %d\n", s);
        if (s < 0 || s < size) {
            if (fp) {
                fclose(fp);
                fp = NULL;
            }
        }
        return s;
    }
    return -1;
}

static int hls_file_cb(char* url, void* tag)
{
    printf("Got url: %s\n", url);
    return 0;
}

int parse_data(hls_handle_t hls, char* url)
{
    int     size = 512;
    uint8_t data[512];
    while (1) {
        int s = read_local(url, data, 512);
        if (s < 0) {
            break;
        }
        int ret = hls_playlist_parse_data(hls, data, s, (s < size));
        if (s < size) {
            break;
        }
    }
    return 0;
}

int test_playlist(char* file)
{
    hls_playlist_cfg_t cfg = {
        .prefer_bitrate = 500 * 1024,
        .cb = hls_file_cb,
        .ctx = NULL,
        .uri = file,
    };
    hls_handle_t hls = hls_playlist_open(&cfg);
    char*        stream_uri = NULL;
    if (hls) {
        do {
            int ret = parse_data(hls, file);
            if (ret != 0) {
                break;
            }
            if (hls_playlist_is_master(hls)) {
                stream_uri = hls_playlist_get_prefer_url(hls, HLS_STREAM_TYPE_AUDIO);
            }
            hls_playlist_close(hls);
            hls = NULL;
            if (stream_uri == NULL) {
                break;
            }
            cfg.uri = stream_uri;
            hls = hls_playlist_open(&cfg);
            if (hls == NULL) {
                break;
            }
            ret = parse_data(hls, stream_uri);
            if (ret != 0) {
                break;
            }
            hls_playlist_close(hls);
            hls = NULL;
        } while (0);
        if (hls) {
            hls_playlist_close(hls);
        }
        if (stream_uri) {
            free(stream_uri);
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    char* file_name;
    if (argc < 2) {
        printf("Your should set input filename firstly\n");
        return -1;
    }
    file_name = argv[1];
    if (argc >= 3) {
        test_parser(file_name);
    } else {
        test_playlist(file_name);
    }
    return 0;
}