/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include <sys/unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "http_stream.h"
#include "http_playlist.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "line_reader.h"
#include "hls_playlist.h"
#include "audio_idf_version.h"

static const char *TAG = "HTTP_STREAM";
#define MAX_PLAYLIST_LINE_SIZE (512)
#define HTTP_STREAM_BUFFER_SIZE (2048)
#define HTTP_MAX_CONNECT_TIMES  (5)

#define HLS_PREFER_BITRATE      (200*1024)
typedef struct http_stream {
    audio_stream_type_t             type;
    bool                            is_open;
    esp_http_client_handle_t        client;
    http_stream_event_handle_t      hook;
    audio_stream_type_t             stream_type;
    void                            *user_data;
    bool                            enable_playlist_parser;
    bool                            auto_connect_next_track; /* connect next track without open/close */
    bool                            is_playlist_resolved;
    bool                            is_valid_playlist;
    bool                            is_main_playlist;
    http_playlist_t                 *playlist;         /* media playlist */
    int                             _errno;            /* errno code for http */
    int                             connect_times;     /* max reconnect times */
} http_stream_t;

static esp_err_t http_stream_auto_connect_next_track(audio_element_handle_t el);

// `errno` is not thread safe in multiple HTTP-clients,
// so it is necessary to save the errno number of HTTP clients to avoid reading and writing exceptions of HTTP-clients caused by errno exceptions
int __attribute__((weak)) esp_http_client_get_errno(esp_http_client_handle_t client)
{
    (void) client;
    ESP_LOGE(TAG, "Not found right %s.\r\nPlease enter ADF-PATH with \"cd $ADF_PATH/idf_patches\" and apply the ADF patch with \"git apply $ADF_PATH/idf_patches/idf_%.4s_esp_http_client.patch\" first\r\n", __func__, IDF_VER);
    return errno;
}

static esp_codec_type_t get_audio_type(const char *content_type)
{
    if (strcasecmp(content_type, "mp3") == 0 ||
        strcasecmp(content_type, "audio/mp3") == 0 ||
        strcasecmp(content_type, "audio/mpeg") == 0 ||
        strcasecmp(content_type, "binary/octet-stream") == 0 ||
        strcasecmp(content_type, "application/octet-stream") == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if (strcasecmp(content_type, "audio/aac") == 0 ||
        strcasecmp(content_type, "audio/x-aac") == 0 ||
        strcasecmp(content_type, "audio/mp4") == 0 ||
        strcasecmp(content_type, "audio/aacp") == 0 ||
        strcasecmp(content_type, "video/MP2T") == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if (strcasecmp(content_type, "audio/wav") == 0) {
        return ESP_CODEC_TYPE_WAV;
    }
    if (strcasecmp(content_type, "audio/opus") == 0) {
        return ESP_CODEC_TYPE_OPUS;
    }
    if (strcasecmp(content_type, "application/vnd.apple.mpegurl") == 0 ||
        strcasecmp(content_type, "vnd.apple.mpegURL") == 0) {
        return ESP_AUDIO_TYPE_M3U8;
    }
    if (strncasecmp(content_type, "audio/x-scpls", strlen("audio/x-scpls")) == 0) {
        return ESP_AUDIO_TYPE_PLS;
    }
    return ESP_CODEC_TYPE_UNKNOW;
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    audio_element_handle_t el = (audio_element_handle_t)evt->user_data;
    if (evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }

    if (strcasecmp(evt->header_key, "Content-Type") == 0) {
        ESP_LOGD(TAG, "%s = %s", evt->header_key, evt->header_value);
        audio_element_set_codec_fmt(el, get_audio_type(evt->header_value));
    }

    return ESP_OK;
}

static int dispatch_hook(audio_element_handle_t self, http_stream_event_id_t type, void *buffer, int buffer_len)
{
    http_stream_t *http_stream = (http_stream_t *)audio_element_getdata(self);

    http_stream_event_msg_t msg;
    msg.event_id = type;
    msg.http_client = http_stream->client;
    msg.user_data = http_stream->user_data;
    msg.buffer = buffer;
    msg.buffer_len = buffer_len;
    msg.el = self;
    if (http_stream->hook) {
        return http_stream->hook(&msg);
    }
    return ESP_OK;
}

static bool _is_playlist(audio_element_info_t *info, const char *uri)
{
    if (info->codec_fmt == ESP_AUDIO_TYPE_M3U8 || info->codec_fmt == ESP_AUDIO_TYPE_PLS) {
        return true;
    }
    const char *s = uri;
    while (*s) {
        if (*s == '.') {
            if (strncasecmp(s, ".m3u", 3) == 0) {
                return true;
            }
        }
        s++;
    }
    return false;
}

static int _hls_uri_cb(char *uri, void *ctx)
{
    http_stream_t *http = (http_stream_t *) ctx;
    if (uri) {
        http_playlist_insert(http->playlist, uri);
        http->is_valid_playlist = true;
    }
    return 0;
}

static esp_err_t _resolve_playlist(audio_element_handle_t self, const char *uri)
{
    audio_element_info_t info;
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);
    // backup new uri firstly
    char *new_uri = audio_strdup(uri);
    if (new_uri == NULL) {
        return ESP_FAIL;
    }
    if (http->is_main_playlist) {
        http_playlist_clear(http->playlist);
    }
    if (http->playlist->host_uri) {
        audio_free(http->playlist->host_uri);
    }
    http->playlist->host_uri = new_uri;
    http->is_valid_playlist = false;

    // handle PLS playlist
    if (info.codec_fmt == ESP_AUDIO_TYPE_PLS) {
        line_reader_t *reader = line_reader_init(MAX_PLAYLIST_LINE_SIZE);
        if (reader == NULL) {
            return ESP_FAIL;
        }
        int need_read = MAX_PLAYLIST_LINE_SIZE;
        int rlen = need_read;
        while (rlen == need_read) {
            rlen = esp_http_client_read(http->client, http->playlist->data, need_read);
            if (rlen < 0) {
                break;
            }
            line_reader_add_buffer(reader, (uint8_t *)http->playlist->data, rlen, (rlen < need_read));
            char *line;
            while ((line = line_reader_get_line(reader)) != NULL) {
                if (!strncmp(line, "File", sizeof("File") - 1)) { // This line contains url
                    int i = 4;
                    while (line[i++] != '='); // Skip till '='
                    http_playlist_insert(http->playlist, line + i);
                    http->is_valid_playlist = true;
                }
            }
        }
        line_reader_deinit(reader);
        return http->is_valid_playlist ? ESP_OK : ESP_FAIL;
    }
    http->is_main_playlist = false;
    hls_playlist_cfg_t cfg = {
        .prefer_bitrate = HLS_PREFER_BITRATE,
        .cb = _hls_uri_cb,
        .ctx = http,
        .uri = (char *)new_uri,
    };
    hls_handle_t hls = hls_playlist_open(&cfg);
    do {
        if (hls == NULL) {
            break;
        }
        int need_read = MAX_PLAYLIST_LINE_SIZE;
        int rlen = need_read;
        while (rlen == need_read) {
            rlen = esp_http_client_read(http->client, http->playlist->data, need_read);
            if (rlen < 0) {
                break;
            }
            hls_playlist_parse_data(hls, (uint8_t *)http->playlist->data, rlen, (rlen < need_read));
        }
        if (hls_playlist_is_master(hls)) {
            char *url = hls_playlist_get_prefer_url(hls, HLS_STREAM_TYPE_AUDIO);
            if (url) {
                http_playlist_insert(http->playlist, url);
                ESP_LOGI(TAG, "Add media uri %s\n", url);
                http->is_valid_playlist = true;
                http->is_main_playlist = true;
                audio_free(url);
            }
        } else {
            http->playlist->is_incomplete = !hls_playlist_is_media_end(hls);
            if (http->playlist->is_incomplete) {
                ESP_LOGI(TAG, "Live stream URI. Need to be fetched again!");
            }
        }
    } while (0);
    if (hls) {
        hls_playlist_close(hls);
    }
    return http->is_valid_playlist ? ESP_OK : ESP_FAIL;
}

static char *_playlist_get_next_track(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    if (http->enable_playlist_parser && http->is_playlist_resolved) {
        return http_playlist_get_next_track(http->playlist);
    }
    return NULL;
}

static esp_err_t _http_open(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    esp_err_t err;
    char *uri = NULL;
    audio_element_info_t info;
    ESP_LOGD(TAG, "_http_open");

    if (http->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_OK;
    }
    http->_errno = 0;
    audio_element_getinfo(self, &info);
_stream_open_begin:
    if (info.byte_pos == 0) {
        uri = _playlist_get_next_track(self);
    } else if (http->is_playlist_resolved) {
        uri = http_playlist_get_last_track(http->playlist);
    }
    if (uri == NULL) {
        if (http->is_playlist_resolved && http->enable_playlist_parser) {
            if (dispatch_hook(self, HTTP_STREAM_FINISH_PLAYLIST, NULL, 0) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process user callback");
                return ESP_FAIL;
            }
            goto _stream_open_begin;
        }
        uri = audio_element_get_uri(self);
    }

    if (uri == NULL) {
        ESP_LOGE(TAG, "Error open connection, uri = NULL");
        return ESP_FAIL;
    }
    audio_element_getinfo(self, &info);
    ESP_LOGD(TAG, "URI=%s", uri);
    // if not initialize http client, initial it
    if (http->client == NULL) {
        esp_http_client_config_t http_cfg = {
            .url = uri,
            .event_handler = _http_event_handle,
            .user_data = self,
            .timeout_ms = 30 * 1000,
            .buffer_size = HTTP_STREAM_BUFFER_SIZE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
            .buffer_size_tx = 1024,
#endif
        };
        http->client = esp_http_client_init(&http_cfg);
        AUDIO_MEM_CHECK(TAG, http->client, return ESP_ERR_NO_MEM);
    } else {
        esp_http_client_set_url(http->client, uri);
    }

    if (info.byte_pos) {
        char rang_header[32];
        snprintf(rang_header, 32, "bytes=%d-", (int)info.byte_pos);
        esp_http_client_set_header(http->client, "Range", rang_header);
    } else {
        esp_http_client_delete_header(http->client, "Range");
    }

    if (dispatch_hook(self, HTTP_STREAM_PRE_REQUEST, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to process user callback");
        return ESP_FAIL;
    }

    if (http->stream_type == AUDIO_STREAM_WRITER) {
        err = esp_http_client_open(http->client, -1);
        if (err == ESP_OK) {
            http->is_open = true;
        }
        return err;
    }

    char *buffer = NULL;
    int post_len = esp_http_client_get_post_field(http->client, &buffer);
_stream_redirect:
    if ((err = esp_http_client_open(http->client, post_len)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open http stream");
        return err;
    }

    int wrlen = dispatch_hook(self, HTTP_STREAM_ON_REQUEST, buffer, post_len);
    if (wrlen < 0) {
        ESP_LOGE(TAG, "Failed to process user callback");
        return ESP_FAIL;
    }

    if (post_len && buffer && wrlen == 0) {
        if (esp_http_client_write(http->client, buffer, post_len) <= 0) {
            ESP_LOGE(TAG, "Failed to write data to http stream");
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "len=%d, data=%s", post_len, buffer);
    }

    if (dispatch_hook(self, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
        esp_http_client_close(http->client);
        return ESP_FAIL;
    }
    /*
    * Due to the total byte of content has been changed after seek, set info.total_bytes at beginning only.
    */
    int64_t cur_pos = esp_http_client_fetch_headers(http->client);
    audio_element_getinfo(self, &info);
    if (info.byte_pos <= 0) {
        info.total_bytes = cur_pos;
    }

    ESP_LOGI(TAG, "total_bytes=%d", (int)info.total_bytes);
    int status_code = esp_http_client_get_status_code(http->client);
    if (status_code == 301 || status_code == 302) {
        esp_http_client_set_redirection(http->client);
        goto _stream_redirect;
    }
    if (status_code != 200
        && (esp_http_client_get_status_code(http->client) != 206)) {
        ESP_LOGE(TAG, "Invalid HTTP stream, status code = %d", status_code);
        if (http->enable_playlist_parser) {
            http_playlist_clear(http->playlist);
            http->is_playlist_resolved = false;
        }
        return ESP_FAIL;
    }

    /**
     * `audio_element_setinfo` is risky affair.
     * It overwrites URI pointer as well. Pay attention to that!
     */
    audio_element_set_total_bytes(self, info.total_bytes);

    if (_is_playlist(&info, uri) == true) {
        /**
         * `goto _stream_open_begin` blocks on http_open until it gets valid URL.
         * Ensure that the stop command is processed
        */
        if (audio_element_is_stopping(self) == true) {
            ESP_LOGW(TAG, "Http_open got stop cmd at opening");
            return ESP_OK;
        }

        if (_resolve_playlist(self, uri) == ESP_OK) {
            http->is_playlist_resolved = true;
            goto _stream_open_begin;
        }
    }

    http->is_open = true;
    audio_element_report_codec_fmt(self);
    return ESP_OK;
}

static esp_err_t _http_close(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    ESP_LOGD(TAG, "_http_close");
    while (http->is_open) {
        http->is_open = false;
        if (http->stream_type != AUDIO_STREAM_WRITER) {
            break;
        }
        if (dispatch_hook(self, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
            break;
        }
        esp_http_client_fetch_headers(http->client);

        if (dispatch_hook(self, HTTP_STREAM_FINISH_REQUEST, NULL, 0) < 0) {
            break;
        }
    }
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        if (http->enable_playlist_parser) {
            http_playlist_clear(http->playlist);
            http->is_playlist_resolved = false;
        }
        audio_element_report_pos(self);
        audio_element_set_byte_pos(self, 0);
    }
    if (http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
    return ESP_OK;
}

static esp_err_t _http_reconnect(audio_element_handle_t self)
{
    esp_err_t err = ESP_OK;
    audio_element_info_t info = {0};
    AUDIO_NULL_CHECK(TAG, self, return ESP_FAIL);
    err |= audio_element_getinfo(self, &info);
    err |= _http_close(self);
    err |= audio_element_set_byte_pos(self, info.byte_pos);
    err |= _http_open(self);
    return err;
}


static int _http_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_info_t info;
    audio_element_getinfo(self, &info);
    int wrlen = dispatch_hook(self, HTTP_STREAM_ON_RESPONSE, buffer, len);
    int rlen = wrlen;
    if (rlen == 0) {
        rlen = esp_http_client_read(http->client, buffer, len);
    }
    if (rlen <= 0 && http->auto_connect_next_track) {
        if (http_stream_auto_connect_next_track(self) == ESP_OK) {
            rlen = esp_http_client_read(http->client, buffer, len);
        }
    }
    if (rlen <= 0) {
        http->_errno = esp_http_client_get_errno(http->client);
        ESP_LOGW(TAG, "No more data,errno:%d, total_bytes:%llu, rlen = %d", http->_errno, info.byte_pos, rlen);
        if (http->_errno != 0) {  // Error occuered, reset connection
            ESP_LOGW(TAG, "Got %d errno(%s)", http->_errno, strerror(http->_errno));
            return http->_errno;
        }
        if (http->auto_connect_next_track) {
            if (dispatch_hook(self, HTTP_STREAM_FINISH_PLAYLIST, NULL, 0) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process user callback");
                return ESP_FAIL;
            }
        } else {
            if (dispatch_hook(self, HTTP_STREAM_FINISH_TRACK, NULL, 0) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process user callback");
                return ESP_FAIL;
            }
        }
        return ESP_OK;
    } else {
        audio_element_update_byte_pos(self, rlen);
    }
    ESP_LOGD(TAG, "req lengh=%d, read=%d, pos=%d/%d", len, rlen, (int)info.byte_pos, (int)info.total_bytes);
    return rlen;
}

static int _http_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    int wrlen = dispatch_hook(self, HTTP_STREAM_ON_REQUEST, buffer, len);
    if (wrlen < 0) {
        ESP_LOGE(TAG, "Failed to process user callback");
        return ESP_FAIL;
    }
    if (wrlen > 0) {
        return wrlen;
    }

    if ((wrlen = esp_http_client_write(http->client, buffer, len)) <= 0) {
        http->_errno = esp_http_client_get_errno(http->client);
        ESP_LOGE(TAG, "Failed to write data to http stream, wrlen=%d, errno=%d(%s)", wrlen, http->_errno, strerror(http->_errno));
    }
    return wrlen;
}

static int _http_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    if (audio_element_is_stopping(self) == true) {
        ESP_LOGW(TAG, "No output due to stopping");
        return AEL_IO_ABORT;
    }
    int w_size = 0;
    if (r_size > 0) {
        http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
        if (http->_errno != 0) {
            esp_err_t ret = ESP_OK;
            if (http->connect_times > HTTP_MAX_CONNECT_TIMES) {
                ESP_LOGE(TAG, "reconnect times more than %d, disconnect http stream", HTTP_MAX_CONNECT_TIMES);
                return ESP_FAIL;
            };
            http->connect_times++;
            ret = _http_reconnect(self);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to reset connection");
                return ret;
            }
            ESP_LOGW(TAG, "reconnect to peer successful");
            return ESP_ERR_INVALID_STATE;
        } else {
            http->connect_times = 0;
            w_size = audio_element_output(self, in_buffer, r_size);
            audio_element_multi_output(self, in_buffer, r_size, 0);
        }
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t _http_destroy(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    if (http->playlist) {
        audio_free(http->playlist->data);
        audio_free(http->playlist);
    }
    audio_free(http);
    return ESP_OK;
}

audio_element_handle_t http_stream_init(http_stream_cfg_t *config)
{
    audio_element_handle_t el;
    http_stream_t *http = audio_calloc(1, sizeof(http_stream_t));

    AUDIO_MEM_CHECK(TAG, http, return NULL);

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.open = _http_open;
    cfg.close = _http_close;
    cfg.process = _http_process;
    cfg.destroy = _http_destroy;
    cfg.task_stack = config->task_stack;
    cfg.task_prio = config->task_prio;
    cfg.task_core = config->task_core;
    cfg.stack_in_ext = config->stack_in_ext;
    cfg.out_rb_size = config->out_rb_size;
    cfg.multi_out_rb_num = config->multi_out_num;
    cfg.tag = "http";

    http->type = config->type;
    http->enable_playlist_parser = config->enable_playlist_parser;
    http->auto_connect_next_track = config->auto_connect_next_track;
    http->hook = config->event_handle;
    http->stream_type = config->type;
    http->user_data = config->user_data;

    if (http->enable_playlist_parser) {
        http->playlist = audio_calloc(1, sizeof(http_playlist_t));
        AUDIO_MEM_CHECK(TAG, http->playlist, {
            audio_free(http);
            return NULL;
        });
        http->playlist->data = audio_calloc(1, MAX_PLAYLIST_LINE_SIZE + 1);
        AUDIO_MEM_CHECK(TAG, http->playlist->data, {
            audio_free(http->playlist);
            audio_free(http);
            return NULL;
        });
        STAILQ_INIT(&http->playlist->tracks);
    }

    if (config->type == AUDIO_STREAM_READER) {
        cfg.read = _http_read;
    } else if (config->type == AUDIO_STREAM_WRITER) {
        cfg.write = _http_write;
    }

    el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {
        audio_free(http->playlist);
        audio_free(http);
        return NULL;
    });
    audio_element_setdata(el, http);
    return el;
}

esp_err_t http_stream_next_track(audio_element_handle_t el)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    if (!(http->enable_playlist_parser && http->is_playlist_resolved)) {
        /**
         * This is not a playlist!
         * Do not reset states for restart element.
         * Just return.
         */
        ESP_LOGD(TAG, "Direct URI. Stream will be stopped");
        return ESP_OK;
    }
    audio_element_reset_state(el);
    audio_element_set_byte_pos(el, 0);
    audio_element_set_total_bytes(el, 0);
    http->is_open = false;
    return ESP_OK;
}

esp_err_t http_stream_auto_connect_next_track(audio_element_handle_t el)
{
    audio_element_info_t info;
    audio_element_getinfo(el, &info);
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    char *track = _playlist_get_next_track(el);
    if (track) {
        esp_http_client_set_url(http->client, track);
        char *buffer = NULL;
        int post_len = esp_http_client_get_post_field(http->client, &buffer);
redirection:
        if ((esp_http_client_open(http->client, post_len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open http stream");
            return ESP_FAIL;
        }
        if (dispatch_hook(el, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
            esp_http_client_close(http->client);
            return ESP_FAIL;
        }
        info.total_bytes = esp_http_client_fetch_headers(http->client);
        ESP_LOGI(TAG, "total_bytes=%d", (int)info.total_bytes);
        int status_code = esp_http_client_get_status_code(http->client);
        if (status_code == 301 || status_code == 302) {
            esp_http_client_set_redirection(http->client);
            goto redirection;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t http_stream_fetch_again(audio_element_handle_t el)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    if (!http->playlist->is_incomplete) {
        ESP_LOGI(TAG, "Finished playing.");
        return ESP_ERR_NOT_SUPPORTED;
    } else {
        ESP_LOGI(TAG, "Fetching again %s %p", http->playlist->host_uri, http->playlist->host_uri);
        audio_element_set_uri(el, http->playlist->host_uri);
        http->is_playlist_resolved = false;
    }
    return ESP_OK;
}

esp_err_t http_stream_restart(audio_element_handle_t el)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    http->is_playlist_resolved = false;
    return ESP_OK;
}
