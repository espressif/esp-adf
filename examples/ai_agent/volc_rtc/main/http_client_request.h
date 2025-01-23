/* http client request example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

typedef struct {
    char *body;
    int   body_len;
} http_response_t;

typedef struct {
    const char *key;
    const char *value;
} http_req_header_t;

int http_client_post(const char *url, http_req_header_t *header , char *body, http_response_t *response);
