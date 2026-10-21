/*
 * Copyright (2025) Beijing Volcano Engine Technology Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Legacy mbedtls symbols used by prebuilt libVolcEngineRTCLite.a on IDF 6.x
 * (mbedtls 4.x): one-shot hashes, entropy helpers, and ssl_conf_rng.
 */

#include "volc_mbedtls_compat.h"

#if VOLC_MBEDTLS_V4

#include <string.h>

#include <mbedtls/error.h>
#include "psa/crypto.h"

/* Prebuilt RTC was built against mbedtls 3.x entropy context layout. */
struct mbedtls_entropy_context {
    unsigned char opaque[512];
};

#define MBEDTLS_ERR_ENTROPY_SOURCE_FAILED (-0x003C)

static psa_status_t volc_psa_init_once(void)
{
    psa_status_t status = psa_crypto_init();
    if (status == PSA_ERROR_BAD_STATE) {
        return PSA_SUCCESS;
    }
    return status;
}

int mbedtls_md5(const unsigned char *input, size_t ilen, unsigned char output[16])
{
    const size_t out_len = 16;
    size_t hash_len = 0;
    psa_status_t status = volc_psa_init_once();
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    status = psa_hash_compute(PSA_ALG_MD5, input, ilen, output, out_len, &hash_len);
    if (status != PSA_SUCCESS || hash_len != out_len) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }
    return 0;
}

int mbedtls_sha1(const unsigned char *input, size_t ilen, unsigned char output[20])
{
    const size_t out_len = 20;
    size_t hash_len = 0;
    psa_status_t status = volc_psa_init_once();
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }

    status = psa_hash_compute(PSA_ALG_SHA_1, input, ilen, output, out_len, &hash_len);
    if (status != PSA_SUCCESS || hash_len != out_len) {
        return MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    }
    return 0;
}

void mbedtls_entropy_init(struct mbedtls_entropy_context *ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

void mbedtls_entropy_free(struct mbedtls_entropy_context *ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

int mbedtls_entropy_func(void *data, unsigned char *output, size_t len)
{
    (void)data;

    psa_status_t status = volc_psa_init_once();
    if (status != PSA_SUCCESS) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    status = psa_generate_random(output, len);
    return status == PSA_SUCCESS ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

struct mbedtls_ssl_config;

void mbedtls_ssl_conf_rng(struct mbedtls_ssl_config *conf,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng)
{
    /* mbedtls 4.x uses PSA for TLS RNG; f_rng/p_rng fields were removed. */
    (void)conf;
    (void)f_rng;
    (void)p_rng;
}

#endif /* VOLC_MBEDTLS_V4 */
