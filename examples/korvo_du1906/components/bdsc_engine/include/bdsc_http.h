/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */
#ifndef _BDSC_HTTP_H__
#define _BDSC_HTTP_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send http post directly
 *
 * @param[in] url            post url
 * @param[in] post_data      post data
 * @param[in] data_len       post data length
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int bdsc_send_http_post(char *url, char *post_data, size_t data_len);

/**
 * @brief Send https post sync
 *
 * @param[in] server            The post url
 * @param[in] port              The post port
 * @param[in] cacert_pem_buf    server certificate
 * @param[in] cacert_pem_bytes  server certificate length
 * @param[in] post_data_in      post data
 * @param[in] data_in_len       post data length
 * @param[in] ret_data_out      server returned data
 * @param[in] data_out_len      server returned data length
 *
 * @return
 *      - 0: on success
 *      - -1: fail
 */
int bdsc_send_https_post_sync(char *server, int port, 
                                char *cacert_pem_buf, size_t cacert_pem_bytes, 
                                char *post_data_in, size_t data_in_len, 
                                char **ret_data_out, size_t *data_out_len);

/**
 * @brief Send http GET with cb
 *
 * @param[in] client    esp_http_client_handle_t handle
 * @param[in] url       server url
 *
 * @return
 *      - 0: on success
 *      - -1: fail
 */
int bdsc_send_http_get_with_cb(esp_http_client_handle_t client, char *url, uint64_t pos);

esp_err_t bdsc_http_cb_destory();
#ifdef __cplusplus
}
#endif

#endif
