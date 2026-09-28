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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MBEDTLS_USER_CONFIG_FILE)
#include MBEDTLS_USER_CONFIG_FILE
#endif

#define TAG "tls_client"

#include "tls_certificate.h"
#include "tls_client.h"
#include "volc_mbedtls_compat.h"

#if VOLC_MBEDTLS_V4
#include "psa/crypto.h"
#endif

#include "volc_osal.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

#ifdef MBEDTLS_DEBUG_C
#define DEBUG_LEVEL (2)
#endif

static char error_buf[100];

static void _ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
  ((void)level);

  // LOGD("%s:%04d: %s", file, line, str);
}

static int mbedtls_ssl_certificate_verify(MbedTLSSession *session)
{
  int ret = 0;
  ret = mbedtls_ssl_get_verify_result(&session->ssl);
  if (ret != 0) {
    LOGE("verify peer certificate fail....");
    memset(session->buffer, 0x00, session->buffer_len);
    mbedtls_x509_crt_verify_info((char *)session->buffer, session->buffer_len, "  ! ", ret);
    LOGE("verification info: %s", session->buffer);
    return -1;
  }
  return 0;
}

int mbedtls_client_init(MbedTLSSession *session, void *entropy, size_t entropyLen)
{
  int ret = 0;

#if defined(MBEDTLS_DEBUG_C)
  LOGD("Set debug level (%d)", (int)DEBUG_LEVEL);
  mbedtls_debug_set_threshold((int)DEBUG_LEVEL);
#endif

  mbedtls_net_init(&session->server_fd);
  mbedtls_ssl_init(&session->ssl);
  mbedtls_ssl_config_init(&session->conf);
#if !VOLC_MBEDTLS_V4
  mbedtls_ctr_drbg_init(&session->ctr_drbg);
  mbedtls_entropy_init(&session->entropy);
#endif
  mbedtls_x509_crt_init(&session->cacert);

#if VOLC_MBEDTLS_V4
  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    LOGE("psa_crypto_init error, return %ld", (long)status);
    return (int)status;
  }
  (void)entropy;
  (void)entropyLen;
#else
  ret = mbedtls_ctr_drbg_seed(&session->ctr_drbg, mbedtls_entropy_func, &session->entropy,
                              (unsigned char *)entropy, entropyLen);
  if (ret != 0) {
    LOGE("mbedtls_ctr_drbg_seed error, return -0x%x\n", -ret);
    return ret;
  }
#endif
  LOGD("mbedtls client struct init success...");

  return 0;
}

int mbedtls_client_close(MbedTLSSession *session)
{
  if (session == NULL) {
    return -1;
  }

  mbedtls_ssl_close_notify(&session->ssl);
  mbedtls_net_free(&session->server_fd);
  mbedtls_x509_crt_free(&session->cacert);
#if !VOLC_MBEDTLS_V4
  mbedtls_entropy_free(&session->entropy);
  mbedtls_ctr_drbg_free(&session->ctr_drbg);
#endif
  mbedtls_ssl_config_free(&session->conf);
  mbedtls_ssl_free(&session->ssl);

  HAL_SAFE_FREE(session->buffer);
  HAL_SAFE_FREE(session->host);
  HAL_SAFE_FREE(session->port);
  HAL_SAFE_FREE(session);

  return 0;
}

int mbedtls_client_context(MbedTLSSession *session)
{
  int ret = 0;

  ret = mbedtls_x509_crt_parse(&session->cacert, (const unsigned char *)GLOBAL_ROOT_CERT, GLOBAL_ROOT_CERT_LEN);
  if (ret < 0) {
    LOGE("mbedtls_x509_crt_parse error,  return -0x%x", -ret);
    return ret;
  }

  LOGD("Loading the CA root certificate success...");

  /* Hostname set here should match CN in server certificate */
  if (session->host) {
    ret = mbedtls_ssl_set_hostname(&session->ssl, session->host);
    if (ret != 0) {
      LOGE("mbedtls_ssl_set_hostname error, return -0x%x", -ret);
      return ret;
    }
  }

  ret = mbedtls_ssl_config_defaults(&session->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) {
    LOGE("mbedtls_ssl_config_defaults error, return -0x%x", -ret);
    return ret;
  }

  mbedtls_ssl_conf_authmode(&session->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&session->conf, &session->cacert, NULL);
#if !VOLC_MBEDTLS_V4
  mbedtls_ssl_conf_rng(&session->conf, mbedtls_ctr_drbg_random, &session->ctr_drbg);
#endif

  mbedtls_ssl_conf_dbg(&session->conf, _ssl_debug, NULL);

  mbedtls_ssl_conf_read_timeout(&session->conf, 1000);

  ret = mbedtls_ssl_setup(&session->ssl, &session->conf);
  if (ret != 0) {
    LOGE("mbedtls_ssl_setup error, return -0x%x", -ret);
    return ret;
  }
  LOGD("mbedtls client context init success...");

  return 0;
}

int mbedtls_client_connect(MbedTLSSession *session)
{
  int ret = 0;

  ret = mbedtls_net_connect(&session->server_fd, session->host, session->port,
                            MBEDTLS_NET_PROTO_TCP);
  if (ret != 0) {
    LOGE("mbedtls_net_connect error, return -0x%x", -ret);
    return ret;
  }

  LOGD("Connected %s:%s success...", session->host, session->port);

  mbedtls_ssl_set_bio(&session->ssl, &session->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

  while ((ret = mbedtls_ssl_handshake(&session->ssl)) != 0) {
    if (0 != mbedtls_ssl_certificate_verify(session)) {
      return -1;
    }
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      LOGE("mbedtls_ssl_handshake error, return -0x%x", -ret);
      return ret;
    }
  }

  if (0 != mbedtls_ssl_certificate_verify(session)) {
    return -1;
  }

  LOGD("Certificate verified success...");

  return 0;
}

int mbedtls_client_read(MbedTLSSession *session, unsigned char *buf, size_t len)
{
  int ret = 0;

  if (!session || !buf || (len == 0)) {
    LOGW("invalid arguments: session(%p), buf(%p), len(%d)",
        session, buf, (int)len);
    return -1;
  }

  do {
    ret = mbedtls_ssl_read(&session->ssl, (unsigned char *)buf, len);
  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
  if (ret <= 0) {
    switch (ret) {
    case MBEDTLS_ERR_SSL_TIMEOUT:
      LOGE("timeout");
      break;
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
      LOGE("connection was close");
      break;
    default:
      mbedtls_strerror(ret, error_buf, 100);
      LOGE("mbedtls_ssl_read failed: %d, %s", ret, error_buf);
      break;
    }
  }

  return ret;
}

int mbedtls_client_write(MbedTLSSession *session, const unsigned char *buf, size_t len)
{
  int ret = 0;

  if (session == NULL || buf == NULL) {
    return -1;
  }

  ret = mbedtls_ssl_write(&session->ssl, (unsigned char *)buf, len);
  if (ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
    LOGE("mbedtls_client_write data error, return -0x%x", -ret);
  }

  return ret;
}
