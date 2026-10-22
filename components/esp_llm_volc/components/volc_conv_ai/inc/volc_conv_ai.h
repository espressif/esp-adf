// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __VOLC_CONV_AI_H__
#define __VOLC_CONV_AI_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define __volc_rt_api__ __attribute__((visibility("default")))

#define VOLC_VERSION_MAJOR 1
#define VOLC_VERSION_MINOR 0
#define VOLC_VERSION_PATCH 0
#define VOLC_VERSION_CHECK(major, minor, patch)     ((major * 10000U) + (minor * 100U) + patch)
#define VOLC_VERSION                                VOLC_VERSION_CHECK(VOLC_VERSION_MAJOR, VOLC_VERSION_MINOR, VOLC_VERSION_PATCH)

typedef enum {
    VOLC_ERR_NO_ERROR = 0,
    VOLC_ERR_FAILED   = -1,
    VOLC_ERR_LICENSE_EXHAUSTED = -10,
    VOLC_ERR_LICENSE_EXPIRED   = -11,
} volc_error_code_e;

typedef enum {
    VOLC_AUDIO_DATA_TYPE_UNKNOWN = 0,
    VOLC_AUDIO_DATA_TYPE_OPUS    = 1,
    VOLC_AUDIO_DATA_TYPE_G722    = 2,
    VOLC_AUDIO_DATA_TYPE_AACLC   = 3,
    VOLC_AUDIO_DATA_TYPE_G711A    = 4,
    VOLC_AUDIO_DATA_TYPE_PCM     = 5,
    VOLC_AUDIO_DATA_TYPE_G711U    = 6,
} volc_audio_data_type_e;

typedef enum {
    VOLC_AUDIO_CODEC_TYPE_UNKNOWN = 0,
    VOLC_AUDIO_CODEC_TYPE_OPUS = 1,
    VOLC_AUDIO_CODEC_TYPE_G722 = 2,
    VOLC_AUDIO_CODEC_TYPE_G711A = 3,
    VOLC_AUDIO_CODEC_TYPE_PCM   = 4,
    VOLC_AUDIO_CODEC_TYPE_G711U = 5,
} volc_audio_codec_type_e;

typedef struct {
    volc_audio_data_type_e data_type;
    // uint16_t sent_ts;
    bool commit;
} volc_audio_frame_info_t;

typedef enum {
    VOLC_VIDEO_CODEC_TYPE_UNKNOWN = 0,
    VOLC_VIDEO_CODEC_TYPE_H264    = 1,
    VOLC_VIDEO_CODEC_TYPE_H265    = 2,
    VOLC_VIDEO_CODEC_TYPE_JPEG    = 3,
} volc_video_codec_type_e;

typedef enum {
    VOLC_VIDEO_DATA_TYPE_UNKNOWN = 0,
    VOLC_VIDEO_DATA_TYPE_H264    = 1,
    VOLC_VIDEO_DATA_TYPE_H265    = 2,
    VOLC_VIDEO_DATA_TYPE_JPEG    = 3,
} volc_video_data_type_e;

typedef struct {
    volc_video_data_type_e  data_type;
} volc_video_frame_info_t;

typedef enum {
    VOLC_CONV_STATUS_LISTENING = 1,
    VOLC_CONV_STATUS_THINKING,
    VOLC_CONV_STATUS_ANSWERING,
    VOLC_CONV_STATUS_INTERRUPTED,
    VOLC_CONV_STATUS_ANSWER_FINISH,
} volc_conv_status_e;

typedef struct {
    // place holder
    bool is_binary;
} volc_message_info_t;

typedef enum {
  VOLC_DATA_TYPE_AUDIO = 0,
  VOLC_DATA_TYPE_VIDEO,
  VOLC_DATA_TYPE_MESSAGE,
  VOLC_DATA_TYPE_CNT,
} volc_data_type_e;

typedef struct {
  volc_data_type_e type;
  union {
    volc_audio_frame_info_t audio;
    volc_video_frame_info_t video;
    volc_message_info_t message;
  } info;
} volc_data_info_t;

typedef enum {
    VOLC_EV_UNKNOWN = 0,          // 未知事件
    VOLC_EV_CONNECTED,            // 成功连接
    VOLC_EV_DISCONNECTED,         // 断开连接
    VOLC_EV_QUOTA_EXCEEDED,
} volc_event_code_e;

typedef struct {
    volc_event_code_e code; // 包含错误码、告警码、关键事件码等
    union {
        int placeholder;
    } data;   // 事件数据，具体内容根据event_code而定
} volc_event_t;

typedef enum {
    VOLC_MODE_RTC  = 0,
    VOLC_MODE_WS   = 1,
    VOLC_MODE_UNKNOWN,
} volc_mode_e;
typedef struct {
    volc_mode_e mode;
    char* bot_id;
    char* params;
} volc_opt_t;

typedef enum {
    VOLC_AGENT_TYPE_TTS = 0,
    VOLC_AGENT_TYPE_LLM = 1,
    VOLC_AGENT_TYPE_UNKNOW,
} volc_agent_type_e;

typedef void* volc_engine_t;

typedef struct {
    void (*on_volc_event)(volc_engine_t handle, volc_event_t* event, void* user_data);
    void (*on_volc_conversation_status)(volc_engine_t handle, volc_conv_status_e status, void* user_data);
    void (*on_volc_audio_data)(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_audio_frame_info_t* info_ptr, void* user_data);
    void (*on_volc_video_data)(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_video_frame_info_t* info_ptr, void* user_data);
    void (*on_volc_message_data)(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_message_info_t* info_ptr, void* user_data);
} volc_event_handler_t;

__volc_rt_api__ const char* volc_get_version(void);

__volc_rt_api__ const char* volc_err_2_str(int err_code);

__volc_rt_api__ int volc_create(volc_engine_t* handle, const char* config_json, volc_event_handler_t* event_handler, void* user_data);

__volc_rt_api__ void volc_destroy(volc_engine_t handle);

__volc_rt_api__ int volc_start(volc_engine_t handle, volc_opt_t* opt);

__volc_rt_api__ int volc_stop(volc_engine_t handle);

__volc_rt_api__ int volc_update(volc_engine_t handle, const void* data_ptr, size_t data_len);

__volc_rt_api__ int volc_send_audio_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_audio_frame_info_t* info_ptr);

__volc_rt_api__ int volc_send_video_data(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_video_frame_info_t* info_ptr);

__volc_rt_api__ int volc_send_message(volc_engine_t handle, const void* data_ptr, size_t data_len, volc_message_info_t* info_ptr);

__volc_rt_api__ int volc_send_text_to_agent(volc_engine_t handle, const char* text, volc_agent_type_e type, int interrupt_mode);

__volc_rt_api__ int volc_interrupt(volc_engine_t handle);

#ifdef __cplusplus
}
#endif
#endif /* __VOLC_CONV_AI_H__ */
