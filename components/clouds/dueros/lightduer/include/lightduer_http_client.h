/**
 * Copyright (2017) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_http_client.h
 * Author: Pan Haijun, Gang Chen(chengang12@baidu.com)
 * Desc: HTTP Client Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_HTTP_CLIENT_H
#define BAIDU_DUER_LIGHTDUER_HTTP_CLIENT_H

#include <stdlib.h>
#include <stdio.h>
#include "lightduer_timers.h"
#include "lightduer_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DUER_HTTP_CONTENT_TYPE_LEN_MAX  (32)  // the max length of "content type" section

//http client results
typedef enum {
    DUER_HTTP_ERR_FAILED = -1,
    DUER_HTTP_OK,                // Success
    DUER_HTTP_PROCESSING,        // Processing
    DUER_HTTP_ERR_PARSE,         // url Parse error
    DUER_HTTP_ERR_DNS,           // Could not resolve name
    DUER_HTTP_ERR_PRTCL,         // Protocol error
    DUER_HTTP_ERR_NOT_FOUND,     // HTTP 404 Error
    DUER_HTTP_ERR_REFUSED,       // HTTP 403 Error
    DUER_HTTP_ERR_TIMEOUT,       // Connection timeout
    DUER_HTTP_ERR_CONNECT,       // Connection error
    DUER_HTTP_CLOSED,            // Connection was closed by remote host
    DUER_HTTP_ERR_NOT_SUPPORT,   // not supported feature
    DUER_HTTP_REDIRECTTION,      // take a redirection when http header contains 'Location'
    DUER_HTTP_TOO_MANY_REDIRECT, // redirect too many times
} duer_http_result_t;

typedef enum {
    DUER_HTTP_GET,     // Just 'GET' is supported, other methods are reserved
    DUER_HTTP_POST,    // reserved
    DUER_HTTP_PUT,     // reserved
    DUER_HTTP_DELETE,  // reserved
    DUER_HTTP_HEAD     // reserved
} duer_http_method_t;

typedef struct {
    duer_handler n_handle;
    duer_handler (*init)(void *socket_args);
    int  (*open)(duer_handler socket_handle);
    int  (*connect)(duer_handler socket_handle, const char *host, duer_u32_t ip, const int port,
            duer_u32_t timeout);
    void (*set_blocking)(duer_handler socket_handle, int blocking);
    void (*set_timeout)(duer_handler socket_handle, int timeout);
    int  (*recv)(duer_handler socket_handle, void *data, unsigned size);
    int  (*send)(duer_handler socket_handle, const void *data, unsigned size);
    int  (*close)(duer_handler socket_handle);
    void (*destroy)(duer_handler socket_handle);
} duer_http_socket_ops_t;

//to tell data output callback user that if the current data block is first block or last block
typedef enum {
    DUER_HTTP_DATA_FIRST  = 0x1,
    DUER_HTTP_DATA_MID    = 0x2,
    DUER_HTTP_DATA_LAST   = 0x4
} duer_http_data_pos_t;

/**
 * DESC:
 * the callback for retrieving http response headers (invoked once for each key/value pair)
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registered by user
 * @param[in] key: key of the header entry
 * @param[in] value: value of the header entry
 */
typedef void (*duer_http_header_cb)(void *p_user_ctx, const char *key, const char *value);

/**
 *
 * DESC:
 * the callback to handler the data download by http
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registed by user
 * @param[in] pos:   to identify if it is data stream's start, or middle , or end of data stream
 * @param[in] buf:   buffer stored media data
 * @param[in] len:   data length in 'buf'
 * @param[in] type:  data type to identify media or others
 *
 * @RETURN: negative number when failed
 */
typedef int (*duer_http_data_handler)(void *p_user_ctx, duer_http_data_pos_t pos,
                                      const char *buf, size_t len, const char *type);

/**
 * DESC:
 * sometimes other module need to know which url is used to download data.
 * this callback is used to get the url
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registed by user
 * @param[in] url: the url used by http to download data
 *
 * RETURN: none
 */
typedef void (*duer_http_get_url_cb_t)(void *p_user_ctx, const char *url);

/**
 * DESC:
 * Sometimes we want to keep retrying until we notify http to stop
 * this callback is used to get the keep retry flag
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registed by user
 *
 * RETURN: 0 stop retry, nonzero keep retry
 */
typedef int (*duer_http_keep_retry_cb)(void *p_user_ctx);

/**
 * DESC:
 * Inform user the connection ip
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registed by user
 * @param[in] ip: the connection ip
 * @param[in] dns_type: defined as duer_auto_dns_resolve_type_t
 * @param[in] result: connect result, 0: success nonzero: failed
 *
 * RETURN: none
 */
typedef void (*duer_http_inform_ip_cb)(
        void *p_user_ctx, const char *host, const char *ip, int dns_type, int result);

typedef struct {
    const char               **pps_custom_headers;
    size_t                     sz_headers_count;
    int                        n_http_response_code; // http response code
    duer_http_socket_ops_t     ops;                  // socket operations
    int                        scheme;               // http or https type,1 is https, 0 is http
    duer_http_data_handler     data_hdlr_cb;         // callback for output data
    void                      *p_data_hdlr_ctx;      // users args for data_hdlr_cb
    int                        n_http_content_len;   // http content length
    // http content type
    char                       p_http_content_type[DUER_HTTP_CONTENT_TYPE_LEN_MAX];
    char                      *p_location;           // http header "Location"
    int                        stop_flag;
    // to get the url used to download, the last url will be returned if 302 location happen
    duer_http_get_url_cb_t     get_url_cb;
    int                        n_is_chunked;
    size_t                     sz_chunk_size;
    char                      *p_chunk_buf;
    size_t                     recv_size;            // size of the data have been dwonload
    // the count of http continuously try to resume from break-point
    int                        resume_retry_count;
    char                      *buf;                  // buf used to receive data
    duer_http_data_pos_t       data_pos;             // play position
    char                      *last_host;            // the lastest connected host
    unsigned short             last_port;            // the lastest connected port
    duer_timer_handler         connect_keep_timer;   // used to close persistent connect
    size_t                     upload_size;          // size of the data have been upload
    size_t                     timeout_count;        // current timeout count

    // max reconnect retry count
    size_t                     max_resume_retry_count;
    // max retry count when timeout becore reconnecct
    size_t                     max_timeout_retry_count;
    size_t                     max_connect_retry_count;
    size_t                     timeout_ms;           // recv timeout in milliseconds
    size_t                     connect_timeout_ms;   // connect timeout in milliseconds
    duer_http_keep_retry_cb    keep_retry_cb;
    duer_http_header_cb        rsp_header_cb;        // callback for response header
    duer_http_inform_ip_cb     inform_ip_cb;         // callback for inform the connection ip
    duer_mutex_t               lock;
} duer_http_client_t;

/**
 * DESC:
 * init the http client
 *
 * PARAM:
 * @param[in] p_client: pointer of the http client
 *
 * RETURN: DUER_HTTP_OK if success, DUER_HTTP_ERR_FAILED if failed
 */
duer_http_result_t duer_http_init(duer_http_client_t *p_client);

/**
 * DESC:
 * destroy the http client
 *
 * PARAM:
 * @param[in] p_client: pointer of the http client
 *
 * RETURN: none
 */
void duer_http_destroy(duer_http_client_t *p_client);

/**
 * DESC:
 * to init http client socket operations
 *
 * PARAM:
 * @param[in] p_client: pointer of the http client
 * @param[in] p_ops: socket operations set
 * @param[in] socket_args: args for ops "get_inst"
 *
 * RETURN: DUER_HTTP_OK if success, DUER_HTTP_ERR_FAILED if failed
 */
duer_http_result_t duer_http_init_socket_ops(duer_http_client_t *p_client,
        duer_http_socket_ops_t *p_ops,
        void *socket_args);

/**
 * DESC:
 * register data output handler callback to handle data block
 *
 * @param[in] p_client: pointer of the http client
 * @param[in] data_hdlr_cb: data output handler callback to be registered
 * @param[in] p_usr_ctx: user context for data output handler
 *
 * @RETURN    none
 */
void duer_http_reg_data_hdlr(duer_http_client_t *p_client,
                             duer_http_data_handler data_hdlr_cb,
                             void *p_usr_ctx);

/**
 *
 * DESC:
 * register callback to get the url which is used by http to get data
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] cb:       the callback to be registered
 *
 * @RETURN    none
 */
void duer_http_reg_url_get_cb(duer_http_client_t *p_client, duer_http_get_url_cb_t cb);

/**
 *
 * DESC:
 * register callback to get the keep retry flag
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] cb:       the callback to be registered
 *
 * @RETURN    none
 */
void duer_http_reg_keep_retry_cb(duer_http_client_t *p_client, duer_http_keep_retry_cb cb);

/**
 *
 * DESC:
 * register callback to get the response headers
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] cb:       the callback to be registered
 *
 * @RETURN    none
 */
void duer_http_reg_rsp_header_cb(duer_http_client_t *p_client, duer_http_header_cb cb);

/**
 *
 * DESC:
 * register callback to get the ip
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] cb:       the callback to be registered
 *
 * @RETURN    none
 */
void duer_http_reg_inform_ip_cb(duer_http_client_t *p_client, duer_http_inform_ip_cb cb);

/**
 *
 * DESC:
 * set max resume retry count
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] count: max resume retry count
 *
 * @RETURN    none
 */
void duer_http_set_max_resume_retry_count(duer_http_client_t *p_client, size_t count);

/**
 *
 * DESC:
 * set max retry count when read timeout
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] count: retry count
 *
 * @RETURN    none
 */
void duer_http_set_max_timeout_retry_count(duer_http_client_t *p_client, size_t count);

/**
 *
 * DESC:
 * set connect timeout in milliseconds
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] timeout_ms: timeout in milliseconds
 *
 * @RETURN    none
 */
void duer_http_set_connect_timeout_ms(duer_http_client_t *p_client, size_t timeout_ms);

/**
 *
 * DESC:
 * set recv timeout in milliseconds
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] timeout_ms: timeout in milliseconds
 *
 * @RETURN    none
 */
void duer_http_set_recv_timeout_ms(duer_http_client_t *p_client, size_t timeout_ms);

/**
 * DESC:
 * Set custom headers for request.
 * Pass NULL, 0 to turn off custom headers.
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 * @param[in] headers: an array (size multiple of two) key-value pairs,
 *                     must remain valid during the whole HTTP session
 *                     const char * hdrs[] =
 *                     {
 *                         "Connection", "keep-alive",
 *                         "Accept", "text/html",
 *                         "User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64)",
 *                         "Accept-Encoding", "gzip,deflate,sdch",
 *                         "Accept-Language", "en-US,en;q=0.8",
 *                     };
 *
 *                     duer_http_set_cust_headers(hdrs, 5);
 * @param[in] pairs:   number of key-value pairs
 *
 * @RETURN    none
 */
void duer_http_set_cust_headers(duer_http_client_t *p_client, const char **headers, size_t pairs);

/**
 * DESC:
 * get data by http client
 *
 * PARAM:
 * @param[in] p_client:          pointer point to the duer_http_client_t
 * @param[in] url:               url resource
 * @param[in] pos:               the position to receive the http data,
 *                               sometimes user only want to get part of the data
 * @param[in] connect_keep_time: how long time the connection should be kept after download finish
 *
 * RETURN     DUER_HTTP_OK if success, other duer_http_result_t type code if failed
 */
duer_http_result_t duer_http_get(duer_http_client_t *p_client,
                                 const char *url,
                                 const size_t pos,
                                 const int connect_keep_time);

/**
 * DESC:
 * get http response code
 *
 * PARAM:
 * @param[in] p_client: pointer point to the duer_http_client_t
 *
 * @RETURN    none
 */
int duer_http_get_rsp_code(duer_http_client_t *p_client);

/**
 * DESC:
 * Notify the http module to stop download.
 *
 * @param[in] p_client:   pointer of the http client
 *
 * @RETURN    none
 */
void duer_http_stop_notify(duer_http_client_t *p_client);

/**
 * DESC:
 * get http download progress
 *
 * PARAM:
 * @param[in]  p_client:   pointer point to the duer_http_client_t
 * @param[out] total_size: pointer point to a varirable to receive the total size to be download,
 *                         0 will be returned if it's chunk transfer
 * @param[out] recv_size:  pointer point to a varirable to receive the data size have been download
 *
 * RETURN      none
 */
void duer_http_get_download_progress(duer_http_client_t *p_client,
                                     size_t *total_size,
                                     size_t *recv_size);

/**
 * DESC:
 * post data to http server
 *
 * PARAM:
 * @param[in] p_client:       pointer point to the duer_http_client_t
 * @param[in] url:            url resource
 * @param[in] connect_keep_time: how long time the connection should be kept after download finish
 * @param[in] data:           the data post to server
 * @param[in] len:            the length of data
 * @param[in] content_type:   the data type
 *
 * RETURN                     DUER_HTTP_OK if success, other duer_http_result_t type code if failed
 */
duer_http_result_t duer_http_post(duer_http_client_t *p_client,
                                 const char *url,
                                 const int connect_keep_time,
                                 const char *data,
                                 size_t len,
                                 const char *content_type);

/**
 * DESC:
 * do http request with method specified
 *
 * PARAM:
 * @param[in] p_client:       pointer point to the duer_http_client_t
 * @param[in] url:            url resource
 * @param[in] method:         http method
 * @param[in] connect_keep_time: how long time the connection should be kept after download finish
 * @param[in] data:           the data post to server
 * @param[in] len:            the length of data
 * @param[in] content_type:   the data type
 *
 * RETURN                     DUER_HTTP_OK if success, other duer_http_result_t type code if failed
 */
duer_http_result_t duer_http_perform(duer_http_client_t *p_client,
                                     const char *url,
                                     duer_http_method_t method,
                                     const int connect_keep_time,
                                     const char *data,
                                     size_t len,
                                     const char *content_type);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_HTTP_CLIENT_H
