// Copyright 2021-2022 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file may have been modified by Volcano and/or its affiliates.

#include "websocket.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/select.h>

#include "volc_osal.h"
#include "util/volc_list.h"
#include "util/volc_log.h"
#include "util/volc_base64.h"
#include "volc_mbedtls_compat.h"
#if VOLC_MBEDTLS_V4
#include "psa/crypto.h"
#else
#include "mbedtls/sha1.h"
#endif

#define WEBSOCKET_SSL_DEFAULT_PORT 443
#define WEBSOCKET_TCP_DEFAULT_PORT 80
#define WEBSOCKET_BUFFER_SIZE_BYTE (4 * 1024)
#define WS_BUFFER_SIZE             (1 * 1600)
#define MAX_WEBSOCKET_HEADER_SIZE  16

#define WS_SIZE64 127
#define WS_MASK   0x80
#define WS_SIZE16 126

#define WEBSOCKET_TASK_PRIORITY        (4)
#define WEBSOCKET_TASK_STACK           (4 * 1024)
#define WEBSOCKET_NETWORK_TIMEOUT_MS   (10 * 1000)
#define WEBSOCKET_PINGPONG_TIMEOUT_SEC (540)
#define WEBSOCKET_PING_INTERVAL_SEC    (3)
#define WEBSOCKET_RECONNECT_TIMEOUT_MS (5 * 1000)
#define WEBSOCKET_RX_RETRY_COUNT       (10)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static void volc_ws_client_dispatch_event(volc_ws_client_t* client, int32_t event, char* data, int data_len, int opcode)
{
    volc_ws_event_data_t event_data;
    event_data.data_ptr = data;
    event_data.data_len = data_len;
    event_data.fin = client->last_fin;
    event_data.op_code = opcode;
    event_data.payload_len = client->payload_len;
    event_data.payload_offset = client->payload_offset;
    LOGD("volc_ws_client_dispatch_event, fin: %d, opcode: %d, payload_len: %d, payload_offset: %d, data_len: %d", event_data.fin, event_data.op_code,
         event_data.payload_len, event_data.payload_offset, event_data.data_len);

    if (client->ws_event_handler)
        (client->ws_event_handler)(client->user_context, event, (void*) &event_data);
}

struct timeval* utils_ms_to_timeval(int timeout_ms, struct timeval* tv);

#define CO_BIT(pos) (1U << (pos))
const static int PING_SENT_BIT = CO_BIT(1);
// const static int TEXT_SENT_BIT = CO_BIT(2);
const static int CLOSE_SENT_BIT = CO_BIT(3);
int status_bits = PING_SENT_BIT;

static char* trimwhitespace(const char* str);
static char* get_http_header(const char* buffer, const char* key);
static int ws_tcp_close(volc_ws_client_t* client);
static int ws_tcp_poll_read(volc_ws_client_t* client, int timeout_ms);
static int ws_tcp_poll_write(volc_ws_client_t* client, int timeout_ms);
static int ws_tcp_read(volc_ws_client_t* client, char* buffer, int len, int timeout_ms);
static int ws_tcp_write(volc_ws_client_t* client, const char* buffer, int len, int timeout_ms);
static int ws_read_payload(volc_ws_client_t* client, char* buffer, int len, int timeout_ms);
static int ws_read_header(volc_ws_client_t* client, char* buffer, int len, int timeout_ms);
static int ws_write(volc_ws_client_t* client, int opcode, int mask_flag, const char* b, int len, int timeout_ms);
static int ws_read(volc_ws_client_t* client, char* buffer, int len, int timeout_ms);
static int ws_poll_connection_closed(int* sockfd, int timeout_ms);
static int ws_client_recv(volc_ws_client_t* client);
static int set_socket_non_blocking(int fd, bool non_blocking);
static int hostname_to_fd(const char* host, size_t hostlen, int port, struct sockaddr_storage* address, int* fd);
static int _tcp_connect(int* sockfd, const char* host, int hostlen, int port, int timeout_ms);
static int ws_tcp_connect(volc_ws_client_t* client, const char* host, int port, int timeout_ms);
static int ws_disconnect(volc_ws_client_t* client);
static int ws_connect(volc_ws_client_t* client, const char* host, int port, int timeout_ms);
static int volc_ws_client_destory_config(volc_ws_client_t* client);

static char* trimwhitespace(const char* str)
{
    char* end;

    // Trim leading space
    while (isspace((unsigned char) *str))
        str++;

    if (*str == 0) {
        return (char*) str;
    }

    // Trim trailing space
    end = (char*) (str + strlen(str) - 1);
    while (end > str && isspace((unsigned char) *end))
        end--;

    // Write new null terminator
    *(end + 1) = 0;

    return (char*) str;
}

static char* get_http_header(const char* buffer, const char* key)
{
    char* found = strcasestr(buffer, key);
    if (found) {
        found += strlen(key);
        char* found_end = strstr(found, "\r\n");
        if (found_end) {
            found_end[0] = 0;

            return trimwhitespace(found);
        }
    }
    return NULL;
}

static int _tcp_close(volc_ws_client_t* client)
{
    int ret = -1;
    if (client->sockfd >= 0) {
        LOGE("%s, sockfd used, closing", __func__);
        ret = close(client->sockfd);
        client->sockfd = -1;
    }
    return ret;
}

static int _tcp_poll_read(int* sockfd, int timeout_ms)
{
    int ret = -1;
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET(*sockfd, &readset);
    FD_SET(*sockfd, &errset);

    ret = select((*sockfd) + 1, &readset, NULL, &errset, utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(*sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        LOGE("poll_read select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), *sockfd);
        ret = -1;
    }
    return ret;
}

static int _tcp_poll_write(int* sockfd, int timeout_ms)
{
    int ret = -1;
    struct timeval timeout;
    fd_set writeset;
    fd_set errset;
    FD_ZERO(&writeset);
    FD_ZERO(&errset);
    FD_SET(*sockfd, &writeset);
    FD_SET(*sockfd, &errset);

    ret = select((*sockfd) + 1, NULL, &writeset, &errset, utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0 && FD_ISSET(*sockfd, &errset)) {
        int sock_errno = 0;
        uint32_t optlen = sizeof(sock_errno);
        getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
        LOGE("poll_write select error %d, errno = %s, fd = %d\r\n", sock_errno, strerror(sock_errno), *sockfd);
        ret = -1;
    }
    return ret;
}

static int _tcp_read(int* sockfd, char* buffer, int len, int timeout_ms)
{
    int poll;

    if ((poll = _tcp_poll_read(sockfd, timeout_ms)) <= 0) {
        return poll;
    }

    int ret = recv(*sockfd, (unsigned char*) buffer, len, 0);
    if (ret < 0) {
        LOGE("tcp_read error, errno=%s", strerror(errno));
    }
    if (ret == 0) {
        if (poll > 0) {
            // no error
            LOGE("%s select > 0, ret = 0 \r\n", __func__);
        }
        ret = -1;
    }
    return ret;
}

static int _tcp_write(int* sockfd, const char* buffer, int len, int timeout_ms)
{
    int poll;
    if ((poll = _tcp_poll_write(sockfd, timeout_ms)) <= 0) {
        LOGE("Poll timeout or error, errno=%s, fd=%d, timeout_ms=%d\r\n", strerror(errno), *sockfd, timeout_ms);
        return poll;
    }

    int ret = send((*sockfd), (const unsigned char*) buffer, len, 0);
    if (ret < 0) {
        LOGE("tcp_write error, errno=%s", strerror(errno));
    }
    return ret;
}

static int ws_read_payload(volc_ws_client_t* client, char* buffer, int len, int timeout_ms)
{
    int bytes_to_read;
    int rlen = 0;
    transport_ws_t* ws = client->ws_transport;
    if (ws->frame_state.bytes_remaining > len) {
        LOGE("Actual data to receive (%d) are longer than ws buffer (%d)\r\n", ws->frame_state.bytes_remaining, len);
        bytes_to_read = len;

    } else {
        bytes_to_read = ws->frame_state.bytes_remaining;
    }

    if (bytes_to_read != 0 && (rlen = ws_tcp_read(client, buffer, bytes_to_read, timeout_ms)) <= 0) {
        LOGE("Error read payload data\r\n");
        return rlen;
    }
    ws->frame_state.bytes_remaining -= rlen;

    // if (ws->frame_state.mask_key) {
        for (int i = 0; i < bytes_to_read; i++) {
            buffer[i] = (buffer[i] ^ ws->frame_state.mask_key[i % 4]);
        }
    // }
    return rlen;
}

static int _tcp_read_completely(volc_ws_client_t* client, char* buffer, int len, int timeout_ms) {
    int rlen = 0;
    while (rlen < len) {
        int ret = ws_tcp_read(client, buffer + rlen, len - rlen, timeout_ms);
        if (ret < 0) {
            return ret;
        }
        rlen += ret;
    }
    return rlen;
}

/* Read and parse the WS header, determine length of payload */
static int ws_read_header(volc_ws_client_t* client, char* buffer, int len, int timeout_ms)
{
    int payload_len;
    transport_ws_t* ws = client->ws_transport;
    char ws_header[MAX_WEBSOCKET_HEADER_SIZE];
    char *data_ptr = ws_header, mask;
    int rlen;
    int poll_read;
    ws->frame_state.header_received = false;
    if ((poll_read = ws_tcp_poll_read(client, timeout_ms)) <= 0) {
        LOGE("error poll read data\r\n");
        return poll_read;
    }

    int header = 2;
    int mask_len = 4;
    if ((rlen = _tcp_read_completely(client, data_ptr, header, timeout_ms)) <= 0) {
        LOGE("first header, Error read data\r\n");
        return rlen;
    }
    ws->frame_state.header_received = true;
    ws->frame_state.fin = (*data_ptr & 0x80) != 0;
    ws->frame_state.opcode = (*data_ptr & 0x0F);
    data_ptr++;
    mask = ((*data_ptr >> 7) & 0x01);
    payload_len = (*data_ptr & 0x7F);
    data_ptr++;
    LOGD("%s, Opcode: %d, mask: %d, fin: %d, payload len: %d", __func__, ws->frame_state.opcode, mask, (int) ws->frame_state.fin, payload_len);
    if (payload_len == WS_SIZE16) {
        if ((rlen = _tcp_read_completely(client, data_ptr, header, timeout_ms)) <= 0) {
            LOGE("126 read: Error read data\r\n");
            return rlen;
        }
        payload_len = (uint8_t) data_ptr[0] << 8 | (uint8_t) data_ptr[1];
    } else if (payload_len == WS_SIZE64) {
        header = 8;
        if ((rlen = _tcp_read_completely(client, data_ptr, header, timeout_ms)) <= 0) {
            LOGE("127 read: Error read data\r\n");
            return rlen;
        }

        if (data_ptr[0] != 0 || data_ptr[1] != 0 || data_ptr[2] != 0 || data_ptr[3] != 0) {
            // really too big!
            payload_len = 0xFFFFFFFF;
            LOGW("127, payload_len:%d", payload_len);
        } else {
            payload_len = (uint8_t) data_ptr[4] << 24 | (uint8_t) data_ptr[5] << 16 | (uint8_t) data_ptr[6] << 8 | (uint8_t) data_ptr[7];
            LOGW("127, payload_len:%d, data_ptr[4]:%d, data_ptr[5]:%d, data_ptr[6]:%d, data_ptr[7]:%d", payload_len, data_ptr[4], data_ptr[5],
                 data_ptr[6], data_ptr[7]);
        }
    }
    LOGD("ws_read_header, payload_len:%d", payload_len);
    if (mask) {
        LOGD("mask: %d, payload_len: %d", mask, payload_len);
        // Read and store mask
        if (payload_len != 0 && (rlen = _tcp_read_completely(client, buffer, mask_len, timeout_ms)) <= 0) {
            LOGE("mask error read data\r\n");
            return rlen;
        }
        memcpy(ws->frame_state.mask_key, buffer, mask_len);
    } else {
        memset(ws->frame_state.mask_key, 0, mask_len);
    }

    ws->frame_state.payload_len = payload_len;
    ws->frame_state.bytes_remaining = payload_len;

    return payload_len;
}

static int ws_write(volc_ws_client_t* client, int opcode, int mask_flag, const char* b, int len, int timeout_ms)
{
    char* buffer = (char*) b;
    char ws_header[MAX_WEBSOCKET_HEADER_SIZE];
    char* mask;
    int header_len = 0, i;
    int poll_write;

    if ((poll_write = ws_tcp_poll_write(client, timeout_ms)) <= 0) {
        LOGE("Error ws_tcp_poll_write\r\n");
        return poll_write;
    }

    ws_header[header_len++] = opcode;
    if (len <= 125) {
        ws_header[header_len++] = (uint8_t) (len | mask_flag);
    } else if (len < 65536) {
        ws_header[header_len++] = WS_SIZE16 | mask_flag;
        ws_header[header_len++] = (uint8_t) (len >> 8);
        ws_header[header_len++] = (uint8_t) (len & 0xFF);
    } else {
        ws_header[header_len++] = WS_SIZE64 | mask_flag;
        /* Support maximum 4 bytes length */
        ws_header[header_len++] = 0; //(uint8_t)((len >> 56) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 48) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 40) & 0xFF);
        ws_header[header_len++] = 0; //(uint8_t)((len >> 32) & 0xFF);
        ws_header[header_len++] = (uint8_t) ((len >> 24) & 0xFF);
        ws_header[header_len++] = (uint8_t) ((len >> 16) & 0xFF);
        ws_header[header_len++] = (uint8_t) ((len >> 8) & 0xFF);
        ws_header[header_len++] = (uint8_t) ((len >> 0) & 0xFF);
    }

    if (mask_flag) {
        mask = &ws_header[header_len];
        volc_osal_fill_random((uint8_t *)ws_header + header_len, 4);
        header_len += 4;

        for (i = 0; i < len; ++i) {
            buffer[i] = (buffer[i] ^ mask[i % 4]);
        }
    }
    LOGD("%s, ws header len:%d\r\n", __func__, header_len);
    int err = ws_tcp_write(client, ws_header, header_len, timeout_ms);
    if (err != header_len) {
        LOGE("Error write header :%d err:%d errno:%d\r\n", header_len, err, errno);
        return -1;
    }

    if (len == 0) {
        return 0;
    }

    LOGD("%s, payload buffer len:%d\r\n", __func__, len);
    int ret = ws_tcp_write(client, buffer, len, timeout_ms);

    if (mask_flag) {
        mask = &ws_header[header_len - 4];
        for (i = 0; i < len; ++i) {
            buffer[i] = (buffer[i] ^ mask[i % 4]);
        }
    }
    return ret;
    ;
}

static int ws_read(volc_ws_client_t* client, char* buffer, int len, int timeout_ms)
{
    int rlen = 0;
    transport_ws_t* ws = client->ws_transport;
    if (ws->frame_state.bytes_remaining <= 0) {
        if ((rlen = ws_read_header(client, buffer, len, timeout_ms)) < 0) {
            LOGE("Error reading header %d, %s\r\n", rlen, buffer);
            ws->frame_state.bytes_remaining = 0;
            return rlen;
        }

        if (rlen == 0) {
            ws->frame_state.bytes_remaining = 0;
            return 0;
        }
    }

    if (ws->frame_state.payload_len) {
        if ((rlen = ws_read_payload(client, buffer, len, timeout_ms)) <= 0) {
            LOGE("Error reading payload data\r\n");
            ws->frame_state.bytes_remaining = 0;
            return rlen;
        }
        LOGD("%s, payload len:%d\r\n", __func__, rlen);
    }

    return rlen;
}

struct timeval* utils_ms_to_timeval(int timeout_ms, struct timeval* tv)
{
    if (timeout_ms == -1) {
        return NULL;
    }
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms - (tv->tv_sec * 1000)) * 1000;
    return tv;
}

static void ms_to_timeval(int timeout_ms, struct timeval* tv)
{
    tv->tv_sec = timeout_ms / 1000;
    tv->tv_usec = (timeout_ms % 1000) * 1000;
}

static int set_socket_non_blocking(int fd, bool non_blocking)
{
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
        LOGE("[sock=%d] get file flags error: %s", fd, strerror(errno));
        return -1;
    }

    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(fd, F_SETFL, flags) < 0) {
        LOGE("[sock=%d] set blocking/nonblocking error: %s", fd, strerror(errno));
        return -1;
    }
    LOGD("[sock=%d] non_block:%d\r\n", fd, non_blocking);
    return 0;
}

static int hostname_to_fd(const char* host, size_t hostlen, int port, struct sockaddr_storage* address, int* fd)
{
    struct addrinfo* address_info;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    LOGD("host:%s: strlen %lu\r\n", host, (unsigned long) hostlen);
    int res = getaddrinfo(host, NULL, &hints, &address_info);
    if (res != 0 || address_info == NULL) {
        LOGE("couldn't get hostname for :%s: "
             "getaddrinfo() returns %d, addrinfo=%p",
             host, res, address_info);
        return -1;
    }
    *fd = socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (*fd < 0) {
        LOGE("Failed to create socket (family %d socktype %d protocol %d)", address_info->ai_family, address_info->ai_socktype,
             address_info->ai_protocol);
        freeaddrinfo(address_info);
        return -1;
    }

    if (address_info->ai_family == AF_INET) {
        struct sockaddr_in* p = (struct sockaddr_in*) address_info->ai_addr;
        p->sin_port = htons(port);
        // LOGE("[sock=%d] Resolved IPv4 address: %s\r\n", *fd, ipaddr_ntoa((const ip_addr_t*)&p->sin_addr.s_addr));
        memcpy(address, p, sizeof(struct sockaddr));
    }
#if LWIP_IPV6
    else if (address_info->ai_family == AF_INET6) {
        struct sockaddr_in6* p = (struct sockaddr_in6*) address_info->ai_addr;
        p->sin6_port = htons(port);
        p->sin6_family = AF_INET6;
        LOGE("[sock=%d] Resolved IPv6 address: %s", *fd, ip6addr_ntoa((const ip6_addr_t*) &p->sin6_addr));
        memcpy(address, p, sizeof(struct sockaddr_in6));
    }
#endif
    else {
        LOGE("Unsupported protocol family %d", address_info->ai_family);
        close(*fd);
        freeaddrinfo(address_info);
        return -1;
    }

    freeaddrinfo(address_info);
    return 0;
}

static int _tcp_connect(int* sockfd, const char* host, int hostlen, int port, int timeout_ms)
{
    struct sockaddr_storage address;
    int fd;
    int ret = hostname_to_fd(host, hostlen, port, &address, &fd);
    if (ret != 0) {
        LOGE("%s error fd\r\n", __func__);
        return ret;
    }

    if (timeout_ms) {
        struct timeval tv;
        ms_to_timeval(timeout_ms, &tv);
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            LOGE("Fail to setsockopt SO_RCVTIMEO");
            return -1;
        }
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            LOGE("Fail to setsockopt SO_SNDTIMEO");
            return -1;
        }
    }

    // Set to non block before connecting to better control connection timeout
    ret = set_socket_non_blocking(fd, true);
    if (ret != 0) {
        LOGE("%s error nb", __func__);
        goto err;
    }

    if (connect(fd, (struct sockaddr*) &address, sizeof(struct sockaddr)) < 0) {
        if (errno == EINPROGRESS) {
            fd_set fdset;
            struct timeval tv = {.tv_usec = 0, .tv_sec = 10}; // Default connection timeout is 10 s

            if (timeout_ms > 0) {
                ms_to_timeval(timeout_ms, &tv);
            }
            FD_ZERO(&fdset);
            FD_SET(fd, &fdset);

            int res = select(fd + 1, NULL, &fdset, NULL, &tv);
            if (res < 0) {
                LOGE("[sock=%d] select() error: %s\r\n", fd, strerror(errno));
                ret = -1;
                goto err;
            } else if (res == 0) {
                LOGE("[sock=%d] select() timeout\r\n", fd);
                ret = -1;
                goto err;
            } else {
                int sockerr;
                socklen_t len = (socklen_t) sizeof(int);
                LOGD("[sock=%d] select() occur", fd);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*) (&sockerr), &len) < 0) {
                    LOGE("[sock=%d] getsockopt() error: %s\r\n", fd, strerror(errno));
                    ret = -1;
                    goto err;
                } else if (sockerr) {
                    LOGE("[sock=%d] delayed connect error: %s\r\n", fd, strerror(sockerr));
                    ret = -1;
                    goto err;
                } else if (!sockerr) {
                    LOGD("[sock=%d] no blocking connected", fd);
                }
            }
        } else {
            LOGE("[sock=%d] connect() error: %s\r\n", fd, strerror(errno));
            ret = -1;
            goto err;
        }
    }
    // reset back to blocking mode (unless non_block configured)
    ret = set_socket_non_blocking(fd, false);
    if (ret != 0) {
        LOGE("%s error nb\r\n", __func__);
        goto err;
    }
    *sockfd = fd;
    return 0;

err:
    close(fd);
    return ret;
}

static int _ssl_init(volc_ws_client_t* client) {
    int tls_ret = 0;
    const char* pers = "websocket";
    if (NULL == client) {
        LOGE("invalid input args");
        return -1;
    }
    client->ssl = volc_osal_calloc(1, sizeof(MbedTLSSession));
    if (NULL == client->ssl) {
        LOGE("malloc ssl failed");
        goto _websocket_init_fail;
    }
    client->ssl->buffer_len = WEBSOCKET_BUFFER_SIZE_BYTE;
    client->ssl->buffer = volc_osal_malloc(client->ssl->buffer_len);
    if (NULL == client->ssl->buffer) {
        LOGE("malloc ssl buffer fail");
        goto _websocket_init_fail;
    }
    if ((tls_ret = mbedtls_client_init(client->ssl, (void*) pers, strlen(pers))) < 0) {
        LOGE("initialize https client failed return: -0x%x.", -tls_ret);
        goto _websocket_init_fail;
    }
    client->ssl->host = strdup(client->host);
    char port[8] = {0};
    if (NULL == client->ssl->host) {
        LOGE("malloc host memory failed");
        goto _websocket_init_fail;
    }
    snprintf(port, sizeof(port), "%d", client->port);
    client->ssl->port = strdup(port);
    LOGD("websocket set port:%d init ssl %p", client->port, client->ssl);
    return 0;
_websocket_init_fail:
    mbedtls_client_close(client->ssl);
    return -1;
}

static int ws_tcp_connect(volc_ws_client_t* client, const char* host, int port, int timeout_ms)
{
    int err = 0;
#if defined(CONFIG_WEBSOCKET_TLS)
    if (client->is_tls == 1) {
        int tls_ret = 0;
        if ((tls_ret = _ssl_init(client)) < 0) {
            LOGE("ssl init failed");
            return -1;
        }
        if ((tls_ret = mbedtls_client_context(client->ssl)) < 0) {
            LOGE("mbedtls_client_connect failed return: -0x%x.\r\n", -tls_ret);
            return -1;
        }
        if ((tls_ret = mbedtls_client_connect(client->ssl)) < 0) {
            LOGE("mbedtls_client_connect failed return: -0x%x.\r\n", -tls_ret);
            return -1;
        }
        client->sockfd = client->ssl->server_fd.fd;
    } else
#endif
        err = _tcp_connect(&client->sockfd, host, strlen(host), port, timeout_ms);
    if (err != 0) {
        LOGE("%s failed\r\n", __func__);
        return -1;
    }

    return 0;
}

static int _ssl_write(volc_ws_client_t* client, const char* buffer, int len) {
    size_t written = 0;
	size_t write_len = len;
	while (written < len) {
		if (write_len > MBEDTLS_SSL_OUT_CONTENT_LEN) {
			write_len = MBEDTLS_SSL_OUT_CONTENT_LEN;
		}
		if (len > MBEDTLS_SSL_OUT_CONTENT_LEN) {
			LOGE("Fragmenting data of excessive size :%d, offset: %d, size %d\r\n", len, written, write_len);
		}
		ssize_t ret = mbedtls_ssl_write(&client->ssl->ssl, (unsigned char*) buffer + written, write_len);
		if (ret < 0) {
			if (ret != MBEDTLS_ERR_SSL_WANT_READ  && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != 0) {
				LOGE("write error :-0x%04X:\r\n", -ret);
				return ret;
			}
		}
		else
			LOGD("mbedtls_ssl_write, ret:%d\r\n", ret);
		written += ret;
		write_len = len - written;
	}
    return written;
}

static int ws_tcp_write(volc_ws_client_t* client, const char* buffer, int len, int timeout_ms)
{
    int err = 0;
#if defined(CONFIG_WEBSOCKET_TLS)
    if (client->is_tls == 1)
        // err = mbedtls_client_write(client->ssl, (const unsigned char*)buffer, len);
        err = _ssl_write(client, buffer, len);
    else
#endif
        err = _tcp_write(&client->sockfd, buffer, len, timeout_ms);
    return err;
}

static int ws_tcp_read(volc_ws_client_t* client, char* buffer, int len, int timeout_ms)
{
    int err = 0;
#if defined(CONFIG_WEBSOCKET_TLS)
    if (client->is_tls == 1)
        err = mbedtls_client_read(client->ssl, (unsigned char*)buffer, len);
    else
#endif
        err = _tcp_read(&client->sockfd, buffer, len, timeout_ms);
    return err;
}

static int ws_tcp_poll_read(volc_ws_client_t* client, int timeout_ms)
{
    int err = 0;
    err = _tcp_poll_read(&client->sockfd, timeout_ms);
    return err;
}

static int ws_tcp_poll_write(volc_ws_client_t* client, int timeout_ms)
{
    int err = 0;
    err = _tcp_poll_write(&client->sockfd, timeout_ms);
    return err;
}

static int ws_tcp_close(volc_ws_client_t* client)
{
    int err = 0;
#if defined(CONFIG_WEBSOCKET_TLS)
    if (client->is_tls == 1) {
        err = mbedtls_client_close(client->ssl);
        client->ssl = NULL;
    } else
#endif
        err = _tcp_close(client);
    if (err != 0) {
        LOGE("%s failed", __func__);
        return -1;
    }

    return 0;
}

static int ws_disconnect(volc_ws_client_t* client)
{
    if (client == NULL) {
        LOGE("client already null\r\n");
        return -1;
    }
    ws_tcp_close(client);
    if (client->auto_reconnect) {
        client->reconnect_tick_ms = volc_osal_get_time_ms();
    }
    client->state = VOLC_WS_STATE_WAIT_TIMEOUT;
    volc_ws_client_dispatch_event(client, VOLC_WS_EVENT_DISCONNECTED, NULL, 0, -1);
    return 0;
}

static int ws_connect(volc_ws_client_t* client, const char* host, int port, int timeout_ms)
{
    transport_ws_t* ws = client->ws_transport;

    if (ws_tcp_connect(client, host, port, timeout_ms) < 0) {
        return -1;
    }
    unsigned char random_key[16];
    volc_osal_fill_random(random_key, sizeof(random_key));

    unsigned char client_key[28] = {0};

    const char* user_agent_ptr = (ws->user_agent) ? (ws->user_agent) : "Volc Websocket Client";

    size_t outlen = 0;

    volc_base64_encode(client_key, sizeof(client_key), &outlen, random_key, sizeof(random_key));

    int len = snprintf(ws->buffer, WS_BUFFER_SIZE,
                       "GET %s HTTP/1.1\r\n"
                       "Pragma: no-cache\r\n"
                       "Cache-Control: no-cache\r\n"
                       "Host: %s\r\n"
                       "Origin: http://%s\r\n"
                       "User-Agent: %s\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Key: %s\r\n"
                       "Sec-WebSocket-Version: 13\r\n",
                       ws->path, host, host, user_agent_ptr, client_key);
    LOGD("http request: %s", ws->buffer);

    if (len <= 0 || len >= WS_BUFFER_SIZE) {
        LOGE("Error in request generation, desired request len: %d, buffer size: %d\r\n", len, WS_BUFFER_SIZE);
        return -1;
    }

    if (ws->sub_protocol) {
        LOGE("sub_protocol: %s\r\n", ws->sub_protocol);
        int r = snprintf(ws->buffer + len, WS_BUFFER_SIZE - len, "Sec-WebSocket-Protocol: %s\r\n", ws->sub_protocol);
        len += r;
        if (r <= 0 || len >= WS_BUFFER_SIZE) {
            LOGE("Error in request generation"
                 "(snprintf of subprotocol returned %d, desired request len: %d, buffer size: %d\r\n",
                 r, len, WS_BUFFER_SIZE);
            return -1;
        }
    }
    if (ws->headers) {
        int r = snprintf(ws->buffer + len, WS_BUFFER_SIZE - len, "%s", ws->headers);
        len += r;
        if (r <= 0 || len >= WS_BUFFER_SIZE) {
            LOGE("Error in request generation"
                 "(strncpy of headers returned %d, desired request len: %d, buffer size: %d\r\n",
                 r, len, WS_BUFFER_SIZE);
            return -1;
        }
    }
    int r = snprintf(ws->buffer + len, WS_BUFFER_SIZE - len, "\r\n");
    len += r;
    if (r <= 0 || len >= WS_BUFFER_SIZE) {
        LOGE("Error in request generation"
             "(snprintf of header terminal returned %d, desired request len: %d, buffer size: %d\r\n",
             r, len, WS_BUFFER_SIZE);
        return -1;
    }
    LOGD("ws->buffer:\r\n%s", ws->buffer);
    if (ws_tcp_write(client, ws->buffer, len, timeout_ms) < 0) {
        LOGE("Write FAIL");
    }

    int header_len = 0;
    do {
        if ((len = ws_tcp_read(client, ws->buffer + header_len, WS_BUFFER_SIZE - header_len, timeout_ms)) <= 0) {
            LOGE("read FAIL for upgrade header:%s\r\n", ws->buffer);
            return -1;
        }
        header_len += len;
        ws->buffer[header_len] = '\0';
        LOGD("Read header chunk %d, current header size: %d, header: %s\r\n", len, header_len, ws->buffer);
    } while (NULL == strstr(ws->buffer, "\r\n\r\n") && header_len < WS_BUFFER_SIZE);

    char* server_key = get_http_header(ws->buffer, "Sec-WebSocket-Accept:");
    if (server_key == NULL) {
        LOGE("Sec-WebSocket-Accept not found\r\n");
        return -1;
    }

    unsigned char expected_server_sha1[20];
    unsigned char expected_server_key[33] = {0};
    // If you are interested, see https://tools.ietf.org/html/rfc6455
    const char expected_server_magic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned char expected_server_text[sizeof(client_key) + sizeof(expected_server_magic) + 1];
    strncpy((char*) expected_server_text, (char*) client_key, sizeof(client_key));
    strncat((char*) expected_server_text, expected_server_magic, sizeof(expected_server_text) - sizeof(client_key) - 1);

    size_t key_len = strlen((char*) expected_server_text);
#if VOLC_MBEDTLS_V4
    size_t hash_len = 0;
    psa_status_t psa_ret = psa_crypto_init();
    if (psa_ret != PSA_SUCCESS) {
        LOGE("psa_crypto_init failed, status=%ld", (long)psa_ret);
        return -1;
    }
    psa_ret = psa_hash_compute(PSA_ALG_SHA_1, expected_server_text, key_len,
                               expected_server_sha1, sizeof(expected_server_sha1), &hash_len);
    int ret = (psa_ret == PSA_SUCCESS) ? 0 : (int)psa_ret;
#else
#if 0
    int ret = mbedtls_sha1_ret(expected_server_text, key_len, expected_server_sha1);
#else
    int ret = mbedtls_sha1(expected_server_text, key_len, expected_server_sha1);
#endif
#endif
    if (ret != 0) {
        LOGE("Error in calculating sha1 sum , Returned 0x%02X\r\n", ret);
        return ret;
    }
    volc_base64_encode(expected_server_key, sizeof(expected_server_key), &outlen, expected_server_sha1, sizeof(expected_server_sha1));
    expected_server_key[(outlen < sizeof(expected_server_key)) ? outlen : (sizeof(expected_server_key) - 1)] = 0;
    LOGD("server key=%s\r\n\r\n", (char*) server_key);
    LOGD("send_key=%s\r\n\r\n", (char*) client_key);
    LOGD("expected_server_key=%s\r\n\r\n", expected_server_key);
    if (strcmp((char*) expected_server_key, (char*) server_key) != 0) {
        LOGE("Invalid websocket key\r\n");
        return -1;
    }
    return 0;
}

int ws_poll_connection_closed(int* sockfd, int timeout_ms)
{
    struct timeval timeout;
    fd_set readset;
    fd_set errset;
    FD_ZERO(&readset);
    FD_ZERO(&errset);
    FD_SET(*sockfd, &readset);
    FD_SET(*sockfd, &errset);

    int ret = select(*sockfd + 1, &readset, NULL, &errset, utils_ms_to_timeval(timeout_ms, &timeout));
    if (ret > 0) {
        if (FD_ISSET(*sockfd, &readset)) {
            uint8_t buffer;
            if (recv(*sockfd, &buffer, 1, MSG_PEEK) <= 0) {
                // socket is readable, but reads zero bytes -- connection cleanly closed by FIN flag
                return 0;
            }
            LOGW("ws_poll_connection_closed: unexpected data readable on socket=%d", *sockfd);
        } else if (FD_ISSET(*sockfd, &errset)) {
            int sock_errno = 0;
            uint32_t optlen = sizeof(sock_errno);
            getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &sock_errno, &optlen);
            LOGD("ws_poll_connection_closed select error %d, errno = %s, fd = %d", sock_errno, strerror(sock_errno), *sockfd);
            if (sock_errno == ENOTCONN || sock_errno == ECONNRESET || sock_errno == ECONNABORTED) {
                return 0;
            }
            LOGE("ws_poll_connection_closed: unexpected errno=%d on socket=%d", sock_errno, *sockfd);
        }
        return -1;
    }
    return ret;
}

static int ws_client_recv(volc_ws_client_t* client)
{
    int rlen;
    client->payload_offset = 0;
    transport_ws_t* ws = client->ws_transport;
    do {
        LOGD("----------begin receive--------------\r\n");
        memset(client->rx_buffer, 0, client->buffer_size);
        rlen = ws_read(client, client->rx_buffer, client->buffer_size, WEBSOCKET_NETWORK_TIMEOUT_MS);
        if (rlen < 0) {
            LOGE("Error read data\r\n");
            return -1;
        }

        client->payload_len = ws->frame_state.payload_len;
        client->last_fin = ws->frame_state.fin;
        client->last_opcode = (volc_ws_opcode_e) ws->frame_state.opcode;

        if (rlen == 0 && client->last_opcode == VOLC_WS_OPCODES_NONE) {
            LOGE("ws read timeouts\r\n");
            return 0;
        }
        volc_ws_client_dispatch_event(client, VOLC_WS_EVENT_DATA, client->rx_buffer, rlen, client->last_opcode);
        client->payload_offset += rlen;
    } while (client->payload_offset < client->payload_len);

    if (client->last_opcode == VOLC_WS_OPCODES_PING) {
        const char* data = (client->payload_len == 0) ? NULL : client->rx_buffer;
        LOGD("Received ping, Sending PONG with payload len=%d\r\n", client->payload_len);
        ws_write(client, VOLC_WS_OPCODES_PONG | VOLC_WS_OPCODES_FIN, WS_MASK, data, client->payload_len, WEBSOCKET_NETWORK_TIMEOUT_MS);
    } else if (client->last_opcode == VOLC_WS_OPCODES_PONG) {
        client->wait_for_pong_resp = false;
    } else if (client->last_opcode == VOLC_WS_OPCODES_CLOSE) {
        LOGI("Received close frame\r\n");
        client->state = VOLC_WS_STATE_CLOSING;
    } else if (client->last_opcode == VOLC_WS_OPCODES_TEXT) {
        LOGD("Received text frame: \r\n");
    }

    return 0;
}

static int volc_ws_client_destory_config(volc_ws_client_t* client)
{
    if (client == NULL) {
        LOGE("%s, client already null, return", __func__);
        return -1;
    }

    HAL_SAFE_FREE(client->host);
    HAL_SAFE_FREE(client->path);
    HAL_SAFE_FREE(client->scheme);

#if defined(CONFIG_WEBSOCKET_TLS)
    client->is_tls = 0;
#endif
    return 0;
}

bool volc_ws_client_is_connected(volc_ws_client_t* client)
{
    if (client == NULL) {
        return false;
    }
    return client->state == VOLC_WS_STATE_CONNECTED;
}

static int volc_ws_client_send_with_opcode(volc_ws_client_t* client, volc_ws_opcode_e opcode, const uint8_t* data, int len, int timeout)
{
    int need_write = len;
    int wlen = 0, widx = 0;
    int ret = -1;
    LOGD("%s: begin send opcode: %d\r\n", __func__, opcode);
    if (client == NULL || len < 0 || (opcode != VOLC_WS_OPCODES_CLOSE && (data == NULL || len <= 0))) {
        LOGE("Invalid arguments\r\n");
        return -1;
    }

    if (!volc_ws_client_is_connected(client)) {
        LOGE("Websocket client is not connected\r\n");
        return -1;
    }

    volc_osal_mutex_lock(client->mutex);

    uint32_t current_opcode = opcode;
    while (widx < len || current_opcode) {
        if (need_write > client->buffer_size) {
            need_write = client->buffer_size;
        } else {
            current_opcode |= VOLC_WS_OPCODES_FIN;
        }
        memcpy(client->tx_buffer, data + widx, need_write);

        wlen = ws_write(client, current_opcode, WS_MASK, (char*) client->tx_buffer, need_write, timeout);
        if (wlen < 0 || (wlen == 0 && need_write != 0)) {
            ret = wlen;
            LOGE("Network error: ws_write() returned %d, errno=%d\r\n", ret, errno);
            if (errno == EAGAIN)
                LOGE("Network error: ws_write() EAGAIN, drop\r\n");
            goto unlock_and_return;
        }
        current_opcode = 0;
        widx += wlen;
        need_write = len - widx;
    }
    ret = widx;

unlock_and_return:
    if (client->mutex)
        volc_osal_mutex_unlock(client->mutex);
    else
        LOGE("mutex already deinit\r\n");
    return ret;
}

int volc_ws_client_send_text(volc_ws_client_t* client, const char* data, int len, int timeout)
{
    return volc_ws_client_send_with_opcode(client, VOLC_WS_OPCODES_TEXT, (const uint8_t*) data, len, timeout);
}

int volc_ws_client_send_binary(volc_ws_client_t* client, const char* data, int len, int timeout)
{
    return volc_ws_client_send_with_opcode(client, VOLC_WS_OPCODES_BINARY, (const uint8_t*) data, len, timeout);
}

int volc_ws_client_send_close(volc_ws_client_t* client, const char* data, int len, int timeout)
{
    if (volc_ws_client_send_with_opcode(client, VOLC_WS_OPCODES_CLOSE, (const uint8_t*) data, len, timeout) < 0) {
        LOGE("Error send close frame\r\n");
        return -1;
    }
    status_bits = 0;
    status_bits |= CLOSE_SENT_BIT;
    return 0;
}

static int __parse_url(volc_ws_client_t* client, char* url) {
	bool is_ipv6 = false;
	char* pos = url;
	char* url_end = NULL;
	char* filed_end = NULL;
	int len = 0;
	if (NULL == client || NULL == url) {
		LOGE("invalid input args");
		return -1;
	}
	url_end = url + strlen(url);
	LOGD("url: %s", url);

	// 1. parse scheme
	filed_end = strstr(pos, "://");
	if (filed_end) {
		len = filed_end - pos;
		client->scheme = (char *)volc_osal_malloc(len + 1);
		if (NULL == client->scheme) {
			LOGE("malloc scheme memory failed");
			return -1;
		}
		strncpy(client->scheme, pos, len);
		client->scheme[len] = 0;
		pos = filed_end+ 3;
	} else {
		client->scheme = strdup("http");
		if (NULL == client->scheme) {
			LOGE("malloc scheme memory failed");
			return -1;
		}
	}

	// 2. parse host
	filed_end = pos;
    while (filed_end < url_end) {
        if (*filed_end == '/' || *filed_end == '?' ||
            *filed_end == '#' || *filed_end == ':') {
            break;
        }
        filed_end++;
    }

    if (*pos == '[') {
        is_ipv6 = true;
        pos++;  // 跳过 '['
        filed_end = strchr(pos, ']');
        if (!filed_end) {
            // IPv6 地址未闭合
            return -1;
        }
    }

	len = filed_end - pos;
	client->host = (char *)volc_osal_malloc(len + 1);
	if (NULL == client->host) {
		LOGE("malloc host memory failed");
		return -1;
	}
	strncpy(client->host, pos, len);
	client->host[len] = 0;
	pos = filed_end;

	// 3. parse port
    if (*pos == ':') {
        pos++;  // 跳过 ':'
        const char *port_end = pos;
        while (port_end < url_end && isdigit((int)*port_end)) port_end++;

        if (port_end == pos) {
            // 端口号缺失
            return -1;
        }

        char port_str[16];
        size_t port_len = port_end - pos;
        if (port_len >= sizeof(port_str)) return -1;

        strncpy(port_str, pos, port_len);
        port_str[port_len] = '\0';
        client->port = atoi(port_str);
        pos = (char *)port_end;
        LOGE("port: %d", (int)client->port);
    }

	// 4. parse path
	if (*pos == '/') {
		len = url_end - pos;
		client->path = (char *)volc_osal_malloc(len + 1);
		if (NULL == client->path) {
			LOGE("malloc path memory failed");
			return -1;
		}
		strncpy(client->path, pos, len);
		client->path[len] = 0;
	} else {
		client->path = strdup("/");
		if (NULL == client->path) {
			LOGE("malloc path memory failed");
			return -1;
		}
	}

	LOGD("scheme: %s, host: %s, port: %d, path: %s", client->scheme, client->host, (int)client->port, client->path);
	return 0;
}

volc_ws_client_t* volc_ws_client_init(const volc_ws_config_t* input)
{
    int tls_ret = 0;
    const char* pers = "websocket";
    volc_ws_client_t* client = (volc_ws_client_t*) volc_osal_malloc(sizeof(volc_ws_client_t));
    memset(client, 0, sizeof(volc_ws_client_t));
    // parse websocket uri to websocket config
    if (input->uri) {
        if (__parse_url(client, (char *)input->uri) != 0) {
            LOGE("set uri fail, client destroy\r\n");
            goto _websocket_init_fail;
        }
    }
    if ((strncmp(input->uri, "ws://", 5) == 0 || strncmp(input->uri, "http://", 7) == 0) && client->port == 0) {
        client->port = WEBSOCKET_TCP_DEFAULT_PORT;
        LOGI("websocket set default port");
    } else if ((strncmp(input->uri, "wss://", 6) == 0 || strncmp(input->uri, "https://", 8) == 0)) {
#if defined(CONFIG_WEBSOCKET_TLS)
        if (client->port == 0) {
            client->port = WEBSOCKET_SSL_DEFAULT_PORT;
        }
        client->is_tls = 1;
        LOGD("websocket set port:%d", client->port);
#else
        LOGE("websocket not open tls");
        goto _websocket_init_fail;
#endif
    }

    client->user_context = input->user_context;

    // set event callback
    client->ws_event_handler = input->ws_event_handler;

    // set autoreconnect
    client->auto_reconnect = true;

    // init lock
    client->mutex = volc_osal_mutex_create();

    // set ws_transport
    client->ws_transport = (transport_ws_t*) volc_osal_malloc(sizeof(transport_ws_t));
    memset(client->ws_transport, 0, sizeof(transport_ws_t));
    if (!client->ws_transport) {
        LOGE("alloc ws_transport fail\r\n");
        goto _websocket_init_fail;
    }

    if (client->path) {
        HAL_SAFE_FREE(client->ws_transport->path);
        client->ws_transport->path = strdup(client->path);
    } else {
        HAL_SAFE_FREE(client->ws_transport->path);
        client->ws_transport->path = strdup("/");
    }
    client->ws_transport->buffer = volc_osal_malloc(WS_BUFFER_SIZE);
    if (!client->ws_transport->buffer) {
        LOGE("alloc ws_transport buffer fail\r\n");
        goto _websocket_init_fail;
    }

    if (input->subprotocol) {
        HAL_SAFE_FREE(client->ws_transport->sub_protocol);
        client->ws_transport->sub_protocol = strdup(input->subprotocol);
    }
    if (input->user_agent) {
        HAL_SAFE_FREE(client->ws_transport->user_agent);
        client->ws_transport->user_agent = strdup(input->user_agent);
    }
    if (input->headers) {
        HAL_SAFE_FREE(client->ws_transport->headers);
        client->ws_transport->headers = strdup(input->headers);
    }
    client->ws_transport->frame_state.bytes_remaining = 0;

    // tick...
    client->reconnect_tick_ms = volc_osal_get_time_ms();
    client->ping_tick_ms = volc_osal_get_time_ms();
    client->wait_for_pong_resp = false;

    // rx retry
    if (input->rx_retry <= 0)
        client->rx_retry = WEBSOCKET_RX_RETRY_COUNT;
    else
        client->rx_retry = input->rx_retry;

    // buf malloc
    int buffer_size = input->buffer_size;
    if (buffer_size <= 0) {
        buffer_size = WEBSOCKET_BUFFER_SIZE_BYTE;
    }
    client->buffer_size = buffer_size;
    if (NULL == (client->rx_buffer = (char*) volc_osal_malloc(buffer_size))) {
        LOGE("alloc rx_buffer fail\r\n");
        goto _websocket_init_fail;
    }

    if (NULL == (client->tx_buffer = (char*) volc_osal_malloc(buffer_size))) {
        LOGE("alloc tx_buffer fail\r\n");
        goto _websocket_init_fail;
    }

    // sockfd
    client->sockfd = -1;

    return client;

_websocket_init_fail:
#if defined(CONFIG_WEBSOCKET_TLS)
    if (client->ssl) {
        mbedtls_client_close(client->ssl);
        client->ssl = NULL;
    }
#endif
    volc_ws_client_destroy(client);
    return NULL;
}

int volc_ws_client_destroy(volc_ws_client_t* client)
{
    if (client == NULL) {
        return -1;
    }
    if (client->run) {
        if (client->state >= VOLC_WS_STATE_CONNECTED) {
            if (volc_ws_client_send_close(client, NULL, 0, WEBSOCKET_NETWORK_TIMEOUT_MS)) {
                LOGE("client send close frame fail");
                return -1;
            }
        }

        if (volc_ws_client_stop(client)) {
            LOGE("client stop fail");
            return -1;
        }
    }

    return 0;
}

static void free_client(volc_ws_client_t* client)
{
    if (client == NULL)
        return;

    volc_osal_mutex_destroy(client->mutex);
    client->mutex = NULL;
    client->ws_event_handler = NULL;

    HAL_SAFE_FREE(client->tx_buffer);
    HAL_SAFE_FREE(client->rx_buffer);

    if (client->ws_transport) {
        HAL_SAFE_FREE(client->ws_transport->buffer);
        HAL_SAFE_FREE(client->ws_transport->path);
        HAL_SAFE_FREE(client->ws_transport->sub_protocol);
        HAL_SAFE_FREE(client->ws_transport->headers);
        HAL_SAFE_FREE(client->ws_transport->user_agent);
        HAL_SAFE_FREE(client->ws_transport);
    }
    HAL_SAFE_FREE(client);
}

#if defined(PLATFORM_MACOS)
void* volc_ws_client_task(void* thread_param)
#else
void volc_ws_client_task(void* thread_param)
#endif
{
    volc_ws_client_t* client = (volc_ws_client_t*) thread_param;

    client->run = true;
    client->state = VOLC_WS_STATE_INIT;

    status_bits = PING_SENT_BIT;
    int read_select = 0;
    while (client->run) {
        switch ((int) client->state) {
            case VOLC_WS_STATE_INIT:
                LOGI("websocket connecting to %s://%s:%d", client->scheme, client->host, client->port);
                if (ws_connect(client, client->host, client->port, WEBSOCKET_NETWORK_TIMEOUT_MS) < 0) {
                    LOGE("Error websocket connect");
                    ws_disconnect(client);
                    break;
                }
                LOGI("websocket connected to %s://%s:%d", client->scheme, client->host, client->port);
                client->state = VOLC_WS_STATE_CONNECTED;
                client->wait_for_pong_resp = false;
                volc_ws_client_dispatch_event(client, VOLC_WS_EVENT_CONNECTED, NULL, 0, -1);
                break;
            case VOLC_WS_STATE_CONNECTED:
                LOGD("%s, status:%02x %llu %llu\r\n", __func__, status_bits, volc_osal_get_time_ms(), client->ping_tick_ms);
                if (volc_osal_get_time_ms() - client->ping_tick_ms > WEBSOCKET_PING_INTERVAL_SEC * 1000) {
                    client->ping_tick_ms = volc_osal_get_time_ms();

                    if (status_bits & PING_SENT_BIT) {
                        volc_osal_mutex_lock(client->mutex);
                        ws_write(client, VOLC_WS_OPCODES_PING | VOLC_WS_OPCODES_FIN, WS_MASK, NULL, 0, WEBSOCKET_NETWORK_TIMEOUT_MS);
                        volc_osal_mutex_unlock(client->mutex);
                    }
                    if (!client->wait_for_pong_resp) {
                        client->pingpong_tick_ms = volc_osal_get_time_ms();
                        client->wait_for_pong_resp = true;
                    }
                }
                if (volc_osal_get_time_ms() - client->pingpong_tick_ms > WEBSOCKET_PINGPONG_TIMEOUT_SEC * 1000) {
                    if (client->wait_for_pong_resp) {
                        LOGW("Error, no PONG received for more than %" PRIu64 " seconds after PING\r\n", client->pingpong_tick_ms);
                        break;
                    }
                }

                if (read_select == 0) {
                    LOGD("Read poll timeout: skipping read()...");
                    break;
                }
                // client->ping_tick_ms = volc_osal_get_time_ms();
                volc_osal_mutex_lock(client->mutex);
                if (ws_client_recv(client) == -1) {
                    LOGE("Error receive data");
                    ws_disconnect(client);
                    volc_osal_mutex_unlock(client->mutex);
                    break;
                }
                volc_osal_mutex_unlock(client->mutex);
                break;

            case VOLC_WS_STATE_WAIT_TIMEOUT:
                if (!client->auto_reconnect) {
                    client->run = false;
                    break;
                }
                if (volc_osal_get_time_ms() - client->reconnect_tick_ms > WEBSOCKET_RECONNECT_TIMEOUT_MS) {
                    client->state = VOLC_WS_STATE_INIT;
                    client->reconnect_tick_ms = volc_osal_get_time_ms();
                    LOGE("Reconnecting...");
                }
                break;
            case VOLC_WS_STATE_CLOSING:
                LOGE("Closing initiated by the server, sending close frame");
                ws_write(client, VOLC_WS_OPCODES_CLOSE | VOLC_WS_OPCODES_FIN, WS_MASK, NULL, 0, WEBSOCKET_NETWORK_TIMEOUT_MS);
                break;
            default:
                LOGE("Client run iteration in a default state: %d", client->state);
                break;
        }

        if (VOLC_WS_STATE_CONNECTED == client->state) {
            read_select = ws_tcp_poll_read(client, 1000); // Poll every 1000ms
            if (read_select < 0) {
                LOGE("Network error: ws_tcp_poll_read() returned %d, errno=%d", read_select, errno);
                ws_disconnect(client);
            }
        } else if (VOLC_WS_STATE_WAIT_TIMEOUT == client->state) {
            if (client->auto_reconnect)
                volc_osal_thread_sleep(WEBSOCKET_RECONNECT_TIMEOUT_MS);
        } else if (VOLC_WS_STATE_CLOSING == client->state) {
            LOGW(" Waiting for TCP connection to be closed by the server");
            int ret = ws_poll_connection_closed(&(client->sockfd), 1000);
            if (ret == 0) {
                // still waiting
                break;
            }
            if (ret < 0) {
                LOGE("Connection terminated while waiting for clean TCP close");
            }
            client->run = false;
            client->state = VOLC_WS_STATE_UNKNOW;
            volc_ws_client_dispatch_event(client, VOLC_WS_EVENT_CLOSED, NULL, 0, -1);
            break;
        }
    }
    LOGW("close connection...");
    volc_osal_mutex_lock(client->mutex);
    ws_tcp_close(client);
    volc_osal_mutex_unlock(client->mutex);
    client->state = VOLC_WS_STATE_UNKNOW;

    if (volc_ws_client_destory_config(client)) {
        LOGE("client config already volc_osal_free");
    }
    if (client->tid) {
        volc_osal_thread_destroy(client->tid);
    }
    client->exit = true;
    volc_osal_thread_exit(NULL);
#if defined(PLATFORM_MACOS)
    return NULL;
#else
    return;
#endif
}

int volc_ws_client_start(volc_ws_client_t* client)
{
    int ret;

    if (client == NULL) {
        LOGE("The client has not be initialized");
        return -1;
    }
    if (client->state >= VOLC_WS_STATE_INIT) {
        LOGE("The client has started");
        return -1;
    }
    volc_osal_thread_param_t param = {0};
    snprintf(param.name, sizeof(param.name), "%s", "websocket");
    param.stack_size = WEBSOCKET_TASK_STACK;
    param.priority = WEBSOCKET_TASK_PRIORITY;
    ret = volc_osal_thread_create(&client->tid, &param, volc_ws_client_task, (void*) client);
    if (ret != 0) {
        LOGE("create volc_ws_client_task fail");
        return -1;
    }

    return ret;
}

int volc_ws_client_stop(volc_ws_client_t* client)
{
    if (client == NULL) {
        LOGW("Client null");
        return -1;
    }
    if (!client->run) {
        LOGW("Client was not started");
        return -1;
    }
    client->run = false;
    client->state = VOLC_WS_STATE_UNKNOW;
    while(!client->exit) {
        LOGI("wait client exit...");
        volc_osal_thread_sleep(10);
    }
    free_client(client);
    client = NULL;
    return 0;
}
