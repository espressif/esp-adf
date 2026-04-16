/***************************************************************************
 *
 * Copyright (c) 2022 Baidu.com, Inc. All Rights Reserved
 *
 * @author: brtc
 *
 ***************************************************************************/
#ifndef BAIDU_RTC_CLIENT_H_
#define BAIDU_RTC_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _BITS_STDINT_INTN_H
#ifndef _STDINT_H
#ifndef _STDINT_H_INCLUDED
typedef signed long long    int64_t;
#endif
#endif
#endif

/*
 * 消息类型定义
 **/
typedef enum  RtcMessageType {
    RTC_MESSAGE_ROOM_EVENT_LOGIN_OK                    = 100,
    RTC_MESSAGE_ROOM_EVENT_LOGIN_TIMEOUT               = 101,
    RTC_MESSAGE_ROOM_EVENT_LOGIN_ERROR                 = 102,
    RTC_MESSAGE_ROOM_EVENT_CONNECTION_LOST             = 103,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_COMING               = 104,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_LEAVING              = 105,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_RENDERING            = 106,
    RTC_MESSAGE_ROOM_EVENT_REMOTE_GONE                 = 107,
    RTC_MESSAGE_ROOM_EVENT_SERVER_ERROR                = 108,

    RTC_ROOM_EVENT_AVAILABLE_SEND_BITRATE              = 200,
    RTC_ROOM_EVENT_FORCE_KEY_FRAME                     = 201,

    RTC_ROOM_EVENT_ON_USER_JOINED_ROOM                 = 300,
    RTC_ROOM_EVENT_ON_USER_LEAVING_ROOM                = 301,
    RTC_ROOM_EVENT_ON_USER_MESSAGE                     = 302,
    RTC_ROOM_EVENT_ON_USER_ATTRIBUTE                   = 303,

    RTC_MESSAGE_STATE_STREAM_UP                        = 2000,
    RTC_MESSAGE_STATE_SENDING_MEDIA_OK                 = 2001,
    RTC_MESSAGE_STATE_SENDING_MEDIA_FAILED             = 2002,
    RTC_MESSAGE_STATE_STREAM_DOWN                      = 2003,
    RTC_STATE_STREAM_SLOW_LINK_NACKS                   = 2100,
}RtcMessageType;

typedef enum  RtcErrorCodeType {
    RTC_ERROR_CODE_USER_KICKOUT                        = 504,
    RTC_ERROR_CODE_ROOM_DISBANDED                      = 505
}RtcErrorCodeType;

typedef struct  RtcMessage {
    RtcMessageType msgType;
    union {
        int64_t feedId;
        int64_t streamId;
        int64_t errorCode;
    }data;
    const char* extra_info;
    int extra_value;
    void* obj;
}RtcMessage;

typedef enum RtcImageType {
    RTC_IMAGE_TYPE_NONE,
    RTC_IMAGE_TYPE_JPEG,
    RTC_IMAGE_TYPE_H263,
    RTC_IMAGE_TYPE_H264,
    RTC_IMAGE_TYPE_I420P,
    RTC_IMAGE_TYPE_RGB,
    RTC_IMAGE_TYPE_VP8,
}RtcImageType;

typedef void(*LogCallback)(char *log, unsigned int len);
typedef void(*IVideoFrameObserver)(int64_t feedid, const char *img, int len, RtcImageType imgtype, int w, int h);
typedef void(*IAudioFrameObserver)(int64_t feedid, const char *audio, int len, int samlplerate, int channels);
typedef void(*IDataFrameObserver)(int64_t feedid, const char *data, int len);
typedef void(*IRtcMessageListener)(RtcMessage* msg);

/*
 * 创建brtc client对象
 * @return 返回创建的实例对象
 * @discuss 返回NULL表示创建失败
 **/
void * brtc_create_client(void);
/*
 * 销毁brtc client对象
 * @param rtc_client 实例对象
 **/
void brtc_destroy_client(void* rtc_client);
/*
 * 登录房间
 * @param rtc_client 实例对象
 * @param room_name 房间名
 * @param user_id 用户ID， 正整数转化的字符串
 * @param display_name 用户名， 字符串
 * @param token 鉴权需要用的token字符串，详细计算方式参见在线文档
 * @return 返回1表示成功， 0表示失败
 * @discuss 登录的消息回调IRtcMessageListener
 **/
int  brtc_login_room(void* rtc_client, const char* room_name, const char* user_id, const char* display_name, const char* token);
/*
 * 登出房间
 * @param rtc_client 实例对象
 * @return 返回1表示成功， 0表示失败
 **/
int  brtc_logout_room(void* rtc_client);
/*
 * 设置BRTC服务器地址
 * @param rtc_client 实例对象
 * @param url BRTC服务器地址，默认值为: wss://rtc.exp.bcelive.com/janus
 **/
void brtc_set_server_url(void* rtc_client, const char* url);
/*
 * 设置AppID值
 * @param rtc_client 实例对象
 * @param app_id 百度云RTC申请的AppID
 **/
void brtc_set_appid(void* rtc_client, const char* app_id);
/*
 * 设置ssl证书
 * @param rtc_client 实例对象
 * @param cer_path ssl证书文件路径，或者是证书内容， BRTC信令服务器的根证书, 已经随SDK提供
 * @discuss 该函数在登录房间前调用，用于设置ssl证书
 **/
void brtc_set_cer(void* rtc_client, const char* cer_path);
/*
 * 注册消息监听函数
 * @param rtc_client 实例对象
 * @param msgListener, 监听SDK 的消息
 * @discuss 消息类型定义RtcMessageType
 **/
void brtc_register_message_listener(void* rtc_client, IRtcMessageListener msgListener);
/*
 * 发送消息给特定id用户
 * @param rtc_client 实例对象
 * @param msg, 发送的消息内容
 * @param id, 用户ID的字符串. 当id为"0"时， 表示广播消息
 **/
void brtc_send_message_to_user(void* rtc_client, const char* msg, const char* id);
/*
 * 开始推流
 * @param rtc_client 实例对象
 * @discuss 用户启动手动推流
 **/
void brtc_start_publish(void* rtc_client);
/*
 * 停止推流
 * @param rtc_client 实例对象
 * @discuss 用户停止手动推流
 **/
void brtc_stop_publish(void* rtc_client);
/*
 * 开始订阅流
 * @param rtc_client 实例对象
 * @param feed_id 要订阅流的ID号
 * @discuss 开始手动订阅流
 **/
void brtc_subscribe_streaming(void* rtc_client, const char * feed_id);
/*
 * 停止订阅流
 * @param rtc_client 实例对象
 * @param feed_id 已经订阅流的ID号
 * @discuss 停止订阅流
 **/
void brtc_stop_subscribe_streaming(void* rtc_client, const char * feed_id);
/*
 * 设置是否自动推流
 * @param rtc_client 实例对象
 * @param auto_publish, 设置1表示登录后自动推流，0表示需要调用API进行推流
 **/
void brtc_set_auto_publish(void* rtc_client, int auto_publish);
/*
 * 设置是否自动拉流
 * @param rtc_client 实例对象
 * @param auto_subscribe, 设置1表示登录后自动拉流，0表示不自动拉流
 **/
void brtc_set_auto_subscribe(void* rtc_client, int auto_subscribe);
/*
 * 设置房间视频编码器类型
 * @param rtc_client 实例对象
 * @param vc, 视频编码器, 可取值为: "h264", "vp8", "vp9", "h263", "av1", "h265", "h266"
 * @discuss 设置房间中使用的视频编码器, 默认值是"h264", 房间级别的配置, 房间中每个客户端的视频编码器必须一致.
 *          需要在login之前调用.
 **/
void brtc_set_videocodec(void* rtc_client, const char* vc);
/*
 * 设置房间音频编码器类型
 * @param rtc_client 实例对象
 * @param ac, 音频编码器, 可取值为: "opus", "pcmu", "pcma"
 * @discuss 设置房间中使用的音频编码器, 默认值是"pcmu", 房间级别的配置, 房间中每个客户端的音频编码器必须一致.
 *          需要在login之前调用.
 **/
void brtc_set_audiocodec(void* rtc_client, const char* ac);
/*
 * 设置候选IP，即UDP代理IP地址
 * @param rtc_client 实例对象
 * @param ip, 候选IP
 * @discuss 设置候选IP，即UDP代理IP地址, 用于UDP代理转发场景
 **/
void brtc_set_candidate_ip(void* rtc_client, const char *ip);
/*
 * 选择媒体服务器地址，即接收UDP数据的媒体服务器地址
 * @param rtc_client 实例对象
 * @param ip, 媒体服务器地址IP
 * @discuss 主动选择媒体服务器地址，即接收UDP数据的媒体服务器地址, 默认值由系统自动分配
 **/
void brtc_set_mediaserver_ip(void* rtc_client, const char *ip);
/*
 * 静默摄像头
 * @param rtc_client 实例对象
 * @param is_muted 是否要静默， 1表示静默，0表示打开
 * @discuss 静默摄像头
 **/
void brtc_mute_camera(void* rtc_client, int is_muted);
/*
 * 静默麦克风
 * @param rtc_client 实例对象
 * @param is_muted 是否要静默， 1表示静默，0表示打开
 * @discuss  静默麦克风
 **/
void brtc_mute_micphone(void* rtc_client, int is_muted);
/*
 * 设置音量
 * @param rtc_client 实例对象
 * @param volume, 设置音量 0 ~ 10
 * @discuss 设置音量
 **/
void brtc_set_volume(void* rtc_client, int volume);
/*
 * 设置直播推流地址
 * @param rtc_client 实例对象
 * @param url 推流地址rtmp://
 * @discuss 设置一个可以使用的直播推流地址
 **/
void brtc_set_livestreaming_url(void* rtc_client, const char *url);
/*
 * 发送音频数据
 * @param rtc_client 实例对象
 * @param data 数据地址
 * @param len 数据的长度
 * @discuss 发送音频数据, 数据类型PCM数据，需要指定输入音频的采样和通道参数
 **/
void brtc_send_audio(void* rtc_client, const void * data, int len);
/*
 * 发送图像数据
 * @param rtc_client 实例对象
 * @param data 数据地址
 * @param len 数据的长度
 * @discuss 发送图像数据, 数据类型可以是jpeg,h264,yuv等，需要先指定
 **/
void brtc_send_image(void* rtc_client, const void * data, int len);
/*
 * 发送DataChannel数据
 * @param rtc_client 实例对象
 * @param data 数据地址
 * @param len 数据的长度
 **/
void brtc_send_data(void* rtc_client, const void * data, int len);
/*
 * 注册音频回调
 * @param rtc_client 实例对象
 * @param afo 音频回调函数， 格式是PCM格式
 * @discuss 接收音频数据, 数据类型PCM数据
 **/
void brtc_register_audio_frame_observer(void* rtc_client, IAudioFrameObserver afo);
/*
 * 发送音频数据
 * @param rtc_client 实例对象
 * @param vfo 视频回调函数， 格式是H264 NAL
 * @discuss 接收视频数据
 **/
void brtc_register_video_frame_observer(void* rtc_client, IVideoFrameObserver vfo);
/*
 * 使能视频推流
 * @param rtc_client 实例对象
 * @param e 1表示使用视频推流，0表示不使用视频推流, 默认值为1
 **/
void brtc_set_usingvideo(void* rtc_client, int e);
/*
 * 使能音频推流
 * @param rtc_client 实例对象
 * @param e 1表示使用音频推流，0表示不使用音频推流, 默认值为1
 **/
void brtc_set_usingaudio(void* rtc_client, int e);
/*
 * 使能接收视频
 * @param rtc_client 实例对象
 * @param e 1表示接收视频，0表示不接收视频, 默认值为1
 **/
void brtc_set_receivingvideo(void* rtc_client, int e);
/*
 * 使能接收音频
 * @param rtc_client 实例对象
 * @param e 1表示接收音频，0表示不接收音频, 默认值为1
 **/
void brtc_set_receivingaudio(void* rtc_client, int e);
/*
 * 使能信令精简压缩，减少信令交互次数和信令大小
 * @param rtc_client 实例对象
 * @param e 1表示开启v4, 减少信令交互次数，0表示不开启v4, 默认值为0
 * @param s 1表示开启信令压缩, 减少信令交互大小，0表示不开启信令压缩, 默认值为0, 信令压缩只有在v4开启后才有效.
 * @param o 1表示关闭信令保活,避免弱网下由信令保活导致的断线重连，0表示开启信令保活, 默认值为0, 关闭信令保活只有在v4开启后才有效.
 **/
void brtc_set_v4(void* rtc_client, int e, int s, int o);
/*
 * 使能接收user事件
 * @param rtc_client 实例对象
 * @param e 1表示接收user事件，0表示不接收user事件, 默认值为1
 **/
void brtc_set_userevent(void* rtc_client, int e);

/*
 * SDK 资源初始化
 * @return 0表示初始化成功， 1表示已经初始化
 **/
int  brtc_sdk_init(void);
/*
 * SDK 资源销毁
 * @return 0表示调用成功， SDK异步执行资源销毁
 **/
int  brtc_sdk_deinit(void);
/*
 * SDK 延迟异步执行函数
 * @param func 函数地址
 * @param arg  异步函数的参数
 * @param time_out_ms  异步延迟的时间， 单位ms
 * @return 0表示调用成功
 **/
int  brtc_sdk_do_async(void (*func)(void *), void *arg, int time_out_ms);
/*
 * 使能SDK内部音视频模块
 * @param e 1表示使用内部音视频设备（仅部分平台有效），0表示不使用内部音视频设备，需要使用api输入、获取音视频
 * @discuss 需要在brtc_sdk_init前执行
 **/
void brtc_sdk_enable_inner_av_devices(int e);
/*
 * 使能SDK内部log日志
 * @param e 1表示开启log，0表示关闭log
 **/
void brtc_sdk_enable_log(int e);
/*
 * 使能SDK内部nack功能
 * @param e 1表示开启nack，0表示关闭nack
 **/
void brtc_sdk_enable_nack(int e);
/*
 * 设置SDK的网络netid，比如4G或wifi
 * @param id 不同的id表示切换到不同的网络
 **/
void brtc_sdk_set_netid(unsigned int id);
/*
 * 获取SDK的网络netid，比如4G或wifi
 * @return 不同的值表示不同的网络
 **/
unsigned int brtc_sdk_get_netid(void);
/*
 * 使能SDK内部neteq功能
 * @param e 1表示开启neteq，0表示关闭neteq
 **/
void brtc_sdk_enable_neteq(int e);
/*
 * 使能SDK内部视频本地预览功能
 * @param e 1表示开启视频本地预览，0表示关闭视频本地预览
 **/
void brtc_sdk_enable_video_local_preview(int e);
/*
 * 注册log函数
 * @param log 函数地址
 **/
void brtc_sdk_register_log_handler(void (*log)(int level, const char *msg));

/*
 * 获得设备媒体信息
 * @return 设备媒体信息字符串
 **/
const char * brtc_sdk_get_device_mediainfo(void);
/*
 * 获得SDK的版本号
 * @return 版本号字符串， 比如: "3.1.4"
 **/
const char * brtc_sdk_version(void);

#ifdef __cplusplus
}
#endif

#endif
