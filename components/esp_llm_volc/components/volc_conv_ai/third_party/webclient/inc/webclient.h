/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-05-05     Bernard      the first version
 * 2013-06-10     Bernard      fix the slow speed issue when download file.
 * 2015-11-14     aozima       add content_length_remainder.
 * 2017-12-23     aozima       update gethostbyname to getaddrinfo.
 * 2018-01-04     aozima       add ipv6 address support.
 * 2018-07-26     chenyong     modify log information
 * 2018-08-07     chenyong     modify header processing
 */

#ifndef __WEBCLIENT_H__
#define __WEBCLIENT_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>

#define CONFIG_WEBCLIENT_HTTPS_SUPPORTED
#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
#include "tls_client.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_TARGET_PLATFORM_RTTHREAD
// RT_Thread
#ifndef web_malloc
#define web_malloc rt_malloc
#endif

#ifndef web_calloc
#define web_calloc rt_calloc
#endif

#ifndef web_realloc
#define web_realloc rt_realloc
#endif

#ifndef web_free
#define web_free rt_free
#endif

#ifndef web_strdup
#define web_strdup rt_strdup
#endif
#else
// Linux X86_64
#define RT_NULL NULL
#define RT_FALSE false
#define rt_int32_t int32_t
#define rt_uint32_t uint32_t

#define RT_ASSERT assert
#define rt_calloc calloc
#define rt_strlen strlen
#define rt_strcpy strcpy
#define rt_strncpy strncpy
#define rt_strcmp strcmp
#define rt_strstr strstr
#define rt_memset memset
#define rt_vsnprintf vsnprintf
#define rt_snprintf snprintf
#define LOG_E printf
#define LOG_D printf
#define rt_kprintf printf

#define web_malloc malloc
#define web_calloc calloc
#define web_realloc realloc
#define web_free free
#define web_strdup strdup

/* RT-Thread error code definitions */
#define RT_EOK 0 /**< There is no error */
#define RT_ERROR 1 /**< A generic error happens */
#define RT_ETIMEOUT 2 /**< Timed out */
#define RT_EFULL 3 /**< The resource is full */
#define RT_EEMPTY 4 /**< The resource is empty */
#define RT_ENOMEM 5 /**< No memory */
#define RT_ENOSYS 6 /**< No system */
#define RT_EBUSY 7 /**< Busy */
#define RT_EIO 8 /**< IO error */
#define RT_EINTR 9 /**< Interrupted system call */
#define RT_EINVAL 10 /**< Invalid argument */
#endif // CONFIG_TARGET_PLATFORM_RTTHREAD

#define WEBCLIENT_SW_VERSION "2.2.0"
#define WEBCLIENT_SW_VERSION_NUM 0x20200

#define WEBCLIENT_HEADER_BUFSZ 4096
#define WEBCLIENT_RESPONSE_BUFSZ 4096

enum WEBCLIENT_STATUS {
  WEBCLIENT_OK,
  WEBCLIENT_ERROR,
  WEBCLIENT_TIMEOUT,
  WEBCLIENT_NOMEM,
  WEBCLIENT_NOSOCKET,
  WEBCLIENT_NOBUFFER,
  WEBCLIENT_CONNECT_FAILED,
  WEBCLIENT_DISCONNECT,
  WEBCLIENT_FILE_ERROR,
};

enum WEBCLIENT_METHOD { WEBCLIENT_USER_METHOD, WEBCLIENT_GET, WEBCLIENT_POST, WEBCLIENT_HEAD, WEBCLIENT_PUT };

struct webclient_header {
  char *buffer;
  size_t length; /* content header buffer size */

  size_t size; /* maximum support header size */
};

typedef struct webclient_session {
  struct webclient_header *header; /* webclient response header information */
  int socket;
  int resp_status;

  char *host; /* server host */
  int port; /* server port */
  char *req_url; /* HTTP request address*/

  int chunk_sz;
  int chunk_offset;

  int content_length;
  size_t content_remainder; /* remainder of content length */
  int (*handle_function)(char *buffer, int size); /* handle function */

  bool is_tls; /* HTTPS connect */

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  MbedTLSSession *tls_session; /* mbedtls connect session */
  char *cert_buf; /* the certificate of Server if URL uses HTTPS to connect */
  size_t cert_len; /* the length of certificate, include the terminal character '\0' */
#endif
} webclient_session;

/**
 * @brief create webclient session and set header response size
 *
 * @param header_sz the max length of header supported
 * @return struct webclient_session*
 */
struct webclient_session *webclient_session_create(size_t header_sz, const char *cert_buf, size_t cert_len);

/**
 * @brief close and release wenclient session
 *
 * @param session the webclient session which is already created successfully
 * @return 0: close successfully
 */
int webclient_close(struct webclient_session *session);

/**
 * @brief send HTTP GET request
 *
 * @param session the webclient session
 * @param URI the address of the server connected by HTTP
 * @return >0: the code of HTTP response;
 *         <0: failure to send GET request.
 */
int webclient_get(struct webclient_session *session, const char *URI);

/* send HTTP HEAD request */
int webclient_shard_head_function(struct webclient_session *session, const char *URI, int *length);

/* send HTTP Range parameter, shard download */
int webclient_shard_position_function(struct webclient_session *session, const char *URI, int start,
                                      int length, int mem_size);
int *webclient_register_shard_position_function(struct webclient_session *session,
                                                int (*handle_function)(char *buffer, int size));

/**
 * @brief send POST request to server and get response header data.
 *
 * @param session webclient session
 * @param URI input server URI address
 * @param post_data data send to the server
 *                = NULL: just connect server and send header
 *               != NULL: send header and body data, resolve response data
 * @param data_len the length of send data
 *
 * @return <0: send POST request failed
 *         =0: send POST header success
 *         >0: response http status code
 */
int webclient_post(struct webclient_session *session, const char *URI, const void *post_data,
                   size_t data_len);

/**
 * @brief send PUT request to server and get response header data.
 *
 * @param session webclient session
 * @param URI input server URI address
 * @param put_data data send to the server
 *                = NULL: just connect server and send header
 *               != NULL: send header and body data, resolve response data
 * @param data_len the length of send data
 *
 * @return <0: send POST request failed
 *         =0: send POST header success
 *         >0: response http status code
 */
int webclient_put(struct webclient_session *session, const char *URI, const void *put_data,
                  size_t data_len);

int webclient_set_timeout(struct webclient_session *session, int millisecond);

/* send or receive data from server */
int webclient_read(struct webclient_session *session, void *buffer, size_t size);
int webclient_write(struct webclient_session *session, const void *buffer, size_t size);

/* webclient GET/POST header buffer operate by the header fields */
int webclient_header_fields_add(struct webclient_session *session, const char *fmt, ...);
const char *webclient_header_fields_get(struct webclient_session *session, const char *fields);

/**
 * @brief get wenclient request response data.
 *
 * @param session wenclient session
 * @param response response buffer address
 * @param resp_len response buffer length
 *
 * @return response data size
 */
int webclient_response(struct webclient_session *session, void **response, size_t *resp_len);

/**
 * @brief send request(GET/POST) to server and get response data.
 *
 * @param URI input server address
 * @param header send header data
 *             = NULL: use default header data, must be GET request
 *            != NULL: user custom header data, GET or POST request
 * @param post_data data sent to the server
 *             = NULL: it is GET request
 *            != NULL: it is POST request
 * @param data_len send data length
 * @param response response buffer address
 * @param resp_len response buffer length
 * @param cert_buf the certificate of Server if URL uses HTTPS to connect
 * @param cert_len the length of certificate, include the terminal character '\0'
 *
 * @return <0: request failed
 *        >=0: response buffer size
 */
int webclient_request(const char *URI, const char *header, const void *post_data, size_t data_len,
                      void **response, size_t *resp_len, const char *cert_buf, size_t cert_len);
int webclient_request_header_add(char **request_header, const char *fmt, ...);
int webclient_resp_status_get(struct webclient_session *session);
int webclient_content_length_get(struct webclient_session *session);

#ifdef RT_USING_DFS
/* file related operations */
int webclient_get_file(const char *URI, const char *filename);
int webclient_post_file(const char *URI, const char *filename, const char *form_data);
#endif

#ifdef __cplusplus
}
#endif

#endif
