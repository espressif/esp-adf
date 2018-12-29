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

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "errno.h"
#include "http_stream.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include <strings.h>

static const char *TAG = "HTTP_STREAM";
#define MAX_PLAYLIST_LINE_SIZE (128)
#define MAX_PLAYLIST_TRACK (20)
#define MAX_PLAYLIST_KEEP_TRACK (18)
#define HTTP_STREAM_BUFFER_SIZE (1024)

typedef struct track_ {
    char *uri;
    bool is_played;
    STAILQ_ENTRY(track_) next;
} track_t;

typedef STAILQ_HEAD(track_list, track_) track_list_t;
typedef struct {
    char            *data;
    int             index;
    int             remain;
    int             total_read;
    track_list_t    tracks;
    int             total_tracks;
    int             content_length;
} playlist_t;

typedef struct http_stream {
    audio_stream_type_t             type;
    char                            *uri;
    bool                            is_open;
    esp_http_client_handle_t        client;
    http_stream_event_handle_t      hook;
    audio_stream_type_t             stream_type;
    void                            *user_data;
    bool                            enable_playlist_parser;
    bool                            is_playlist_resolved;
    playlist_t                      *playlist;
} http_stream_t;


static audio_codec_t get_audio_type(const char *content_type)
{
    if (strcasecmp(content_type, "audio/mp3") == 0) {
        return AUDIO_CODEC_MP3;
    }
    if (strcasecmp(content_type, "audio/mpeg") == 0) {
        return AUDIO_CODEC_MP3;
    }
    if (strcasecmp(content_type, "audio/aac") == 0) {
        return AUDIO_CODEC_AAC;
    }
    if (strcasecmp(content_type, "audio/wav") == 0) {
        return AUDIO_CODEC_WAV;
    }
    if (strcasecmp(content_type, "audio/opus") == 0) {
        return AUDIO_CODEC_OPUS;
    }
    if (strcasecmp(content_type, "application/vnd.apple.mpegurl") == 0) {
        return AUDIO_PLAYLIST;
    }
    if (strcasecmp(content_type, "vnd.apple.mpegURL") == 0) {
        return AUDIO_PLAYLIST;
    }
    return AUDIO_CODEC_NONE;
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    audio_element_info_t *info = (audio_element_info_t *)evt->user_data;

    if (evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }

    if (strcasecmp(evt->header_key, "Content-Disposition") == 0 || strcasecmp(evt->header_key, "Content-Type") == 0) {
        ESP_LOGD(TAG, "%s = %s", evt->header_key, evt->header_value);
        info->codec_fmt = get_audio_type(evt->header_value);
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
    if (info->codec_fmt == AUDIO_PLAYLIST) {
        return true;
    }
    char *dot = strrchr(uri, '.');
    if (dot && (strcasecmp(dot, ".m3u") == 0 || strcasecmp(dot, ".m3u8") == 0)) {
        return true;
    }
    return false;
}

static bool _get_line_in_buffer(http_stream_t *http, char **out)
{
    *out = NULL;
    char c;
    *out = NULL;
    if (http->playlist->remain > 0) {
        bool is_end_of_line = false;
        *out = http->playlist->data + http->playlist->index;
        int idx = http->playlist->index;

        while ((c = http->playlist->data[idx])) {
            if (c == '\r' || c == '\n') {
                http->playlist->data[idx] = 0;
                is_end_of_line = true;
            } else if (is_end_of_line) {
                http->playlist->remain = MAX_PLAYLIST_LINE_SIZE - idx;
                http->playlist->index = idx;
                return true;
            }
            idx ++;
        }
    }
    return false;
}

static char *_client_read_line(http_stream_t *http)
{
    int need_read = MAX_PLAYLIST_LINE_SIZE;
    int rlen;
    char *line;

    if (http->playlist->total_read >= http->playlist->content_length) {
        return NULL;
    }

    if (_get_line_in_buffer(http, &line)) {
        return line;
    }

    need_read -= http->playlist->remain;
    if (need_read > 0) {
        if (http->playlist->remain > 0) {
            memmove(http->playlist->data, http->playlist->data + http->playlist->index, http->playlist->remain);
            http->playlist->index = 0;
        }
        rlen = esp_http_client_read(http->client, http->playlist->data + http->playlist->remain, need_read);
        if (rlen > 0) {
            http->playlist->remain += rlen;
            http->playlist->total_read += rlen;
            if (_get_line_in_buffer(http, &line)) {
                return line;
            }
        } else {
            http->playlist->remain = 0;
        }
    }

    return line;
}

static void _insert_to_playlist(playlist_t *playlist, char *track_uri, const char *uri)
{
    track_t *track;
    while (playlist->total_tracks > MAX_PLAYLIST_TRACK) {
        track = STAILQ_FIRST(&playlist->tracks);
        if (track == NULL) {
            break;
        }
        STAILQ_REMOVE(&playlist->tracks, track, track_, next);
        ESP_LOGD(TAG, "Remove %s", track->uri);
        free(track->uri);
        free(track);
        playlist->total_tracks --;
    }
    track = calloc(1, sizeof(track_t));
    if (track == NULL) {
        return;
    }
    if (strstr(track_uri, "http") == track_uri) { // Full URI
        asprintf(&track->uri, "%s", track_uri);
    } else if (strstr(track_uri, "//") == track_uri) { // schemeless URI
        if (strstr(uri, "https") == uri) {
            asprintf(&track->uri, "https:%s", track_uri);
        } else {
            asprintf(&track->uri, "http:%s", track_uri);
        }
    } else if (strstr(track_uri, "/") == track_uri) { // Root uri
        char *url = strdup(uri);
        if (url == NULL) {
            return;
        }
        char *host = strstr(url, "//");
        if (host == NULL) {
            free(url);
            return;
        }
        host += 2;
        char *path = strstr(host, "/");
        if (path == NULL) {
            free(url);
            return;
        }
        path[0] = 0;
        asprintf(&track->uri, "%s%s", url, track_uri);
        free(url);
    } else { // Relative URI
        asprintf(&track->uri, "%s%s", uri, track_uri);
    }
    if (track->uri == NULL) {
        ESP_LOGE(TAG, "Error insert URI to playlist");
        free(track);
        return;
    }

    track_t *find = NULL;
    STAILQ_FOREACH(find, &playlist->tracks, next) {
        if (strcmp(find->uri, track->uri) == 0) {
            ESP_LOGW(TAG, "URI exist");
            free(track->uri);
            free(track);
            return;
        }
    }

    ESP_LOGD(TAG, "INSERT %s", track->uri);
    STAILQ_INSERT_TAIL(&playlist->tracks, track, next);
    playlist->total_tracks ++;

}

static esp_err_t _resolve_playlist(audio_element_handle_t self, const char *uri)
{
    audio_element_info_t info;
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);

    if (!_is_playlist(&info, uri)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (http->is_playlist_resolved) {
        return ESP_ERR_INVALID_STATE;
    }

    http->playlist->content_length = info.total_bytes;
    http->playlist->remain = 0;
    http->playlist->index = 0;
    http->playlist->total_read = 0;
    char *line = NULL;
    bool valid_playlist = false;
    bool is_playlist_uri = false;

    while ((line = _client_read_line(http))) {
        ESP_LOGD(TAG, "Playlist line = %s", line);
        if (!valid_playlist && strcmp(line, "#EXTM3U") == 0) {
            valid_playlist = true;
            continue;
        }
        if (strstr(line, "http") == (void *)line) {
            _insert_to_playlist(http->playlist, line, uri);
            valid_playlist = true;
            continue;
        }
        if (!valid_playlist) {
            break;
        }
        if (!is_playlist_uri && strstr(line, "#EXTINF") == (void *)line) {
            is_playlist_uri = true;
            continue;
        }
        if (!is_playlist_uri) {
            continue;
        }
        is_playlist_uri = false;

        _insert_to_playlist(http->playlist, line, uri);
    }
    return valid_playlist ? ESP_OK : ESP_FAIL;
}

static track_t *_playlist_get_next_track(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    if (http->enable_playlist_parser && http->is_playlist_resolved) {
        track_t *track;
        STAILQ_FOREACH(track, &http->playlist->tracks, next) {
            if (!track->is_played) {
                return track;
            }
        }
    }
    return NULL;
}

static void _playlist_clear(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    track_t *track, *tmp;
    STAILQ_FOREACH_SAFE(track, &http->playlist->tracks, next, tmp) {
        STAILQ_REMOVE(&http->playlist->tracks, track, track_, next);
        free(track->uri);
        free(track);
    }
}

static esp_err_t _http_open(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    esp_err_t err;
    char *uri = NULL;
    track_t *track;
    audio_element_info_t info;
    audio_element_getinfo(self, &info);
    ESP_LOGD(TAG, "_http_open");

    if (http->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_FAIL;
    }

_stream_open_begin:

    track = _playlist_get_next_track(self);
    if (track == NULL) {
        if (http->is_playlist_resolved && http->enable_playlist_parser) {
            if (dispatch_hook(self, HTTP_STREAM_FINISH_PLAYLIST, NULL, 0) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to process user callback");
                return ESP_FAIL;
            }
            goto _stream_open_begin;
        }
        uri = audio_element_get_uri(self);
    } else {
        uri = track->uri;
    }

    if (uri == NULL) {
        ESP_LOGE(TAG, "Error open connection, uri = NULL");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "URI=%s", uri);
    // if not initialize http client, initial it
    if (http->client == NULL) {
        esp_http_client_config_t http_cfg = {
            .url = uri,
            .event_handler = _http_event_handle,
            .user_data = &info,
            .timeout_ms = 30 * 1000,
            .buffer_size = HTTP_STREAM_BUFFER_SIZE,
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

    info.total_bytes = esp_http_client_fetch_headers(http->client);
    ESP_LOGD(TAG, "total_bytes=%d", (int)info.total_bytes);
    int status_code = esp_http_client_get_status_code(http->client);
    if (status_code == 301 || status_code == 302) {
        esp_http_client_set_redirection(http->client);
        goto _stream_redirect;
    }
    if (status_code != 200
        && (esp_http_client_get_status_code(http->client) != 206)) {
        ESP_LOGE(TAG, "Invalid HTTP stream");
        if (http->enable_playlist_parser) {
            _playlist_clear(self);
            http->is_playlist_resolved = false;
            return ESP_FAIL;
        }

        // return ESP_FAIL;
    }
    audio_element_setinfo(self, &info);

    if (_resolve_playlist(self, uri) == ESP_OK) {
        http->is_playlist_resolved = true;
        goto _stream_open_begin;
    }

    http->is_open = true;
    audio_element_report_codec_fmt(self);
    return ESP_OK;
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
    if (rlen <= 0) {
        ESP_LOGW(TAG, "No more data,errno:%d, total_bytes:%llu", errno, info.byte_pos);
        if (dispatch_hook(self, HTTP_STREAM_FINISH_TRACK, NULL, 0) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to process user callback");
            return ESP_FAIL;
        }
        return ESP_OK;

    } else {
        info.byte_pos += rlen;
        audio_element_setinfo(self, &info);
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
        ESP_LOGE(TAG, "Failed to write data to http stream, wrlen=%d, errno=%d", wrlen, errno);
    }
    return wrlen;
}

static int _http_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);
    int w_size = 0;
    if (r_size > 0) {
        w_size = audio_element_output(self, in_buffer, r_size);
        audio_element_multi_output(self, in_buffer, r_size, 0);
    } else {
        w_size = r_size;
    }
    return w_size;
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
    if (http->enable_playlist_parser) {
        _playlist_clear(self);
        http->is_playlist_resolved = false;
    }
    if (http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_info_t info = {0};
        audio_element_getinfo(self, &info);
        info.byte_pos = 0;
        audio_element_setinfo(self, &info);
    }
    return ESP_OK;
}

static esp_err_t _http_destroy(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    if (http->playlist) {
        free(http->playlist->data);
        free(http->playlist);
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
    cfg.out_rb_size = config->out_rb_size;
    cfg.enable_multi_io = true;
    cfg.tag = "http";

    http->type = config->type;
    http->enable_playlist_parser = config->enable_playlist_parser;
    http->hook = config->event_handle;
    http->stream_type = config->type;
    http->user_data = config->user_data;

    if (http->enable_playlist_parser) {
        http->playlist = calloc(1, sizeof(playlist_t));
        AUDIO_MEM_CHECK(TAG, http->playlist, {
            audio_free(http);
            return NULL;
        });
        http->playlist->data = calloc(1, MAX_PLAYLIST_LINE_SIZE + 1);
        AUDIO_MEM_CHECK(TAG, http->playlist->data, {
            audio_free(http);
            audio_free(http->playlist);
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
        audio_free(http);
        audio_free(http->playlist);
        return NULL;
    });
    audio_element_setdata(el, http);
    return el;
}

esp_err_t http_stream_next_track(audio_element_handle_t el)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);

    track_t *track = _playlist_get_next_track(el);
    if (track) {
        track->is_played = true;
        ESP_LOGD(TAG, "Finish %s", track->uri);
        if (http->playlist->total_tracks > MAX_PLAYLIST_KEEP_TRACK) {
            STAILQ_REMOVE(&http->playlist->tracks, track, track_, next);
            ESP_LOGD(TAG, "Remove %s", track->uri);
            free(track->uri);
            free(track);
            http->playlist->total_tracks --;
        }

    } else {
        ESP_LOGW(TAG, "there are no track");
        return ESP_OK;
    }
    audio_element_reset_state(el);
    audio_element_info_t info;
    audio_element_getinfo(el, &info);
    info.byte_pos = 0;
    info.total_bytes = 0;
    audio_element_setinfo(el, &info);
    http->is_open = false;
    return ESP_OK;
}

esp_err_t http_stream_restart(audio_element_handle_t el)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    http->is_playlist_resolved = false;
    return ESP_OK;
}
