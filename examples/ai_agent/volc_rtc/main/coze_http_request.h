/* Coze http request example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

typedef enum {
    COZE_HTTP_REQ_SVC_TYPE_COZE,
    COZE_HTTP_REQ_SVC_TYPE_RTC,
} req_svc_type_t;

typedef enum {
    COZE_HTTP_REQ_AUDIO_TYPE_G711A,
    COZE_HTTP_REQ_AUDIO_TYPE_OPUS,
} coze_http_req_audio_type_e;

typedef enum {
    COZE_HTTP_REQ_VIDEO_TYPE_MJPEG,
    COZE_HTTP_REQ_VIDEO_TYPE_H264,
} coze_http_req_video_type_e;

#define COZE_HTTP_DEFAULT_CONFIG() {                       \
    .uri           = "https://api.coze.cn/v1/audio/rooms", \
    .authorization = "",                                   \
    .bot_id        = "",                                   \
    .voice_id      = "",                                   \
    .audio_type    = COZE_HTTP_REQ_AUDIO_TYPE_G711A,       \
    .video_type    = COZE_HTTP_REQ_VIDEO_TYPE_H264,        \
}

typedef struct {
    char *room_id;
    char *app_id;
    char *token;
    char *uid;
} coze_http_req_result_t;

typedef struct {
    const char               *uri;
    const char               *authorization;
    const char               *bot_id;
    const char               *voice_id;
    coze_http_req_audio_type_e audio_type;
    coze_http_req_video_type_e video_type;
} coze_http_req_config_t;

/**
 * @brief  Make a request to the ByteRTC service.
 *
 * @param[in] config Pointer to the configuration structure specifying the request parameters.
 *                   The structure should be initialized before calling this function.
 *
 * @return
          - A pointer to a `coze_http_req_result_t` structure containing the result of the request,
 *        - NULL if the request failed
 */
coze_http_req_result_t *coze_http_request(coze_http_req_config_t *config, req_svc_type_t type);

/**
 * @brief  Free the result of a ByteRTC request.
 *
 * @param[in] result Pointer to the result structure to be freed. Passing NULL is safe and has no effect.
 *
 * @return
 *      - None
 */
void coze_http_request_free(coze_http_req_result_t *result);