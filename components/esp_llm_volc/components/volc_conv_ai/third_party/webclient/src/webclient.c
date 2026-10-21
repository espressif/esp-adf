/*
 * Copyright (c) 2006-2019, RT-Thread Development Team
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
 * 2021-06-09     xiangxistu   add shard download function
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include <webclient.h>

#include <sys/errno.h>
#include <sys/time.h>

#include <netdb.h>
#include <sys/socket.h>

#include "util/volc_list.h"
#include "util/volc_log.h"
#include "volc_osal.h"

#define TAG "webclient"

/* default receive or send timeout */
#define WEBCLIENT_DEFAULT_TIMEO 6

extern long int strtol(const char *nptr, char **endptr, int base);

static int webclient_strncasecmp(const char *a, const char *b, size_t n)
{
  uint8_t c1, c2;
  if (n <= 0)
    return 0;
  do {
    c1 = tolower(*a++);
    c2 = tolower(*b++);
  } while (--n && c1 && c1 == c2);
  return c1 - c2;
}

static const char *webclient_strstri(const char *str, const char *subStr)
{
  int len = strlen(subStr);

  if (len == 0) {
    return RT_NULL;
  }

  while (*str) {
    if (webclient_strncasecmp(str, subStr, len) == 0) {
      return str;
    }
    ++str;
  }
  return RT_NULL;
}

static int webclient_send(struct webclient_session *session, const void *buffer, size_t len, int flag)
{
#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (session->tls_session) {
    return mbedtls_client_write(session->tls_session, buffer, len);
  }
#endif

  return send(session->socket, buffer, len, flag);
}

static int webclient_recv(struct webclient_session *session, void *buffer, size_t len, int flag)
{
#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (session->tls_session) {
    return mbedtls_client_read(session->tls_session, buffer, len);
  }
#endif
  return recv(session->socket, buffer, len, flag);
}

static int webclient_read_line(struct webclient_session *session, char *buffer, int size)
{
  int rc, count = 0;
  char ch = 0, last_ch = 0;

  RT_ASSERT(session);
  RT_ASSERT(buffer);

  /* Keep reading until we fill the buffer. */
  while (count < size) {
    rc = webclient_recv(session, (unsigned char *)&ch, 1, 0);
#if defined(CONFIG_WEBCLIENT_HTTPS_SUPPORTED)
    if (session->is_tls && (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)) {
      continue;
    }
#endif
    if (rc <= 0)
      return rc;

    if (ch == '\n' && last_ch == '\r')
      break;

    buffer[count++] = ch;

    last_ch = ch;
  }

  if (count > size) {
    LOGE( "read line failed. The line data length is out of buffer size(%d)!", count);
    return -WEBCLIENT_ERROR;
  }

  return count;
}

/**
 * resolve server address
 *
 * @param session http session
 * @param res the server address information
 * @param url the input server URI address
 * @param request the pointer to point the request url, for example, /index.html
 *
 * @return 0 on resolve server address OK, others failed
 *
 * URL example:
 * http://www.rt-thread.org
 * http://www.rt-thread.org:80
 * https://www.rt-thread.org/
 * http://192.168.1.1:80/index.htm
 * http://[fe80::1]
 * http://[fe80::1]/
 * http://[fe80::1]/index.html
 * http://[fe80::1]:80/index.html
 */
static int webclient_resolve_address(struct webclient_session *session, struct addrinfo **res, const char *url,
                                     const char **request)
{
  int rc = WEBCLIENT_OK;
  char *ptr;
  char port_str[6] = "80"; /* default port of 80(http) */
  const char *port_ptr;
  const char *path_ptr;

  const char *host_addr = 0;
  int url_len, host_addr_len = 0;

  RT_ASSERT(res);
  RT_ASSERT(request);

  /* make *res == NULL before getaddrinfo */
  *res = RT_NULL;

  url_len = rt_strlen(url);

  /* strip protocol(http or https) */
  if (strncmp(url, "http://", 7) == 0) {
    host_addr = url + 7;
  } else if (strncmp(url, "https://", 8) == 0) {
    rt_strncpy(port_str, "443", 4);
    host_addr = url + 8;
  } else {
    rc = -WEBCLIENT_ERROR;
    goto __exit;
  }

  /* ipv6 address */
  if (host_addr[0] == '[') {
    host_addr += 1;
    ptr = rt_strstr(host_addr, "]");
    if (!ptr) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }
    host_addr_len = ptr - host_addr;
  }

  path_ptr = rt_strstr(host_addr, "/");
  *request = path_ptr ? path_ptr : "/";

  /* resolve port */
  port_ptr = rt_strstr(host_addr + host_addr_len, ":");
  if (port_ptr && (!path_ptr || (port_ptr < path_ptr))) {
    if (!path_ptr) {
      rt_strncpy(port_str, port_ptr + 1, url + url_len - (port_ptr + 1));
    } else {
      int port_len = path_ptr - port_ptr - 1;
      rt_strncpy(port_str, port_ptr + 1, port_len);
      port_str[port_len] = '\0';
    }
  } else {
    port_ptr = RT_NULL;
  }

  if (port_ptr && (!path_ptr)) {
    rt_strncpy(port_str, port_ptr + 1, url + url_len - (port_ptr + 1));
  }

  /* ipv4 or domain. */
  if (!host_addr_len) {
    if (port_ptr) {
      host_addr_len = port_ptr - host_addr;
    } else if (path_ptr) {
      host_addr_len = path_ptr - host_addr;
    } else {
      host_addr_len = strlen(host_addr);
    }
  }

  if ((host_addr_len < 1) || (host_addr_len > url_len)) {
    rc = -WEBCLIENT_ERROR;
    goto __exit;
  }

  /* get host address ok. */
  {
    char *host_addr_new = volc_osal_malloc(host_addr_len + 1);

    if (!host_addr_new) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }

    memcpy(host_addr_new, host_addr, host_addr_len);
    host_addr_new[host_addr_len] = '\0';
    session->host = host_addr_new;
  }

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (session->tls_session) {
    session->tls_session->port = web_strdup(port_str);
    session->tls_session->host = web_strdup(session->host);
    if (session->tls_session->port == RT_NULL || session->tls_session->host == RT_NULL) {
      return -WEBCLIENT_NOMEM;
    }

    return rc;
  }
#endif

  /* resolve the host name. */
  {
    struct addrinfo hint;
    int ret;

    rt_memset(&hint, 0, sizeof(hint));
    ret = getaddrinfo(session->host, port_str, &hint, res);
    if (ret != 0) {
      LOGE( "getaddrinfo err: %d '%s'.", ret, session->host);
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }
  }

__exit:
  if (rc != WEBCLIENT_OK) {
    HAL_SAFE_FREE(session->host);

    if (*res) {
      freeaddrinfo(*res);
      *res = RT_NULL;
    }
  }

  return rc;
}

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
static int webclient_open_tls(struct webclient_session *session, const char *URI)
{
  int tls_ret = 0;
  const char *pers = "webclient";

  RT_ASSERT(session);

  session->tls_session = (MbedTLSSession *)volc_osal_calloc(1, sizeof(MbedTLSSession));
  if (session->tls_session == RT_NULL) {
    return -WEBCLIENT_NOMEM;
  }

  session->tls_session->buffer_len = WEBCLIENT_RESPONSE_BUFSZ;
  session->tls_session->buffer = volc_osal_malloc(session->tls_session->buffer_len);
  if (session->tls_session->buffer == RT_NULL) {
    LOGE( "no memory for tls_session buffer!");
    return -WEBCLIENT_ERROR;
  }

  if ((tls_ret = mbedtls_client_init(session->tls_session, (void *)pers, strlen(pers))) < 0) {
    LOGE( "initialize https client failed return: -0x%x.", -tls_ret);
    return -WEBCLIENT_ERROR;
  }

  return WEBCLIENT_OK;
}
#endif

/**
 * connect to http server.
 *
 * @param session webclient session
 * @param URI the input server URI address
 *
 * @return <0: connect failed or other error
 *         =0: connect success
 */
static int webclient_connect(struct webclient_session *session, const char *URI)
{
  int rc = WEBCLIENT_OK;
  int socket_handle;
  struct timeval timeout;
  struct addrinfo *res = RT_NULL;
  const char *req_url;

  RT_ASSERT(session);
  RT_ASSERT(URI);
  LOGD( "uri: %s", URI);

  timeout.tv_sec = WEBCLIENT_DEFAULT_TIMEO;
  timeout.tv_usec = 0;

  if (strncmp(URI, "https://", 8) == 0) {
#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
    if (!session->cert_buf || (session->cert_len <= 1)) {
      LOGE( "invalid certificate");
      return -WEBCLIENT_ERROR;
    }
    if (webclient_open_tls(session, URI) < 0) {
      LOGE( "connect failed, https client open URI(%s) failed!", URI);
      return -WEBCLIENT_ERROR;
    }
    session->is_tls = true;
#else
    LOGE( "not support https connect, please enable webclient https configure!");
    rc = -WEBCLIENT_ERROR;
    goto __exit;
#endif
  }

  /* Check valid IP address and URL */
  rc = webclient_resolve_address(session, &res, URI, &req_url);
  if (rc != WEBCLIENT_OK) {
    LOGE( "connect failed, resolve address error(%d).", rc);
    goto __exit;
  }

  /* Not use 'getaddrinfo()' for https connection */
  if (session->is_tls == RT_FALSE && res == RT_NULL) {
    rc = -WEBCLIENT_ERROR;
    goto __exit;
  }

  /* copy host address */
  if (req_url) {
    session->req_url = web_strdup(req_url);
  } else {
    LOGE( "connect failed, resolve request address error.");
    rc = -WEBCLIENT_ERROR;
    goto __exit;
  }

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (session->tls_session) {
    int tls_ret = 0;

    if ((tls_ret = mbedtls_client_context(session->tls_session)) < 0) {
      LOGE( "connect failed, https client context return: -0x%x", -tls_ret);
      return -WEBCLIENT_ERROR;
    }

    if ((tls_ret = mbedtls_client_connect(session->tls_session)) < 0) {
      LOGE( "connect failed, https client connect return: -0x%x", -tls_ret);
      return -WEBCLIENT_CONNECT_FAILED;
    }

    socket_handle = session->tls_session->server_fd.fd;

    /* set recv timeout option */
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout));

    session->socket = socket_handle;

    return WEBCLIENT_OK;
  }
#endif

  {
    socket_handle = socket(res->ai_family, SOCK_STREAM, IPPROTO_TCP);

    if (socket_handle < 0) {
      LOGE( "connect failed, create socket(%d) error.", socket_handle);
      rc = -WEBCLIENT_NOSOCKET;
      goto __exit;
    }

    /* set receive and send timeout option */
    setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout));
    setsockopt(socket_handle, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout));
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
    int port = ntohs(ipv4->sin_port);
    if (connect(socket_handle, res->ai_addr, res->ai_addrlen) != 0) {
      /* connect failed, close socket */
      LOGE( "connect failed, connect socket(%d) error.", socket_handle);
       close(socket_handle);
      rc = -WEBCLIENT_CONNECT_FAILED;
      goto __exit;
    }
    session->port = ntohs(((struct sockaddr_in *)res->ai_addr)->sin_port);
    session->socket = socket_handle;
  }

__exit:
  if (res) {
    freeaddrinfo(res);
  }

  return rc;
}

/**
 * add fields data to request header data.
 *
 * @param session webclient session
 * @param fmt fields format
 *
 * @return >0: data length of successfully added
 *         <0: not enough header buffer size
 */
int webclient_header_fields_add(struct webclient_session *session, const char *fmt, ...)
{
  rt_int32_t length;
  va_list args;

  RT_ASSERT(session);
  RT_ASSERT(session->header->buffer);

  va_start(args, fmt);
  length = rt_vsnprintf(session->header->buffer + session->header->length,
                        session->header->size - session->header->length, fmt, args);
  va_end(args);
  if (length < 0) {
    LOGE( "add fields header data failed, return length(%d) error.", (int)length);
    return -WEBCLIENT_ERROR;
  }

  session->header->length += length;

  /* check header size */
  if (session->header->length >= session->header->size) {
    LOGE( "not enough header buffer size(%d)!", (int)session->header->size);
    return -WEBCLIENT_ERROR;
  }

  return length;
}

/**
 * get fields information from request/response header data.
 *
 * @param session webclient session
 * @param fields fields keyword
 *
 * @return = NULL: get fields data failed
 *        != NULL: success get fields data
 */
const char *webclient_header_fields_get(struct webclient_session *session, const char *fields)
{
  char *resp_buf = RT_NULL;
  size_t resp_buf_len = 0;

  RT_ASSERT(session);
  RT_ASSERT(session->header->buffer);

  resp_buf = session->header->buffer;
  while (resp_buf_len < session->header->length) {
    if (webclient_strstri(resp_buf, fields) == resp_buf) {
      char *mime_ptr = RT_NULL;

      /* jump space */
      mime_ptr = rt_strstr(resp_buf, ":");
      if (mime_ptr != NULL) {
        mime_ptr += 1;

        while (*mime_ptr && (*mime_ptr == ' ' || *mime_ptr == '\t'))
          mime_ptr++;

        return mime_ptr;
      }
    }

    if (*resp_buf == '\0')
      break;

    resp_buf += rt_strlen(resp_buf) + 1;
    resp_buf_len += rt_strlen(resp_buf) + 1;
  }

  return RT_NULL;
}

/**
 * get http response status code.
 *
 * @param session webclient session
 *
 * @return response status code
 */
int webclient_resp_status_get(struct webclient_session *session)
{
  RT_ASSERT(session);

  return session->resp_status;
}

/**
 * get http response data content length.
 *
 * @param session webclient session
 *
 * @return response content length
 */
int webclient_content_length_get(struct webclient_session *session)
{
  RT_ASSERT(session);

  return session->content_length;
}

static int webclient_send_header(struct webclient_session *session, int method)
{
  int rc = WEBCLIENT_OK;
  char *header = RT_NULL;

  RT_ASSERT(session);

  header = session->header->buffer;

  if (session->header->length == 0 && method <= WEBCLIENT_GET) {
    /* use default header data */
    if (webclient_header_fields_add(session, "GET %s HTTP/1.1\r\n", session->req_url) < 0) {
      return -WEBCLIENT_NOMEM;
    }
    if (webclient_header_fields_add(session, "Host: %s\r\n", session->host) < 0) {
      return -WEBCLIENT_NOMEM;
    }
    webclient_write(session, (unsigned char *)session->header->buffer, session->header->length);
  } else {
    if (method != WEBCLIENT_USER_METHOD) {
      /* check and add fields header data */
      if (memcmp(header, "HTTP/1.", rt_strlen("HTTP/1."))) {
        char *header_buffer = RT_NULL;
        int length = 0;

        header_buffer = web_strdup(session->header->buffer);
        if (header_buffer == RT_NULL) {
          LOGE( "no memory for header buffer!");
          rc = -WEBCLIENT_NOMEM;
          goto __exit;
        }

        /* splice http request header data */
        if (method == WEBCLIENT_GET)
          length = rt_snprintf(session->header->buffer, session->header->size, "GET %s HTTP/1.1\r\n%s",
                               session->req_url ? session->req_url : "/", header_buffer);
        else if (method == WEBCLIENT_POST)
          length = rt_snprintf(session->header->buffer, session->header->size, "POST %s HTTP/1.1\r\n%s",
                               session->req_url ? session->req_url : "/", header_buffer);
        else if (method == WEBCLIENT_PUT)
          length = rt_snprintf(session->header->buffer, session->header->size, "PUT %s HTTP/1.1\r\n%s",
                               session->req_url ? session->req_url : "/", header_buffer);
        else if (method == WEBCLIENT_HEAD)
          length = rt_snprintf(session->header->buffer, session->header->size, "HEAD %s HTTP/1.1\r\n%s",
                               session->req_url ? session->req_url : "/", header_buffer);
        session->header->length = length;

        HAL_SAFE_FREE(header_buffer);
      }

      if (strstr(header, "Host:") == RT_NULL) {
        if (webclient_header_fields_add(session, "Host: %s\r\n", session->host) < 0)
          return -WEBCLIENT_NOMEM;
      }

      if (strstr(header, "Accept:") == RT_NULL) {
        if (webclient_header_fields_add(session, "Accept: */*\r\n") < 0)
          return -WEBCLIENT_NOMEM;
      }

      /* header data end */
      rt_snprintf(session->header->buffer + session->header->length, session->header->size - session->header->length,
                  "\r\n");
      session->header->length += 2;

      /* check header size */
      if (session->header->length > session->header->size) {
        LOGE( "send header failed, not enough header buffer size(%d)!", (int)session->header->size);
        rc = -WEBCLIENT_NOBUFFER;
        goto __exit;
      }

      webclient_write(session, (unsigned char *)session->header->buffer, session->header->length);
    } else {
      webclient_write(session, (unsigned char *)session->header->buffer, session->header->length);
    }
  }

  /* get and echo request header data */
  {
    char *header_str, *header_ptr;
    int header_line_len;
    LOGD( "request header:");

    for (header_str = session->header->buffer; (header_ptr = rt_strstr(header_str, "\r\n")) != RT_NULL;) {
      header_line_len = header_ptr - header_str;

      if (header_line_len > 0) {
         LOGD( "%.*s", header_line_len, header_str);
      }
      header_str = header_ptr + rt_strlen("\r\n");
    }
  }

__exit:
  return rc;
}

/**
 * resolve server response data.
 *
 * @param session webclient session
 *
 * @return <0: resolve response data failed
 *         =0: success
 */
int webclient_handle_response(struct webclient_session *session)
{
  int rc = WEBCLIENT_OK;
  char *mime_buffer = RT_NULL;
  char *mime_ptr = RT_NULL;
  const char *transfer_encoding;
  int i;

  RT_ASSERT(session);

  /* clean header buffer and size */
  rt_memset(session->header->buffer, 0x00, session->header->size);
  session->header->length = 0;

   LOGD( "response header:");
  /* We now need to read the header information */
  while (1) {
    mime_buffer = session->header->buffer + session->header->length;

    /* read a line from the header information. */
    rc = webclient_read_line(session, mime_buffer, session->header->size - session->header->length);
    if (rc < 0)
      break;

    /* End of headers is a blank line.  exit. */
    if (rc == 0)
      break;
    if ((rc == 1) && (mime_buffer[0] == '\r')) {
      mime_buffer[0] = '\0';
      break;
    }

    /* set terminal character */
    mime_buffer[rc - 1] = '\0';

    /* echo response header data */
     LOGD( "%s", mime_buffer);

    session->header->length += rc;

    if (session->header->length >= session->header->size) {
      LOGE( "not enough header buffer size(%d)!", (int)session->header->size);
      return -WEBCLIENT_NOMEM;
    }
  }

  /* get HTTP status code */
  mime_ptr = web_strdup(session->header->buffer);
  if (mime_ptr == RT_NULL) {
    LOGE( "no memory for get http status code buffer!");
    return -WEBCLIENT_NOMEM;
  }

  if (rt_strstr(mime_ptr, "HTTP/1.")) {
    char *ptr = mime_ptr;

    ptr += rt_strlen("HTTP/1.x");

    while (*ptr && (*ptr == ' ' || *ptr == '\t'))
      ptr++;

    /* Terminate string after status code */
    for (i = 0; ((ptr[i] != ' ') && (ptr[i] != '\t')); i++)
      ;
    ptr[i] = '\0';

    session->resp_status = (int)strtol(ptr, RT_NULL, 10);
  }

  /* get content length */
  if (webclient_header_fields_get(session, "Content-Length") != RT_NULL) {
    session->content_length = atoi(webclient_header_fields_get(session, "Content-Length"));
  }
  session->content_remainder = session->content_length ? (size_t)session->content_length : 0xFFFFFFFF;

  transfer_encoding = webclient_header_fields_get(session, "Transfer-Encoding");
  if (transfer_encoding && rt_strcmp(transfer_encoding, "chunked") == 0) {
    char line[16];

    /* chunk mode, we should get the first chunk size */
    webclient_read_line(session, line, session->header->size);
    session->chunk_sz = strtol(line, RT_NULL, 16);
    session->chunk_offset = 0;
  }

  HAL_SAFE_FREE(mime_ptr);

  if (rc < 0) {
    return rc;
  }

  return session->resp_status;
}

struct webclient_session *webclient_session_create(size_t header_sz, const char *cert_buf, size_t cert_len)
{
  struct webclient_session *session = RT_NULL;

  /* create session */
  session = (struct webclient_session *)volc_osal_calloc(1, sizeof(struct webclient_session));
  assert(session);

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (!cert_buf || (cert_len <= 1)) {
    LOGE( "invalid certificate");
    HAL_SAFE_FREE(session);
    return RT_NULL;
  }

  LOGD( "Certificate Length: %d\n%s", (int)cert_len, cert_buf ? cert_buf : "NULL");
  session->cert_buf = (char *)volc_osal_calloc(cert_len, 1);
  assert(session->cert_buf);
  memcpy(session->cert_buf, cert_buf, cert_len);
  session->cert_len = cert_len;
#endif

  /* initialize the socket of session */
  session->socket = -1;
  session->content_length = -1;

  session->header = (struct webclient_header *)volc_osal_calloc(1, sizeof(struct webclient_header));
  assert(session->header);

  session->header->size = header_sz;
  session->header->buffer = (char *)volc_osal_calloc(1, header_sz);
  assert(session->header->buffer);

  return session;
}

static int webclient_clean(struct webclient_session *session);

int webclient_get(struct webclient_session *session, const char *URI)
{
  int rc = WEBCLIENT_OK;
  int resp_status = 0;

  RT_ASSERT(session);
  RT_ASSERT(URI);

  rc = webclient_connect(session, URI);
  if (rc != WEBCLIENT_OK) {
    /* connect to webclient server failed. */
    return rc;
  }

  rc = webclient_send_header(session, WEBCLIENT_GET);
  if (rc != WEBCLIENT_OK) {
    /* send header to webclient server failed. */
    return rc;
  }

  /* handle the response header of webclient server */
  resp_status = webclient_handle_response(session);

   LOGI( "get position handle response(%d).", resp_status);

  if (resp_status > 0) {
    const char *location = webclient_header_fields_get(session, "Location");

    /* relocation */
    if ((resp_status == 302 || resp_status == 301) && location) {
      char *new_url;

      new_url = web_strdup(location);
      if (new_url == RT_NULL) {
        return -WEBCLIENT_NOMEM;
      }

      /* clean webclient session */
      webclient_clean(session);
      /* clean webclient session header */
      session->header->length = 0;
      rt_memset(session->header->buffer, 0, session->header->size);

      rc = webclient_get(session, new_url);

      HAL_SAFE_FREE(new_url);
      return rc;
    }
  }

  return resp_status;
}

/**
 *  register a handle function for http breakpoint resume and shard download.
 *
 * @param function
 *
 * @return the pointer
 */
int *webclient_register_shard_position_function(struct webclient_session *session,
                                                int (*handle_function)(char *buffer, int size))
{
  session->handle_function = handle_function;

  return (int *)session->handle_function;
}

int webclient_shard_head_function(struct webclient_session *session, const char *URI, int *length)
{
  RT_ASSERT(session);
  RT_ASSERT(URI);

  int rc = WEBCLIENT_OK;
  int resp_status = 0;

  if (session->socket == -1) {
    rc = webclient_connect(session, URI);
    if (rc != WEBCLIENT_OK) {
      return rc;
    }
  }

  /* clean header buffer and size */
  rt_memset(session->header->buffer, 0x00, session->header->size);
  session->header->length = 0;

  rc = webclient_send_header(session, WEBCLIENT_HEAD);
  if (rc != WEBCLIENT_OK) {
    return rc;
  }

  /* handle the response header of webclient server by HEAD request */
  resp_status = webclient_handle_response(session);
  if (resp_status >= 0) {
    *length = webclient_content_length_get(session);
     LOGI( "The length[%04d] of real data of URI.", *length);
  }

  return rc;
}

int webclient_shard_position_function(struct webclient_session *session, const char *URI, int start, int length,
                                      int mem_size)
{
  int rc = WEBCLIENT_OK;
  int result = RT_EOK;
  int resp_status = 0;
  int resp_len = 0;
  char *buffer = RT_NULL;
  int start_position, end_position = 0;
  int total_len = 0;

  RT_ASSERT(session);
  RT_ASSERT(URI);
  RT_ASSERT(mem_size);

  /* set the offset of "Range" and "total_len"  */
  end_position = start;
  total_len = start + length;

  /* clean header buffer and size */
  rt_memset(session->header->buffer, 0x00, session->header->size);
  session->header->length = 0;

  for (start_position = end_position; total_len != end_position + 1;) {
    RT_ASSERT(start_position <= total_len);
    int data_len = 0;

    end_position = start_position + mem_size - 1;
    if (end_position >= total_len) {
      end_position = total_len - 1;
    }

    /* splice header and send header */
     LOGI( "Range: [%04d -> %04d]", start_position, end_position);
    webclient_header_fields_add(session, "Range: bytes=%d-%d\r\n", start_position, end_position);
    rc = webclient_send_header(session, WEBCLIENT_GET);
    if (rc != WEBCLIENT_OK) {
      return rc;
    }

    /* handle the response header of webclient server */
    resp_status = webclient_handle_response(session);
     LOGI( "get position handle response(%d).", resp_status);
    if (resp_status == 206) {
      /* normal resp_status */
    } else if (resp_status > 0) {
      const char *location = webclient_header_fields_get(session, "Location");

      /* relocation */
      if ((resp_status == 302 || resp_status == 301) && location) {
        char *new_url;

        new_url = web_strdup(location);
        if (new_url == RT_NULL) {
          return -WEBCLIENT_NOMEM;
        }

        /* clean webclient session */
        webclient_clean(session);
        /* clean webclient session header */
        session->header->length = 0;
        rt_memset(session->header->buffer, 0, session->header->size);

        rc = webclient_shard_position_function(session, new_url, start, length, mem_size);

        HAL_SAFE_FREE(new_url);
        return rc;
      }
    } else {
      if (resp_status == -WEBCLIENT_ERROR) {
        if (session->socket == -WEBCLIENT_ERROR) {
          /* clean webclient session */
          webclient_clean(session);
          if (webclient_connect(session, URI) == WEBCLIENT_OK) {
             LOGI( "webclient reconnect success, retry at [%06d]", end_position);
            continue;
          } else {
            LOGE( "webclient reconnect failed. Please retry by yourself.");
            return -WEBCLIENT_ERROR;
          }
        }
      }
    }

    /* receive the incoming data */
    data_len = webclient_response(session, (void **)&buffer, (size_t *)&resp_len);
    if (data_len > 0) {
      start_position += mem_size;
      result = session->handle_function(buffer, data_len);
      if (result != RT_EOK) {
        return -WEBCLIENT_ERROR;
      }
    } else {
      /* clean webclient session */
      webclient_clean(session);
      if (session->socket == -WEBCLIENT_ERROR) {
        webclient_connect(session, URI);
      }
    }

    /* clean header buffer and size */
    rt_memset(session->header->buffer, 0x00, session->header->size);
    session->header->length = 0;
  }

  return rc;
}

int webclient_put(struct webclient_session *session, const char *URI, const void *put_data, size_t data_len)
{
  int rc = WEBCLIENT_OK;
  int resp_status = 0;

  /* check arguments */
  RT_ASSERT(session);
  RT_ASSERT(URI);

  if ((put_data != RT_NULL) && (data_len == 0)) {
    LOGE( "input put data length failed");
    return -WEBCLIENT_ERROR;
  }

  rc = webclient_connect(session, URI);
  if (rc != WEBCLIENT_OK) {
    /* connect to webclient server failed. */
    return rc;
  }

  rc = webclient_send_header(session, WEBCLIENT_PUT);
  if (rc != WEBCLIENT_OK) {
    /* send header to webclient server failed. */
    return rc;
  }

  if (put_data && (data_len > 0)) {
    webclient_write(session, put_data, data_len);
  }

  /* resolve response data, get http status code */
  resp_status = webclient_handle_response(session);
  LOGD( "put handle response(%d).", resp_status);

  return resp_status;
}

int webclient_post(struct webclient_session *session, const char *URI, const void *post_data, size_t data_len)
{
  int rc = WEBCLIENT_OK;
  int resp_status = 0;

  /* check arguments */
  RT_ASSERT(session);
  RT_ASSERT(URI);

  if ((post_data != RT_NULL) && (data_len == 0)) {
    LOGE( "input post data length failed");
    return -WEBCLIENT_ERROR;
  }

  rc = webclient_connect(session, URI);
  if (rc != WEBCLIENT_OK) {
    /* connect to webclient server failed. */
    return rc;
  }

  rc = webclient_send_header(session, WEBCLIENT_POST);
  if (rc != WEBCLIENT_OK) {
    /* send header to webclient server failed. */
    return rc;
  }

  if (post_data && (data_len > 0)) {
    webclient_write(session, post_data, data_len);
  }

  /* resolve response data, get http status code */
  resp_status = webclient_handle_response(session);
  LOGD( "post handle response(%d).", resp_status);

  return resp_status;
}

#define RT_WAITING_FOREVER -1
#define RT_TICK_PER_SECOND 100
rt_uint32_t rt_tick_from_millisecond(rt_int32_t ms)
{
  rt_uint32_t tick;

  if (ms < 0) {
    tick = (rt_uint32_t)RT_WAITING_FOREVER;
  } else {
    tick = RT_TICK_PER_SECOND * (ms / 1000);
    tick += (RT_TICK_PER_SECOND * (ms % 1000) + 999) / 1000;
  }

  /* return the calculated tick */
  return tick;
}

/**
 * set receive and send data timeout.
 *
 * @param session http session
 * @param millisecond timeout millisecond
 *
 * @return 0: set timeout success
 */
int webclient_set_timeout(struct webclient_session *session, int millisecond)
{
  struct timeval timeout;
  int second = rt_tick_from_millisecond(millisecond) / 1000;

  RT_ASSERT(session);

  timeout.tv_sec = second;
  timeout.tv_usec = 0;

  /* set recv timeout option */
  setsockopt(session->socket, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout));
  setsockopt(session->socket, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout));

  return 0;
}

static int webclient_next_chunk(struct webclient_session *session)
{
  char line[64];
  int length;

  RT_ASSERT(session);

  rt_memset(line, 0x00, sizeof(line));
  length = webclient_read_line(session, line, sizeof(line));
  if (length > 0) {
    if (rt_strcmp(line, "\r") == 0) {
      length = webclient_read_line(session, line, sizeof(line));
      if (length <= 0) {
         close(session->socket);
        session->socket = -1;
        return length;
      }
    }
  } else {
     close(session->socket);
    session->socket = -1;

    return length;
  }

  session->chunk_sz = strtol(line, RT_NULL, 16);
  session->chunk_offset = 0;

  if (session->chunk_sz == 0) {
    /* end of chunks */
     close(session->socket);
    session->socket = -1;
    session->chunk_sz = -1;
  }

  return session->chunk_sz;
}

/**
 *  read data from http server.
 *
 * @param session http session
 * @param buffer read buffer
 * @param length the maximum of read buffer size
 *
 * @return <0: read data error
 *         =0: http server disconnect
 *         >0: successfully read data length
 */
int webclient_read(struct webclient_session *session, void *buffer, size_t length)
{
  int bytes_read = 0;
  int total_read = 0;
  int left;

  RT_ASSERT(session);

  /* get next chunk size is zero, client is already closed, return zero */
  LOGD( "chunk_sz: %d", session->chunk_sz);
  if (session->chunk_sz < 0) {
    return 0;
  }

  if (session->socket < 0) {
    return -WEBCLIENT_DISCONNECT;
  }

  if (length == 0) {
    return 0;
  }

  /* which is transferred as chunk mode */
  if (session->chunk_sz) {
    if ((int)length > (session->chunk_sz - session->chunk_offset)) {
      length = session->chunk_sz - session->chunk_offset;
    }

    bytes_read = webclient_recv(session, buffer, length, 0);
    LOGV( "recv bytes: %d", bytes_read);
    if (bytes_read <= 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        /* recv timeout */
        return -WEBCLIENT_TIMEOUT;
      } else {
         close(session->socket);
        session->socket = -1;
        return 0;
      }
    }

    session->chunk_offset += bytes_read;
    if (session->chunk_offset >= session->chunk_sz) {
      webclient_next_chunk(session);
    }

    return bytes_read;
  }

  if (session->content_length > 0) {
    if (length > session->content_remainder) {
      length = session->content_remainder;
    }

    if (length == 0) {
      return 0;
    }
  }

  /*
   * Read until: there is an error, we've read "size" bytes or the remote
   * side has closed the connection.
   */
  left = length;
  do {
    bytes_read = webclient_recv(session, (void *)((char *)buffer + total_read), left, 0);
    LOGV( "recv bytes: %d", bytes_read);

    if (session->is_tls) {
      /* finish reading */
      total_read = bytes_read;
      break;
    }

    /* read until error happen */
    if (bytes_read <= 0) {
      if (total_read > 0) {
        break;
      }

      /* can't read data from socket */
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
         LOGW( "receive data timeout.");
        return -WEBCLIENT_TIMEOUT;
      } else {
        return 0;
      }
    }

    left -= bytes_read;
    total_read += bytes_read;
  } while (left);

  if (session->content_length > 0) {
    session->content_remainder -= total_read;
  }

  return total_read;
}

/**
 *  write data to http server.
 *
 * @param session http session
 * @param buffer write buffer
 * @param length write buffer size
 *
 * @return <0: write data error
 *         =0: http server disconnect
 *         >0: successfully write data length
 */
int webclient_write(struct webclient_session *session, const void *buffer, size_t length)
{
  int bytes_write = 0;
  int total_write = 0;
  int left = length;

  RT_ASSERT(session);

  if (session->socket < 0) {
    return -WEBCLIENT_DISCONNECT;
  }

  if (length == 0) {
    return 0;
  }

  /* send all of data on the buffer. */
  do {
    bytes_write = webclient_send(session, (void *)((char *)buffer + total_write), left, 0);
    if (bytes_write <= 0) {
#if defined(CONFIG_WEBCLIENT_HTTPS_SUPPORTED)
      if (session->is_tls && (bytes_write == MBEDTLS_ERR_SSL_WANT_READ || bytes_write == MBEDTLS_ERR_SSL_WANT_WRITE)) {
        continue;
      }
#endif
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        /* send timeout */
        if (total_write) {
          return total_write;
        }
        continue;
        /* TODO: whether return the TIMEOUT
                 * return -WEBCLIENT_TIMEOUT; */
      } else {
         close(session->socket);
        session->socket = -1;

        if (total_write == 0) {
          return -WEBCLIENT_DISCONNECT;
        }
        break;
      }
    }

    left -= bytes_write;
    total_write += bytes_write;
  } while (left);

  return total_write;
}

/* close session socket, free host and request url */
static int webclient_clean(struct webclient_session *session)
{
#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  if (session->tls_session) {
    mbedtls_client_close(session->tls_session);
  } else {
    if (session->socket >= 0) {
       close(session->socket);
      session->socket = -1;
    }
  }
#else
  if (session->socket >= 0) {
     close(session->socket);
    session->socket = -1;
  }
#endif

  HAL_SAFE_FREE(session->host);
  HAL_SAFE_FREE(session->req_url);

  session->content_length = -1;

  return 0;
}

/**
 * close a webclient client session.
 *
 * @param session http client session
 *
 * @return 0: close success
 */
int webclient_close(struct webclient_session *session)
{
  RT_ASSERT(session);

  webclient_clean(session);

  if (session->header) {
    HAL_SAFE_FREE(session->header->buffer);
  }

  HAL_SAFE_FREE(session->header);

#ifdef CONFIG_WEBCLIENT_HTTPS_SUPPORTED
  HAL_SAFE_FREE(session->cert_buf);
#endif
  HAL_SAFE_FREE(session);

  return 0;
}

int webclient_response(struct webclient_session *session, void **response, size_t *resp_len)
{
  unsigned char *buf_ptr;
  unsigned char *response_buf = 0;
  int length, total_read = 0;

  RT_ASSERT(session);
  RT_ASSERT(response);

  /* initialize response */
  *response = RT_NULL;

  /* not content length field kind */
  if (session->content_length < 0) {
    size_t result_sz;

    total_read = 0;
    while (1) {
      unsigned char *new_resp = RT_NULL;

      result_sz = total_read + WEBCLIENT_RESPONSE_BUFSZ;
      new_resp = volc_osal_realloc(response_buf, result_sz + 1);
      if (new_resp == RT_NULL) {
        LOGE( "no memory for realloc new response buffer!");
        break;
      }

      response_buf = new_resp;
      buf_ptr = (unsigned char *)response_buf + total_read;

      /* read result */
      length = webclient_read(session, buf_ptr, result_sz - total_read);
      if (length <= 0)
        break;

      total_read += length;
    }
  } else {
    int result_sz;

    result_sz = session->content_length;
    response_buf = volc_osal_calloc(1, result_sz + 1);
    if (response_buf == RT_NULL) {
      return -WEBCLIENT_NOMEM;
    }

    buf_ptr = (unsigned char *)response_buf;
    for (total_read = 0; total_read < result_sz;) {
      length = webclient_read(session, buf_ptr, result_sz - total_read);
      if (length <= 0)
        break;

      buf_ptr += length;
      total_read += length;
    }
  }

  if ((total_read == 0)) {
    HAL_SAFE_FREE(response_buf);
  }

  if (response_buf) {
    *response = (void *)response_buf;
    *(response_buf + total_read) = '\0';
    *resp_len = total_read;
  }

  return total_read;
}

/**
 * add request(GET/POST) header data.
 *
 * @param request_header add request buffer address
 * @param fmt fields format
 *
 * @return <=0: add header failed
 *          >0: add header data size
 */

int webclient_request_header_add(char **request_header, const char *fmt, ...)
{
  rt_int32_t length, header_length;
  char *header;
  va_list args;

  RT_ASSERT(request_header);

  if (*request_header == RT_NULL) {
    header = volc_osal_calloc(1, WEBCLIENT_HEADER_BUFSZ);
    if (header == RT_NULL) {
      LOGE( "No memory for webclient request header add.");
      // return RT_NULL;
      return 0;
    }
    *request_header = header;
  } else {
    header = *request_header;
  }

  va_start(args, fmt);
  header_length = rt_strlen(header);
  length = rt_vsnprintf(header + header_length, WEBCLIENT_HEADER_BUFSZ - header_length, fmt, args);
  if (length < 0) {
    LOGE( "add request header data failed, return length(%d) error.", (int)length);
    return -WEBCLIENT_ERROR;
  }
  va_end(args);

  /* check header size */
  if (rt_strlen(header) >= WEBCLIENT_HEADER_BUFSZ) {
    LOGE( "not enough request header data size(%d)!", WEBCLIENT_HEADER_BUFSZ);
    return -WEBCLIENT_ERROR;
  }

  return length;
}

int webclient_request(const char *URI, const char *header, const void *post_data, size_t data_len, void **response,
                      size_t *resp_len, const char *cert_buf, size_t cert_len)
{
  struct webclient_session *session = RT_NULL;
  int rc = WEBCLIENT_OK;
  int totle_length = 0;

  RT_ASSERT(URI);

  if (post_data == RT_NULL && response == RT_NULL) {
    LOGE( "request get failed, get response data cannot be empty.");
    return -WEBCLIENT_ERROR;
  }

  if ((post_data != RT_NULL) && (data_len == 0)) {
    LOGE( "input post data length failed");
    return -WEBCLIENT_ERROR;
  }

  if ((response != RT_NULL && resp_len == RT_NULL) || (response == RT_NULL && resp_len != RT_NULL)) {
    LOGE( "input response data or length failed");
    return -WEBCLIENT_ERROR;
  }

  if (post_data == RT_NULL) {
    /* send get request */
    session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ, cert_buf, cert_len);
    if (session == RT_NULL) {
      rc = -WEBCLIENT_NOMEM;
      goto __exit;
    }

    if (header != RT_NULL) {
      char *header_str, *header_ptr;
      int header_line_length;

      for (header_str = (char *)header; (header_ptr = rt_strstr(header_str, "\r\n")) != RT_NULL;) {
        header_line_length = header_ptr + rt_strlen("\r\n") - header_str;
        webclient_header_fields_add(session, "%.*s", header_line_length, header_str);
        header_str += header_line_length;
      }
    }

    if (webclient_get(session, URI) != 200) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }

    totle_length = webclient_response(session, response, resp_len);
    if (totle_length <= 0) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }
  } else {
    /* send post request */
    session = webclient_session_create(WEBCLIENT_HEADER_BUFSZ, cert_buf, cert_len);
    if (session == RT_NULL) {
      rc = -WEBCLIENT_NOMEM;
      goto __exit;
    }

    if (header != RT_NULL) {
      char *header_str, *header_ptr;
      int header_line_length;

      for (header_str = (char *)header; (header_ptr = rt_strstr(header_str, "\r\n")) != RT_NULL;) {
        header_line_length = header_ptr + rt_strlen("\r\n") - header_str;
        webclient_header_fields_add(session, "%.*s", header_line_length, header_str);
        header_str += header_line_length;
      }
    }

    if (rt_strstr(session->header->buffer, "Content-Length") == RT_NULL) {
      webclient_header_fields_add(session, "Content-Length: %d\r\n", rt_strlen(post_data));
    }

    if (rt_strstr(session->header->buffer, "Content-Type") == RT_NULL) {
      webclient_header_fields_add(session, "Content-Type: application/octet-stream\r\n");
    }

    if (webclient_post(session, URI, post_data, data_len) != 200) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }

    totle_length = webclient_response(session, response, resp_len);
    if (totle_length <= 0) {
      rc = -WEBCLIENT_ERROR;
      goto __exit;
    }
  }

__exit:
  if (session) {
    webclient_close(session);
    session = RT_NULL;
  }

  if (rc < 0) {
    return rc;
  }

  return totle_length;
}
