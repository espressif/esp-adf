// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_http.h"

#include "webclient.h"
#include "tls_certificate.h"
#include "util/volc_list.h"
#include "util/volc_log.h"

char* volc_http_post(const char* uri, const char* post_data, int data_len)
{
    struct webclient_session* session = NULL;
    char* buffer = NULL;
    int resp_status;
    size_t res_len = 0;

    /* create webclient session and set header response size */
    session = webclient_session_create(2048, GLOBAL_ROOT_CERT, GLOBAL_ROOT_CERT_LEN);
    if (session == NULL) {
        goto err_out_label;
    }

    webclient_header_fields_add(session, "Content-Type: application/json\r\n");
    webclient_header_fields_add(session, "Content-Length: %d\r\n", strlen(post_data));

    /* send POST request by default header */
    if ((resp_status = webclient_post(session, uri, post_data, data_len)) != 200) {
        LOGE("webclient POST request failed, response(%d) error.\n", resp_status);
    }

    webclient_response(session, (void**) &buffer, &res_len);
    LOGD("url: %s, request: %s, response: %s", uri, post_data, buffer);
err_out_label:
    if (session) {
        webclient_close(session);
    }

    return buffer;
}
