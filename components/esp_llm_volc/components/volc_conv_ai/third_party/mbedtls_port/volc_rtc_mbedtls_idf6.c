/*
 * Copyright (2025) Beijing Volcano Engine Technology Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * mbedtls 4.x replacements for volc_tls_* in libVolcEngineRTCLite.a
 * (prebuilt against mbedtls 3.x struct layouts).
 */

#include "volc_mbedtls_compat.h"

#if VOLC_MBEDTLS_V4

#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include <stddef.h>

#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include "psa/crypto.h"

#include "tls_certificate.h"
#include "volc_osal.h"

typedef struct volc_tls_ctx {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
} volc_tls_ctx_t;

static psa_status_t volc_psa_init_once(void)
{
    psa_status_t status = psa_crypto_init();
    if (status == PSA_ERROR_BAD_STATE) {
        return PSA_SUCCESS;
    }
    return status;
}

static int volc_tls_configure(volc_tls_ctx_t *ctx, const char *host, int auth_required)
{
    int ret;

    ret = mbedtls_x509_crt_parse(&ctx->cacert, (const unsigned char *)GLOBAL_ROOT_CERT,
                                   GLOBAL_ROOT_CERT_LEN);
    if (ret < 0) {
        return ret;
    }

    ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        return ret;
    }

    mbedtls_ssl_conf_authmode(&ctx->conf,
                              auth_required ? MBEDTLS_SSL_VERIFY_REQUIRED
                                              : MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);

    ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
    if (ret != 0) {
        return ret;
    }

    if (host != NULL && host[0] != '\0') {
        ret = mbedtls_ssl_set_hostname(&ctx->ssl, host);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

/*
 * Match prebuilt volc_tls_start: set_bio + one mbedtls_ssl_handshake call.
 * WANT_READ / WANT_WRITE must be returned so Tls_mbedtls can continue async.
 */
static int volc_tls_run_handshake(volc_tls_ctx_t *ctx, void *session,
                                  void *send_cb, void *recv_cb)
{
    mbedtls_ssl_set_bio(&ctx->ssl, session, (mbedtls_ssl_send_t *)send_cb,
                        (mbedtls_ssl_recv_t *)recv_cb, NULL);
    return mbedtls_ssl_handshake(&ctx->ssl);
}

int volc_tls_create(void **out)
{
    volc_tls_ctx_t *ctx;

    if (out == NULL) {
        return 1;
    }

    if (volc_psa_init_once() != PSA_SUCCESS) {
        return (int)MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    ctx = (volc_tls_ctx_t *)volc_osal_calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return (int)MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cacert);

    *out = ctx;
    return 0;
}

void volc_tls_destroy(void *tls)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;

    if (ctx == NULL) {
        return;
    }

    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_x509_crt_free(&ctx->cacert);
    volc_osal_free(ctx);
}

/* volc_tls_start(tls, io, host, send_cb, recv_cb, session) */
int volc_tls_start(void *tls, void *io, const char *host_lite, void *send_cb, void *recv_cb,
                   void *session)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;
    int ret;

    (void)io;

    if (ctx == NULL || host_lite == NULL || send_cb == NULL || recv_cb == NULL || session == NULL) {
        return -1;
    }

    ret = volc_tls_configure(ctx, host_lite, 0);
    if (ret != 0) {
        return ret;
    }

    return volc_tls_run_handshake(ctx, session, send_cb, recv_cb);
}

/*
 * volc_tls_start_with_verify(tls, io, host_lite, host_arg, send_cb, recv_cb, session)
 * CA chain comes from GLOBAL_ROOT_CERT; host_arg is SNI fallback.
 */
int volc_tls_start_with_verify(void *tls, void *io, const char *host_lite, const char *host_arg,
                               void *send_cb, void *recv_cb, void *session)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;
    const char *host;
    int ret;

    (void)io;

    if (ctx == NULL || host_lite == NULL || host_arg == NULL || send_cb == NULL ||
        recv_cb == NULL || session == NULL) {
        return -1;
    }

    host = (host_lite[0] != '\0') ? host_lite : host_arg;

    ret = volc_tls_configure(ctx, host, 1);
    if (ret != 0) {
        return ret;
    }

    return volc_tls_run_handshake(ctx, session, send_cb, recv_cb);
}

int volc_tls_read(void *tls, unsigned char *buf, size_t len)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;

    if (ctx == NULL || buf == NULL || len == 0) {
        return -1;
    }

    return mbedtls_ssl_read(&ctx->ssl, buf, len);
}

int volc_tls_write(void *tls, const unsigned char *buf, size_t len)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;

    if (ctx == NULL || buf == NULL) {
        return -1;
    }

    return mbedtls_ssl_write(&ctx->ssl, buf, len);
}

int volc_tls_is_handshake_over(void *tls)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;

    if (ctx == NULL) {
        return 0;
    }

    return ctx->ssl.MBEDTLS_PRIVATE(state) >= MBEDTLS_SSL_HANDSHAKE_OVER ? 1 : 0;
}

int volc_tls_get_bytes_avail(void *tls)
{
    volc_tls_ctx_t *ctx = (volc_tls_ctx_t *)tls;

    if (ctx == NULL) {
        return 0;
    }

    return (int)mbedtls_ssl_get_bytes_avail(&ctx->ssl);
}

#endif /* VOLC_MBEDTLS_V4 */
