#ifndef BAIDU_CHAT_AGENTS_ENGINE_H_
#define BAIDU_CHAT_AGENTS_ENGINE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "baidu_rtc_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASR_Q_FIN_PREFIX "[Q]:"                              /** ASR结果 fin */
#define ASR_Q_MID_PREFIX "[Q]:[M]:"                          /** ASR结果 mid */
#define ASR_Q_FIN_APPEND_PREFIX "[Q]:[C]:"                   /** ASR结果 fin append 追加结果 */
#define ASR_Q_MID_APPEND_PREFIX "[Q]:[M]:[C]:"               /** ASR结果 mid append 追加结果 */

#define AGENT_ANSWER_PREFIX "[A]:"
#define AGENT_ANSWER_MIN_PREFIX "[A]:[M]:"
#define AGENT_BREAK_STR "[B]:"
#define INPUT_TEXT_TO_AI_AGENT "[T]:"
#define INPUT_TEXT_TO_TTS "[TTS]:"
#define AGENT_EVENT_PREFIX "[E]:"
#define AGENT_EVENT_AUDIO_PLAY "[E]:[PLAY_AUDIO]"                 /*播放语音功能*/
#define FUNCTION_CALL_PREFIX "[F]:"                          /** function call */
#define AGENT_EVENT_TTS_PLY_BEGIN "[E]:[TTS_BEGIN_SPEAKING]"
#define AGENT_EVENT_TTS_PLY_END "[E]:[TTS_END_SPEAKING]"
#define AGENT_EVENT_VOICE_COMING "[E]:[VOICE_COMING]"
#define AGENT_EVENT_VOICE_DISAPPEAR "[E]:[VOICE_DISAPPEAR]"
#define VISION_MODE_IMAGE 0     /**视觉理解图片模式 */
#define VISION_MODE_STREAM 1    /**视觉视频流模式 */
#define ENHANCE_QUERY_TYPE_NULL 0   /**取消增强Query */
#define ENHANCE_QUERY_TYPE_PRE 1    /**增强Query 用户query插入句首文本 */
#define ENHANCE_QUERY_TYPE_POST 2   /**增强Query 用户query插入句未文本 */
#define ENHANCE_QUERY_TYPE_BOTHWAY 3    /**增强Query 用户query双向插入句首、句未文本 */

#define AGENT_ASR_HEADER_LEN  4
#define AGENT_ASR_HEADER_LEN_APPEND  8
#define BDCloudDefaultServerDOMAIN      "rtc2.exp.bcelive.com"
#define BDCloudDefaultRTCRoom           "654321"

//#define BDCloudDefaultRTCAppID          "xxxx" //需要客户申请 https://cloud.baidu.com/doc/RTC/index.html
//#define RTC_SERVER_URL                  "ws://rtc.exp.bcelive.com:8186/janus"
#define RTC_SERVER_URL                  "udp://rtc-ss-udp.exp.bcelive.com:8202"

//需要客户自己搭建服务，服务端的参考代码
//https://cloud.baidu.com/doc/RTC/s/wm8y592lc下载userserver-and-web-demo.zip
//下面SERVER_HOST_ONLINE的地址为172.18.102.172:8936换成客户搭建服务的ip或者域名
//#define SERVER_HOST_ONLINE              "http://rtc-aiagent.baidubce.com/api/v1/aiagent"
#define SERVER_HOST_ONLINE                "http://172.30.209.212:8936/api/v1/aiagent"

typedef enum AIAudioStateType {
    STOPPED = 1,
    SPEAKING = 2
} AIAudioStateType;

typedef struct AgentEngineParams {
    char agent_platform_url[256];        // 登陆agent智能体中心的地址
    char config[256];                    // 配置文件路径
    char appid[64];                      // 应用ID
    char userId[256];                    //用户id，根据id可以指定人设
    char cer[256];                       // 默认证书路径
    char token[256];                     // 鉴权token
    char workflow[64];                   // 工作流类型，默认VoiceChat
    char llm[64];                        // 大模型类型
    char lang[16];                       // 语言类型，默认zh
    char cid[64];                        // 通讯id
    long long instance_id;               // 实例ID，服务器下发，当前默认即可
    int level_voice_interrupt;           // 本地音频打断能量值，默认80
    int AudioInChannel;                  // 音频输入通道数，默认1
    int AudioInFrequency;                // 音频输入采样率，默认16000
    char AudioIncodecType[16];           // 音频输入的格式，默认是pcm
    char AudioOutcodecType[16];          // 音频播放的格式，默认是g711u
    int VideoWidth;                      // 视频宽度
    int VideoHeight;                     // 视频高度
    bool verbose;                        // 是否开启详细日志，默认关闭
    bool enable_voice_interrupt;         // 是否开启本地音频打断，默认关闭
    bool enable_internal_device;         // true：内部采集数据模式，flase: 外部采集数据模式
    bool enable_local_agent;             // true：本地请求创建智能体，flase: 服务端请求创建智能体
    char remote_params[1024];            // 获取远端的服务的参数，替代用户自己解析，例如instance_id，token;
    char license_key[256];               // 客户需要向百度购买设备license获取对应的key
    bool enable_video;                   // 是否开启视频
    bool disable_send_mute_data;         // 服务端是否需要发送静音包，true：不发送静音包, false: 发送静音包
} AgentEngineParams;

typedef enum AGentCallState {
    AGENT_LOGIN_SUCCESS = 1,
    AGENT_LOGIN_OUT_SUCCESS = 2,
    AGENT_CALL_SUCCESS = 3,

    AGENT_LOGIN_FAIL = 401,               //智能体登陆失败，应用层结合业务先退出，再重新登陆
    AGENT_LOGIN_OUT_FAIL = 402,
    AGENT_CALL_FAIL = 403
} AGentCallState;

typedef enum AGentConnectState {
    AGENT_CONNECTION_STATE_DISCONNECTED = 1,  //网络连接断开了，应用层结合业务可进行重连操作
    AGENT_CONNECTION_STATE_CONNECTING,
    AGENT_CONNECTION_STATE_CONNECTED,
    AGENT_CONNECTION_STATE_RECONNECTING
} AGentConnectState;

/**
 * @brief AI agent event callback collection
 *
 */
typedef struct BaiduChatAgentEvent {
    void (*onError)(int errCode, const char* errMsg);
    void (*onCallStateChange)(AGentCallState state);
    void (*onConnectionStateChange)(AGentConnectState state);
    void (*onUserAsrSubtitle)(const char* text,  bool final);
    void (*onAIAgentSubtitle)(const char* text,  bool final);
	void (*onAIAgentSpeaking)(bool speaking);
    void (*onFunctionCall)(const char* id, const char* params);
    void (*onAudioPlayerOp)(const char* path, bool start);
    void (*onMediaSetup)(void);
    void (*onAudioData)(const uint8_t* data, size_t len);
    void (*onVideoData)(const uint8_t* data, size_t len, int width, int height);
    void (*onLicenseResult)(bool result);
    void (*onVisionImageRequest)(void);
    void (*onVisionImageAck)(const char* name);
} BaiduChatAgentEvent;


typedef struct BaiduChatAgentEngine BaiduChatAgentEngine;
/**
 * @brief 创建智能体引擎实例
 * @param events 事件回调
 * @return 智能体引擎指针
 *
 */
BaiduChatAgentEngine* baidu_create_chat_agent_engine(const BaiduChatAgentEvent* events);

/**
 * @brief 初始化 engine，内部会进行异步登录等操作
 * @param engine engine 实例指针
 * @param param  登录相关参数
 * @return 0 表示初始化成功，非 0 表示失败
 */
int baidu_chat_agent_engine_init(BaiduChatAgentEngine* engine, const AgentEngineParams* param);

/**
 * @brief 发起呼叫（上层在 init 后调用）
 * @param engine engine 实例指针
 */
void baidu_chat_agent_engine_call(BaiduChatAgentEngine* engine);

/**
 * @brief 中断呼叫（例如发送中断信号给对端）
 * @param engine engine 实例指针
 */
void baidu_chat_agent_engine_interrupt(BaiduChatAgentEngine* engine);

/**
 * @brief 向 AI agent 发送文本消息
 * @param engine engine 实例指针
 * @param text 文本消息字符串
 */
void baidu_chat_agent_engine_send_text(BaiduChatAgentEngine* engine, const char* text);

/**
 * @brief 发送文本给TTS，直接进行播报
 * @param engine engine 实例指针
 * @param text 文本消息字符串
 */
void baidu_chat_agent_engine_send_text_to_TTS(BaiduChatAgentEngine* engine, const char* text);

/**
 * @brief 更新视觉理解模式
 * @param engine engine 实例指针
 * @param mode 0:图片模式; 1:视频流模式
 */
void baidu_chat_agent_engine_update_visual_mode(BaiduChatAgentEngine *engine, const int mode);

/**
 * @brief 设置用户query增强，通过向用户原始query插入扩展描述
 * @param engine engine 实例指针
 * @param enhanceType 0:不插入; 1:query前插；2:query后插；3:前后双向插；
 * @param preQuery 前向插入文本；
 * @param postQuery 后置插入文本；
 * @note 如用户原始query为 “今晚吃什么”; 如预设了query增强（enhanceType：3 preQuery：我现在在成都 postQuery：用2个字回答）
 * 则大模型最终得到的query为： 这现在在成都，今晚吃什么，用3个字回答；则模型可能会返回： “火锅”
 */
void baidu_chat_agent_engine_set_enhance_query(BaiduChatAgentEngine *engine, const int enhanceType, const char *preQuery, const char *postQuery);

/**
 * @brief 向 AI agent 发送音频数据
 * @param engine engine 实例指针
 * @param data 音频数据
 * @param len  数据长度
 */
void baidu_chat_agent_engine_send_audio(BaiduChatAgentEngine* engine, const uint8_t* data, size_t len);

/**
 * @brief 向 AI agent 发送视频数据
 * @param engine engine 实例指针
 * @param data 视频数据
 * @param len  数据长度
 */
void baidu_chat_agent_engine_send_video(BaiduChatAgentEngine* engine, const uint8_t* data, size_t len);

/**
 * 发送functionCall 结果
 * @param id 唯一标识
 * @param result 结果 ，例如: {"result":"ok"}
 * */
void baidu_chat_agent_engine_send_functioncall_result(BaiduChatAgentEngine* engine, const char* id, const char* result);

/**
 * @brief 销毁 engine 实例并释放所有资源
 * @param engine engine 实例指针
 */
void baidu_chat_agent_engine_destroy(BaiduChatAgentEngine* engine);

#ifdef __cplusplus
}
#endif

#endif /* BAIDU_CHAT_AGENTS_ENGINE_H_ */
