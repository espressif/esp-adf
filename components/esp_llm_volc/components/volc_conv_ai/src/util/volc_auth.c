// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "util/volc_auth.h"

#include <stdlib.h>
#include <string.h>

#include "volc_mbedtls_compat.h"
#include <mbedtls/error.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#if VOLC_MBEDTLS_V4
#include "psa/crypto.h"
#else
#include <mbedtls/aes.h>
#endif

#include "volc_osal.h"
#include "util/volc_base64.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

typedef bool(predicate_func)(unsigned char c);

static bool _url_trim_pred(unsigned char ch) {
    if (ch < 32) {
        return true;
    }
    switch (ch) {
        case 0x20: /* ' ' - space */
        case 0x09: /* '\t' - horizontal tab */
        case 0x0A: /* '\n' - line feed */
        case 0x0B: /* '\v' - vertical tab */
        case 0x0C: /* '\f' - form feed */
        case 0x0D: /* '\r' - carriage return */
        case '\a': /* '\r' - carriage return */
            return true;
        default:
            return false;
    }
}
static int _right_trim_pred(unsigned char* str, predicate_func* pred) {
    int len = strlen((const char*)str);

    while(len > 0 && pred(str[len - 1])) {
        --len;
    }

    str[len] = '\0';
    return len;
}

static int __aes_cbc(bool encrypt, const unsigned char* key, unsigned char* iv, const unsigned char* input, uint32_t ilen, int key_bits, unsigned char* output) {
#if VOLC_MBEDTLS_V4
    psa_status_t status;
    psa_key_id_t key_id = PSA_KEY_ID_NULL;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    size_t output_len = 0;
    size_t finish_len = 0;

    if (encrypt) {
        return -1;
    }

    status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        return -1;
    }

    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, (size_t)key_bits);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_CBC_NO_PADDING);

    status = psa_import_key(&attributes, key, (size_t)key_bits / 8, &key_id);
    if (status != PSA_SUCCESS) {
        return -1;
    }

    status = psa_cipher_decrypt_setup(&operation, key_id, PSA_ALG_CBC_NO_PADDING);
    if (status != PSA_SUCCESS) {
        psa_destroy_key(key_id);
        return -1;
    }

    status = psa_cipher_set_iv(&operation, iv, 16);
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(&operation);
        psa_destroy_key(key_id);
        return -1;
    }

    status = psa_cipher_update(&operation, input, ilen, output, ilen, &output_len);
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(&operation);
        psa_destroy_key(key_id);
        return -1;
    }

    status = psa_cipher_finish(&operation, output + output_len, ilen - output_len, &finish_len);
    psa_destroy_key(key_id);
    return (status == PSA_SUCCESS) ? 0 : -1;
#else
    int mode = encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, key, key_bits);
    mbedtls_aes_crypt_cbc(&aes_ctx, mode, ilen, iv, (const unsigned char *)input, output);
    mbedtls_aes_free(&aes_ctx);

    return 0;
#endif
}

#define AES_KEY_SIZE 16 // AES key size in bytes
#define AES_KEY_SIZE_24 24
char* volc_aes_decode(const char* secret, const char* input, const bool partial_secret) {
    size_t decoded_len = 0;
    size_t secret_len = 0;
    int key_bits = 0;
    char* key = NULL;
    unsigned char iv[16] = {0};
    unsigned char* decoded_payload = NULL;
    if (input == NULL || strlen(input) == 0) {
        LOGE("Input is NULL or empty");
        return NULL;
    }

    if (partial_secret) {
        // If partial secret is used, we only use the first 8 bytes of the key and iv
        key = (char *)volc_osal_calloc(AES_KEY_SIZE, 1);
        memcpy(key, secret, AES_KEY_SIZE);
        key_bits = 128; // AES-128
    } else {
        secret_len = strlen(secret);
        if (secret_len < AES_KEY_SIZE_24) {
            LOGE("Secret length is less than 24, secret: %s", secret);
            return NULL;
        }
        key = (char *)volc_osal_calloc(AES_KEY_SIZE_24, 1); // +1 for null terminator
        memcpy(key, secret, AES_KEY_SIZE_24);
        key_bits = 192; // AES-192
    }

    decoded_len = volc_base64_decoded_length((const uint8_t*)input, strlen(input));
    decoded_payload = (unsigned char *)volc_osal_calloc(decoded_len + 1, 1);
    if (!decoded_payload) {
        LOGE("Failed to allocate memory for decoded payload");
        return NULL;
    }
    volc_base64_decode((unsigned char *)decoded_payload, decoded_len, &decoded_len, (const unsigned char *)input, strlen(input));
    LOGV("decoded payload: %s, len: %zu", decoded_payload, decoded_len);

    uint8_t *output = volc_osal_calloc(decoded_len + 16, 1); // +1 for null terminator

    memcpy(iv, secret, AES_KEY_SIZE); // Use the same secret for IV
    __aes_cbc(false, (const unsigned char *)key, (unsigned char *)iv, (const unsigned char *)decoded_payload, decoded_len, key_bits, output);

    _right_trim_pred(output, _url_trim_pred);

    LOGV("trimmed decrypted payload: %s, len: %d", output, (int)strlen((const char*)output));
    HAL_SAFE_FREE(decoded_payload);
    HAL_SAFE_FREE(key);
    return (char *)output;
}

void volc_sha256_hmac(const unsigned char* key, int keylen, const unsigned char* input, int ilen, unsigned char* output, int* plen) {
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), key, keylen, input, ilen, output);
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256));
}
