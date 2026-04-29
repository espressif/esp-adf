/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_check.h"
#include "esp_idf_version.h"

/* IDF 6+ ships Mbed TLS 4 (no app-level entropy/ctr_drbg for PK). IDF 5.x uses Mbed TLS 3 API. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "mbedtls/bignum.h"
#include "psa/crypto.h"
#include "mbedtls/asn1.h"
#include "mbedtls/pem.h"
#else
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) */

#include "mbedtls/base64.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"
#include "mbedtls/md.h"

#include "esp_coze_jwt.h"

static const char *TAG = "ESP_COZE_JWT";

#define JWT_HEADER_FMT              "{\"alg\":\"RS256\",\"typ\":\"JWT\",\"kid\":\"%s\"}"
#define JWT_HEADER_BUF_LEN          256
#define JWT_SIGNATURE_BUF_LEN       1024
#define JWT_DRBG_PERSONALIZE        "esp_coze_jwt_pers"
#define JWT_RSA_MIN_BITLEN          2048
#define JWT_SHA256_LEN              32
#define JWT_SHA256_DIGEST_INFO_LEN  19

static void log_mbedtls_error(const char *what, int err)
{
    char buf[160];
    mbedtls_strerror(err, buf, sizeof(buf));
    ESP_LOGE(TAG, "Operation failed: %s: %d (-0x%04x): %s", what, err, (unsigned)-err, buf);
}

/**
 * Convert a NUL-terminated standard base64 string in place to base64url
 * (RFC 4648 §5): replace '+' with '-', '/' with '_', strip trailing '='.
 *
 * @param[in,out]  s    Buffer containing the base64 string. Modified in place.
 * @param[in,out]  len  On entry, current string length (excluding NUL). On
 *                      exit, the new length after stripping padding.
 */
static void base64_to_url_inplace(char *s, size_t *len)
{
    size_t n = *len;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '+') {
            s[i] = '-';
        }
        else if (s[i] == '/') {
            s[i] = '_';
        }
    }
    while (n > 0 && s[n - 1] == '=') {
        s[--n] = '\0';
    }
    *len = n;
}

/**
 * mbedtls-base64-encode + URL conversion. The output buffer must be sized
 * to hold the standard base64 (with padding) plus a NUL terminator.
 */
static int base64url_encode(const unsigned char *in, size_t in_len,
                            char *out, size_t out_size, size_t *out_len)
{
    size_t std_len = 0;
    int ret = mbedtls_base64_encode((unsigned char *)out, out_size, &std_len,
                                    in, in_len);
    if (ret != 0) {
        return ret;
    }
    base64_to_url_inplace(out, &std_len);
    if (out_len) {
        *out_len = std_len;
    }
    return 0;
}

/**
 * Capacity (including trailing NUL) needed by base64url_encode() for the
 * intermediate standard base64 form: 4 * ceil(n/3) + 1.
 */
static size_t base64url_capacity(size_t in_len)
{
    return ((in_len + 2) / 3) * 4 + 1;
}

static char *base64url_encode_alloc(const unsigned char *in, size_t in_len)
{
    size_t out_cap = base64url_capacity(in_len);
    char *out = (char *)calloc(1, out_cap);

    if (!out) {
        return NULL;
    }
    if (base64url_encode(in, in_len, out, out_cap, NULL) != 0) {
        free(out);
        return NULL;
    }
    return out;
}

static char *build_jwt_header_b64(const char *kid)
{
    char header[JWT_HEADER_BUF_LEN];
    int header_len = snprintf(header, sizeof(header), JWT_HEADER_FMT, kid);

    if (header_len <= 0 || header_len >= (int)sizeof(header)) {
        ESP_LOGE(TAG, "kid is too long");
        return NULL;
    }
    return base64url_encode_alloc((const unsigned char *)header, (size_t)header_len);
}

static char *join_with_dot(const char *left, const char *right)
{
    size_t out_cap = strlen(left) + 1 + strlen(right) + 1;
    char *out = (char *)calloc(1, out_cap);

    if (!out) {
        return NULL;
    }
    if (snprintf(out, out_cap, "%s.%s", left, right) <= 0) {
        free(out);
        return NULL;
    }
    return out;
}

static unsigned char *copy_private_key_with_nul(const uint8_t *private_key,
                                                size_t private_key_len)
{
    unsigned char *key = (unsigned char *)calloc(1, private_key_len + 1);

    if (!key) {
        return NULL;
    }
    memcpy(key, private_key, private_key_len);
    return key;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
static const char *const PEM_RSA_PRIVATE_KEY_BEGIN = "-----BEGIN RSA PRIVATE KEY-----";
static const char *const PEM_RSA_PRIVATE_KEY_END = "-----END RSA PRIVATE KEY-----";
static const char *const PEM_PRIVATE_KEY_BEGIN = "-----BEGIN PRIVATE KEY-----";
static const char *const PEM_PRIVATE_KEY_END = "-----END PRIVATE KEY-----";

static const unsigned char RSA_ENCRYPTION_OID[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};

static const unsigned char SHA256_DIGEST_INFO_PREFIX[JWT_SHA256_DIGEST_INFO_LEN] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
    0x00, 0x04, 0x20};

typedef struct {
    const unsigned char *n;
    size_t               n_len;
    const unsigned char *d;
    size_t               d_len;
    size_t               bitlen;
} rsa_pkcs1_minimal_key_t;

static void log_psa_error(const char *what, psa_status_t status)
{
    ESP_LOGE(TAG, "Operation failed: %s: %ld", what, (long)status);
}

static size_t trim_trailing_nul_len(const unsigned char *buf, size_t len)
{
    while (len > 0 && buf[len - 1] == '\0') {
        len--;
    }
    return len;
}

static int copy_der(const unsigned char *der, size_t der_len,
                    unsigned char **out, size_t *out_len)
{
    unsigned char *buf = (unsigned char *)calloc(1, der_len);
    if (!buf) {
        return MBEDTLS_ERR_PK_ALLOC_FAILED;
    }
    memcpy(buf, der, der_len);
    *out = buf;
    *out_len = der_len;
    return 0;
}

static int pem_to_der(const unsigned char *pem_key,
                      const char *header, const char *footer,
                      unsigned char **der, size_t *der_len)
{
    int ret = 0;
    size_t use_len = 0;
    size_t pem_len = 0;
    const unsigned char *pem_buf = NULL;
    mbedtls_pem_context pem;

    mbedtls_pem_init(&pem);
    ret = mbedtls_pem_read_buffer(&pem, header, footer, pem_key, NULL, 0, &use_len);
    if (ret == 0) {
        pem_buf = mbedtls_pem_get_buffer(&pem, &pem_len);
        ret = copy_der(pem_buf, pem_len, der, der_len);
    }
    mbedtls_pem_free(&pem);
    return ret;
}

static int asn1_get_sequence_end(unsigned char **p, unsigned char *end, unsigned char **seq_end)
{
    size_t len = 0;
    int ret = mbedtls_asn1_get_tag(p, end, &len,
                                   MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    if ((size_t)(end - *p) < len) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }
    *seq_end = *p + len;
    return 0;
}

static int asn1_get_zero_version(unsigned char **p, unsigned char *end)
{
    int version = 0;
    int ret = mbedtls_asn1_get_int(p, end, &version);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    return version == 0 ? 0 : MBEDTLS_ERR_PK_KEY_INVALID_VERSION;
}

static size_t unsigned_integer_bitlen(const unsigned char *integer, size_t integer_len)
{
    unsigned char msb = integer[0];
    size_t high_bits = 0;

    while (msb != 0) {
        high_bits++;
        msb >>= 1;
    }
    return (integer_len - 1) * 8 + high_bits;
}

static int asn1_get_unsigned_integer(unsigned char **p, unsigned char *end,
                                     const unsigned char **integer, size_t *integer_len)
{
    int ret = 0;
    size_t len = 0;

    ret = mbedtls_asn1_get_tag(p, end, &len, MBEDTLS_ASN1_INTEGER);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    if (len == 0 || (size_t)(end - *p) < len) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    while (len > 0 && **p == 0) {
        (*p)++;
        len--;
    }
    if (len == 0) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    *integer = *p;
    *integer_len = len;
    *p += len;
    return 0;
}

static int pkcs8_extract_rsa_der(const unsigned char *pkcs8, size_t pkcs8_len,
                                 unsigned char **rsa_der, size_t *rsa_der_len)
{
    int ret = 0;
    size_t len = 0;
    unsigned char *p = (unsigned char *)pkcs8;
    unsigned char *end = p + pkcs8_len;
    unsigned char *pkcs8_end = NULL;
    unsigned char *alg_end = NULL;

    ret = asn1_get_sequence_end(&p, end, &pkcs8_end);
    if (ret != 0) {
        return ret;
    }

    ret = asn1_get_zero_version(&p, pkcs8_end);
    if (ret != 0) {
        return ret;
    }

    ret = asn1_get_sequence_end(&p, pkcs8_end, &alg_end);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_asn1_get_tag(&p, alg_end, &len, MBEDTLS_ASN1_OID);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    if (len != sizeof(RSA_ENCRYPTION_OID) ||
        memcmp(p, RSA_ENCRYPTION_OID, sizeof(RSA_ENCRYPTION_OID)) != 0) {
        return MBEDTLS_ERR_PK_INVALID_ALG;
    }
    /* rsaEncryption parameters are optional for our decoder. Skip the rest of AlgorithmIdentifier. */
    p = alg_end;

    ret = mbedtls_asn1_get_tag(&p, pkcs8_end, &len, MBEDTLS_ASN1_OCTET_STRING);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    if (len == 0 || (size_t)(pkcs8_end - p) < len) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }

    return copy_der(p, len, rsa_der, rsa_der_len);
}

static int decode_rsa_private_key_der(const unsigned char *key, size_t key_len,
                                      unsigned char **rsa_der, size_t *rsa_der_len)
{
    int ret = 0;
    unsigned char *pkcs8_der = NULL;
    size_t pkcs8_der_len = 0;

    key_len = trim_trailing_nul_len(key, key_len);

    if (strstr((const char *)key, PEM_RSA_PRIVATE_KEY_BEGIN) != NULL) {
        return pem_to_der(key, PEM_RSA_PRIVATE_KEY_BEGIN, PEM_RSA_PRIVATE_KEY_END,
                          rsa_der, rsa_der_len);
    }

    if (strstr((const char *)key, PEM_PRIVATE_KEY_BEGIN) != NULL) {
        ret = pem_to_der(key, PEM_PRIVATE_KEY_BEGIN, PEM_PRIVATE_KEY_END,
                         &pkcs8_der, &pkcs8_der_len);
        if (ret != 0) {
            return ret;
        }
        ret = pkcs8_extract_rsa_der(pkcs8_der, pkcs8_der_len, rsa_der, rsa_der_len);
        free(pkcs8_der);
        return ret;
    }

    ret = pkcs8_extract_rsa_der(key, key_len, rsa_der, rsa_der_len);
    if (ret == 0) {
        return 0;
    }

    return copy_der(key, key_len, rsa_der, rsa_der_len);
}

static int rsa_pkcs1_parse_minimal_key(const unsigned char *rsa_der, size_t rsa_der_len,
                                       rsa_pkcs1_minimal_key_t *key)
{
    int ret = 0;
    int version = 0;
    const unsigned char *ignored = NULL;
    size_t ignored_len = 0;
    unsigned char *p = (unsigned char *)rsa_der;
    unsigned char *end = p + rsa_der_len;
    unsigned char *seq_end = NULL;

    memset(key, 0, sizeof(*key));

    ret = asn1_get_sequence_end(&p, end, &seq_end);
    if (ret != 0) {
        return ret;
    }

    ret = mbedtls_asn1_get_int(&p, seq_end, &version);
    if (ret != 0) {
        return MBEDTLS_ERROR_ADD(MBEDTLS_ERR_PK_KEY_INVALID_FORMAT, ret);
    }
    if (version != 0 && version != 1) {
        return MBEDTLS_ERR_PK_KEY_INVALID_VERSION;
    }

    ret = asn1_get_unsigned_integer(&p, seq_end, &key->n, &key->n_len);
    if (ret != 0) {
        return ret;
    }
    ret = asn1_get_unsigned_integer(&p, seq_end, &ignored, &ignored_len);
    if (ret != 0) {
        return ret;
    }
    ret = asn1_get_unsigned_integer(&p, seq_end, &key->d, &key->d_len);
    if (ret != 0) {
        return ret;
    }

    key->bitlen = unsigned_integer_bitlen(key->n, key->n_len);
    if (key->bitlen < JWT_RSA_MIN_BITLEN) {
        return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
    }
    return 0;
}

static int sign_rs256_legacy_mpi(const unsigned char *rsa_der, size_t rsa_der_len,
                                 const unsigned char *hash, size_t hash_len,
                                 unsigned char *signature, size_t signature_size,
                                 size_t *signature_len)
{
    int ret = 0;
    rsa_pkcs1_minimal_key_t rsa_key;
    mbedtls_mpi m;
    mbedtls_mpi n;
    mbedtls_mpi d;
    mbedtls_mpi s;
    unsigned char *encoded = NULL;
    size_t encoded_len = 0;
    size_t digest_info_len = sizeof(SHA256_DIGEST_INFO_PREFIX) + hash_len;
    size_t ps_len = 0;

    mbedtls_mpi_init(&m);
    mbedtls_mpi_init(&n);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&s);

    if (hash_len != JWT_SHA256_LEN) {
        ret = MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        goto cleanup;
    }

    ret = rsa_pkcs1_parse_minimal_key(rsa_der, rsa_der_len, &rsa_key);
    if (ret != 0) {
        log_mbedtls_error("rsa_pkcs1_parse_minimal_key", ret);
        goto cleanup;
    }

    encoded_len = rsa_key.n_len;
    if (signature_size < encoded_len) {
        ret = MBEDTLS_ERR_PK_BUFFER_TOO_SMALL;
        goto cleanup;
    }
    if (encoded_len < digest_info_len + 11) {
        ret = MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
        goto cleanup;
    }

    encoded = (unsigned char *)calloc(1, encoded_len);
    if (!encoded) {
        ret = MBEDTLS_ERR_PK_ALLOC_FAILED;
        goto cleanup;
    }

    ps_len = encoded_len - digest_info_len - 3;
    encoded[0] = 0x00;
    encoded[1] = 0x01;
    memset(encoded + 2, 0xff, ps_len);
    encoded[2 + ps_len] = 0x00;
    memcpy(encoded + 3 + ps_len, SHA256_DIGEST_INFO_PREFIX, sizeof(SHA256_DIGEST_INFO_PREFIX));
    memcpy(encoded + 3 + ps_len + sizeof(SHA256_DIGEST_INFO_PREFIX), hash, hash_len);

    ret = mbedtls_mpi_read_binary(&m, encoded, encoded_len);
    if (ret != 0) {
        log_mbedtls_error("mbedtls_mpi_read_binary(message)", ret);
        goto cleanup;
    }
    ret = mbedtls_mpi_read_binary(&n, rsa_key.n, rsa_key.n_len);
    if (ret != 0) {
        log_mbedtls_error("mbedtls_mpi_read_binary(N)", ret);
        goto cleanup;
    }
    ret = mbedtls_mpi_read_binary(&d, rsa_key.d, rsa_key.d_len);
    if (ret != 0) {
        log_mbedtls_error("mbedtls_mpi_read_binary(D)", ret);
        goto cleanup;
    }

    ret = mbedtls_mpi_exp_mod(&s, &m, &d, &n, NULL);
    if (ret != 0) {
        log_mbedtls_error("mbedtls_mpi_exp_mod", ret);
        goto cleanup;
    }
    ret = mbedtls_mpi_write_binary(&s, signature, encoded_len);
    if (ret != 0) {
        log_mbedtls_error("mbedtls_mpi_write_binary(signature)", ret);
        goto cleanup;
    }

    *signature_len = encoded_len;
    ret = 0;

cleanup:
    free(encoded);
    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&d);
    mbedtls_mpi_free(&n);
    mbedtls_mpi_free(&m);
    return ret;
}

static int sign_rs256_psa(const unsigned char *key, size_t key_len,
                          const unsigned char *hash, size_t hash_len,
                          unsigned char *signature, size_t signature_size,
                          size_t *signature_len)
{
    int ret = 0;
    psa_status_t status = PSA_SUCCESS;
    psa_key_id_t key_id = 0;
    psa_algorithm_t alg = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    unsigned char *rsa_der = NULL;
    size_t rsa_der_len = 0;
    rsa_pkcs1_minimal_key_t rsa_key;

    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        log_psa_error("psa_crypto_init", status);
        return -1;
    }

    ret = decode_rsa_private_key_der(key, key_len, &rsa_der, &rsa_der_len);
    if (ret != 0) {
        log_mbedtls_error("decode_rsa_private_key_der", ret);
        return ret;
    }

    ret = rsa_pkcs1_parse_minimal_key(rsa_der, rsa_der_len, &rsa_key);
    if (ret != 0) {
        log_mbedtls_error("rsa_pkcs1_parse_minimal_key", ret);
        goto cleanup;
    }

    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_KEY_PAIR);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);

    status = psa_import_key(&attributes, rsa_der, rsa_der_len, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "PSA RSA import input: der_len=%u, bitlen=%u",
                 (unsigned)rsa_der_len, (unsigned)rsa_key.bitlen);
        log_psa_error("psa_import_key", status);
        ESP_LOGW(TAG, "Falling back to legacy RS256 signer for IDF 5.x key compatibility");
        ret = sign_rs256_legacy_mpi(rsa_der, rsa_der_len, hash, hash_len,
                                    signature, signature_size, signature_len);
        goto cleanup;
    }

    status = psa_sign_hash(key_id, alg, hash, hash_len,
                           signature, signature_size, signature_len);
    if (status != PSA_SUCCESS) {
        log_psa_error("psa_sign_hash", status);
        ret = -1;
        goto cleanup;
    }

cleanup:
    if (key_id != 0) {
        status = psa_destroy_key(key_id);
        if (status != PSA_SUCCESS && ret == 0) {
            log_psa_error("psa_destroy_key", status);
            ret = -1;
        }
    }
    psa_reset_key_attributes(&attributes);
    free(rsa_der);
    return ret;
}
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) */

static int sign_rs256(const unsigned char *key, size_t key_len,
                      const unsigned char *hash, size_t hash_len,
                      unsigned char *signature, size_t signature_size,
                      size_t *signature_len
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
                      ,
                      mbedtls_pk_context *pk, mbedtls_ctr_drbg_context *ctr_drbg
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */
)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    return sign_rs256_psa(key, key_len, hash, hash_len,
                          signature, signature_size, signature_len);
#else
    return mbedtls_pk_sign(pk, MBEDTLS_MD_SHA256, hash, hash_len,
                           signature, signature_size, signature_len,
                           mbedtls_ctr_drbg_random, ctr_drbg);
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) */
}

char *esp_coze_jwt_create(const char *kid, const char *payload,
                          const uint8_t *private_key, size_t private_key_len)
{
    ESP_RETURN_ON_FALSE(kid != NULL && payload != NULL && private_key != NULL && private_key_len > 0,
                        NULL, TAG, "Invalid arguments");

    int r = 0;
    char *jwt = NULL;
    char *header_b64 = NULL;
    char *payload_b64 = NULL;
    char *header_and_payload = NULL;
    char *signature_b64 = NULL;
    unsigned char *signature_bytes = NULL;
    unsigned char *key = NULL;
    size_t signature_len = 0;
    size_t key_length = private_key_len;
    unsigned char hash[JWT_SHA256_LEN] = {0};
    const mbedtls_md_info_t *md_info = NULL;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    size_t pk_parse_key_length = private_key_len;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */

    /* Build and base64url-encode header */
    header_b64 = build_jwt_header_b64(kid);
    if (!header_b64) {
        ESP_LOGE(TAG, "Header base64url encode failed");
        goto cleanup;
    }

    /* Base64url-encode payload (size determined dynamically) */
    size_t payload_len = strlen(payload);
    payload_b64 = base64url_encode_alloc((const unsigned char *)payload, payload_len);
    if (!payload_b64) {
        ESP_LOGE(TAG, "Payload base64url encode failed");
        goto cleanup;
    }

    /* "<header_b64>.<payload_b64>" */
    header_and_payload = join_with_dot(header_b64, payload_b64);
    if (!header_and_payload) {
        ESP_LOGE(TAG, "Out of memory for header+payload");
        goto cleanup;
    }

    /* Working buffers */
    signature_bytes = (unsigned char *)calloc(1, JWT_SIGNATURE_BUF_LEN);
    key = copy_private_key_with_nul(private_key, key_length);
    if (!signature_bytes || !key) {
        ESP_LOGE(TAG, "Out of memory for working buffers");
        goto cleanup;
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    if (key[key_length - 1] != '\0') {
        pk_parse_key_length = key_length + 1;
    }
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    /* Random number generator (Mbed TLS 3.x only; TLS 4 / IDF 6 uses internal PSA RNG) */
    r = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)JWT_DRBG_PERSONALIZE,
                              sizeof(JWT_DRBG_PERSONALIZE) - 1);
    if (r != 0) {
        log_mbedtls_error("mbedtls_ctr_drbg_seed", r);
        goto cleanup;
    }

    /* Parse private key */
    r = mbedtls_pk_parse_key(&pk, key, pk_parse_key_length, NULL, 0,
                             mbedtls_ctr_drbg_random, &ctr_drbg);
    if (r != 0) {
        log_mbedtls_error("mbedtls_pk_parse_key", r);
        goto cleanup;
    }
    if (!mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA) &&
        !mbedtls_pk_can_do(&pk, MBEDTLS_PK_RSA_ALT)) {
        ESP_LOGE(TAG, "Private key is not RSA");
        goto cleanup;
    }
    if (mbedtls_pk_get_bitlen(&pk) < JWT_RSA_MIN_BITLEN) {
        ESP_LOGE(TAG, "RSA key length < %d bits", JWT_RSA_MIN_BITLEN);
        goto cleanup;
    }
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */

    /* SHA-256 hash of "<header_b64>.<payload_b64>" */
    md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        ESP_LOGE(TAG, "MD info SHA256 not available");
        goto cleanup;
    }
    r = mbedtls_md(md_info, (const unsigned char *)header_and_payload,
                   strlen(header_and_payload), hash);
    if (r != 0) {
        log_mbedtls_error("mbedtls_md", r);
        goto cleanup;
    }

    /* RSA-PKCS1-v1.5 signature */
    r = sign_rs256(key, key_length, hash, sizeof(hash),
                   signature_bytes, JWT_SIGNATURE_BUF_LEN, &signature_len
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
                   ,
                   &pk, &ctr_drbg
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */
    );
    if (r != 0) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        ESP_LOGE(TAG, "PSA RS256 sign failed");
#else
        log_mbedtls_error("mbedtls_pk_sign", r);
#endif  /* ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) */
        goto cleanup;
    }
    if (signature_len == 0) {
        ESP_LOGE(TAG, "MbedTLS pk_sign produced zero-length signature");
        goto cleanup;
    }

    /* Encode signature */
    signature_b64 = base64url_encode_alloc(signature_bytes, signature_len);
    if (!signature_b64) {
        ESP_LOGE(TAG, "Signature base64url encode failed");
        goto cleanup;
    }

    /* "<header_b64>.<payload_b64>.<sig_b64>" */
    jwt = join_with_dot(header_and_payload, signature_b64);
    if (!jwt) {
        ESP_LOGE(TAG, "Out of memory for JWT output");
        goto cleanup;
    }

cleanup:
    free(header_b64);
    free(payload_b64);
    free(header_and_payload);
    free(signature_b64);
    free(signature_bytes);
    free(key);
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
#endif  /* ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0) */
    return jwt;
}
