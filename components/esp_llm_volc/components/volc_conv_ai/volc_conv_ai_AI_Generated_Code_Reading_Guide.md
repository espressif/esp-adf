# `volc_conv_ai` 代码梳理

> 说明：本文档由 AI 在阅读当前目录代码后生成，目标是帮助读者快速理解代码结构、模块职责与主要运行流程，作为代码导读使用。

## 1. 项目定位

这是一个面向嵌入式/端侧场景的会话式 AI SDK，目标是帮助设备快速与火山引擎云端智能体建立连接，并完成音频、视频、文本消息的双向交互。

从实现上看，它不是一个“纯算法库”，而是一个“连接层 + 协议封装层 + 平台适配层”的组合，核心职责包括：

- 解析用户传入的 JSON 配置。
- 使用 IoT 动态注册接口换取 `device_secret` 和 `RTCAppID`。
- 根据模式选择 RTC 或 WebSocket 传输通道。
- 向上层暴露统一的发送/控制 API。
- 将底层事件、会话状态、媒体数据统一回调给用户。

## 2. 对外 API 边界

需要特别注意：**对外暴露的 API 只有 `inc/volc_conv_ai.h` 中定义的内容**。

目录中的其他头文件，例如：

- `src/base/*.h`
- `src/transports/**/*.h`
- `src/util/*.h`
- `osal/inc/volc_osal.h`

都属于内部实现、传输层封装、工具函数或平台移植接口，不应被业务侧当作稳定公共接口直接依赖。

`inc/volc_conv_ai.h` 对外主要提供了以下能力：

- 基础错误码与版本号查询：
  - `volc_get_version()`
  - `volc_err_2_str()`
- 生命周期管理：
  - `volc_create()`
  - `volc_destroy()`
  - `volc_start()`
  - `volc_stop()`
- 数据发送：
  - `volc_send_audio_data()`
  - `volc_send_video_data()`
  - `volc_send_message()`
  - `volc_update()`，本质上是以二进制消息方式发送更新数据
- 控制能力：
  - `volc_send_text_to_agent()`
  - `volc_interrupt()`
- 回调模型：
  - `on_volc_event`
  - `on_volc_conversation_status`
  - `on_volc_audio_data`
  - `on_volc_video_data`
  - `on_volc_message_data`

也就是说，业务方应该只依赖一个抽象句柄 `volc_engine_t`，其余传输方式、鉴权、设备注册、消息魔术字等细节都被隐藏在内部实现中。

## 3. 代码总体结构

### 3.1 顶层目录

- `inc/`
  - 唯一公共头文件目录，定义 SDK 对外接口。
- `src/`
  - 主体实现目录。
- `src/base/`
  - 基础消息类型、设备注册与 RTC 建房配置逻辑。
- `src/transports/high_quality/`
  - 高质量链路，基于 RTC。
- `src/transports/low_load/`
  - 低负载链路，基于 WebSocket。
- `src/util/`
  - 鉴权、HTTP、Base64、JSON 读取、日志、链表等通用工具。
- `osal/`
  - 平台抽象层，屏蔽内存、线程、时间、UUID 等系统差异。
- `third_party/`
  - 证书、TLS、HTTP 客户端、zlib 等第三方依赖。
- `sample/`
  - 最小使用示例。
- `cmake/`
  - 各平台构建脚本及源码归类。

### 3.2 主入口文件

`src/volc_conv_ai.c` 是整个 SDK 的统一入口和调度中心，负责：

- 维护 `volc_engine_impl_t` 运行时对象。
- 解析配置并执行设备注册。
- 创建 RTC/WS 传输对象。
- 在 `start/stop/send/interrupt` 时按当前模式分发到底层传输实现。
- 将底层内部消息 `volc_msg_t` 转换成对外事件/会话状态回调。
- 将底层数据 `volc_data_info_t` 路由到对应用户回调。

## 4. 核心数据模型

### 4.1 对外数据类型

`inc/volc_conv_ai.h` 中定义了 SDK 面向业务的统一数据模型：

- 音频：
  - `volc_audio_data_type_e`
  - `volc_audio_codec_type_e`
  - `volc_audio_frame_info_t`
- 视频：
  - `volc_video_data_type_e`
  - `volc_video_codec_type_e`
  - `volc_video_frame_info_t`
- 文本/控制消息：
  - `volc_message_info_t`
- 通用数据路由包装：
  - `volc_data_info_t`
- 会话状态：
  - `VOLC_CONV_STATUS_LISTENING`
  - `VOLC_CONV_STATUS_THINKING`
  - `VOLC_CONV_STATUS_ANSWERING`
  - `VOLC_CONV_STATUS_INTERRUPTED`
  - `VOLC_CONV_STATUS_ANSWER_FINISH`
- 工作模式：
  - `VOLC_MODE_RTC`
  - `VOLC_MODE_WS`

### 4.2 内部消息模型

`src/base/volc_base.h` 定义了内部传输层与主入口之间的统一消息协议：

- `volc_msg_t`
  - 表示连接成功、断开、远端用户加入、token 过期、关键帧请求、码率变化、会话状态变化等内部事件。
- `volc_msg_cb`
  - 传输层向主入口上报事件。
- `volc_data_cb`
  - 传输层向主入口上报音视频/消息数据。

这层抽象的意义是：RTC 和 WS 两条链路虽然底层协议差异很大，但都能通过相同的内部回调接口接入 `src/volc_conv_ai.c`。

## 5. 生命周期主流程

### 5.1 `volc_create()`

`volc_create()` 是 SDK 初始化入口，主要步骤如下：

1. 为 `volc_engine_impl_t` 分配内存并保存用户回调。
2. 解析用户传入的 `config_json`。
3. 读取 `iot` 配置：
   - `instance_id`
   - `product_key`
   - `product_secret`
   - `device_name`
4. 调用 `volc_device_register()` 完成设备动态注册。
5. 从注册结果中得到：
   - `device_secret`
   - `rtc_app_id`
6. 如果配置中带有 `rtc` 节点，则创建 RTC 传输对象。
7. 如果编译启用了 WS，则创建 WS 传输对象。
8. 将引擎状态置为 `CREATED`。

这里有一个关键设计点：**设备动态注册在 `create` 阶段完成，而不是在 `start` 阶段完成**。因此 `create()` 成功后，引擎已经持有后续建链所需的基础鉴权信息。

### 5.2 `volc_start()`

`volc_start()` 根据 `volc_opt_t.mode` 选择具体传输通道：

- `VOLC_MODE_RTC`:
  - 调用 `volc_rtc_start()`
- `VOLC_MODE_WS`:
  - 调用 `volc_ws_start()`

同时要求：

- `handle` 非空
- `opt` 非空
- `bot_id` 有效
- 当前状态必须是 `CREATED` 或 `STOPPED`

### 5.3 `volc_stop()` / `volc_destroy()`

- `volc_stop()` 仅停止当前会话链路，保留引擎对象。
- `volc_destroy()` 释放传输层对象、IoT 信息和引擎对象。

`destroy()` 会根据当前 `mode` 分别销毁 RTC 或 WS 实例，因此 SDK 的运行时是“单引擎单活跃模式”模型，而不是双通道同时工作模型。

## 6. 设备注册与建链前鉴权

相关代码位于 `src/base/volc_device_manager.c`。

### 6.1 动态注册

`volc_device_register()` 的职责：

- 基于 `product_secret`、`product_key`、`device_name`、时间戳、随机数生成签名。
- 向 IoT 服务 `DynamicRegister` 接口发起 HTTP POST。
- 解析响应中的：
  - `Result.payload`
  - `Result.RTCAppID`
- 用 `volc_aes_decode()` 解密 `payload`，得到 `device_secret`。

这一步是整个 SDK 的前置条件。没有 `device_secret`，后续 RTC 拉取房间配置或 WS 握手都无法继续。

### 6.2 拉取 RTC 建房参数

`volc_get_rtc_config()` 会在 RTC 模式的 `start()` 阶段调用，向服务端请求：

- `RoomID`
- `UserID`
- `Token`
- `TaskID`

这些字段最终用于 RTC 进房。

### 6.3 鉴权与解密工具

`src/util/volc_auth.c` 提供两个核心能力：

- `volc_sha256_hmac()`
  - 用于请求签名。
- `volc_aes_decode()`
  - 用于解密动态注册返回的加密载荷。

配合 `src/util/volc_base64.c`，构成完整的签名与解密工具链。

## 7. RTC 模式实现

相关代码位于：

- `src/transports/high_quality/inc/volc_rtc.h`
- `src/transports/high_quality/src/volc_rtc.c`

RTC 模式是“高质量方案”，依赖火山引擎 RTC Lite SDK。

### 7.1 RTC 对象职责

`rtc_impl_t` 维护了 RTC 运行所需的关键状态：

- 是否已启动 pipeline
- 是否已进房
- 是否有远端用户加入
- 是否收到了第一帧关键帧
- 是否启用了音视频发布/订阅
- appid / channel / uid / token / remote uid
- 回调上下文
- RTC 引擎实例

### 7.2 RTC 初始化

`volc_rtc_create()` 会读取 `rtc` 配置中的：

- `audio.codec`
- `video.codec`
- `audio.publish`
- `audio.subscribe`
- `video.publish`
- `video.subscribe`
- `log_level`
- `params`

然后完成：

- 创建 `byte_rtc_engine_t`
- 注册底层事件回调
- 设置日志级别
- 设置扩展参数
- 设置音频/视频编解码器
- 调用 `byte_rtc_init()`

### 7.3 RTC 启动流程

`volc_rtc_start()` 的链路为：

1. 调用 `volc_get_rtc_config()` 获取房间配置。
2. 用返回的 `RoomID/UserID/Token` 调用 `byte_rtc_join_room()`。
3. 房间加入成功后，通过 `_on_join_channel_success()` 上报 `VOLC_MSG_CONNECTED`。

### 7.4 RTC 收发逻辑

- 接收音频：
  - `_on_audio_data()` 将 RTC 音频帧包装为 `VOLC_DATA_TYPE_AUDIO` 并回调给主入口。
- 接收视频：
  - `_on_video_data()` 会先处理首关键帧逻辑。
  - 若首帧不是关键帧，会主动请求关键帧。
- 接收消息：
  - `_on_message_received()` 区分控制/会话状态消息和普通消息。

### 7.5 RTC 控制消息

RTC 内部使用了带“魔术字 + 长度 + 内容”的二进制消息封装：

- `ctrl`
  - 控制命令通道。
- `conv`
  - 会话状态通道。

对应辅助函数是 `_build_binary_message()`。

`volc_interrupt()` 在 RTC 模式下最终会发送：

```json
{"Command":"interrupt"}
```

`volc_send_text_to_agent()` 在 RTC 模式下会发送控制消息：

- `ExternalTextToSpeech`
- `ExternalTextToLLM`

所以 RTC 模式不仅能发媒体数据，也支持通过 RTS 消息向云端智能体下发控制/文本命令。

## 8. WebSocket 模式实现

相关代码位于：

- `src/transports/low_load/inc/volc_ws.h`
- `src/transports/low_load/src/volc_ws.c`

WS 模式是“低负载方案”，整体更接近 Realtime API 风格。

### 8.1 WS 启动流程

`volc_ws_start()` 主要做了这些事：

1. 保存 `bot_id`。
2. 解析运行参数里的音频编码配置。
3. 生成带鉴权头的 WebSocket URL 和 Header。
4. 建立到 `wss://ai-gateway.vei.volces.com/v1/realtime` 的连接。
5. 某些音频格式下会等待连接建立后发送 `session.update`。

WS 握手使用的鉴权信息来自 `device_secret`，其签名生成逻辑与 RTC 前置注册链路一致。

### 8.2 WS 音频发送

`volc_send_audio_data()` 在 WS 模式下最终会进入 `__ws_send_audio()`，流程为：

1. 将原始音频做 Base64 编码。
2. 发送 `input_audio_buffer.append`。
3. 如果 `commit=true`，继续发送：
   - `input_audio_buffer.commit`
   - `response.create`

因此 WS 模式下，`commit` 的语义很重要，它不仅表示音频片段结束，还会触发一次云端响应生成。

### 8.3 WS 消息接收

`__ws_recv_data()` 会解析服务端返回的 JSON 事件，重点包括：

- `response.audio.delta`
  - Base64 解码后作为 PCM 音频回调给上层。
- `input_audio_buffer.speech_started`
  - 映射为 `LISTENING`
- `input_audio_buffer.speech_stopped`
  - 映射为 `THINKING`
- `response.done`
  - 根据状态映射为 `ANSWER_FINISH` 或 `INTERRUPTED`
- 其他事件
  - 作为普通消息数据透传给用户

### 8.4 WS 中断机制

WS 模式下中断命令发送的是：

```json
{"type": "response.cancel"}
```

如果中断发生在正在回答阶段，还会记录 `response_id`，把被取消响应后续迟到的音频/文本增量丢弃，避免业务层收到过期数据。

## 9. 主入口中的统一路由机制

`src/volc_conv_ai.c` 做了两层关键统一：

### 9.1 事件统一

内部消息 `volc_msg_t` 会被映射成对外的 `volc_event_t` 或会话状态回调：

- `VOLC_MSG_CONNECTED` -> `VOLC_EV_CONNECTED`
- `VOLC_MSG_DISCONNECTED` -> `VOLC_EV_DISCONNECTED`
- `VOLC_MSG_QUOTA_EXCEEDED` -> `VOLC_EV_QUOTA_EXCEEDED`
- `VOLC_MSG_CONV_STATUS` -> `on_volc_conversation_status`

### 9.2 数据统一

无论底层来自 RTC 还是 WS，最终都会通过 `volc_data_info_t` 被分发到：

- `on_volc_audio_data`
- `on_volc_video_data`
- `on_volc_message_data`

这保证了业务层只需要按“统一数据类型 + 回调”的方式处理，不需要关心底层通道差异。

## 10. 工具与基础支撑

### 10.1 JSON 工具

`src/util/volc_json.c` 提供了点路径读取能力，例如：

- `ResponseMetadata.Error.CodeN`
- `Result.payload`
- `audio.codec`

这使得配置解析和响应解析代码保持相对简洁。

### 10.2 HTTP 能力

`src/util/volc_http.c` 通过第三方 `webclient` 封装了一个最简单的 `volc_http_post()`，用于访问 IoT 接口。

### 10.3 OSAL

`osal/inc/volc_osal.h` 定义了统一的平台适配接口：

- 内存分配
- 互斥锁/信号量
- 时间获取
- UUID 获取
- 线程创建与休眠
- 平台信息获取
- 随机数填充

目前仓库中已经给出：

- `osal/src/linux/volc_osal.c`
- `osal/src/macos/volc_osal.c`
- `osal/src/espressif/volc_osal.c`

这说明 SDK 的移植思路是：**业务逻辑层不感知平台差异，平台差异统一下沉到 OSAL。**

## 11. 构建与编译组织

构建脚本的核心信息在 `cmake/common.cmake`、`cmake/linux.cmake`、`cmake/macos.cmake`。

### 11.1 公共源码集合

`cmake/common.cmake` 将源码分为几组：

- 公共源码：
  - `src/volc_conv_ai.c`
  - `src/base/volc_device_manager.c`
  - `src/util/*.c`
  - TLS / webclient
- 低负载源码：
  - `src/transports/low_load/src/volc_ws.c`
- 高质量源码：
  - `src/transports/high_quality/src/volc_rtc.c`
- 平台源码：
  - `osal/src/<platform>/volc_osal.c`

### 11.2 模式选择

通过编译选项控制启用的传输模式：

- `ENABLE_RTC_MODE`
- `ENABLE_WS_MODE`

Linux 配置中默认更偏向 RTC，macOS 脚本里两个模式默认都关闭，需要显式开启。

## 12. sample 的意义

`sample/sample.c` 展示了最小调用方式：

1. 拼接 `config_json`
2. 填写回调函数
3. `volc_create()`
4. `volc_start()`
5. 循环发送文本给智能体
6. `volc_stop()`
7. `volc_destroy()`

这个 sample 也说明了业务接入 SDK 的典型顺序就是：

`create -> start -> send/recv -> stop -> destroy`

## 13. 一句话总结

这个仓库的本质是一个“统一外部 API + 双传输通道实现 + 设备注册鉴权 + 平台抽象层”的端侧会话 AI SDK：

- 对外只暴露 `inc/volc_conv_ai.h`
- 对内由 `src/volc_conv_ai.c` 做统一调度
- RTC 模式负责高质量实时音视频/控制链路
- WS 模式负责低负载实时语音交互链路
- `src/base` 负责设备注册与 RTC 建房参数获取
- `src/util` 提供鉴权、HTTP、JSON、Base64 等基础工具
- `osal` 负责平台移植

如果后续继续阅读源码，建议优先按这条链路理解：

1. `inc/volc_conv_ai.h`
2. `src/volc_conv_ai.c`
3. `src/base/volc_device_manager.c`
4. `src/transports/high_quality/src/volc_rtc.c`
5. `src/transports/low_load/src/volc_ws.c`
6. `osal/inc/volc_osal.h`
