// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: MIT

#ifndef __BYTE_RTC_API_H__
#define __BYTE_RTC_API_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#if defined(__BUILDING_BYTE_RTC_SDK__)
#define __byte_rtc_api__ __declspec(dllexport)
#else
#define __byte_rtc_api__ __declspec(dllimport)
#endif
#else
#define __byte_rtc_api__ __attribute__((visibility("default")))
#endif

#define BYTE_RTC_API_VERSION "1.0.6"
#define BYTE_RTC_API_VERSION_NUM 0x1006


/**
 * @locale zh
 * @type errorcode
 * @brief 回调错误码。  <br>
 *        SDK 内部遇到不可恢复的错误时，会通过 `on_global_error` 或 `on_room_error` 回调通知用户。
 */

typedef enum {
    /**
     * @locale zh
     * @brief Token 无效。
     *        加入房间时使用的 Token 无效或过期失效。需要用户重新获取 Token，并调用 `byte_rtc_renew_token` 方法更新 Token。
     */
    ERR_INVALID_TOKEN = -1000,
    /**
     * @locale zh
     * @brief 加入房间错误。
     *        加入房间时发生未知错误导致加入房间失败。需要用户重新加入房间。
     */
    ERR_JOIN_ROOM = -1001,
    /**
     * @locale zh
     * @brief 没有发布音视频流权限。
     *        用户在所在房间中发布音视频流失败，失败原因为用户没有发布流的权限。
     */
    ERR_NO_PUBLISH_PERMISSION = -1002,
    /**
     * @locale zh
     * @brief 没有订阅音视频流权限。
     *        用户订阅所在房间中的音视频流失败，失败原因为用户没有订阅流的权限。
     */
    ERR_NO_SUBSCRIBE_PERMISSION = -1003,
    /**
     * @locale zh
     * @brief 相同用户 ID 的用户加入本房间，当前用户被踢出房间
     */
    ERR_NO_DUPLICATE_LOGIN = -1004,
    /**
     * @locale zh
     * @brief 服务端调用 OpenAPI 将当前用户踢出房间
     */
    ERR_KICKED_OUT = -1006,
    /**
     * @locale zh
     * @brief 当调用 `byte_rtc_join_room` ，如果room 非法，会返回null，并抛出该error
     */
    ERR_ROOMID_ILLEGAL = -1007,
    /**
     * @locale zh
     * @brief Token 过期。调用 `byte_rtc_join_room` 使用新的 Token 重新加入房间。
     */
    ERR_ROOM_TOKEN_EXPIRED = -1009,
    /**
     * @locale zh
     * @brief 调用 `updateToken` 传入的 Token 无效
     */
    ERR_UPDATE_TOKEN_WITH_INVALID_TOKEN = -1010,
    /**
     * @locale zh
     * @brief 服务端调用 OpenAPI 解散房间，所有用户被移出房间。
     */
    ERR_ROOM_DISMISS = -1011,
    /**
     * @locale zh
     * @brief 加入房间错误。 <br>
     *        加入房间时, LICENSE 计费账号未使用 LICENSE_AUTHENTICATE SDK，加入房间错误。
     */
    ERR_JOIN_ROOM_WITHOUT_LICENSE_AUTHENTICATE_SDK = -1012,
    /**
     * @locale zh
     * @brief 通话回路检测已经存在同样 room 的房间了
     */
    ERR_ROOM_ALREADY_EXIST = -1013,
    /**
     * @locale zh
     * @brief 加入多个房间时使用了不同的 uid。<br>
     *        同一个引擎实例实例中，用户需使用同一个 uid 加入不同的房间。
     */
    ERR_USERID_DIFFERENT = -1014,
    /**
     * @locale zh
     * @brief 服务端license过期，拒绝加入房间。 <br>
     */
    ERR_JOIN_ROOM_SERVER_LICENSE_EXPIRED = -1017,
    /**
     * @locale zh
     * @brief 超过服务端license许可的并发量上限，拒绝加入房间。 <br>
     */
    ERR_JOIN_ROOM_EXCEEDS_THE_UPPER_LIMIT = -1018,
    /**
     * @locale zh
     * @brief license参数错误，拒绝加入房间。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_PARAMETER_ERROR = -1019,
    /**
     * @locale zh
     * @brief license 证书路径错误。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_FILE_PATH = -1020,
    /**
     * @locale zh
     * @brief license 证书不合法。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_ILLEGAL = -1021,
    /**
     * @locale zh
     * @brief license 证书已经过期，拒绝加入房间。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_EXPIRED = -1022,
    /**
     * @locale zh
     * @brief license 证书内容不匹配。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_INFORMATION_NOT_MATCH = -1023,
    /**
     * @locale zh
     * @brief license 当前证书与缓存证书不匹配。 <br>
     */
    ERR_JOIN_ROOM_LICENSE_NOT_MATCH_WITH_CACHE = -1024,
    /**
     * @locale zh
     * @brief 房间被封禁。 <br>
     */
    ERR_JOIN_ROOM_FORBIDDEN = -1025,
    /**
     * @locale zh
     * @brief 用户被封禁。 <br>
     */
    ERR_JOIN_ROOM_USER_FORBIDDEN = -1026,
    /**
     * @locale zh
     * @brief 订阅音视频流失败，订阅音视频流总数超过上限。
     */
    ERR_OVER_STREAM_SUBSCRIBE_LIMIT = -1070,
    /**
     * @locale zh
     * @brief 发布流失败，发布流总数超过上限。
     *        RTC 系统会限制单个房间内发布的总流数，总流数包括视频流、音频流和屏幕流。如果房间内发布流数已达上限时，本地用户再向房间中发布流时会失败，同时会收到此错误通知。
     */
    ERR_OVER_STREAM_PUBLISH_LIMIT = -1080,
    /**
     * @locale zh
     * @brief 服务端异常状态导致退出房间。  <br>
     *        SDK与信令服务器断开，并不再自动重连，可联系技术支持。  <br>
     */
    ERR_ABNORMAL_SERVER_STATUS = -1084,

} error_code_e;

/** Error code. */

/**
 * @locale zh
 * @type keytype
 * @brief 用户离线原因。
 */
typedef enum {
    /**
     * @locale zh
     * @brief 用户主动离线
     */
    USER_OFFLINE_QUIT = 0,
    /**
     * @locale zh
     * @brief 用户超时掉线
     */
    USER_OFFLINE_DROPPED = 1,

} user_offline_reason_e;

/**
 * @locale zh
 * @type keytype
 * @brief 视频数据类型。
 */
typedef enum {
    /**
     * @locale zh
     
     * @brief 未知视频数据类型
     */
     VIDEO_DATA_TYPE_UNKNOWN = 0,

    /**
     * @locale zh
     
     * @brief H264
     */
    VIDEO_DATA_TYPE_H264    = 1,
    /**
     * @locale zh
     * @brief BYTEVC1
     */
    VIDEO_DATA_TYPE_BYTEVC1 = 2,


} video_data_type_e;

/**
 * @locale zh
 * @type keytype
 * @brief 视频帧类型。
 */
typedef enum {
    /**
     * @locale zh
     * @brief 未知类型   <br>
     *        如果设置为 `VIDEO_FRAME_AUTO_DETECT`，SDK 会自行判断视频帧类型。
     */
    VIDEO_FRAME_AUTO_DETECT = 0,
    /**
     * @locale zh
     * @brief 关键帧
     */
    VIDEO_FRAME_KEY = 1,
    /**
     * @locale zh
     * @brief P 帧
     */
    VIDEO_FRAME_DELTA = 2,
} video_frame_type_e;


/**
 * @locale zh
 * @type keytype
 * @brief 视频流类型。
 */
typedef enum {
    /**
     * @locale zh
     * @brief 主流
     */
    VIDEO_STREAM_HIGH = 0,
    /**
     * @locale zh
     * @brief 辅流
     */
    VIDEO_STREAM_LOW = 1,

} video_stream_type_e;

/**
 * @locale zh
 * @type keytype
 * @brief 视频帧信息。
 */
typedef struct {
    /**
     * @locale zh
     * @brief 视频数据类型，参看 video_data_type_e{@link #video_data_type_e}。
     */
    video_data_type_e data_type;
    /**
     * @locale zh
     * @brief 视频流类型，参看 video_data_type_e{@link #video_data_type_e}。
     */
    video_stream_type_e stream_type;
    /**
     * @locale zh
     * @brief 视频帧类型，参看 video_frame_type_e{@link #video_frame_type_e}。
     */
    video_frame_type_e frame_type;
    /**
     * @locale zh
     * @brief 视频帧率
     */
    int frame_rate;
    
} video_frame_info_t;

/**
 * @locale zh
 * @type keytype
 * @brief 音频编码类型。
 */
typedef enum {
    /**
     * @locale zh
     * @brief OPUS
     */
    AUDIO_CODEC_TYPE_OPUS   = 1,
    /**
     * @locale zh
     * @brief G722
     */
    AUDIO_CODEC_TYPE_G722   = 2,
    /**
    * @locale zh
    * @brief AACLC
    */
    AUDIO_CODEC_TYPE_AACLC  = 3,

    /**
     * @locale zh
     * @brief G711A
     */
    AUDIO_CODEC_TYPE_G711A  = 4,

    /**
     * @locale zh
     * @brief G711U
     */
    AUDIO_CODEC_TYPE_G711U  = 5,
    
} audio_codec_type_e;


/**
 * @locale zh
 * @type keytype
 * @brief 视频编码类型。
 */
/**
 * @locale en
 * @type keytype
 * @brief video codec type list.
 */
typedef enum {
    /**
     * @locale zh
     * @brief 编码类型H264
     */
    /**
     * @locale en
     * @brief codec type H264
     */
    VIDEO_CODEC_TYPE_H264 = 0,
    
    /**
     * @locale zh
     * @brief 编码类型ByteVC1
     */
    /**
     * @locale en
     * @brief codec type ByteVC1
     */
    VIDEO_CODEC_TYPE_BYTEVC1 = 1,
    
} video_codec_type_e;

/**
 * @locale zh
 * @type keytype
 * @brief 音频数据类型。
 */
typedef enum {
    /**
     * @locale zh
     * @brief OPUS
     */
    AUDIO_DATA_TYPE_UNKNOWN = 0,
    /**
     * @locale zh
     * @brief OPUS
     */
    AUDIO_DATA_TYPE_OPUS    = 1,
    /**
     * @locale zh
     * @brief G722
     */
    AUDIO_DATA_TYPE_G722    = 2,
    /**
     * @locale zh
     * @brief AACLC
     */
    AUDIO_DATA_TYPE_AACLC   = 3,
   
    /**
     * @locale zh
     * @brief PCMA
     */
    AUDIO_DATA_TYPE_PCMA    = 4,

    /**
     * @locale zh
     * @brief PCM
     */
    AUDIO_DATA_TYPE_PCM = 5,

    /**
     * @locale zh
     * @brief PCMU
     */
    AUDIO_DATA_TYPE_PCMU = 6,
} audio_data_type_e;

/**
 * @locale zh
 * @type keytype
 * @brief 音频帧信息。
 */
typedef struct {
    /**
     * @locale zh
     * @brief 音频数据类型，参看 audio_data_type_e{@link #audio_data_type_e}。
     */
    audio_data_type_e data_type;

} audio_frame_info_t;

/**
 * @locale zh
 * @type keytype
 * @brief SDK 日志等级。
 */
typedef enum {
    /**
    * @locale zh
    * @brief 信息级别。
    */
    BYTE_RTC_LOG_LEVEL_INFO    = 1,
    /**
    * @locale zh
    * @brief 警告级别。
    */
    BYTE_RTC_LOG_LEVEL_WARN    = 2,
    /**
    * @locale zh
    * @brief 错误级别。
    */
    BYTE_RTC_LOG_LEVEL_ERROR   = 3,
    /**
    * @locale zh
    * @brief 严重错误级别。
    */
    BYTE_RTC_LOG_LEVEL_FATAL   = 4

} byte_rtc_log_level_e;

/**
 * @locale zh
 * @type keytype
 * @brief 房间音视频自动订阅选项。
 */
typedef struct {
    /**
     * @locale zh
     * @brief 是否自动订阅远端用户的音频流。<br>
     *        - true： 是
     *        - false：否   
     */
    bool auto_subscribe_audio;
    /**
     * @locale zh
     * @brief 是否自动订阅远端用户的视频流。<br>
     *        - true： 是
     *        - false：否        
     */
    bool auto_subscribe_video;

    /**
     * @locale zh
     * @brief 是否自动发布本端用户的音频流。<br>
     *        - true： 是
     *        - false：否   
     */

    bool auto_publish_audio;

    /**
     * @locale zh
     * @brief 是否自动发布本端用户的视频流。<br>
     *        - true： 是
     *        - false：否   
     */
    bool auto_publish_video;

} byte_rtc_room_options_t;

/**
 * @locale zh
 * @type keytype
 * @brief 网络事件类型。
 * @hidden
 */
typedef enum {
    NETWORK_EVENT_DOWN = 0,
    NETWORK_EVENT_UP,
    NETWORK_EVENT_CHANGE,
} network_event_type_e;

/**
 * @locale zh
 * @type keytype
 * @brief 实时信令消息类型。
 */
typedef enum {
    /**
     * @locale zh
     * @brief 可靠消息
     */
    RTS_MESSAGE_RELIABLE = 0,
    
} rts_message_type;

typedef void * byte_rtc_engine_t;


/**
 * @locale zh
 * @type callback
 * @brief SDK 事件回调结构体
 * @note 回调函数是在 SDK 内部线程同步抛出来的，请不要做耗时操作，否则可能导致 SDK 运行异常。
 */
typedef struct {

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 0
 * @brief SDK 错误信息回调 <br>
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param code 错误码，参看 error_code_e{@link #error_code_e}
 * @param msg 错误信息
 */
void (*on_global_error)(byte_rtc_engine_t engine, int code, const char* msg);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 2
 * @brief 加入房间成功回调。
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param elapsed_ms 从开始加入房间到加入房间成功的耗时，单位：毫秒
 * @param rejoin 当网络断开时，重连后自动触发重新加入房间
 *               - True：重新加入房间
 *               - False：首次加入房间
 */
void (*on_join_room_success)(byte_rtc_engine_t engine, const char* room, int elapsed_ms, bool rejoin);


/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 1
 * @brief 加入房间失败或异常退出房间回调
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param code 错误码，参看 error_code_e{@link #error_code_e}
 * @param msg 错误信息
 */
void (*on_room_error)(byte_rtc_engine_t engine, const char* room, int code, const char* msg);


/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 3
 * @brief 远端用户加入房间回调 <br>
          远端用户断网后重新连入房间时，房间内其他用户将收到该事件  <br>
          新加入房间用户也会收到加入房间前已在房间内的用户的入房间回调通知
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 * @param elapsed_ms 加入房间耗时（保留字段）
 */
void (*on_user_joined)(byte_rtc_engine_t engine, const char* room, const char* uid, int elapsed_ms);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 4
 * @brief 远端用户离开房间<br>
 *        房间内其他用户会收到此事件
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 * @param reason 用户离开房间的原因, 参看 user_offline_reason_e{@link #user_offline_reason_e}
 */
void (*on_user_offline)(byte_rtc_engine_t engine, const char* room, const char* uid, int reason);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 8
 * @brief 房间内用户暂停/恢复发送音频流时，房间内其他用户会收到此回调
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 * @param muted 发送状态 <br>
*         - true(1)：不发送 <br>
 *        - false(0)：发送
 */
void (*on_user_mute_audio)(byte_rtc_engine_t engine, const char* room, const char* uid, int muted);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 7
 * @brief 房间内用户暂停/恢复发送视频流时，房间内其他用户会收到此回调
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 * @param muted 发送状态 <br>
*         - true(1)：不发送 <br>
 *        - false(0)：发送
 */
void (*on_user_mute_video)(byte_rtc_engine_t engine, const char* room, const char* uid, int muted);

/**
 * @locale zh
 * @type callback 
 * @list 回调
 * @order 9
 * @brief 提示流发布端需重新生成关键帧的回调
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 */
void (*on_key_frame_gen_req)(byte_rtc_engine_t engine, const char* room, const char* uid);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 6
 * @brief 返回远端单个用户的音频数据
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 远端用户名
 * @param sent_ts 发送时间 （暂不支持）
 * @param data 音频数据类型，参看 audio_data_type_e{@link #audio_data_type_e}
 * @param data_ptr 音频数据
 * @param data_len 音频数据长度，单位字节
 */
void (*on_audio_data)(byte_rtc_engine_t engine, const char* room, const char* uid, uint16_t sent_ts,
    audio_data_type_e codec, const void * data_ptr, size_t data_len);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 5
 * @brief 返回远端单个用户的视频数据
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid  远端用户名
 * @param sent_ts 发送时间（暂不支持）
 * @param codec 视频编码类型，参看 video_data_type_e{@link #video_data_type_e}
 * @param is_key_frame 帧类型
 *                     - 0: 非关键帧
 *                     - !0: 关键帧
 * @param data_ptr 视频数据
 * @param data_len 视频数据长度，单位字节
 */
void (*on_video_data)(byte_rtc_engine_t engine, const char* room, const char* uid, uint16_t sent_ts,
                      video_data_type_e codec, int is_key_frame,
                      const void * data_ptr, size_t data_len);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 10
 * @brief 当带宽估计码率发生变化时，触发该回调。<br>
 *        此时你需要将编码器码率调至目标码率
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param target_bps 目标码率，单位 bps
 */
void (*on_target_bitrate_changed)(byte_rtc_engine_t engine, const char* room, uint32_t target_bps);


/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 12
 * @brief 返回远端用户发送的实时信令消息
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param src  远端用户名
 * @param message 实时信令消息
 * @param size 实时信令消息长度
 * @param binary 是否未二进制消息
 */

void (*on_message_received)(byte_rtc_engine_t engine, const char* room, const char* src, const uint8_t* message, int size, bool binary);


/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 11
 * @brief 实时信令消息发送结果通知
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param msgid  发送消息的id，用来和发送的消息匹配
 * @param error 发送消息错误码，0表示发送成功
 * @param extencontent 扩展信息，暂时未使用
 */
void (*on_message_send_result)(byte_rtc_engine_t engine, const char* room, int64_t msgid, int error, const char* extencontent);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 13
 * @brief Token 加入房间权限过期前 30 秒，触发该回调。<br>
 *        收到该回调后，你需调用 `byte_rtc_renew_token` 更新 Token 加入房间权限
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 */
void (*on_token_privilege_will_expire)(byte_rtc_engine_t engine, const char* room);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 14
 * @brief license 过期提醒。在剩余天数低于 30 天时，收到此回调
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param daysleft license 剩余有效天数
 */

void (*on_license_expire_warning)(byte_rtc_engine_t engine, int daysleft);

/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 15
 * @brief engine 实例清理(byte_rtc_fini)结束通知，只有收到该通知之后，重新创建实例(byte_rtc_init)才是安全的
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 */

void (*on_fini_notify)(byte_rtc_engine_t engine);


/**
 * @locale zh
 * @type callback
 * @list 回调
 * @order 16
 * @brief agic场景下使用license模式，当配额用尽时，触发该回调。当收到该回调时，请及时续费。当配额用尽的时候，智能体会自动离房，会话已不能继续进行，应用层应该调用反初始化接口，待完成充值之后再重新初始化。
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param message 配额即将用尽的提示信息
 * @param extra 扩展信息，暂未使用

 */
void (*on_quota_exceeded)(byte_rtc_engine_t engine, const char* message, void * extra);

} byte_rtc_event_handler_t;


/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 1
 * @brief 获取 SDK 版本号
 * @return SDK 版本号
 */
extern __byte_rtc_api__ const char* byte_rtc_get_version(void);

/**
 * @locale zh
 * @type api
 * @hidden
 * @brief 错误码转对应字符串
 * @note 不必释放此字符串
 * @param err 错误码
 * @return 错误信息
 */
extern __byte_rtc_api__ const char* byte_rtc_err_2_str(int err);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 7
 * @brief 设置 SDK 日志等级
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param level 日志等级，参看 byte_rtc_log_level_e{@link #byte_rtc_log_level_e}
 */
extern __byte_rtc_api__ void byte_rtc_set_log_level(byte_rtc_engine_t engine, int level);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 5
 * @brief 设置 SDK 日志文件路径、大小和数目
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param log_path 日志文件存放路径
 * @param size_per_file 单个日志文件大小，单位字节
 * @param max_file_count 日志文件最大个数
 * @return 方法调用结果：<br>
 *          - 0：成功
 *          - -1：失败，路径参数无效
 */
extern __byte_rtc_api__ int byte_rtc_config_log(byte_rtc_engine_t engine, const char* log_path, int size_per_file, int max_file_count);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 3
 * @brief 创建引擎实例,该方法是整个SDK调用的第一个方法
 * @param app_id 应用 ID
 * @param event_handler 回调方法，参看 byte_rtc_event_handler_t{@link #byte_rtc_event_handler_t}
 * @return 引擎实例
 */
extern __byte_rtc_api__ byte_rtc_engine_t byte_rtc_create(const char* app_id, const byte_rtc_event_handler_t * event_handler);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 13
 * @brief 初始化引擎实例
 * @note 仅能被初始化一次
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1: appid 或 event_handler 为空 <br>
 *         - -2：引擎实例已被初始化 <br>
 *         - -3：引擎实例创建失败，请检查是否有可用内存
 */
extern __byte_rtc_api__ int byte_rtc_init(byte_rtc_engine_t engine);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 35
 * @brief 销毁引擎实例,VolcEngineRTCLite内部完成销毁操作之后，通过回调（on_fini_notify）通知上层 参看 byte_rtc_event_handler_t{@link #byte_rtc_event_handler_t}
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @return 方法调用结果：
 *         - 0：成功 <br>
 *         - -1: 引擎实例不存在
 */
extern __byte_rtc_api__ int byte_rtc_fini(byte_rtc_engine_t engine);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 37
 * @brief 销毁引擎实例,只有在收到on_fini_notify的回调之后，调用此方法才是安全的
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 */
extern __byte_rtc_api__ void byte_rtc_destroy(byte_rtc_engine_t engine);


/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 9
 * @brief 将自定义的数据与引擎实例关联起来
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param user_data 设置用户自定义数据
 */
extern __byte_rtc_api__ void byte_rtc_set_user_data(byte_rtc_engine_t engine, void * user_data);


/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 11
 * @brief 获取与引擎实例相关联的自定义数据
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 */
extern __byte_rtc_api__ void * byte_rtc_get_user_data(byte_rtc_engine_t engine);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 15
 * @brief 设置音频编码格式
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param audio_codec_type 音频编码类型，参看 audio_codec_type_e{@link #audio_codec_type_e}
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1: 引擎实例不存在 <br>
 *         - -2：编码格式暂不被支持
 */
extern __byte_rtc_api__ int byte_rtc_set_audio_codec(byte_rtc_engine_t engine, audio_codec_type_e audio_codec_type);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 17
 * @brief 设置视频编码格式，暂仅支持 `VIDEO_CODEC_TYPE_H264`{@link #VIDEO_CODEC_TYPE_H264} 和 `VIDEO_CODEC_TYPE_BYTEVC1`{@link #VIDEO_CODEC_TYPE_BYTEVC1}
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例 
 * @param video_codec_type 视频编码类型，参看 video_codec_type{@link #video_codec_type}
 * @return 方法调用结果：<br>
 *         - 0：成功。 <br>
 *         - -1: 引擎不存在。 <br>
 *         - -2：编码格式暂不被支持。
 */
extern __byte_rtc_api__ int byte_rtc_set_video_codec(byte_rtc_engine_t engine, video_codec_type_e video_codec_type);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 19
 * @brief 加入房间
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 用户名
 * @param token 动态密钥，用于对加入房间用户进行鉴权验证  <br>
 * @param options 房间音视频自动订阅设置，参看 byte_rtc_room_options_t{@link #byte_rtc_room_options_t} <br>
 *                此版本无效，默认使用自动订阅
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空 <br>
 *         - -3：已加入过房间
 */
extern __byte_rtc_api__ int byte_rtc_join_room(byte_rtc_engine_t engine, const char* room, const char* uid,
                                                const char* token, byte_rtc_room_options_t* options);


/**
 * @locale zh
 * @type api
 * @order 33
 * @list 方法
 * @brief  退出房间
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_leave_room(byte_rtc_engine_t engine, const char* room);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 31
 * @brief 更新 Token <br>
 *        收到 on_token_privilege_will_expire{@link #byte_rtc_event_handler_t#on_token_privilege_will_expire} 时，必须重新获取 Token，调用此方法更新 Token，以保证通话的正常进行
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param token 动态密钥。用于对加入房间用户进行鉴权验证  <br>
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_renew_token(byte_rtc_engine_t engine, const char* room, const char* token);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 27
 * @brief 控制媒体流(本端 &远端)(音频 & 视频)流状态
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param uid 用户Id，非空控制的是远端的用户，空值控制的是本端用户
 * @param video 媒体类型，true:视频流，false:音频流
 * @param mute 接收状态
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_mute(byte_rtc_engine_t engine, const char* room, const char* uid, bool video, bool mute);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 29
 * @brief 请求远端用户重编关键帧
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param remote_uid 远端用户名
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_request_video_key_frame(byte_rtc_engine_t engine, const char* room, const char* remote_uid);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 21
 * @brief 发送音频帧
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param data_ptr 音频数据
 * @param data_len 数据长度，单位字节
 * @param info_ptr 音频帧信息，参看 audio_frame_info_t{@link #audio_frame_info_t}
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_send_audio_data(byte_rtc_engine_t engine, const char* room, const void * data_ptr, size_t data_len,
                                                   audio_frame_info_t * info_ptr);

/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 23
 * @brief 发送视频帧
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param data_ptr 视频数据
 * @param data_len 数据长度
 * @param info_ptr 视频帧信息，参看 video_frame_info_t{@link #video_frame_info_t}
 * @note - 仅支持 `VIDEO_DATA_TYPE_H264` <br>
 *       - 每个用户仅支持一路流，仅使用 `VIDEO_STREAM_HIGH`
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - -1：引擎实例不存在 <br>
 *         - -2：输入参数为空
 */
extern __byte_rtc_api__ int byte_rtc_send_video_data(byte_rtc_engine_t engine, const char* room, const void *data_ptr, size_t data_len,
                                                   video_frame_info_t * info_ptr);


/**
 * @locale zh
 * @type api
 * @list 方法
 * @order 25
 * @brief 发送实时信令消息
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param room 房间名
 * @param target 接收消息的目标用户，如果target传NULL表示发送的是房间内广播消息
 * @param data_ptr 实时信令消息数据
 * @param data_len 实时信令消息长度
 * @param binary 指定消息是否是二进制消息
 * @param type 用于指定实时信令消息类型，目前仅支持RTS_MESSAGE_RELIABLE

 * @return 方法调用结果：<br>
 *         - < 0：失败 <br>
 *         - >= 0：消息id<br>
 */

extern __byte_rtc_api__ int64_t byte_rtc_rts_send_message(byte_rtc_engine_t engine, const char* room, const char* target, const void * data_ptr, 
                                                    size_t data_len, bool binary, rts_message_type type);

/**
 * @locale zh
 * @type api
 * @hidden
 * @brief 设置 SDK 参数
 * @param engine 通过byte_rtc_create{@link #byte_rtc_create}创建的引擎实例
 * @param params json 格式参数
 * @return 方法调用结果：<br>
 *         - 0：成功 <br>
 *         - <0：失败
 */
extern __byte_rtc_api__ int byte_rtc_set_params(byte_rtc_engine_t engine, const char* params);

#ifdef __cplusplus
}
#endif
#endif /* __BYTE_RTC_API_H__ */
