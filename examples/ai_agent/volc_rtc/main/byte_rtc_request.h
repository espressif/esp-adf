#pragma once

typedef enum {
    BYTE_RTC_REQ_AUDIO_TYPE_G711A,
    BYTE_RTC_REQ_AUDIO_TYPE_OPUS,
} byte_rtc_req_audio_type_e;

typedef enum {
    BYTE_RTC_REQ_VIDEO_TYPE_MJPEG,
    BYTE_RTC_REQ_VIDEO_TYPE_H264,
} byte_rtc_req_video_type_e;

#define BYTE_RTC_DEFAULT_CONFIG() {                    \
    .uri           = "",                               \
    .authorization = "",                               \
    .bot_id        = "",                               \
    .voice_id      = "",                               \
    .audio_type    = BYTE_RTC_REQ_AUDIO_TYPE_G711A,    \
    .video_type    = BYTE_RTC_REQ_VIDEO_TYPE_H264,     \
}

typedef struct {
    char *room_id;
    char *app_id;
    char *token;
    char *uid;
} byte_rtc_req_result_t;

typedef struct {
    const char               *uri;
    const char               *authorization;
    const char               *bot_id;
    const char               *voice_id;
    byte_rtc_req_audio_type_e audio_type;
    byte_rtc_req_video_type_e video_type;
} byte_rtc_req_config_t;

/**
 * @brief  Make a request to the ByteRTC service.
 *
 * @param[in] config Pointer to the configuration structure specifying the request parameters.
 *                   The structure should be initialized before calling this function.
 *
 * @return
          - A pointer to a `byte_rtc_req_result_t` structure containing the result of the request,
 *        - NULL if the request failed
 */
byte_rtc_req_result_t *byte_rtc_request(byte_rtc_req_config_t *config);

/**
 * @brief  Free the result of a ByteRTC request.
 *
 * @param[in] result Pointer to the result structure to be freed. Passing NULL is safe and has no effect.
 *
 * @return
 *      - None
 */
void byte_rtc_request_free(byte_rtc_req_result_t *result);