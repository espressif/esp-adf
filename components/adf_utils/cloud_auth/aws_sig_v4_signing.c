/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "mbedtls/sha256.h" /* SHA-256 only */
#include "mbedtls/md.h"     /* generic interface */

// Example from: https://docs.aws.amazon.com/general/latest/gr/sigv4-signed-request-examples.html

char *join(const char *first_str, const char *second_str)
{
    int first_str_len = strlen(first_str);
    int second_str_len = strlen(second_str);
    char *ret = NULL;
    ret = malloc(first_str_len + second_str_len + 1);
    assert(ret);
    memcpy(ret, first_str, first_str_len);
    memcpy(ret + first_str_len, second_str, second_str_len);
    ret[first_str_len + second_str_len] = 0;
    return ret;
}

static char *sign(const char *key, bool free_key, const char *payload, bool free_payload)
{
    unsigned char hmacResult[32];
    char *result = calloc(1, 33);
    assert(result);

    mbedtls_md_context_t ctx;

    const size_t payloadLength = strlen(payload);
    const size_t keyLength = strlen(key);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *) key, keyLength);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *) payload, payloadLength);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // for (int i = 0; i < sizeof(hmacResult); i++) {
    //     sprintf(result + i * 2, "%02x", (int)hmacResult[i]);
    // }
    memcpy(result, hmacResult, 32);
    if (free_key) {
        free((void *)key);
    }
    if (free_payload) {
        free((void *)payload);
    }
    return result;
}

static char *sha256_hex(const char *data, bool need_free)
{
    unsigned char sha256_res[32];
    char *result = calloc(1, 65);
    assert(result);
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); /* SHA-256, not 224 */
    mbedtls_sha256_update(&ctx, (const unsigned char*)data, strlen(data));
    mbedtls_sha256_finish(&ctx, sha256_res);
    for (int i = 0; i < sizeof(sha256_res); i++) {
        sprintf(result + i * 2, "%02x", (int)sha256_res[i]);
    }
    mbedtls_sha256_free(&ctx);
    return result;
}

static char *get_signature_key(const char *key, const char *date_stamp, const char *region_name, const char *service_name)
{
    char *aws4_key = join("AWS4", key);
    char *k_date = sign(aws4_key, true, date_stamp, false);
    char *k_region = sign(k_date, true, region_name, false);
    char *k_service = sign(k_region, true, service_name, false);
    char *k_signing = sign(k_service, true, "aws4_request", false);
    return k_signing;
}

static const char *polly_method = "POST";
static const char *polly_path = "/v1/speech";
static const char *polly_service_name = "polly";
static const char *polly_signed_headers = "content-type;host;x-amz-date";
static const char *polly_algorithm = "AWS4-HMAC-SHA256";

char *aws_polly_authentication_header(const char *payload,
                                      const char *region_name,
                                      const char *content_type,
                                      const char *secret_key,
                                      const char *access_key,
                                      const char *amz_date,
                                      const char *date_stamp)
{

    char *host = calloc(1, 128);
    assert(host);
    snprintf(host, 128, "%s.%s.amazonaws.com", polly_service_name, region_name); //polly.us-west-2.amazonaws.com/v1/speech
    char *polly_canonical_headers = calloc(1, 512);
    assert(polly_canonical_headers);
    //canonical_headers = 'content-type:' + content_type + '\n' + 'host:' + host + '\n' + 'x-amz-date:' + amz_date + '\n'
    snprintf(polly_canonical_headers, 512, "content-type:%s\nhost:%s\nx-amz-date:%s\n", content_type, host, amz_date);


    char *payload_hash = sha256_hex(payload, false);

    char *canonical_request = calloc(1, 512);
    assert(canonical_request);
    //canonical_request = method + '\n' + canonical_uri + '\n' + canonical_querystring + '\n' + canonical_headers + '\n' + signed_headers + '\n' + payload_hash
    snprintf(canonical_request, 512, "%s\n%s\n\n%s\n%s\n%s",
             polly_method,
             polly_path,
             polly_canonical_headers,
             polly_signed_headers,
             payload_hash);

    char *canonical_request_sha256 = sha256_hex(canonical_request, false);


    char *credential_scope = calloc(1, 512);
    assert(credential_scope);
    // credential_scope = date_stamp + '/' + region + '/' + service + '/' + 'aws4_request'
    snprintf(credential_scope, 512, "%s/%s/%s/aws4_request", date_stamp, region_name, polly_service_name);

    // signing_key = getSignatureKey(secret_key, date_stamp, region, service)
    char *signing_key = get_signature_key(secret_key, date_stamp, region_name, polly_service_name);

    char *string_to_sign = calloc(1, 512);
    assert(string_to_sign);
    //string_to_sign = algorithm + '\n' +  amz_date + '\n' +  credential_scope + '\n' +  hashlib.sha256(canonical_request).hexdigest()
    snprintf(string_to_sign, 512, "%s\n%s\n%s\n%s", polly_algorithm, amz_date, credential_scope, canonical_request_sha256);


    char *signature = sign(signing_key, false, string_to_sign, false);
    char *signature_hex = calloc(1, 65);
    assert(signature_hex);
    for (int i=0; i<32; i++) {
        sprintf(signature_hex + i * 2, "%02x", (int)signature[i]);
    }


    char *authorization_header = calloc(1, 512);
    assert(authorization_header);
    snprintf(authorization_header, 512, "%s Credential=%s/%s, SignedHeaders=%s, Signature=%s",
        polly_algorithm, access_key, credential_scope,
        polly_signed_headers,
        signature_hex);
    // authorization_header = algorithm + ' ' + 'Credential=' + access_key + '/' + credential_scope + ', ' +  'SignedHeaders=' + signed_headers + ', ' + 'Signature=' + signature
    free(host);
    free(polly_canonical_headers);
    free(payload_hash);
    free(canonical_request_sha256);
    free(credential_scope);
    free(signing_key);
    free(string_to_sign);
    free(signature);
    free(signature_hex);
    free(canonical_request);
    return authorization_header;
}
