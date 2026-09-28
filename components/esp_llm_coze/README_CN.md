# ESP LLM Coze

- [English](./README.md)

[esp_llm_service](../esp_llm_service) 的 Coze backend adapter。它封装 [`esp_coze`](../esp_coze) 并实现 `esp_llm_adapter.h` 契约，应用通过通用的 `esp_llm_service_*` API 和一个 typed create 函数即可驱动 Coze 语音对话。

Adapter 只拥有 Coze WebSocket 会话。Capture 和 player 仍归应用所有；adapter 消费服务交给它的上行帧，并把 TTS 帧经服务下行通路发布回去。

## 功能特性

- 基于 WebSocket 的 Coze 流式对话，会话通过 Coze REST 端点创建
- 上下行音频编码：OPUS 16 kHz、PCM 16 kHz、G.711 A-law 8 kHz、G.711 mu-law 8 kHz
- 下行 Opus 码率和帧长可配置，默认 16000 bit/s 和 60 ms
- 服务端 VAD 静音检测支持 fast 与 normal 两种模式
- 通过 `esp_llm_service_send_text()` 输入文本，通过 `esp_llm_service_cancel_response()` 取消回答
- 通过 `esp_llm_service_send_custom_data()` 发送 backend 原生消息
- 字幕、服务端 VAD、错误和对话完成以 `esp_llm_service` 事件转发
- Audio AI 功能挂载到应用 capture 上：AEC、NS、VAD 和 WakeNet
- 唤醒词策略：唤醒前保持上行关闭，以及唤醒时自动打断
- 拒绝视频流；adapter 只声明音频和文本能力

## 目录结构

```
esp_llm_coze/
├── include/esp_llm_coze.h   Typed 配置、DEFAULT 宏和 esp_llm_coze_create()
├── src/                     基于 esp_coze 的 adapter 实现
├── examples/coze_chat/      组件内对话示例
├── Kconfig                  音频档位与唤醒词策略选项
└── agent.md                 Adapter 实现与评审指引
```

## 快速开始

产品例程是 [`adf_examples/ai_agent/esp_llm_coze_agent`](../../adf_examples/ai_agent/esp_llm_coze_agent)。它把媒体所有权留在应用侧，是接入本 adapter 的参考实现：

```bash
cd adf_examples/ai_agent/esp_llm_coze_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi 以及 Coze bot ID 和 access token
idf.py build flash monitor
```

应用的调用顺序是：Wi-Fi，然后用 `cfg.service.streams[]` 创建并配置 capture 和 player，接着填写 `cfg.service.audio_ai_cfg.capture`，调用 `esp_llm_coze_create()`，再依次 link、订阅事件，并按 player、LLM、capture 的顺序启动。

adapter 的最简用法如下：

```c
esp_llm_coze_cfg_t cfg = ESP_LLM_COZE_CFG_DEFAULT();
cfg.bot_id = CONFIG_COZE_BOT_ID;
cfg.access_token = CONFIG_COZE_ACCESS_TOKEN;
cfg.service.audio_ai_cfg.capture = capture;           /* 由应用持有 */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* 为零时关闭 audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_coze_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/coze_chat` 是组件内的小型示例，不搭建完整应用媒体图即可验证 adapter。

## 配置

`esp_llm_coze_create()` 要求 `bot_id` 和 `access_token`，任一为空时拒绝调用。`esp_llm_coze_cfg_t` 的其余字段由 `ESP_LLM_COZE_CFG_DEFAULT()` 填充：

| 字段 | 用途 |
| --- | --- |
| `bot_id` | 对话目标 bot，必填 |
| `access_token` | Coze access token，必填 |
| `ws_base_url` | WebSocket 端点；为 NULL 时使用默认值 |
| `conv_create_base_url` | 会话创建 REST 端点；为 NULL 时使用默认值 |
| `user_id` | 上报给 Coze 的用户身份；为 NULL 时使用 adapter 默认值 |
| `voice_id` | TTS 音色；为 NULL 时使用 bot 默认音色 |
| `service` | 共享的 `esp_llm_service_cfg_t`：流、audio AI、下行回调和名称 |

音频编码、采样率和码率来自 `cfg.service.streams[]`，DEFAULT 宏会按 Kconfig 音频档位预填。create 之后可用 `esp_llm_service_get_stream_info()` 读回这些格式。

组件 Kconfig 位于 `ESP LLM Coze` 菜单下：

| 选项 | 默认值 |
| --- | --- |
| `ESP_LLM_COZE_AUDIO_CODEC` | OPUS 16 kHz；也可选 PCM、G.711 A-law 和 G.711 mu-law |
| `ESP_LLM_COZE_DOWNLINK_BITRATE` | 16000 bit/s，编码为 OPUS 时生效 |
| `ESP_LLM_COZE_DOWNLINK_FRAME_SIZE_MS` | 60 ms |
| `ESP_LLM_COZE_VAD_MODE` | fast 静音检测 |
| `ESP_LLM_COZE_GATE_UPLINK_UNTIL_WAKEUP` | 关闭 |
| `ESP_LLM_COZE_AUTO_INTERRUPT_ON_WAKEUP` | 关闭 |

## 注意事项

- 不支持视频流。在 `cfg.service.streams[]` 中启用 `VIDEO_UP` 或 `VIDEO_DN` 会让 create 返回 `ESP_ERR_NOT_SUPPORTED`。
- 所有上行必须在 `esp_service_start()` 之前 link；对运行中的服务 link 会返回 `ESP_ERR_INVALID_STATE`。
- player 要在 LLM 之前启动，capture 要在其之后启动，AFE sidecar 才能挂载。停止顺序相反。
- WebSocket 连接成功会先后发布 `ESP_LLM_EVENT_CONNECTED` 和 `ESP_LLM_EVENT_MEDIA_READY`。收到 media-ready 之后才认为会话可用。
- 下行 TTS 走服务下行通路，因此必须把 player link 到 `AUDIO_DN` 才能听到声音。
- 向 `esp_llm_service_send_text()` 或 `send_custom_data()` 传空字符串会返回 `ESP_ERR_INVALID_ARG`。
- access token 属于凭据。量产构建中请勿把它留在提交的默认值里。

## 性能

| 项目 | 取值 |
| --- | --- |
| 上行音频 | 默认 OPUS 16 kHz 单声道；也可选 PCM 或 G.711 |
| 下行音频 | 与上行同编码，OPUS 时为 16000 bit/s、60 ms 帧 |
| 传输 | 每个会话一条 TLS WebSocket 连接 |
| TLS 缓冲 | 产品例程把 `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` 提高到 10240，并要求 PSRAM |
| 应用分区 | 产品例程为 6 MB |

## SoC 兼容性

在 ESP32-S3、ESP32-S31 和 ESP32-P4 上验证，开发板需提供 `audio_adc` 和 `audio_dac`。adapter 不要求相机或显示屏。TLS 和音频缓冲需要 PSRAM，OPUS 编解码依赖 Kconfig 选项自动选择的 `AUDIO_ENCODER_OPUS_SUPPORT` 和 `AUDIO_DECODER_OPUS_SUPPORT`。

## FAQ

**这个 adapter 能给 bot 发图片吗？**

不能。adapter 只声明音频和文本能力，并在 create 时拒绝视频流。需要一次性图片输入请用 [esp_llm_tongyi](../esp_llm_tongyi)，需要相机上行请用 [esp_llm_baidu](../esp_llm_baidu)。

**会话连上了但听不到声音。**

下行 TTS 经服务下行 provider 传输。请把 player link 到 `ESP_LLM_MEDIA_STREAM_AUDIO_DN`，或者设置 `cfg.service.downlink_cb` 自行渲染这些帧。

**唤醒词是怎么做到唤醒前不上传麦克风的？**

adapter 自身不做上行门控。应用在 create 之后调用 `esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false)`，再在唤醒事件里重新打开；`ESP_LLM_COZE_GATE_UPLINK_UNTIL_WAKEUP` 用于向例程说明这一策略。

## 技术支持

如需技术支持，请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题报告和功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
