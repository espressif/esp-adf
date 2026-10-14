// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "volc_rtc.h"

#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "volc_osal.h"
#include "util/volc_list.h"
#include "util/volc_log.h"
#include "util/volc_json.h"

#define MAGIC_CONTROL "ctrl"
#define MAGIC_CONV    "conv"
#define MAGIC_LENGTH 4
#define MAGIC_OFFSET 8
const char* interrupt_str = "{\"Command\":\"interrupt\"}";

typedef struct {
    bool b_pipeline_started;
    bool b_user_joined;
    bool b_channel_joined;
    bool b_first_keyframe_received;
    bool b_fini;
    bool b_audio_publish;
    bool b_video_publish;
    bool b_audio_subscribe;
    bool b_video_subscribe;
    char* p_appid;
    char* p_channel_name;
    char* p_user_id;
    char* p_remote_user_id;
    char* p_token;
    void* context;
    int audio_codec;
    volc_room_info_t info;
    volc_msg_cb message_callback;
    volc_data_cb data_callback;
    byte_rtc_engine_t rtc;
    byte_rtc_event_handler_t event_handler;
} rtc_impl_t;

static bool __is_first_keyframe_not_received(rtc_impl_t* rtc, int is_key_frame)
{
    return (!rtc->b_first_keyframe_received && !is_key_frame);
}

static int _build_binary_message(const char* magic, const char* message,
                        uint8_t** out_buf, size_t* out_len) {
    size_t magic_len = strlen(magic);
    size_t msg_len = strlen(message);

    // 分配内存：魔术字 + 4字节长度 + 消息内容
    *out_len = magic_len + 4 + msg_len;
    *out_buf = (uint8_t*)volc_osal_malloc(*out_len);
    if (!*out_buf) {
        LOGE("volc_osal_malloc failed");
        return -1;
    }

    // 填充魔术字
    memcpy(*out_buf, magic, magic_len);

    // 以大端序填充长度
    uint8_t* len_ptr = *out_buf + magic_len;
    len_ptr[0] = (uint8_t)((msg_len >> 24) & 0xFF); // 最高位字节
    len_ptr[1] = (uint8_t)((msg_len >> 16) & 0xFF);
    len_ptr[2] = (uint8_t)((msg_len >> 8)  & 0xFF);
    len_ptr[3] = (uint8_t)(msg_len & 0xFF);         // 最低位字节

    // 填充消息内容
    memcpy(*out_buf + magic_len + 4, message, msg_len);

    return 0;
}

static int __rtc_start(rtc_impl_t* rtc, volc_rtc_option_t* option)
{
    volc_opt_t* p_opt = NULL;
    byte_rtc_room_options_t room_opt = {0};
    if (!rtc || !option) {
        LOGE("rtc or option is NULL");
        return -1;
    }

    room_opt.auto_publish_audio = rtc->b_audio_publish;
    room_opt.auto_publish_video = rtc->b_video_publish;
    room_opt.auto_subscribe_audio = rtc->b_audio_subscribe;
    room_opt.auto_subscribe_video = rtc->b_video_subscribe;
    LOGI("Joining channel: %s, uid: %s, token: %s, vpub: %d, vsub: %d, apub: %d, asub: %d", option->p_channel_name, option->p_uid, option->p_token, (int)room_opt.auto_publish_video, (int)room_opt.auto_subscribe_video, (int)room_opt.auto_publish_audio, (int)room_opt.auto_subscribe_audio);
    int ret = byte_rtc_join_room(rtc->rtc, option->p_channel_name, option->p_uid, option->p_token, &room_opt);
    if (ret != 0) {
        LOGE("Failed to join room: %d", ret);
        return ret;
    }
    rtc->b_pipeline_started = true;
    rtc->p_channel_name = strdup(option->p_channel_name);
    rtc->p_user_id = strdup(option->p_uid);

    return 0;
}

static void __rtc_stop(rtc_impl_t* rtc)
{
    if (!rtc) {
        LOGE("rtc instance is NULL");
        return;
    }
    if (!rtc->b_pipeline_started) {
        LOGI("pipeline not started");
        return;
    }

    int ret = byte_rtc_leave_room(rtc->rtc, rtc->p_channel_name);
    if (ret != 0) {
        LOGE("Failed to leave room: %d", ret);
        return;
    }
    rtc->b_pipeline_started = false;
    HAL_SAFE_FREE(rtc->p_channel_name);

    return;
}

static void __send_data_2_user(rtc_impl_t* rtc, const void* data, int data_len, volc_data_info_t* info) {
    if (rtc->data_callback) {
        rtc->data_callback(rtc->context, data, data_len, info);
    }
}

static void _register_message_router(rtc_impl_t* rtc, volc_msg_cb callback)
{
    rtc->message_callback = callback;
}

static void _send_message_2_user(rtc_impl_t* rtc, volc_msg_t* msg)
{
    if (rtc->message_callback) {
        rtc->message_callback(rtc->context, msg);
    }
}

static bool _is_target_message(const uint8_t* message, const char* target) {
    if (message == NULL || target == NULL) {
        return false;
    }
    // Check if the first 4 bytes match the magic number for "subv"
    if (*(const uint32_t*)message != *(const uint32_t*)target) {
        return false;
    }
    return true;
}

static int _on_conversion_status_message_parsed(uint8_t* message, rtc_impl_t* rtc) {
    int c = -1;
    char* error_reason = NULL;
    volc_msg_t msg = { 0 };
    cJSON *root = cJSON_Parse((const char*)message);
    if (root == NULL) {
        return c;
    }
    volc_json_read_int(root, "Stage.Code", &c);
    volc_json_read_string(root, "ErrorInfo.Reason", &error_reason);
    if(error_reason != NULL) {
        LOGW("ErrorInfo.Reason %s",error_reason);
        if(strcmp(error_reason, "preParamCheck:AI license duration quota exceeded") == 0) {
            msg.code = VOLC_MSG_QUOTA_EXCEEDED;
            msg.data.conv_status = 0;
            _send_message_2_user(rtc, &msg);
        }
        HAL_SAFE_FREE(error_reason);
        return -1;
    }
    if(root != NULL) cJSON_Delete(root);
    return c;
}

static void _on_join_channel_success(byte_rtc_engine_t engine, const char* channel, int elapsed_ms, bool rejoin)
{
    volc_msg_t msg = {0};
    LOGI("join channel success %s elapsed %d ms\n", channel, elapsed_ms);
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);

    rtc->b_first_keyframe_received = false;
    rtc->b_channel_joined = true;

    msg.code = VOLC_MSG_CONNECTED;
    _send_message_2_user(rtc, &msg);
};

static void _on_user_joined(byte_rtc_engine_t engine, const char* channel, const char* user_name, int elapsed_ms)
{
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_msg_t msg = {0};
    LOGI("remote user joined %s:%s elapsed %d ms\n", channel, user_name, elapsed_ms);

    rtc->b_user_joined = true;
    rtc->p_remote_user_id = strdup(user_name);

    msg.code = VOLC_MSG_USER_JOINED;
    _send_message_2_user(rtc, &msg);
};

static void _on_user_offline(byte_rtc_engine_t engine, const char* channel, const char* user_name, int reason)
{
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_msg_t msg = {0};
    LOGI("remote user offline %s:%s reason %d\n", channel, user_name, reason);
    rtc->b_user_joined = false;

    msg.code = VOLC_MSG_USER_OFFLINE;
    _send_message_2_user(rtc, &msg);
};

static void _on_user_mute_audio(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted)
{
    LOGD("remote user mute audio  %s:%s %d\n", channel, user_name, muted);
};

static void _on_user_mute_video(byte_rtc_engine_t engine, const char* channel, const char* user_name, int muted)
{
    LOGD("remote user mute video  %s:%s %d\n", channel, user_name, muted);
};

static void _on_audio_data(byte_rtc_engine_t engine, const char* channel, const char* user_name, uint16_t sent_ts, audio_data_type_e data_type,
                           const void* data_ptr, size_t data_len, const uint8_t* extra_info, size_t extra_info_size)
{
    volc_data_info_t info = {0};
    rtc_impl_t* rtc = (rtc_impl_t*)byte_rtc_get_user_data(engine);

    if (NULL == rtc || !rtc->b_pipeline_started || !rtc->b_channel_joined || !rtc->b_user_joined) {
        LOGE("pipeline not started or channel not joined or user not joined");
        return;
    }
    info.type = VOLC_DATA_TYPE_AUDIO;
    info.info.audio.data_type = (volc_audio_data_type_e) data_type;
    // info.info.audio.sent_ts = sent_ts;
    __send_data_2_user(rtc, data_ptr, data_len, &info);
};

static void _on_video_data(byte_rtc_engine_t engine, const char* channel, const char* user_name, uint16_t sent_ts, video_data_type_e codec,
                           int is_key_frame, const void* data_ptr, size_t data_len)
{
    volc_data_info_t info = {0};
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);

    if (NULL == rtc || !rtc->b_pipeline_started || !rtc->b_channel_joined || !rtc->b_user_joined) {
        LOGD("pipeline not started or channel not joined or user not joined");
        return;
    }

    if (__is_first_keyframe_not_received(rtc, is_key_frame)) {
        LOGD("first keyframe not received, request key frame");
        byte_rtc_request_video_key_frame(rtc->rtc, channel, user_name);
        return;
    }

    rtc->b_first_keyframe_received = true;

    info.type = VOLC_DATA_TYPE_VIDEO;
    info.info.video.data_type = (volc_video_data_type_e) codec;
    __send_data_2_user(rtc, data_ptr, data_len, &info);
};

static void _on_channel_error(byte_rtc_engine_t engine, const char* channel, int code, const char* msg)
{
    volc_msg_t msg_data = {0};
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    LOGE("channel error %s:%d %s\n", channel, code, msg ? msg : "");
    msg_data.code = code;
    msg_data.data.msg = (char *)msg;
    _send_message_2_user(rtc, &msg_data);
};

static void _on_global_error(byte_rtc_engine_t engine, int code, const char* message)
{
    volc_msg_t msg_data = {0};
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    rtc->b_channel_joined = false;

    rtc->b_first_keyframe_received = false;
    LOGI("global error %d %s\n", code, message ? message : "");
    msg_data.code = VOLC_MSG_DISCONNECTED;
    _send_message_2_user(rtc, &msg_data);
};

static void _on_key_frame_gen_req(byte_rtc_engine_t engine, const char* channel, const char* user_name)
{
    LOGI("remote req key frame %s:%s\n", channel, user_name);
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_msg_t msg_data = {0};
    msg_data.code = VOLC_MSG_KEY_FRAME_REQ;
    _send_message_2_user(rtc, &msg_data);
};

static void _on_target_bitrate_changed(byte_rtc_engine_t engine, const char* channel, uint32_t target_bps)
{
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_msg_t msg_data = {0};

    LOGD("target bitrate changed %s %d bps\n", channel, (int)target_bps);
    // TODO: do not send it to user
    msg_data.code = VOLC_MSG_TARGET_BITRATE_CHANGED;
    msg_data.data.target_bitrate = target_bps;
    _send_message_2_user(rtc, &msg_data);
};

static void _on_token_privilege_will_expire(byte_rtc_engine_t engine, const char* token)
{
    LOGI("\ntoken privilege will expire %s", token ? token : "");
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_msg_t msg_data = {0};
    msg_data.code = VOLC_MSG_TOKEN_EXPIRED;
    // TODO: token?
    _send_message_2_user(rtc, &msg_data);
};

static void _on_message_received(byte_rtc_engine_t engine, const char* channel_name, const char* src, const uint8_t* message, int size, bool binary)
{

    int ret = 0;
    volc_msg_t msg = { 0 };
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    volc_data_info_t info = {0};
    info.type = VOLC_DATA_TYPE_MESSAGE;
    info.info.message.is_binary = binary;

    if (NULL == rtc || !rtc->b_pipeline_started || !rtc->b_channel_joined || !rtc->b_user_joined) {
        LOGE("pipeline not started or channel not joined or user not joined");
        return;
    }

    if (_is_target_message(message, MAGIC_CONV)) {
        ret = _on_conversion_status_message_parsed((uint8_t *)message + MAGIC_OFFSET,rtc);
        if(ret != -1){
            msg.code = VOLC_MSG_CONV_STATUS;
            msg.data.conv_status = ret;
            _send_message_2_user(rtc, &msg);
        }
        return;
    }
    __send_data_2_user(rtc, message, size, &info);
};

static void _on_message_send_result(byte_rtc_engine_t engine, const char* channel_name, int64_t msgid, int error, const char* extencontent)
{
    LOGD("----------------------->MessageSendResult msg id %" PRId64 ", error %d, extencontent %s \n", msgid, error, extencontent ? extencontent : "");
};

static void _on_license_will_expire(byte_rtc_engine_t engine, int daysleft)
{
    LOGI("----------------------->license will expire in %d days \n", daysleft);
    volc_msg_t msg_data = {0};
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    msg_data.code = VOLC_MSG_LICENSE_EXPIRED;
    _send_message_2_user(rtc, &msg_data);
};

static void _on_fini_notify(byte_rtc_engine_t engine)
{
    rtc_impl_t* rtc = (rtc_impl_t*) byte_rtc_get_user_data(engine);
    rtc->b_fini = true;
}

/*
    VOLC_AUDIO_CODEC_TYPE_UNKNOWN = 0,
    VOLC_AUDIO_CODEC_TYPE_OPUS = 1,
    VOLC_AUDIO_CODEC_TYPE_G722 = 2,
    VOLC_AUDIO_CODEC_TYPE_G711A = 3,
    VOLC_AUDIO_CODEC_TYPE_PCM   = 4,
    VOLC_AUDIO_CODEC_TYPE_G711U = 5,
*/
static int __volc_to_rtc_audio_codec(int volc_codec)
{
    switch (volc_codec) {
        case VOLC_AUDIO_CODEC_TYPE_OPUS:
            return AUDIO_CODEC_TYPE_OPUS;
        case VOLC_AUDIO_CODEC_TYPE_G722:
            return AUDIO_CODEC_TYPE_G722;
        case VOLC_AUDIO_CODEC_TYPE_G711A:
            return AUDIO_CODEC_TYPE_G711A;
        case VOLC_AUDIO_CODEC_TYPE_G711U:
            return AUDIO_CODEC_TYPE_G711U;
        default:
            LOGW("volc audio codec %d not supported, use opus instead", volc_codec);
            return AUDIO_CODEC_TYPE_OPUS;
    }
}

static int __rtc_init(rtc_impl_t* engine, cJSON* p_config)
{
    int ret = 0;
    int video_codec = 0;
    int log_level = 0;
    int params_cnt = 0;
    cJSON* p_params = NULL;
    byte_rtc_event_handler_t rtc_event_handler = {.on_global_error = _on_global_error,
                                                  .on_join_room_success = _on_join_channel_success,
                                                  .on_room_error = _on_channel_error,
                                                  .on_user_joined = _on_user_joined,
                                                  .on_user_offline = _on_user_offline,
                                                  .on_user_mute_audio = _on_user_mute_audio,
                                                  .on_user_mute_video = _on_user_mute_video,
                                                  .on_audio_data = _on_audio_data,
                                                  .on_video_data = _on_video_data,
                                                  .on_key_frame_gen_req = _on_key_frame_gen_req,
                                                  .on_target_bitrate_changed = _on_target_bitrate_changed,
                                                  .on_token_privilege_will_expire = _on_token_privilege_will_expire,
                                                  .on_message_received = _on_message_received,
                                                  .on_message_send_result = _on_message_send_result,
                                                  .on_license_expire_warning = _on_license_will_expire,
                                                  .on_fini_notify = _on_fini_notify};
    ret = volc_json_read_int(p_config, "audio.codec", &engine->audio_codec);
    if (ret != 0 || engine->audio_codec < 0 || engine->audio_codec > AUDIO_CODEC_TYPE_G711U) {
        LOGE("volc_rtc_create: read audio_codec failed");
        return -1;
    }
    ret = volc_json_read_int(p_config, "video.codec", &video_codec);
    if (ret != 0 || video_codec < 0 || video_codec > VIDEO_DATA_TYPE_MJPEG) {
        LOGE("volc_rtc_create: read video_codec failed");
        return -1;
    }
    ret = volc_json_read_int(p_config, "log_level", &log_level);
    if (ret != 0) {
       log_level = BYTE_RTC_LOG_LEVEL_WARN; // default log level
    }
    ret = volc_json_read_bool(p_config, "audio.publish", &engine->b_audio_publish);
    if (ret != 0) {
        engine->b_audio_publish = true; // default to publish audio
    }
    ret = volc_json_read_bool(p_config, "video.publish", &engine->b_video_publish);
    if (ret != 0) {
        engine->b_video_publish = false; // default to no publish video
    }
    ret = volc_json_read_bool(p_config, "audio.subscribe", &engine->b_audio_subscribe);
    if (ret != 0) {
        engine->b_audio_subscribe = true; // default to subscribe audio
    }
    ret = volc_json_read_bool(p_config, "video.subscribe", &engine->b_video_subscribe);
    if (ret != 0) {
        engine->b_video_subscribe = false; // default to no subscribe video
    }

    engine->rtc = byte_rtc_create(engine->p_appid, &rtc_event_handler);
    byte_rtc_set_user_data(engine->rtc, (void*) engine);
    byte_rtc_set_log_level(engine->rtc, log_level);
    ret = volc_json_read_object(p_config, "params", &p_params);
    if (0 == ret) {
        params_cnt = cJSON_GetArraySize(p_params);
        if (params_cnt > 0) {
            for (int i = 0; i < params_cnt; i++) {
                cJSON* p_param = cJSON_GetArrayItem(p_params, i);
                if (p_param && cJSON_IsString(p_param)) {
                    const char* param_str = cJSON_GetStringValue(p_param);
                    if (param_str && strlen(param_str) > 0) {
                        byte_rtc_set_params(engine->rtc, param_str);
                        LOGI("volc_rtc_create: set param[%d]: %s", i, param_str);
                    }
                }
            }
        }
        cJSON_Delete(p_params);
    }
    byte_rtc_init(engine->rtc);
    byte_rtc_set_audio_codec(engine->rtc, __volc_to_rtc_audio_codec(engine->audio_codec));
    byte_rtc_set_video_codec(engine->rtc, video_codec - 1); // -1 for default codec
    return 0;
}

volc_rtc_t volc_rtc_create(const char* appid, void* context, cJSON* p_config, volc_msg_cb message_callback, volc_data_cb data_callback)
{
    rtc_impl_t* rtc = (rtc_impl_t*) volc_osal_calloc(1, sizeof(rtc_impl_t));
    if (!rtc) {
        LOGE("volc_rtc_create: malloc rtc failed");
        return NULL;
    }
    rtc->message_callback = message_callback;
    rtc->data_callback = data_callback;
    rtc->context = context;
    rtc->p_appid = strdup(appid);
    if (NULL == rtc->p_appid) {
        LOGE("malloc appid memory failed");
        goto err_out_label;
    }

    if (__rtc_init(rtc, p_config) != 0) {
        LOGE("volc_rtc_create: rtc init failed");
        goto err_out_label;
    }

    LOGD("rtc create success");
    return (volc_rtc_t)rtc;
err_out_label:
    HAL_SAFE_FREE(rtc->p_appid);
    HAL_SAFE_FREE(rtc);
    return NULL;
}

void volc_rtc_destroy(volc_rtc_t handle)
{
    rtc_impl_t* rtc = (rtc_impl_t*)handle;
    if (!rtc) {
        LOGE("rtc instance is NULL");
        return;
    }
    __rtc_stop(rtc);

    byte_rtc_fini(rtc->rtc);
    while (!rtc->b_fini) {
        usleep(1000 * 10);
    }
    byte_rtc_destroy(rtc->rtc);
    HAL_SAFE_FREE(rtc->p_channel_name);
    HAL_SAFE_FREE(rtc->p_remote_user_id);
    HAL_SAFE_FREE(rtc->p_token);
    HAL_SAFE_FREE(rtc);
    LOGD("rtc destroy success");
}

int volc_rtc_start(volc_rtc_t rtc, const char* bot_id, volc_iot_info_t* iot_info, const char* params) {
    int ret = 0;
    volc_rtc_option_t *opt = NULL;
    rtc_impl_t* rtc_impl = (rtc_impl_t*) rtc;
    if (!rtc_impl) {
        LOGE("rtc instance is NULL");
        return -1;
    }
    char* task_id = "test";
    if (volc_get_rtc_config(iot_info, __volc_to_rtc_audio_codec(rtc_impl->audio_codec), bot_id, task_id, &rtc_impl->info, params)) {
        LOGE("get rtc config failed");
        return -1;
    }
    opt = &rtc_impl->info.rtc_opt;
    return __rtc_start(rtc_impl, opt);
}

int volc_rtc_stop(volc_rtc_t handle) {
    rtc_impl_t* rtc = (rtc_impl_t *)handle;
    if (!rtc) {
        LOGE("rtc instance is NULL");
        return -1;
    }
    __rtc_stop(rtc);
    return 0;
}

int volc_rtc_send(volc_rtc_t handle, const void* data, int size, volc_data_info_t* data_info) {
    rtc_impl_t* rtc = (rtc_impl_t *)handle;
    audio_frame_info_t audio_info = {0};
    video_frame_info_t video_info = {0};
    if (NULL == rtc || NULL == data || NULL == data_info) {
        LOGE("input args is invalid, rtc(%p), data(%p), data_info(%p)", rtc, data, data_info);
        return -1;
    }
    switch (data_info->type) {
        case VOLC_DATA_TYPE_AUDIO: {
            audio_info.data_type = (audio_data_type_e) data_info->info.audio.data_type;
            byte_rtc_send_audio_data(rtc->rtc, rtc->p_channel_name, data, size, &audio_info);
            break;
        }
        case VOLC_DATA_TYPE_VIDEO: {
            video_info.data_type = (video_data_type_e) data_info->info.video.data_type;
            video_info.stream_type = VIDEO_STREAM_HIGH;
            video_info.frame_type = (video_info.data_type == VIDEO_DATA_TYPE_MJPEG) ? VIDEO_FRAME_KEY : VIDEO_FRAME_AUTO_DETECT;
            LOGD("Sending video to channel: %s, data length: %d, data type: %d", rtc->p_channel_name, size, video_info.data_type);
            byte_rtc_send_video_data(rtc->rtc, rtc->p_channel_name, data, size, &video_info);
            break;
        }
        case VOLC_DATA_TYPE_MESSAGE: {
            LOGD("Sending message to channel: %s, remote user: %s, data length: %d, is_binary: %d",
                 rtc->p_channel_name, rtc->p_remote_user_id, size, data_info->info.message.is_binary);
            byte_rtc_rts_send_message(rtc->rtc, rtc->p_channel_name, rtc->p_remote_user_id, data, size, data_info->info.message.is_binary,
                                      RTS_MESSAGE_RELIABLE);
            break;
        }
        default:
            LOGW("unsupported data type: %d", data_info->type);
            return -1;
    }
    return 0;
}

int volc_rtc_interrupt(volc_rtc_t rtc) {
    int ret = 0;
    uint8_t* msg_ctrl = NULL;
    size_t msg_len = 0;
    rtc_impl_t* rtc_impl = (rtc_impl_t*) rtc;
    volc_data_info_t data_info = {0};
    if (!rtc_impl) {
        LOGE("rtc instance is NULL");
        return -1;
    }
    if (_build_binary_message(MAGIC_CONTROL, interrupt_str, &msg_ctrl, &msg_len) != 0) {
        LOGE("build control message failed");
        return -1;
    }
    data_info.type = VOLC_DATA_TYPE_MESSAGE;
    data_info.info.message.is_binary = true;
    if ((ret = volc_rtc_send(rtc, msg_ctrl, msg_len, &data_info)) != 0) {
        LOGE("send interrupt message failed");
    }
    HAL_SAFE_FREE(msg_ctrl);
    return ret;
}

int volc_rtc_send_text_to_agent(volc_rtc_t rtc, const char* text, volc_agent_type_e type, int interrupt_mode){
    int ret = 0;
    cJSON * pJsn =  cJSON_CreateObject();
    uint8_t* msg_ctrl = NULL;
    size_t msg_len = 0;
    rtc_impl_t* rtc_impl = (rtc_impl_t*) rtc;
    volc_data_info_t data_info = {0};
    if (!rtc_impl) {
        LOGE("rtc instance is NULL");
        return -1;
    }
    if(type == VOLC_AGENT_TYPE_TTS){
        cJSON_AddStringToObject(pJsn,"Command","ExternalTextToSpeech");
    }  else if(type == VOLC_AGENT_TYPE_LLM){
        cJSON_AddStringToObject(pJsn,"Command","ExternalTextToLLM");
    }
    cJSON_AddStringToObject(pJsn,"Message",text);
    cJSON_AddNumberToObject(pJsn,"InterruptMode",interrupt_mode);
    char* text_str =  cJSON_Print(pJsn);
    if (_build_binary_message(MAGIC_CONTROL, text_str, &msg_ctrl, &msg_len) != 0) {
        LOGE("build control message failed");
        return -1;
    }
    data_info.type = VOLC_DATA_TYPE_MESSAGE;
    data_info.info.message.is_binary = true;
    if ((ret = volc_rtc_send(rtc, msg_ctrl, msg_len, &data_info)) != 0) {
        LOGE("send interrupt message failed");
    }
    HAL_SAFE_FREE(msg_ctrl);
    cJSON_Delete(pJsn);
    return ret;
}

int volc_rtc_send_jpg(volc_rtc_t rtc, void* data, int size) {
    return 0;
}
