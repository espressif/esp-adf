# ESP LLM Baidu

- [English](./README.md)

[esp_llm_service](../esp_llm_service) 的百度 RTC backend adapter。它封装百度 BRTC 预编译 SDK 并实现 `esp_llm_adapter.h` 契约，应用通过通用的 `esp_llm_service_*` API 和一个 typed create 函数即可驱动百度对话智能体或点对点对讲。

Adapter 只拥有 BRTC 引擎和房间会话。Capture 和 player 仍归应用所有；adapter 消费服务交给它的上行帧，并把下行音频和视频经服务下行通路发布回去。它是本系列中唯一能产生云端生成视频下行的 adapter。

## 功能特性

- 两种模式：agent 为 AI 对话智能体引擎，intercom 为基于房间的点对点通话
- 四种 agent 场景：语音对话、图像理解、图像生成和多模态
- Agent 音频档位：应用侧编解码并透传的 OPUS 16 kHz，或由百度 SDK 编成 G.722 的 PCM 16 kHz
- 视觉上行模式：云端请求时抓拍，或连续发帧，并可配置流过期时间
- RTC 视频编码可在 JPEG 和 H.264 之间选择，上下行通用
- 把智能体生成的图像经视频下行送到应用视频播放器
- 服务端语音打断，灵敏度等级可配置
- 通过通用 API 输入文本、取消回答，以及发送 backend 原生引擎命令
- 通过 `esp_llm_baidu_send_text_to_tts()` 直接朗读，不发起对话查询
- 通过 `esp_llm_baidu_send_function_call_result()` 回复 FunctionCall
- 通过 `esp_llm_baidu_set_enhance_query()` 为每次提问追加上下文
- Audio AI 功能挂载到应用 capture 上：AEC、NS、VAD 和 WakeNet
- 提供借用原始引擎和 RTC client 句柄的出口，用于未封装的 SDK 调用

## 目录结构

```
esp_llm_baidu/
├── include/
│   ├── esp_llm_baidu.h          Typed 配置、场景宏和公开 API
│   └── esp_llm_baidu_config.h   模式、场景和视觉配置结构
├── src/                         基于 BRTC SDK 的 adapter 实现
├── components/                  随附的 BRTC 预编译 SDK
├── examples/
│   ├── baidu_rtc_agent/         组件内 agent 示例
│   └── baidu_rtc_intercom/      组件内 intercom 示例
├── test_apps/                   离线的 create、流信息和控制面测试
├── Kconfig                      模式、场景、音频和视频档位选项
└── agent.md                     Adapter 用法、场景与事件指引
```

## 快速开始

产品例程是 [`adf_examples/ai_agent/esp_llm_baidu_agent_app`](../../adf_examples/ai_agent/esp_llm_baidu_agent_app)。它把媒体所有权留在应用侧，并演示了全部四种场景：

```bash
cd adf_examples/ai_agent/esp_llm_baidu_agent_app
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi 以及百度智能体凭据和场景
idf.py build flash monitor
```

应用的调用顺序是：Wi-Fi，然后做 SNTP，接着用 `cfg.service.streams[]` 创建并配置 capture 和 player，填写 `cfg.service.audio_ai_cfg.capture`，调用 `esp_llm_baidu_create()`，再依次 link、订阅事件，并按 player、LLM、capture 的顺序启动。

adapter 的最简用法如下：

```c
esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
cfg.agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
cfg.agent.app_id = CONFIG_BAIDU_APP_ID;
cfg.agent.user_id = CONFIG_BAIDU_USER_ID;
cfg.service.audio_ai_cfg.capture = capture;           /* 由应用持有 */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* 为零时关闭 audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_baidu_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/baidu_rtc_agent` 和 `examples/baidu_rtc_intercom` 是组件内的小型示例，不搭建完整应用媒体图即可分别验证两种模式。

## 模式与场景

`ESP_LLM_BAIDU_MODE` 选择引擎：

| 模式 | 说明 |
| --- | --- |
| `ESP_LLM_BAIDU_MODE_AGENT` | 百度对话智能体引擎，含 ASR、TTS 和视觉场景 |
| `ESP_LLM_BAIDU_MODE_INTERCOM` | 基于房间的点对点 RTC 对讲，支持音频和视频 |

agent 模式下，场景决定会话需要哪些媒体流。场景配置与 `esp_baidu_rtc_service` 保持一致：

| 场景 | 相机上行 | 视频下行 |
| --- | --- | --- |
| 语音对话 | 否 | 否 |
| 图像理解 | 是 | 否 |
| 图像生成 | 否 | 是 |
| 多模态 | 是 | 是 |

启用场景只是声明协议层的需求。设备侧仍由应用决定：在 `cfg.service.streams[]` 中打开对应条目，并创建相机和显示设备。场景本身不会创建任何媒体对象。

## 配置

音频编码、采样率和视频格式来自 `cfg.service.streams[]`，各场景的 DEFAULT 宏会预填。create 之后可用 `esp_llm_service_get_stream_info()` 读回这些格式。

组件 Kconfig 位于 `ESP LLM Baidu` 菜单下：

| 选项 | 默认值 |
| --- | --- |
| `ESP_LLM_BAIDU_MODE` | Agent；也可选 intercom |
| `ESP_LLM_BAIDU_AGENT_AUDIO_CODEC` | 应用侧编解码并透传的 OPUS；也可选由 SDK 编成 G.722 的 PCM |
| `ESP_LLM_BAIDU_AGENT_SCENE` | 语音对话 |
| `ESP_LLM_BAIDU_AGENT_VISION_MODE` | Image，即云端请求时抓拍；也可选 stream |
| `ESP_LLM_BAIDU_VISION_EXPIRE_SEC` | 10 秒 |
| `ESP_LLM_BAIDU_VIDEO_CODEC` | JPEG；也可选 H.264 |
| `ESP_LLM_BAIDU_VOICE_INTERRUPT_ENABLE` | 开启，`ESP_LLM_BAIDU_VOICE_INTERRUPT_LEVEL` 为 80 |
| `ESP_LLM_BAIDU_AGENT_AUTO_INTERRUPT_ON_WAKEUP` | 开启 |
| `ESP_LLM_BAIDU_AGENT_GATE_UPLINK_UNTIL_WAKEUP` | 关闭 |
| `ESP_LLM_BAIDU_INTERCOM_USING_VIDEO` | intercom 模式下开启 |
| `ESP_LLM_BAIDU_INTERCOM_RECEIVING_VIDEO` | intercom 模式下开启 |

## Backend 专用 API

大部分控制走通用的 `esp_llm_service_*` API。下列接口覆盖通用 API 无法表达的 BRTC 特性：

| API | 模式 | 用途 |
| --- | --- | --- |
| `esp_llm_baidu_send_text_to_tts()` | Agent | 朗读 UTF-8 文本，不发起对话查询 |
| `esp_llm_baidu_send_function_call_result()` | Agent | 回复 `FUNCTION_CALL` 事件 |
| `esp_llm_baidu_set_enhance_query()` | Agent | 为每次用户提问追加上下文 |
| `esp_llm_baidu_backend_acquire()` | 两者 | 借用原始引擎和 RTC client 句柄 |
| `esp_llm_baidu_backend_release()` | 两者 | 归还借用的句柄 |

`esp_llm_service_send_custom_data()` 在两种模式下都已实现：agent 把字符串作为引擎事件发送，使用 `[E]:` 或 `[SET]:` 命令，例如 `AGENT_EVENT_UPDATE_SYSTEM_PROMPT`；intercom 则把它广播到房间。

借出的句柄在头文件中是 `void *`，这样 `baidu_chat_agents_engine.h` 就不会把 `SPEAKING`、`Region` 这类名字泄露到每个应用中。请自行包含 SDK 头文件并做类型转换：

```c
esp_llm_baidu_backend_t backend;
if (esp_llm_baidu_backend_acquire(llm, &backend) == ESP_OK) {
    if (backend.mode == ESP_LLM_BAIDU_MODE_INTERCOM) {
        brtc_set_volume(backend.rtc_client, 8);
    }
    esp_llm_baidu_backend_release(llm);
}
```

## 注意事项

- 借用期间会持有引擎互斥锁并钉住 adapter 上下文。请尽量缩短借用时间、在同一线程内成对调用，期间不要调用其他 LLM API；漏掉 release 会让 destroy 死锁。
- 所有上行必须在 `esp_service_start()` 之前 link；对运行中的服务 link 会返回 `ESP_ERR_INVALID_STATE`。
- player 要在 LLM 之前启动，capture 要在其之后启动，AFE sidecar 才能挂载。停止顺序相反。
- 下行音频和生成图像都走服务下行通路，需要 link 一个视频播放器来接收。
- 场景只声明协议层需求。还要打开 `cfg.service.streams[]` 中对应的条目，否则能力掩码会把它们求交去掉。
- 相机 JPEG 宽高与 LCD 尺寸无关；采集尺寸必须匹配一个真实存在的 esp-video 传感器 mode。
- 许可证密钥和应用凭据属于机密。量产构建中请勿把它们留在提交的默认值里。
- Agent 音频对齐 `multimodal_assistant_app`：`AudioIncodecType` 为 `send_audio` 载荷（`opus` 或 `pcm`）；`rtc_ac` / `AudioOutcodecType` 为 `opus`（应用侧 OPUS，SDK 不转码）或 `g722`（应用侧 PCM，SDK 内做 G.722）。

## 性能

| 项目 | 取值 |
| --- | --- |
| Agent 音频 | 默认 OPUS 16 kHz 单声道透传；也可选由 SDK 编成 G.722 的 PCM 16 kHz |
| 视频 | 默认 JPEG，流宏给出 320x240 @ 3 fps；也可选 H.264 |
| 视觉抓拍过期 | `ESP_LLM_BAIDU_VISION_EXPIRE_SEC`，默认 10 秒 |
| 语音打断等级 | `ESP_LLM_BAIDU_VOICE_INTERRUPT_LEVEL`，默认 80（满分 100） |
| 传输 | 每台设备一个 BRTC 房间会话 |
| 应用分区 | 产品例程为 4 MB，另有 512 KB WakeNet 模型分区和 128 KB SPIFFS |

## SoC 兼容性

支持的芯片为 ESP32-S3、ESP32-S31 和 ESP32-P4。音频需要板级提供 `audio_adc` 和 `audio_dac`；视觉场景还需要 `camera` 和 `display_lcd`。需要 PSRAM。

`test_apps` 在不连 Wi-Fi、不加入百度房间的情况下覆盖 create、destroy、流信息和控制面错误。由于 `esp_llm_service` 仍会编译 `esp_audio_capture_service`，请先用与芯片匹配的开发板生成板级头文件：

| 芯片 | 开发板 |
| --- | --- |
| ESP32-S3 | `esp32_s3_korvo_2_3` |
| ESP32-S31 | `esp32_s31_korvo_1` |
| ESP32-P4 | `esp32_p4_function_ev_board` |

```bash
cd test_apps
idf.py bmgr -b esp32_s3_korvo_2_3   # 或 esp32_s31_korvo_1 / esp32_p4_function_ev_board
idf.py build
```

## FAQ

**这个 adapter 的 `esp_llm_service_get_capabilities()` 返回什么？**

adapter 描述符声明了一个涵盖音频、视频、文本和视觉的静态超集。实例值是该超集与 `cfg.service.streams[]` 中已启用流的交集，在 create 时一次性确定。因此 agent 与 intercom 的差异通过已启用的流和 typed 配置体现，而不是另一套掩码。应用侧的 link 不会改变它。

**选了视觉场景，但图像始终没有上云。**

场景只声明协议层需求，设备侧是另一回事。请在 `cfg.service.streams[]` 中启用 `ESP_LLM_MEDIA_STREAM_VIDEO_UP`，创建相机并完成 link。此外 image 模式下 adapter 还要等云端请求抓拍，在此之前发送的帧会被丢弃，而 `send_frame()` 仍返回 `ESP_OK`。

**本地 LCD 预览和 `VIDEO_DN` 有什么区别？**

预览是应用在本地绘制的第二路采集流，不会离开设备。`VIDEO_DN` 承载的是云端智能体生成的图像。多模态场景下两者显示在同一块屏幕上，但属于相互独立的通路。

**OPUS 和 PCM 音频档位如何对应到百度 SDK？**

`AudioIncodecType` 表示 `send_audio` 载荷。`rtc_ac` 和 `AudioOutcodecType` 表示线上编码。

OPUS 是默认档。应用负责上行编码和下行解码。百度库不对 OPUS 转码，因此 `send_audio` 和 `onAudioData` 携带 OPUS 帧，`AudioIn` / `rtc_ac` / `AudioOut` 为 `opus`。

PCM 表示应用收发 PCM。百度库在线上编解码 G.722，因此 `AudioIn` 为 `pcm`，`rtc_ac` / `AudioOut` 为 `g722`。

## 技术支持

如需技术支持，请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题报告和功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
