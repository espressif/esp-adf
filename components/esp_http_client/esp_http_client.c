// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <string.h>

#include "esp_system.h"
#include "esp_log.h"

#include "http_header.h"
#include "transport.h"
#include "transport_tcp.h"
#include "http_utils.h"
#include "http_auth.h"
#include "sdkconfig.h"
#include "transport.h"
#include "esp_http_client.h"

#ifdef CONFIG_ENABLE_HTTPS
#include "transport_ssl.h"
#endif

static const char *TAG = "HTTP_CLIENT";


#ifndef mem_check
#define mem_check(x) assert(x)
#endif


typedef struct {
    char *data;
    int len;
    char *raw_data;
    int raw_len;
} http_buffer_t;
/**
 * private HTTP Data structure
 */
typedef struct {
    http_header_handle_t headers;       /*!< http header */
    http_buffer_t       *buffer;        /*!< data buffer as linked list */
    int                 status_code;    /*!< status code (integer) */
    int                 content_length; /*!< data length */
    int                 data_offset;    /*!< offset to http data (Skip header) */
    int                 data_process;   /*!< data processed */
    int                 method;         /*!< http method */
    bool                is_chunked;
} http_data_t;

typedef struct {
    char                         *uri;
    char                         *scheme;
    char                         *host;
    int                          port;
    char                         *username;
    char                         *password;
    char                         *path;
    char                         *query;
    char                         *cert_pem;
    esp_http_client_method_t     method;
    esp_http_client_auth_type_t  auth_type;
    esp_http_client_transport_t  transport_type;
    int                          max_store_header_size;
} connection_info_t;

typedef enum {
    HTTP_STATE_UNINIT = 0,
    HTTP_STATE_INIT,
    HTTP_STATE_CONNECTED,
    HTTP_STATE_REQ_COMPLETE_HEADER,
    HTTP_STATE_REQ_COMPLETE_DATA,
    HTTP_STATE_RES_COMPLETE_HEADER,
    HTTP_STATE_RES_COMPLETE_DATA,
    HTTP_STATE_CLOSE
} esp_http_state_t;
/**
 * HTTP client class
 */
struct esp_http_client {
    int                         redirect_counter;
    int                         max_redirection_count;
    int                         process_again;
    struct http_parser          *parser;
    struct http_parser_settings *parser_settings;
    transport_list_handle_t     transport_list;
    transport_handle_t          transport;
    http_data_t                 *request;
    http_data_t                 *response;
    void                        *user_data;
    esp_http_auth_data_t        *auth_data;
    char                        *post_data;
    char                        *location;
    char                        *auth_header;
    char                        *current_header_key;
    char                        *current_header_value;
    int                         post_len;
    connection_info_t           connection_info;
    bool                        is_chunk_complete;
    esp_http_state_t            state;
    http_event_handle_cb        event_handle;
    int                         timeout_ms;
    int                         buffer_size;
    bool                        disable_auto_redirect;
    esp_http_client_event_t     event;
};

typedef struct esp_http_client esp_http_client_t;

/**
 * Default settings
 */
#define DEFAULT_HTTP_PORT (80)
#define DEFAULT_HTTPS_PORT (443)

static const char *DEFAULT_HTTP_USER_AGENT = "ESP32 HTTP Client/1.0";
static const char *DEFAULT_HTTP_PROTOCOL = "HTTP/1.1";
static const char *DEFAULT_HTTP_PATH = "/";
static int DEFAULT_MAX_REDIRECT = 10;
static int DEFAULT_TIMOUT_MS = 5000;

static const char *HTTP_METHOD_MAPPING[] = {
    "GET",
    "GET",
    "POST",
    "PUT",
    "PATCH",
    "DELETE"
};

static esp_err_t http_dispatch_event(esp_http_client_t *client, esp_http_client_event_id_t event_id, void *data, int len)
{
    esp_http_client_event_t *event = &client->event;

    if (client->event_handle) {
        event->event_id = event_id;
        event->user_data = client->user_data;
        event->data = data;
        event->data_len = len;
        return client->event_handle(event);
    }
    return ESP_OK;
}

static int http_on_message_begin(http_parser *parser)
{
    esp_http_client_t *client = parser->data;
    ESP_LOGD(TAG, "on_message_begin");

    client->response->is_chunked = false;
    client->is_chunk_complete = false;
    return 0;
}

static int http_on_url(http_parser *parser, const char *at, size_t length)
{
    ESP_LOGD(TAG, "http_on_url");
    return 0;
}

static int http_on_status(http_parser *parser, const char *at, size_t length)
{
    return 0;
}

static int http_on_header_field(http_parser *parser, const char *at, size_t length)
{
    esp_http_client_t *client = parser->data;
    assign_string(&client->current_header_key, at, length);

    return 0;
}

static int http_on_header_value(http_parser *parser, const char *at, size_t length)
{
    esp_http_client_handle_t client = parser->data;
    //TODO: Pre malloc header_value_str to reduce malloc function
    if (client->current_header_key == NULL) {
        return 0;
    }
    if (strcasecmp(client->current_header_key, "Location") == 0) {
        assign_string(&client->location, at, length);
    } else if (strcasecmp(client->current_header_key, "Transfer-Encoding") == 0 && memcmp(at, "chunked", length) == 0) {
        client->response->is_chunked = true;
    } else if (strcasecmp(client->current_header_key, "WWW-Authenticate") == 0) {
        assign_string(&client->auth_header, at, length);
    }
    assign_string(&client->current_header_value, at, length);

    ESP_LOGD(TAG, "HEADER=%s:%s", client->current_header_key, client->current_header_value);
    client->event.header_key = client->current_header_key;
    client->event.header_value = client->current_header_value;
    http_dispatch_event(client, HTTP_EVENT_ON_HEADER, NULL, 0);
    free(client->current_header_key);
    free(client->current_header_value);
    client->current_header_key = NULL;
    client->current_header_value = NULL;
    return 0;
}

static int http_on_headers_complete(http_parser *parser)
{
    esp_http_client_handle_t client = parser->data;
    client->response->status_code = parser->status_code;
    client->response->data_offset = parser->nread;
    client->response->content_length = parser->content_length;
    client->response->data_process = 0;
    ESP_LOGD(TAG, "http_on_headers_complete, status=%d, offset=%d, nread=%d", parser->status_code, client->response->data_offset, parser->nread);
    client->state = HTTP_STATE_RES_COMPLETE_HEADER;
    return 0;
}

static int http_on_body(http_parser *parser, const char *at, size_t length)
{
    //TODO: Makeit support gzip
    esp_http_client_t *client = parser->data;
    ESP_LOGD(TAG, "http_on_body %d", length);
    client->response->buffer->raw_data = (char*)at;
    client->response->buffer->raw_len = length;
    client->response->data_process += length;
    http_dispatch_event(client, HTTP_EVENT_ON_DATA, (void *)at, length);
    return 0;
}

static int http_on_message_complete(http_parser *parser)
{
    ESP_LOGD(TAG, "http_on_message_complete, parser=%x", (int)parser);
    // esp_http_client_handle_t client = parser->data;
    esp_http_client_handle_t client = parser->data;
    client->is_chunk_complete = true;
    return 0;
}

static int http_on_chunk_complete(http_parser *parser)
{
    ESP_LOGD(TAG, "http_on_chunk_complete");
    // esp_http_client_handle_t client = parser->data;
    // client->is_chunk_complete = true;
    return 0;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t client, const char *key, const char *value)
{
    return http_header_set(client->request->headers, key, value);
}

esp_err_t esp_http_client_delete_header(esp_http_client_handle_t client, const char *key)
{
    return http_header_delete(client->request->headers, key);
}

static esp_err_t _set_config(esp_http_client_handle_t client, esp_http_client_config_t *config)
{
    client->connection_info.method = config->method;
    client->connection_info.port = config->port;
    client->connection_info.auth_type = config->auth_type;
    client->event_handle = config->event_handle;
    client->timeout_ms = config->timeout_ms;
    client->max_redirection_count = config->max_redirection_count;
    client->user_data = config->user_data;
    client->buffer_size = config->buffer_size;
    client->disable_auto_redirect = config->disable_auto_redirect;

    if (config->buffer_size == 0) {
        client->buffer_size = DEFAULT_HTTP_BUF_SIZE;
    }

    if (client->max_redirection_count == 0) {
        client->max_redirection_count = DEFAULT_MAX_REDIRECT;
    }

    if (config->path) {
        client->connection_info.path = strdup(config->path);
    } else {
        client->connection_info.path = strdup(DEFAULT_HTTP_PATH);
    }
    mem_check(client->connection_info.path);

    if (config->host) {
        client->connection_info.host = strdup(config->host);
        mem_check(client->connection_info.host);
    }

    if (config->query) {
        client->connection_info.query = strdup(config->query);
        mem_check(client->connection_info.query);
    }

    if (config->username) {
        client->connection_info.username = strdup(config->username);
        mem_check(client->connection_info.username);
    }

    if (config->password) {
        client->connection_info.password = strdup(config->password);
        mem_check(client->connection_info.password);
    }

    if (config->transport_type == HTTP_TRANSPORT_OVER_SSL) {
        assign_string(&client->connection_info.scheme, "https", 0);
        if (client->connection_info.port == 0) {
            client->connection_info.port = DEFAULT_HTTPS_PORT;
        }
    } else {
        assign_string(&client->connection_info.scheme, "http", 0);
        if (client->connection_info.port == 0) {
            client->connection_info.port = DEFAULT_HTTP_PORT;
        }
    }
    if (client->timeout_ms == 0) {
        client->timeout_ms = DEFAULT_TIMOUT_MS;
    }

    if (client->connection_info.method == HTTP_METHOD_AUTO) {
        client->connection_info.method = HTTP_METHOD_GET;
    }
    return ESP_OK;
}

static esp_err_t _clear_connection_info(esp_http_client_handle_t client)
{
    if (client->connection_info.path) {
        free(client->connection_info.path);
    }

    if (client->connection_info.host) {
        free(client->connection_info.host);
    }

    if (client->connection_info.query) {
        free(client->connection_info.query);
    }

    if (client->connection_info.username) {
        free(client->connection_info.username);
    }

    if (client->connection_info.password) {
        free(client->connection_info.password);
    }

    if (client->connection_info.scheme) {
        free(client->connection_info.scheme);
    }

    if (client->connection_info.uri) {
        free(client->connection_info.uri);
    }
    memset(&client->connection_info, 0, sizeof(connection_info_t));
    return ESP_OK;
}

static esp_err_t _clear_auth_data(esp_http_client_handle_t client)
{
    if (client->auth_data == NULL) {
        return ESP_FAIL;
    }

    if (client->auth_data->method) {
        free(client->auth_data->method);
        client->auth_data->method = NULL;
    }

    if (client->auth_data->realm) {
        free(client->auth_data->realm);
        client->auth_data->realm = NULL;
    }

    if (client->auth_data->algorithm) {
        free(client->auth_data->algorithm);
        client->auth_data->algorithm = NULL;
    }

    if (client->auth_data->qop) {
        free(client->auth_data->qop);
        client->auth_data->qop = NULL;
    }

    if (client->auth_data->nonce) {
        free(client->auth_data->nonce);
        client->auth_data->nonce = NULL;
    }

    if (client->auth_data->opaque) {
        free(client->auth_data->opaque);
        client->auth_data->opaque = NULL;
    }

    return ESP_OK;
}

static esp_err_t esp_http_client_prepare(esp_http_client_handle_t client)
{
    client->process_again = 0;
    client->response->data_process = 0;
    http_parser_init(client->parser, HTTP_RESPONSE);
    if (client->connection_info.username) {
        char *auth_response = NULL;

        if (client->connection_info.auth_type == HTTP_AUTH_TYPE_BASIC) {
            auth_response = http_auth_basic(client->connection_info.username, client->connection_info.password);
        } else if (client->connection_info.auth_type == HTTP_AUTH_TYPE_DIGEST && client->auth_data) {
            client->auth_data->uri = client->connection_info.path;
            client->auth_data->cnonce = ((uint64_t)esp_random() << 32) + esp_random();
            auth_response = http_auth_digest(client->connection_info.username, client->connection_info.password, client->auth_data);
            client->auth_data->nc ++;
        }

        if (auth_response) {
            ESP_LOGD(TAG, "auth_response=%s", auth_response);
            esp_http_client_set_header(client, "Authorization", auth_response);
            free(auth_response);
        }
    }
    return ESP_OK;
}

esp_http_client_handle_t esp_http_client_init(esp_http_client_config_t *config)
{

    esp_http_client_handle_t client = calloc(1, sizeof(esp_http_client_t));
    assert(client);


    client->parser = calloc(1, sizeof(struct http_parser));
    client->parser_settings = calloc(1, sizeof(struct http_parser_settings));
    client->parser_settings->on_message_begin = http_on_message_begin;
    client->parser_settings->on_url = http_on_url;
    client->parser_settings->on_status = http_on_status;
    client->parser_settings->on_header_field = http_on_header_field;
    client->parser_settings->on_header_value = http_on_header_value;
    client->parser_settings->on_headers_complete = http_on_headers_complete;
    client->parser_settings->on_body = http_on_body;
    client->parser_settings->on_message_complete = http_on_message_complete;
    client->parser_settings->on_chunk_complete = http_on_chunk_complete;

    client->parser->data = client;

    client->auth_data = calloc(1, sizeof(esp_http_auth_data_t));
    assert(client->auth_data);

    client->transport_list = transport_list_init();

    transport_handle_t tcp = transport_tcp_init();
    transport_set_default_port(tcp, DEFAULT_HTTP_PORT);
    transport_list_add(client->transport_list, tcp, "http");

#ifdef CONFIG_ENABLE_HTTPS
    transport_handle_t ssl = transport_ssl_init();
    transport_set_default_port(ssl, DEFAULT_HTTPS_PORT);
    if (config->cert_pem) {
        transport_ssl_set_cert_data(ssl, config->cert_pem, strlen(config->cert_pem));
    }
    transport_list_add(client->transport_list, ssl, "https");
#endif

    _set_config(client, config);

    client->request = calloc(1, sizeof(http_data_t));
    assert(client->request);
    client->response = calloc(1, sizeof(http_data_t));
    assert(client->response);
    client->request->headers = http_header_init();
    client->response->headers = http_header_init();
    /**
     * Init buffer
     */
    client->request->buffer = calloc(1, sizeof(http_buffer_t));
    assert(client->request->buffer);
    client->response->buffer = calloc(1, sizeof(http_buffer_t));
    assert(client->response->buffer);
    // Processing buffer
    client->request->buffer->data = malloc(client->buffer_size);
    assert(client->request->buffer->data);
    client->response->buffer->data = malloc(client->buffer_size);
    assert(client->response->buffer->data);

    esp_http_client_set_uri(client, config->uri);

    // Set default HTTP options
    esp_http_client_set_header(client, "User-Agent", DEFAULT_HTTP_USER_AGENT);
    esp_http_client_set_header(client, "Host", client->connection_info.host);

    client->event.client = client;

    client->state = HTTP_STATE_INIT;
    return client;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client)
{
    esp_http_client_close(client);
    transport_list_destroy(client->transport_list);
    http_header_destroy(client->request->headers);
    free(client->request->buffer->data);
    free(client->request->buffer);
    free(client->request);
    http_header_destroy(client->response->headers);
    free(client->response->buffer->data);
    free(client->response->buffer);
    free(client->response);

    free(client->parser);
    free(client->parser_settings);
    _clear_connection_info(client);

    _clear_auth_data(client);
    free(client->auth_data);

    if (client->current_header_key) {
        free(client->current_header_key);
    }

    if (client->location) {
        free(client->location);
    }

    if (client->auth_header) {
        free(client->auth_header);
    }
    free(client);
    return ESP_OK;
}

static esp_err_t esp_http_check_response(esp_http_client_handle_t client)
{
    char *auth_header = NULL;

    if (client->redirect_counter >= client->max_redirection_count || client->disable_auto_redirect) {
        ESP_LOGE(TAG, "Error, reach max_redirection_count count=%d", client->redirect_counter);
        return ESP_FAIL;
    }
    switch (client->response->status_code) {
        case 301:
        case 302:
            ESP_LOGI(TAG, "Redirect to %s", client->location);
            esp_http_client_set_uri(client, client->location);
            client->redirect_counter ++;
            client->process_again = 1;
            break;
        case 401:
            auth_header = client->auth_header;
            trimwhitespace(&auth_header);
            ESP_LOGI(TAG, "UNAUTHORIZED: %s", auth_header);
            client->redirect_counter ++;
            if (auth_header) {
                if (str_starts_with(auth_header, "Digest") == 0) {
                    ESP_LOGD(TAG, "type = Digest");
                    client->connection_info.auth_type = HTTP_AUTH_TYPE_DIGEST;
                } else if (str_starts_with(auth_header, "Basic") == 0) {
                    ESP_LOGD(TAG, "type = Basic");
                    client->connection_info.auth_type = HTTP_AUTH_TYPE_BASIC;
                } else {
                    client->connection_info.auth_type = HTTP_AUTH_TYPE_NONE;
                    ESP_LOGE(TAG, "Unsupport Auth Type");
                    break;
                }

                _clear_auth_data(client);

                client->auth_data->method = strdup(HTTP_METHOD_MAPPING[client->connection_info.method]);

                client->auth_data->nc = 1;
                client->auth_data->realm = get_string_between(auth_header, "realm=\"", "\"");
                client->auth_data->algorithm = get_string_between(auth_header, "algorithm=", ",");
                client->auth_data->qop = get_string_between(auth_header, "qop=\"", "\"");
                client->auth_data->nonce = get_string_between(auth_header, "nonce=\"", "\"");
                client->auth_data->opaque = get_string_between(auth_header, "opaque=\"", "\"");
                client->process_again = 1;
            }
    }
    return ESP_OK;
}

esp_err_t esp_http_client_set_uri(esp_http_client_handle_t client, const char *uri)
{
    char *old_host = NULL;
    struct http_parser_url puri;
    int old_port;

    if (client == NULL || uri == NULL) {
        ESP_LOGE(TAG, "client or uri must not NULL");
        return ESP_FAIL;
    }

    http_parser_url_init(&puri);

    int parser_status = http_parser_parse_url(uri, strlen(uri), 0, &puri);

    if (parser_status != 0) {
        ESP_LOGE(TAG, "Error parse uri %s", uri);
        return ESP_FAIL;
    }
    old_host = client->connection_info.host;
    old_port = client->connection_info.port;

    if (puri.field_data[UF_HOST].len) {
        assign_string(&client->connection_info.host, uri + puri.field_data[UF_HOST].off, puri.field_data[UF_HOST].len);
    }
    // Close the connection if host was changed
    if (old_host && client->connection_info.host
            && strcasecmp(old_host, (const void *)client->connection_info.host) != 0) {
        ESP_LOGD(TAG, "New host assign = %s", client->connection_info.host)
        esp_http_client_set_header(client, "Host", client->connection_info.host);
        esp_http_client_close(client);
    }

    if (puri.field_data[UF_SCHEMA].len) {
        assign_string(&client->connection_info.scheme, uri + puri.field_data[UF_SCHEMA].off, puri.field_data[UF_SCHEMA].len);
        if (strcasecmp(client->connection_info.scheme, "http") == 0) {
            client->connection_info.port = DEFAULT_HTTP_PORT;
        } else if (strcasecmp(client->connection_info.scheme, "https") == 0) {
            client->connection_info.port = DEFAULT_HTTPS_PORT;
        }
    }

    if (puri.field_data[UF_HOST].len) {
        assign_string(&client->connection_info.host, uri + puri.field_data[UF_HOST].off, puri.field_data[UF_HOST].len);
    }

    if (puri.field_data[UF_PORT].len) {
        char *port_str = NULL;
        assign_string(&port_str, uri + puri.field_data[UF_PORT].off, puri.field_data[UF_PORT].len);
        if (port_str) {
            client->connection_info.port = atoi(port_str);
            free(port_str);
        }
    }

    if (old_port != client->connection_info.port) {
        esp_http_client_close(client);
    }

    if (puri.field_data[UF_USERINFO].len) {
        char *user_info = NULL;
        assign_string(&user_info, uri + puri.field_data[UF_USERINFO].off, puri.field_data[UF_USERINFO].len);
        if (user_info) {
            char *username = user_info;
            char *password = strchr(user_info, ':');
            if (password) {
                *password = 0;
                password ++;
                assign_string(&client->connection_info.password, password, 0);
            }
            assign_string(&client->connection_info.username, username, 0);
            free(user_info);
        }
    }


    //Reset path and query if there are no information
    if (puri.field_data[UF_PATH].len) {
        assign_string(&client->connection_info.path, uri + puri.field_data[UF_PATH].off, puri.field_data[UF_PATH].len);
    } else {
        assign_string(&client->connection_info.path, "/", 0);
    }

    if (puri.field_data[UF_QUERY].len) {
        assign_string(&client->connection_info.query, uri + puri.field_data[UF_QUERY].off, puri.field_data[UF_QUERY].len);
    }

    return ESP_OK;
}

esp_err_t esp_http_client_set_method(esp_http_client_handle_t client, esp_http_client_method_t method)
{
    client->connection_info.method = method;
    return ESP_OK;
}

static int esp_http_client_get_data(esp_http_client_handle_t client)
{
    if (client->state < HTTP_STATE_RES_COMPLETE_HEADER) {
        return -1;
    }
    http_buffer_t *res_buffer = client->response->buffer;

    ESP_LOGD(TAG, "data_process=%d, content_length=%d", client->response->data_process, client->response->content_length);

    int rlen = transport_read(client->transport, res_buffer->data, client->buffer_size, client->timeout_ms);
    if (rlen >= 0) {
        http_parser_execute(client->parser, client->parser_settings, res_buffer->data, rlen);
    }
    return rlen;
}

int esp_http_client_read(esp_http_client_handle_t client, char *buffer, int len)
{
    http_buffer_t *res_buffer = client->response->buffer;

    int rlen = -1, ridx = 0;
    if (res_buffer->raw_len) {
        int remain_len = client->response->buffer->raw_len;
        if (remain_len > len) {
            remain_len = len;
        }
        memcpy(buffer, res_buffer->raw_data, remain_len);
        res_buffer->raw_len -= remain_len;
        res_buffer->raw_data += remain_len;
        ridx = remain_len;
    }
    int need_read = len - ridx;
    bool is_data_remain = true;
    while (need_read > 0 && is_data_remain) {
        if (client->response->is_chunked) {
            is_data_remain = !client->is_chunk_complete;
        } else {
            is_data_remain = client->response->data_process < client->response->content_length;
        }
        ESP_LOGD(TAG, "is_data_remain=%d, is_chunked=%d", is_data_remain, client->response->is_chunked);
        if (!is_data_remain) {
            break;
        }
        int byte_to_read = need_read;
        if (byte_to_read > client->buffer_size) {
            byte_to_read = client->buffer_size;
        }
        rlen = transport_read(client->transport, res_buffer->data, byte_to_read, client->timeout_ms);
        ESP_LOGD(TAG, "need_read=%d, byte_to_read=%d, rlen=%d, ridx=%d", need_read, byte_to_read, rlen, ridx);

        if (rlen <= 0) {
            return ridx;
        }
        http_parser_execute(client->parser, client->parser_settings, res_buffer->data, rlen);

        if (res_buffer->raw_len) {
            memcpy(buffer + ridx, res_buffer->raw_data, res_buffer->raw_len);
            ridx += res_buffer->raw_len;
            need_read -= res_buffer->raw_len;
        }
        res_buffer->raw_len = 0; //clear
    }

    return ridx;
}

esp_err_t esp_http_client_perform(esp_http_client_handle_t client)
{
    do {
        if (esp_http_client_open(client, client->post_len) != ESP_OK) {
            return ESP_FAIL;
        }
        if (client->post_data && client->post_len) {
            if (esp_http_client_write(client, client->post_data, client->post_len) <= 0) {
                ESP_LOGE(TAG, "Error upload data");
                return ESP_FAIL;
            }
        }
        esp_http_client_fetch_headers(client);

        if (esp_http_check_response(client) != ESP_OK) {
            ESP_LOGE(TAG, "Error response");
            return ESP_FAIL;
        }
        //TODO: add suport chunk download
        while (client->response->is_chunked && !client->is_chunk_complete) {
            if (esp_http_client_get_data(client) <= 0) {
                ESP_LOGI(TAG, "Read finish or server requests close");
                break;
            }
        }
        while (client->response->data_process < client->response->content_length) {
            if (esp_http_client_get_data(client) <= 0) {
                ESP_LOGI(TAG, "Read finish or server requests close");
                break;
            }
        }

        http_dispatch_event(client, HTTP_EVENT_ON_FINISH, NULL, 0);

        if (!http_should_keep_alive(client->parser)) {
            ESP_LOGD(TAG, "Close connection");
            esp_http_client_close(client);
        } else {
            if (client->state > HTTP_STATE_CONNECTED) {
                client->state = HTTP_STATE_CONNECTED;
            }
        }
    } while (client->process_again);
    return ESP_OK;
}


int esp_http_client_fetch_headers(esp_http_client_handle_t client)
{
    if (client->state < HTTP_STATE_REQ_COMPLETE_HEADER) {
        return -1;
    }

    client->state = HTTP_STATE_REQ_COMPLETE_DATA;
    http_buffer_t *buffer = client->response->buffer;
    client->response->status_code = -1;

    while (client->state < HTTP_STATE_RES_COMPLETE_HEADER) {
        buffer->len = transport_read(client->transport, buffer->data, client->buffer_size, client->timeout_ms);
        if (buffer->len <= 0) {
            return -1;
        }
        http_parser_execute(client->parser, client->parser_settings, buffer->data, buffer->len);
    }
    ESP_LOGD(TAG, "content_length = %d", client->response->content_length);
    if (client->response->content_length <= 0) {
        client->response->is_chunked = true; //TODO: separate chunk and stream
    }
    return client->response->content_length;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int write_len)
{
    if (client->state == HTTP_STATE_UNINIT) {
        ESP_LOGE(TAG, "Client has not been initialized");
        return ESP_FAIL;
    }

    if (esp_http_client_prepare(client) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize request data");
        esp_http_client_close(client);
        return ESP_FAIL;
    }

    if (client->state < HTTP_STATE_CONNECTED) {
        ESP_LOGD(TAG, "Begin connect to: %s://%s:%d", client->connection_info.scheme, client->connection_info.host, client->connection_info.port);
        client->transport = transport_list_get_transport(client->transport_list, client->connection_info.scheme);
        if (client->transport == NULL) {
            ESP_LOGE(TAG, "No transport found");
            return ESP_FAIL;
        }
        if (transport_connect(client->transport, client->connection_info.host, client->connection_info.port, client->timeout_ms) < 0) {
            ESP_LOGE(TAG, "Connection failed, sock < 0");
            return ESP_FAIL;
        }
        http_dispatch_event(client, HTTP_EVENT_ON_CONNECTED, NULL, 0);
        client->state = HTTP_STATE_CONNECTED;
    }

    if (write_len >= 0) {
        http_header_set_format(client->request->headers, "Content-Length", "%d", write_len);
    } else if (write_len < 0) {
        esp_http_client_set_header(client, "Transfer-Encoding", "chunked");
        esp_http_client_set_method(client, HTTP_METHOD_POST);
    }

    int header_index = 0;
    int wlen = client->buffer_size;

    const char *method = HTTP_METHOD_MAPPING[client->connection_info.method];

    int first_line = snprintf(client->request->buffer->data,
                              client->buffer_size, "%s %s",
                              method,
                              client->connection_info.path);
    if (client->connection_info.query) {
        first_line += snprintf(client->request->buffer->data + first_line,
                               client->buffer_size - first_line, "?%s", client->connection_info.query);
    }
    first_line += snprintf(client->request->buffer->data + first_line,
                           client->buffer_size - first_line, " %s\r\n", DEFAULT_HTTP_PROTOCOL);
    wlen -= first_line;

    while ((header_index = http_header_generate_string(client->request->headers, header_index, client->request->buffer->data + first_line, &wlen))) {
        if (wlen <= 0) {
            break;
        }
        if (first_line) {
            wlen += first_line;
            first_line = 0;
        }
        client->request->buffer->data[wlen] = 0;
        ESP_LOGD(TAG, "Write header[%d]:\r\n%s", header_index, client->request->buffer->data);
        if (transport_write(client->transport, client->request->buffer->data, wlen, client->timeout_ms) <= 0) {
            ESP_LOGE(TAG, "Error write request");
            esp_http_client_close(client);
            return ESP_FAIL;
        }
        wlen = client->buffer_size;
    }
    client->state = HTTP_STATE_REQ_COMPLETE_HEADER;
    return ESP_OK;
}


int esp_http_client_write(esp_http_client_handle_t client, const char *buffer, int len)
{
    if (client->state < HTTP_STATE_REQ_COMPLETE_HEADER) {
        return -1;
    }
    int need_write;
    int wlen = 0, widx = 0;
    while (len > 0) {
        need_write = len;
        if (need_write > client->buffer_size) {
            need_write = client->buffer_size;
        }
        wlen = transport_write(client->transport, buffer + widx, need_write, client->timeout_ms);
        if (wlen <= 0) {
            return wlen;
        }
        widx += wlen;
        len -= wlen;
    }
    return widx;
}

esp_err_t esp_http_client_close(esp_http_client_handle_t client)
{
    if (client->state >= HTTP_STATE_INIT) {
        http_dispatch_event(client, HTTP_EVENT_DISCONNECTED, NULL, 0);
        client->state = HTTP_STATE_INIT;
        return transport_close(client->transport);
    }
    return ESP_OK;
}

esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t client, const char *data, int len)
{
    client->post_data = (char *)data;
    client->post_len = len;
    ESP_LOGD(TAG, "set post file length = %d", len);
    if (client->post_data) {
        esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    } else {
        client->post_len = 0;
        esp_http_client_set_header(client, "Content-Type", NULL);
    }
    return ESP_OK;
}

int esp_http_client_get_post_field(esp_http_client_handle_t client, char **data)
{
    if (client->post_data) {
        *data = client->post_data;
        return client->post_len;
    }
    return 0;
}

int esp_http_client_get_status_code(esp_http_client_handle_t client)
{
    return client->response->status_code;
}

int esp_http_client_get_content_length(esp_http_client_handle_t client)
{
    return client->response->content_length;
}

bool esp_http_client_is_chunked_response(esp_http_client_handle_t client)
{
    return client->response->is_chunked;
}
