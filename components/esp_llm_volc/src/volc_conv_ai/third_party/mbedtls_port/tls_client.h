/*
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

#ifndef MBEDTLS_CLIENT_H
#define MBEDTLS_CLIENT_H

#include "volc_mbedtls_compat.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#if !VOLC_MBEDTLS_V4
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif
#include "mbedtls/error.h"
#include "mbedtls/timing.h"
#include "mbedtls/platform.h"

typedef struct MbedTLSSession {
  char *host;
  char *port;

  unsigned char *buffer;
  size_t buffer_len;

  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
#if !VOLC_MBEDTLS_V4
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
#endif
  mbedtls_net_context server_fd;
  mbedtls_x509_crt cacert;
} MbedTLSSession;

extern int mbedtls_client_init(MbedTLSSession *session, void *entropy, size_t entropyLen);
extern int mbedtls_client_close(MbedTLSSession *session);
extern int mbedtls_client_context(MbedTLSSession *session);
extern int mbedtls_client_connect(MbedTLSSession *session);
extern int mbedtls_client_read(MbedTLSSession *session, unsigned char *buf, size_t len);
extern int mbedtls_client_write(MbedTLSSession *session, const unsigned char *buf, size_t len);

#endif
