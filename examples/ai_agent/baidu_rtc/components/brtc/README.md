### BRTC.RTOS.AGENT.SDK.demo

    -  用于进行BRTC SDK开发的demo程序。  
       百度智能云实时音视频RTC：https://cloud.baidu.com/doc/RTC/index.html  
       sdkdemo中各文件说明：  
        ./src/brtc_esp32_chat_agent_demo.c   智能体语音互动的demo程序代码， 演示了开启对话及退出对话的全流程。
	./include/baidu_chat_agents_engine.h 是BaiduRTC AGENT SDK的头文件。
        ./libs/libbrtc.a 是BaiduRTC AGENT  SDK的库文件。 
        ./README.md    说明文档；

       brtc_esp32_chat_agent_demo.c 中主要函数说明如下：

        1. baidu_create_chat_agent_engine 创建智能体引擎

	2. baidu_chat_agent_engine_init 初始化参数和设置事件回调
            1). 设置事件回调。  
            2). 设置登录BRTC 房间需要的参数BDCloudDefaultRTCAppID。//需要客户申请 https://cloud.baidu.com/doc/RTC/index.html

        3. baidu_chat_agent_engine_call 开启智能体对话  

        4. baidu_chat_agent_engine_destroy 结束智能体对话和销毁 

#集成注意事项：//libbrtc.a是基于下面版本编译的
1. esp-adf版本：拉取的master最新分支，commit信息如下：
commit 1e839afb5a5166aad6028ac2796272f32454dced (HEAD -> master, origin/master, origin/HEAD)
Merge: 5770ffd7 a6f8c1ae
Author: Jason-mao <maojianxin@espressif.com>
Date:   Tue Jun 24 21:27:47 2025 +0800

2. esp-idf版本：支持v5.4.2，libbrtc.a默认是v5.4.2版本编译的。
//拉取idf版本的方法：git clone --recursive --branch v5.4.2 https://github.com/espressif/esp-idf.git

3. chataent.zip是基于 ESP32-S3-Korvo-2L v1板卡调试的工程，可以拿来直接编译运行。
//默认模式是视觉互动模式，如果需要切换到成语音互动模式，请将brtc_esp32_chat_agent_demo.c中的#define BRTC_DEMO_VIDEO 宏注释掉
 //components/baidurtc/include/baidu_chat_agents_engine.h BDCloudDefaultRTCAppID输入可用的appid即可。
 
4. 修改系统配置sdkconfig.defaults.esp32s3/sdkconfig   //建议你们使用工程中的chataent的配置文件替换你们自己的对应config文件。
//步骤如下：
//a.请将该文件中从CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM至文件末尾的修改复制到你们的对应文件中
//b.执行idf.py set-target esp32s3
//c. 执行idf.py build

###特别注意事项###
1.客户需要搭建自己的服务，可以在本地或者服务器上运行我们的服务端demo参考示例，具体详情请参考《BRTC实时交互智能体接入指引文档V1.4》中的rtos sdk章节

######版本更新须知#######
时间：20250904
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.9B03.20250904
功能：
1. 增加了关闭发送静音数据包的逻辑。
2. 优化了关闭静音包时频繁打断存在的吞音问题。
3. 优化了sdk长时间无数据突然有数据时爆破音问题。
4. 针对音频数据出现欠载或者过载添加了自适应缓冲策略。

时间：20250811
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.9B02
功能：
1. 优化了灵优客户反馈的弱网下ws容易断开问题。
2. 增加了灵优客户反馈的brtc没有订阅流的事件上报逻辑。
3. 增加了信令KCP连接功能，优化KCP冷启动延时。
4. 解决了rtos sdk偶现的“我来了”不播放的问题。
5. 解决了rtos sdk偶现的没有到5min中会自动退出的问题。
6. 解决了audio task偶现的栈溢出崩溃问题。
7. 针对Mic声音过小导致无asr问题，增加了音频录制的agc增强逻辑。
8. 增加了demo层默认切到线上环境的代码逻辑。
//注意事项
a. esp-idf版本：支持v5.4.2 //拉取idf版本的方法：git clone --recursive --branch v5.4.2 https://github.com/espressif/esp-idf.git
b. esp-adf版本：拉取的master最新分支，commit信息如下：
commit 1e839afb5a5166aad6028ac2796272f32454dced (HEAD -> master, origin/master, origin/HEAD)
Merge: 5770ffd7 a6f8c1ae
Author: Jason-mao <maojianxin@espressif.com>
Date:   Tue Jun 24 21:27:47 2025 +0800

时间：20250721
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B10
功能：
1. 增加了rtos sdk视觉互动功能。
2. 优化了视频通话不流畅的性能问题。
3. 增加了librtc.a同一个库支持视觉互动、语音互动、音视频通话功能。
4. 升级了idf5.4.2基线版本，rtos sdk库已支持idf5.3.1和idf5.4.2版本。
//注意事项
a. esp-idf版本：支持v5.3.1/v5.4.2 //拉取idf版本的方法：git clone --recursive --branch v5.4.2 https://github.com/espressif/esp-idf.git
b. esp-adf版本：拉取的master最新分支，commit信息如下：
commit 1e839afb5a5166aad6028ac2796272f32454dced (HEAD -> master, origin/master, origin/HEAD)
Merge: 5770ffd7 a6f8c1ae
Author: Jason-mao <maojianxin@espressif.com>
Date:   Tue Jun 24 21:27:47 2025 +0800


时间：20250708
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B09
功能：
1. 增加了实时音视频通话功能。
2. 增加了应用层视频JPEG采集编码/解码显示功能。
3. 增加了应用层demo回声消除改成了aec_stream接口。
4. 该版本的librtc.a既可以用于videocall，也可以用于chatagent。
//注意事项
esp-idf版本：v5.3.1
esp-adf版本：拉取的master最新分支，commit信息如下：
commit 1e839afb5a5166aad6028ac2796272f32454dced (HEAD -> master, origin/master, origin/HEAD)
Merge: 5770ffd7 a6f8c1ae
Author: Jason-mao <maojianxin@espressif.com>
Date:   Tue Jun 24 21:27:47 2025 +0800

时间：20250619
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B08
功能：
1. 优化了functioncallResult内部传参的逻辑。
2. 优化了demo层网络时onConnectionStateChangeCallback接口处理逻辑。
3. 优化了demo层超时退出和发送录制音频task的异常处理逻辑。
4. 增加了通过userId绑定声纹识别的功能。
5. 修复了升级IDF5.4.1过程中遇到的编译问题。
//注意事项
//BRTC.RTOS.SDK.ESP32S3.V3.0.8B08版本是基于IDF5.4.1编译的;ADF版本是最新master分支上，commit信息如下：4200c64d4a78b9e9e2297818733aff21c72079a1

时间：20250609
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B06
功能：
1. 修复了播放长文章或者长故事崩溃问题。
2. 解决了应用层收不到onFunctionCall回调的问题。
3. 新增了baidu_chat_agent_engine_send_text_to_TTS接口。 //示例：baidu_chat_agent_engine_send_text(g_engine, "我来了");该接口发送中文字符串时需要时UTF-8格式
4. 修改了baidu_chat_agent_engine_send_text传参的逻辑。   //示例：baidu_chat_agent_engine_send_text(g_engine, "你是谁");该接口发送中文字符串时需要时UTF-8格式
5. 新增了应用层onCallStateChangeCallback处理agent登录失败或者断网重连的参考示例。
//baidu_chat_agent_engine_send_text接口是向AI agent发送文本消息，baidu_chat_agent_engine_send_text_to_TTS接口是发送文本给TTS，直接进行播报。
//请将应用层baidu_chat_agent_engine_send_text(g_engine, "[TTS]:我来了")修改成baidu_chat_agent_engine_send_text_to_TTS(g_engine, "我来了")
//应用层baidu_chat_agent_engine_send_text(g_engine, "[TTS]:ByeBye,我退下了")修改成baidu_chat_agent_engine_send_text_to_TTS(g_engine, "ByeBye,我退下了")


时间：20250521
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B05
功能：
1. 增加了rtos sdk抗弱网功能。
2. 优化了底层播放的逻辑，提升了抗弱网能力。
3. 优化了rtos sdk多个task调度的逻辑。
4. 优化了系统配置sdkconfig.defaults.esp32s3  //步骤如下：
//a.请将该文件中从CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM至文件末尾的修改复制到你们的对应文件中;
//b.执行idf.py set-target esp32s3
//c. 执行idf.py build


时间：20250516
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B04.tmp20250516
功能：
1. 增加了G722 16k编解码功能
2. 增加了license鉴权的功能   //brtc_esp32_chat_agent_demo.c 中的license_key需要填写公司购买的license值
3. 优化了正常网络下播放声音不清晰的问题。
4. 特别注意事项：需要调整下面的值LWIP_UDP_RECVMBOX_SIZE=64  //步骤：1. idf.py memuconfig 2. 进入界面后，输入：/ 3. 然后输入LWIP_UDP_RECVMBOX_SIZE 调整为64


时间：20250321
版本：BRTC.RTOS.SDK.ESP32S3.V3.0.8B02
功能：
1. 增加了回声消除功能
2. 增加了启动时进入对话模式时“我来了”的通知
3. 增加了5分钟无声音输入时AI自动退出机制
4. 增加了用户讲话内容和AI讲话的内容回调给应用的功能


### BRTC.RTOS.VIDEOCALL.SDK.demo
    -  用于进行BRTC SDK开发的demo程序。  
       百度智能云实时音视频RTC：https://cloud.baidu.com/doc/RTC/index.html  
       sdkdemo中各文件说明：  
        ./src/brtc_esp32_demo.c   实时音视频通话的demo程序代码， 演示了视频通话的全流程。
        ./include/baidu_rtc_client.h 是BaiduRTC VIDEOCALL SDK的头文件。
        ./libs/libbrtc.a 是BaiduRTC VIDEOCALL SDK的库文件。 
        ./README.md    说明文档；

       brtc_esp32_demo.c中主要函数说明如下：  
        1. app_main  
            这个是程序主入口， 可以在UI界面中进行调用。  

        2. brtc_demo_init  
            这个是进行SDK的初始化, 设置appId/域名地址/音频/视频编解码格式。

        3. brtc_demo_login 进行视频通话  
            1). 设置登录BRTC 房间需要的参数userid，appid， roomname和token。  
            2). 调用brtc_login_room登录房间后自动进行视频通话。

        4. OnRtcMessage 为SDK消息处理函数  
            当收到消息RTC_ROOM_EVENT_ON_USER_LEAVING_ROOM表示对方挂断了， 那么本端也进行挂断。

        5. brtc_demo_audio  创建task进行发送音频数据  
            1).sdk库会对音视频数据进行g711音频编码。  
            2).调用brtc_send_audio周期性发送g711数据。

        6. brtc_demo_vsend_task  创建task进行发送视频数据  
            1).乐鑫平台会进行视频采集JPEG编码。  
            2).调用brtc_send_image周期性发送jpeg视频数据。

        7. OnAudioFrame 服务端发送给设备端的音频数据  
            数据类型是8k采样率，320字节每帧的PCM音频数据。

        8. OnVideoFrame 服务端发送给设备端的视频数据  
            数据类型是JPEG格式的数据。

        9. brtc_demo_exit sdk资源销毁释放接口 
            1). 调用brtc_demo_logout接口登出房间。  
            2). 调用brtc_destroy_client和brtc_sdk_deinit资源释放。

#集成注意事项：//libbrtc.a是基于下面版本编译的
1. esp-adf版本：拉取的master最新分支，commit信息如下：
commit 1e839afb5a5166aad6028ac2796272f32454dced (HEAD -> master, origin/master, origin/HEAD)
Merge: 5770ffd7 a6f8c1ae
Author: Jason-mao <maojianxin@espressif.com>
Date:   Tue Jun 24 21:27:47 2025 +0800

2. esp-idf版本：支持v5.4.2，libbrtc.a默认是v5.4.2版本编译的。

3. videocall.zip是基于 ESP32-S3-Korvo-2L v1板卡调试的工程，可以拿来直接编译运行。//brtc_esp32_demo.c中需要输入appId
////默认模式是音视频通话模式，如果需要切换到成语音通话模式，brtc_esp32_demo.c中的#define BRTC_DEMO_VIDEO 宏注释掉
