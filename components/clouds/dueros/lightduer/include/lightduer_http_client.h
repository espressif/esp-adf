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

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_HTTP_CLIENT_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_HTTP_CLIENT_H

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//http client results
typedef enum http_result {
    HTTP_OK = 0,        // Success
    HTTP_PROCESSING,    // Processing
    HTTP_PARSE,         // url Parse error
    HTTP_DNS,           // Could not resolve name
    HTTP_PRTCL,         // Protocol error
    HTTP_NOTFOUND,      // HTTP 404 Error
    HTTP_REFUSED,       // HTTP 403 Error
    HTTP_ERROR,         // HTTP xxx error
    HTTP_TIMEOUT,       // Connection timeout
    HTTP_CONN,          // Connection error
    HTTP_CLOSED,        // Connection was closed by remote host
    HTTP_NOT_SUPPORT,   // not supported feature
    HTTP_REDIRECTTION,  //take a redirection when http header contains 'Location'
    HTTP_FAILED = -1,
} e_http_result;

typedef enum http_meth {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD
} e_http_meth;

typedef struct http_client_socket_ops {
    int  n_handle;
    int  (*init)(void* socket_args);
    int  (*open)(int socket_handle);
    int  (*connect)(int socket_handle, const char* host, const int port);
    void (*set_blocking)(int socket_handle, int blocking);
    void (*set_timeout)(int socket_handle, int timeout);
    int  (*recv)(int socket_handle, void* data, unsigned size);
    int  (*send)(int socket_handle, const void* data, unsigned size);
    int  (*close)(int socket_handle);
    void (*destroy)(int socket_handle);
} socket_ops;

//to tell data output callback user that if the current data block is first block or last block
typedef enum data_pos {
    DATA_FIRST  = 0x1,
    DATA_MID    = 0x2,
    DATA_LAST   = 0x4
} e_data_pos;

/**
 *
 * DESC:
 * the callback to handler the media data download by http
 *
 * PARAM:
 * @param[in] p_user_ctx: usr ctx registed by user
 * @param[in] pos:  to identify if it is data stream's start, or middle , or end of data stream
 * @param[in] buf:   buffer stored media data
 * @param[in] len:   data length in 'buf'
 * @param[in] type: data type to identify media or others
 *
 * @RETURN: negative number when failed
 */
typedef int (*data_out_handler_cb)(void* p_user_ctx, e_data_pos pos,
                                   const char* buf, size_t len, const char* type);
/**
 * DESC:
 * callback to check whether need to stop getting data
 *
 * PARAM: none
 *
 * @RETURN
 * 1: to stop; 0: no stop
 */
typedef int (*check_stop_notify_cb_t)();

/**
 * DESC:
 * sometimes other module need to know which url is used to download data.
 * this callback is used to get the url
 *
 * PARAM:
 * @param[in] url: the url used by http to download data
 *
 * RETURN: none
 */
typedef void (*get_url_cb_t)(const char *url);

#define HC_CONTENT_TYPE_LEN_MAX 32 // the max length of "content type" section

typedef struct http_client_c {
    const char**           pps_custom_headers;
    size_t                 sz_headers_count;
    int                    n_http_response_code; // http response code
    socket_ops             ops;                  // socket operations
    data_out_handler_cb    data_hdlr_cb;         // callback for output data
    void*                  p_data_hdlr_ctx;      // users args for data_hdlr_cb
    int                    n_http_content_len;   // http content length
    char                   p_http_content_type[HC_CONTENT_TYPE_LEN_MAX]; // http content type
    char*                  p_location;           // http header "Location"
    // a callback to check that stopping http client getting data or not
    check_stop_notify_cb_t check_stop_notify_cb;
    get_url_cb_t           get_url_cb;           // get the url which is used by http to get data
    int                    n_is_chunked;
    size_t                 sz_chunk_size;
    char*                  p_chunk_buf;
    size_t                 recv_size;             // size of the data have been dwonload
    // the count of http continuously try to resume from break-point
    int                    resume_retry_count;
    char*                  buf;                   // buf used to receive data
    e_data_pos             data_pos;              // play position
} http_client_c;

typedef struct _http_client_statistic_s {
    size_t download_data_size;
    size_t upload_data_size;
    int    error_count;
    int    last_error_code;
    size_t recv_packet_longest_time;
} http_client_statistic_t;

int duer_http_client_init(http_client_c* p_client);

void duer_http_client_destroy(http_client_c* p_client);

int duer_http_client_init_socket_ops(
        http_client_c* p_client,
        socket_ops* p_ops,
        void* socket_args);

void duer_http_client_reg_data_hdlr_cb(
        http_client_c* p_client,
        data_out_handler_cb data_hdlr_cb,
        void* p_usr_ctx);
void duer_http_client_reg_cb_to_get_url(http_client_c* p_client, get_url_cb_t cb);

void duer_http_client_set_cust_headers(
        http_client_c* p_client,
        const char** headers,
        size_t pairs);

e_http_result duer_http_client_get(http_client_c* p_client, const char* url, const size_t pos);

int duer_http_client_get_resp_code(http_client_c* p_client);

void duer_http_client_reg_stop_notify_cb(
        http_client_c* p_client,
        check_stop_notify_cb_t chk_stp_cb);

/**
 * FUNC:
 * int duer_get_http_statistic(http_client_c *p_client, socket_ops *p_ops, void *socket_args)
 *
 * DESC:
 * used to get statistic info of http module
 *
 * PARAM:
 * @param[out] statistic_result: buffer to accept the statistic info
 *
 */
void duer_get_http_statistic(http_client_statistic_t* statistic_result);

/**
 * DESC:
 * get http download progress
 *
 * PARAM:
 * @param[in] p_client: pointer point to the http_client_c
 * @param[out] total_size: pointer point to a varirable to receive the total size to be download
 * @param[out] recv_size: pointer point to a varirable to receive the data size have been download
 */
void duer_http_client_get_download_progress(http_client_c* p_client,
                                       size_t* total_size,
                                       size_t* recv_size);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_HTTP_CLIENT_H
