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
#include "gzip_miniz.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
#include "aes/esp_aes.h"
#elif (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#if CONFIG_IDF_TARGET_ESP32
#include "esp32/aes.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/aes.h"
#endif
#else
#include "hwcrypto/aes.h"
#endif

static const char *TAG = "HTTP_STREAM";
#define MAX_PLAYLIST_LINE_SIZE (512)
#define HTTP_STREAM_BUFFER_SIZE (2048)
#define HTTP_MAX_CONNECT_TIMES  (5)

#define HLS_PREFER_BITRATE      (200*1024)
#define HLS_KEY_CACHE_SIZE      (32)
typedef struct {
    bool             key_loaded;
    char             *key_url;
    uint8_t          key_cache[HLS_KEY_CACHE_SIZE];
    uint8_t          key_size;
    hls_stream_key_t key;
    uint64_t         sequence_no;
    esp_aes_context  aes_ctx;
    bool             aes_used;
} http_stream_hls_key_t;

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
    const char                     *cert_pem;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    esp_err_t                      (*crt_bundle_attach)(void *conf); /*  Function pointer to esp_crt_bundle_attach*/
#endif
    bool                            gzip_encoding;     /* Content is encoded */
    gzip_miniz_handle_t             gzip;              /* GZIP instance */
    http_stream_hls_key_t           *hls_key;
    hls_handle_t                    *hls_media;
    int                             request_range_size;
    int64_t                         request_range_end;
    bool                            is_last_range;
    const char                      *user_agent;
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

static int _gzip_read_data(uint8_t *data, int size, void *ctx)
{
    http_stream_t *http = (http_stream_t *) ctx;
    return esp_http_client_read(http->client, (char *)data, size);
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
    else if (strcasecmp(evt->header_key, "Content-Encoding") == 0) {
        http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
        http->gzip_encoding = true;
        if (strcasecmp(evt->header_value, "gzip") == 0) {
            gzip_miniz_cfg_t cfg = {
                .chunk_size = 1024,
                .ctx = http,
                .read_cb = _gzip_read_data,
            };
            http->gzip = gzip_miniz_init(&cfg);
        }
        if (http->gzip == NULL) {
            ESP_LOGE(TAG, "Content-Encoding %s not supported", evt->header_value);
            return ESP_FAIL;
        }
    }
    else if (strcasecmp(evt->header_key, "Content-Range") == 0) {
        http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
        if (http->request_range_size) {
            char* end_pos = strchr(evt->header_value, '-');
            http->is_last_range = true;
            if (end_pos) {
                end_pos++;
                int64_t range_end = atoll(end_pos);
                if (range_end == http->request_range_end) {
                    http->is_last_range = false;
                }
                // Update total bytes to range end
                audio_element_set_total_bytes(el, range_end+1);
            }
        }
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

static int _http_read_data(http_stream_t *http, char *buffer, int len)
{
    if (http->gzip_encoding == false) {
        return esp_http_client_read(http->client, buffer, len);
    }
    // use gzip to uncompress data
    return gzip_miniz_read(http->gzip, (uint8_t*) buffer, len);
}

static esp_err_t _resolve_hls_key(http_stream_t *http)
{
    int ret = _http_read_data(http, (char*)http->hls_key->key_cache, sizeof(http->hls_key->key_cache));
    if (ret < 0) {
        return ESP_FAIL;
    }
    http->hls_key->key_size = (uint8_t)ret;
    http->hls_key->key_loaded = true;
    return ESP_OK;
}

static esp_err_t _prepare_crypt(http_stream_t *http)
{
    http_stream_hls_key_t* hls_key = http->hls_key;
    if (hls_key->aes_used) {
        esp_aes_free(&hls_key->aes_ctx);
        hls_key->aes_used = false;
    }
    int ret = hls_playlist_parse_key(http->hls_media, http->hls_key->key_cache, http->hls_key->key_size);
    if (ret < 0) {
        return ESP_FAIL;
    }
    ret = hls_playlist_get_key(http->hls_media, http->hls_key->sequence_no, &http->hls_key->key);
    if (ret != 0) {
        return ESP_FAIL;
    }
    esp_aes_init(&hls_key->aes_ctx);
    esp_aes_setkey(&hls_key->aes_ctx, (unsigned char*)hls_key->key.key, 128);
    hls_key->aes_used = true;
    http->hls_key->sequence_no++;
    return ESP_OK;
}

static void _free_hls_key(http_stream_t *http)
{
    if (http->hls_key == NULL) {
        return;
    }
    if (http->hls_key->aes_used) {
        esp_aes_free(&http->hls_key->aes_ctx);
        http->hls_key->aes_used = false;
    }
    if (http->hls_key->key_url) {
        audio_free(http->hls_key->key_url);
    }
    audio_free(http->hls_key);
    http->hls_key = NULL;
}

static esp_err_t _resolve_playlist(audio_element_handle_t self, const char *uri)
{
    audio_element_info_t info;
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_getinfo(self, &info);
    if (http->hls_media) {
        hls_playlist_close(http->hls_media);
        http->hls_media = NULL;
    }
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
            rlen = _http_read_data(http, http->playlist->data, need_read);
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
            rlen = _http_read_data(http, http->playlist->data, need_read);
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
        if (hls_playlist_is_encrypt(hls) == false) {
            _free_hls_key(http);
            hls_playlist_close(hls);
        } else {
            // When content is encrypted, need keep hls instance
            http->hls_media = hls;
            const char *key_url = hls_playlist_get_key_uri(hls);
            if (key_url == NULL) {
                ESP_LOGE(TAG, "Hls do not have key url");
                return ESP_FAIL;
            }
            if (http->hls_key == NULL) {
                http->hls_key = (http_stream_hls_key_t *) calloc(1, sizeof(http_stream_hls_key_t));
                if (http->hls_key == NULL) {
                    ESP_LOGE(TAG, "No memory for hls key");
                    return ESP_FAIL;
                }
            }
            if (http->hls_key->key_url && strcmp(http->hls_key->key_url, key_url) == 0) {
                http->hls_key->key_loaded = true;
            } else {
                if (http->hls_key->key_url) {
                    audio_free(http->hls_key->key_url);
                }
                http->hls_key->key_loaded = false;
                http->hls_key->key_url = audio_strdup(key_url);
                if (http->hls_key->key_url == NULL) {
                    ESP_LOGE(TAG, "No memory for hls key url");
                    return ESP_FAIL;
                } 
            }
            http->hls_key->sequence_no = hls_playlist_get_sequence_no(hls);
        }
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

static void _prepare_range(http_stream_t *http, int64_t pos)
{
    if (http->request_range_size > 0 || pos != 0) {
        char range_header[64] = {0};
        if (http->request_range_size == 0) {
            snprintf(range_header, sizeof(range_header), "bytes=%lld-", pos);
        } else {
            int64_t end_pos = pos + http->request_range_size - 1;
            if (pos < 0 && end_pos > 0) {
                end_pos = 0;
            }
            snprintf(range_header, sizeof(range_header), "bytes=%lld-%lld", pos, end_pos);
            http->request_range_end = end_pos;
        }
        esp_http_client_set_header(http->client, "Range", range_header);
    } else {
        esp_http_client_delete_header(http->client, "Range");
    }
}

static esp_err_t _http_load_uri(audio_element_handle_t self, audio_element_info_t* info)
{
    esp_err_t err;
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);

    esp_http_client_close(http->client);

    if (dispatch_hook(self, HTTP_STREAM_PRE_REQUEST, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to process user callback");
        return ESP_FAIL;
    }

    _prepare_range(http, info->byte_pos);

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
    if (http->gzip_encoding) {
        gzip_miniz_deinit(http->gzip);
        http->gzip = NULL;
        http->gzip_encoding = false;
    }
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
    audio_element_getinfo(self, info);
    if (info->byte_pos <= 0) {
        info->total_bytes = cur_pos;
        ESP_LOGI(TAG, "total_bytes=%d", (int)info->total_bytes);
        audio_element_set_total_bytes(self, info->total_bytes);
    }
    int status_code = esp_http_client_get_status_code(http->client);
    if (status_code == 301 || status_code == 302) {
        esp_http_client_set_redirection(http->client);
        goto _stream_redirect;
    }
    if (status_code != 200
        && (esp_http_client_get_status_code(http->client) != 206)
        && (esp_http_client_get_status_code(http->client) != 416)) {
        ESP_LOGE(TAG, "Invalid HTTP stream, status code = %d", status_code);
        if (http->enable_playlist_parser) {
            http_playlist_clear(http->playlist);
            http->is_playlist_resolved = false;
        }
        return ESP_FAIL;
    }
    return err;
}

static esp_err_t _http_open(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
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
    if (http->hls_key && http->hls_key->key_loaded == false) {
        uri = http->hls_key->key_url;
    } else if (info.byte_pos == 0) {
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
            .cert_pem = http->cert_pem,
#if  (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)) && defined CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
            .crt_bundle_attach = http->crt_bundle_attach,
#endif //  (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)) && defined CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
            .user_agent = http->user_agent,
        };
        http->client = esp_http_client_init(&http_cfg);
        AUDIO_MEM_CHECK(TAG, http->client, return ESP_ERR_NO_MEM);
    } else {
        esp_http_client_set_url(http->client, uri);
    }
    audio_element_getinfo(self, &info);

    if (_http_load_uri(self, &info) != ESP_OK) {
        return ESP_FAIL;
    }

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
    // Load key and parse key
    if (http->hls_key) {
        if (http->hls_key->key_loaded == false) {
            if (_resolve_hls_key(http) != ESP_OK) {
                return ESP_FAIL;
            }
            // Load media url after key loaded
            goto _stream_open_begin;
        } else {
            if (_prepare_crypt(http) != ESP_OK) {
                return ESP_FAIL;
            }
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
    if (http->is_open) {
        http->is_open = false;
        do {
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
        } while (0);
    }

    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        if (http->enable_playlist_parser) {
            http_playlist_clear(http->playlist);
            http->is_playlist_resolved = false;
        }
        audio_element_report_pos(self);
        audio_element_set_byte_pos(self, 0);
    }
    _free_hls_key(http);
    if (http->hls_media) {
        hls_playlist_close(http->hls_media);
        http->hls_media = NULL;
    }
    if (http->gzip) {
        gzip_miniz_deinit(http->gzip);
        http->gzip = NULL;
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

static bool _check_range_done(audio_element_handle_t self)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    bool last_range = http->is_last_range;
    audio_element_info_t info = {};
    audio_element_getinfo(self, &info);
    // If not last range need reload uri from last position
    if (last_range == false && _http_load_uri(self, &info) != ESP_OK) {
        return true;
    }
    return last_range;
}

static int _http_read(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(self);
    audio_element_info_t info;
    audio_element_getinfo(self, &info);
    int wrlen = dispatch_hook(self, HTTP_STREAM_ON_RESPONSE, buffer, len);
    int rlen = wrlen;
    if (rlen == 0) {
        rlen = _http_read_data(http, buffer, len);
    }
    if (rlen <= 0 && http->request_range_size) {
        if (_check_range_done(self) == false) {
            rlen = _http_read_data(http, buffer, len);
        }
    }
    if (rlen <= 0 && http->auto_connect_next_track) {
        if (http_stream_auto_connect_next_track(self) == ESP_OK) {
            rlen = _http_read_data(http, buffer, len);
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
        if (http->hls_key) {
            int ret = esp_aes_crypt_cbc(&http->hls_key->aes_ctx, ESP_AES_DECRYPT, 
                 rlen, (unsigned char*)http->hls_key->key.iv, 
                 (unsigned char*)buffer, (unsigned char*)buffer);
            if (rlen % 16 != 0) {
                ESP_LOGE(TAG, "Data length %d not aligned", rlen);
            }
            if (ret != 0) {
                ESP_LOGE(TAG, "Fail to decrypt aes ret %d", ret);
                return ESP_FAIL;
            }
            if ((info.total_bytes && rlen + info.byte_pos >= info.total_bytes) ||
                rlen < len) {
                // Remove padding according PKCS#7
                uint8_t padding = buffer[rlen-1];
                if (padding && padding <= rlen) {
                    int idx = rlen - padding;
                    int paddin_n = padding -1;
                    while (paddin_n) {
                        if (buffer[idx++] != padding) {
                            break;
                        }
                        paddin_n--;
                    }
                    if (paddin_n == 0) {
                        rlen -= padding;
                    }
                }
            }
        }
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
    http->cert_pem = config->cert_pem;
    http->user_agent = config->user_agent;

    if (config->crt_bundle_attach) {
#if  (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    #if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        http->crt_bundle_attach = config->crt_bundle_attach;
    #else
        ESP_LOGW(TAG, "Please enbale CONFIG_MBEDTLS_CERTIFICATE_BUNDLE configuration in menuconfig");
    #endif
#else
        ESP_LOGW(TAG, "Just support MBEDTLS_CERTIFICATE_BUNDLE on esp-idf to v4.3 or later");
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    }

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
    http->request_range_size = config->request_range_size;
    if (config->request_size) {
        cfg.buffer_len = config->request_size;
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

esp_err_t http_stream_set_server_cert(audio_element_handle_t el, const char *cert)
{
    http_stream_t *http = (http_stream_t *)audio_element_getdata(el);
    http->cert_pem = cert;
    return ESP_OK;
}
