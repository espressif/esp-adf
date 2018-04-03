// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "tcpip_adapter.h"
#include "lwip/sockets.h"
#include "rom/md5_hash.h"
#include "mbedtls/base64.h"

#include "esp_system.h"
#include "esp_log.h"

#include "http_utils.h"
#include "http_auth.h"

#define MD5_MAX_LEN (33)
#define HTTP_AUTH_BUF_LEN (1024)

static const char *TAG = "HTTP_AUTH";

#ifndef mem_check
#define mem_check(x) assert(x)
#endif

static int md5_printf(char *md, const char *fmt, ...)
{
    unsigned char *buf = calloc(1, HTTP_AUTH_BUF_LEN);
    mem_check(buf);
    unsigned char *digest = calloc(1, MD5_MAX_LEN);
    mem_check(digest);
    int len, i;
    struct MD5Context *md5_ctx = calloc(1, sizeof(struct MD5Context));
    mem_check(md5_ctx);
    va_list ap;
    va_start(ap, fmt);
    len = vsnprintf((char *)buf, HTTP_AUTH_BUF_LEN, fmt, ap);

    MD5Init(md5_ctx);
    MD5Update(md5_ctx, buf, len);
    MD5Final(digest, md5_ctx);

    for (i = 0; i < 16; ++i) {
        sprintf(&md[i * 2], "%02x", (unsigned int)digest[i]);
    }

    va_end(ap);
    free(digest);
    free(buf);
    free(md5_ctx);
    return 32;
}
char *http_auth_digest(const char *username, const char *password, esp_http_auth_data_t *auth_data)
{
    char *ha1, *ha2;
    char *digest;
    char *auth_str;

    if (username == NULL ||
        password == NULL ||
        auth_data->nonce == NULL ||
        auth_data->uri == NULL ||
        auth_data->realm == NULL) {
        return NULL;
    }

    ha1 = calloc(1, MD5_MAX_LEN);
    mem_check(ha1);

    ha2 = calloc(1, MD5_MAX_LEN);
    mem_check(ha2);

    digest = calloc(1, MD5_MAX_LEN);
    mem_check(digest);

    auth_str = calloc(1, 512);
    mem_check(auth_str);

    md5_printf(ha1, "%s:%s:%s", username, auth_data->realm, password);

    ESP_LOGD(TAG, "%s %s %s %s\r\n", "Digest", username, auth_data->realm, password);
    if (strcasecmp(auth_data->algorithm, "md5-sess") == 0) {
        md5_printf(ha1, "%s:%s:%016llx", ha1, auth_data->nonce, auth_data->cnonce);
    }
    md5_printf(ha2, "%s:%s", auth_data->method, auth_data->uri);

    //support qop = auth
    if (auth_data->qop && strcasecmp(auth_data->qop, "auth-int") == 0) {
        md5_printf(ha2, "%s:%s", ha2, "entity");
    }

    if (auth_data->qop) {
        //response=MD5(HA1:nonce:nonceCount:cnonce:qop:HA2)
        md5_printf(digest, "%s:%s:%08x:%016llx:%s:%s", ha1, auth_data->nonce, auth_data->nc, auth_data->cnonce, auth_data->qop, ha2);
    } else {
        //response=MD5(HA1:nonce:HA2)
        md5_printf(digest, "%s:%s:%s", ha1, auth_data->nonce, ha2);
    }
    free(ha1);
    free(ha2);
    snprintf(auth_str, 512, "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=\"MD5\", "
             "response=\"%s\", opaque=\"%s\", qop=%s, nc=%08x, cnonce=\"%016llx\"",
             username, auth_data->realm, auth_data->nonce, auth_data->uri, digest, auth_data->opaque, auth_data->qop, auth_data->nc, auth_data->cnonce);
    free(digest);
    return auth_str;
}

char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *digest = calloc(1, MD5_MAX_LEN + 6);
    mem_check(digest);
    char *user_info = calloc(1, strlen(username) + strlen(password) + 1);
    mem_check(user_info);
    sprintf(user_info, "%s:%s", username, password);
    strcpy(digest, "Basic ");
    mbedtls_base64_encode((unsigned char *)digest + 6, MD5_MAX_LEN, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    free(user_info);
    return digest;
}
