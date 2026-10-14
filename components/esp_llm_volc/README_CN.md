# ESP LLM Volc

- [English](./README.md)

[esp_llm_service](../esp_llm_service) 的火山引擎 RTC backend adapter。它封装 `volc_conv_ai` SDK 并实现 `esp_llm_adapter.h` 契约，应用通过通用的 `esp_llm_service_*` API 和一个 typed create 函数即可驱动火山对话智能体。

Adapter 只拥有火山 IoT 鉴权和 RTC 会话。Capture 和 player 仍归应用所有；adapter 消费服务交给它的上行帧，并把 TTS 帧经服务下行通路发布回去。公开头文件把火山 SDK 隐藏在内部，应用无需包含 `volc_conv_ai.h`。

## 功能特性

- 基于 RTC 的火山引擎对话式 AI，设备身份经火山 IoT 签名
- RTC 音频编码：默认 G.711 A-law 8 kHz，另可选 G.711 mu-law 8 kHz 和 OPUS 16 kHz
- RTC 视频发布与订阅，默认关闭，帧编码可选 JPEG 或 H.264
- 通过 `esp_llm_service_send_text()` 输入文本并经模型往返
- 通过 `esp_llm_volc_send_text_to_tts()` 直接朗读，不经模型往返
- 通过 `esp_llm_service_send_custom_data()` 发送 backend 原生 RTS 消息
- 通过 `esp_llm_service_cancel_response()` 取消回答
- 对话状态以后端事件 `CONV_STATUS` 发布
- Audio AI 功能挂载到应用 capture 上：AEC、NS、VAD 和 WakeNet
- 提供借用原始引擎句柄的出口，用于本组件未封装的 SDK 功能

## 目录结构

```
esp_llm_volc/
├── include/esp_llm_volc.h   Typed 配置、DEFAULT 宏和公开 API
├── src/                     基于 volc_conv_ai 的 adapter 实现
├── examples/volc_chat/      组件内对话示例
├── Kconfig                  音频档位与视频 RTC 档位选项
└── agent.md                 Adapter 实现、IoT 凭据与事件说明
```

## 快速开始

产品例程是 [`adf_examples/ai_agent/esp_llm_volc_agent`](../../adf_examples/ai_agent/esp_llm_volc_agent)。它把媒体所有权留在应用侧，是接入本 adapter 的参考实现：

```bash
cd adf_examples/ai_agent/esp_llm_volc_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi 以及火山 IoT 凭据和 bot ID
idf.py build flash monitor
```

应用的调用顺序是：Wi-Fi，然后做 SNTP（RTC 请求签名会拒绝不准的时钟），接着用 `cfg.service.streams[]` 创建并配置 capture 和 player，填写 `cfg.service.audio_ai_cfg.capture`，调用 `esp_llm_volc_create()`，再依次 link、订阅事件，并按 player、LLM、capture 的顺序启动。

adapter 的最简用法如下：

```c
esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
cfg.iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
cfg.iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
cfg.iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
cfg.iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
cfg.bot_id = CONFIG_VOLC_BOT_ID;
cfg.service.audio_ai_cfg.capture = capture;           /* 由应用持有 */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* 为零时关闭 audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/volc_chat` 是组件内的小型示例，不搭建完整应用媒体图即可验证 adapter。

## 配置

`esp_llm_volc_create()` 要求 IoT 的 `instance_id`、`product_key`、`product_secret`、`device_name` 以及一个 `bot_id`。音频编码、采样率和码率来自 `cfg.service.streams[]`，DEFAULT 宏会按 Kconfig 音频档位预填。除非应用拥有相机和显示屏，否则请关闭 `VIDEO_UP` 和 `VIDEO_DN`，因为没有任何一处会连接这两路流。

组件 Kconfig 位于 `ESP LLM Volc` 菜单下：

| 选项 | 默认值 |
| --- | --- |
| `ESP_LLM_VOLC_AUDIO_CODEC` | G.711 A-law 8 kHz；也可选 G.711 mu-law 和 OPUS 16 kHz |
| `ESP_LLM_VOLC_VIDEO_PUBLISH` | 关闭；启用视频上行时发布 RTC 视频轨道 |
| `ESP_LLM_VOLC_VIDEO_SUBSCRIBE` | 关闭；订阅远端 RTC 视频 |
| `ESP_LLM_VOLC_VIDEO_CODEC` | JPEG；也可选 H.264 |

## Backend 专用 API

大部分控制走通用的 `esp_llm_service_*` API。下列接口覆盖通用 API 无法表达的火山特性：

| API | 用途 |
| --- | --- |
| `esp_llm_volc_send_text_to_tts()` | 原样朗读 UTF-8 文本，不经模型往返 |
| `esp_llm_volc_backend_acquire()` | 借用原始引擎句柄，调用未封装的 SDK 接口 |
| `esp_llm_volc_backend_release()` | 归还借用的句柄 |

`esp_llm_service_send_text()` 向模型提问，而 `esp_llm_volc_send_text_to_tts()` 只驱动 TTS。`esp_llm_service_send_custom_data()` 发送 backend 原生 RTS 消息。

借用期间会持有引擎锁并钉住 adapter 上下文。请尽量缩短借用时间、在同一线程内成对调用，期间不要调用其他 LLM API；漏掉 release 会让 destroy 死锁。

## 注意事项

- Wi-Fi 连上后要用 SNTP 同步时钟。RTC 配置请求是带签名的，时钟不准会被拒绝。
- Kconfig 档位和产品例程都默认关闭视频。只有在具备相机和显示屏时才打开这些流。
- 所有上行必须在 `esp_service_start()` 之前 link；对运行中的服务 link 会返回 `ESP_ERR_INVALID_STATE`。
- player 要在 LLM 之前启动，capture 要在其之后启动，AFE sidecar 才能挂载。停止顺序相反。
- 下行 TTS 走服务下行通路，因此必须把 player link 到 `AUDIO_DN` 才能听到声音。
- `esp_llm_volc_backend_acquire()` 必须与 `esp_llm_volc_backend_release()` 在同一线程内成对使用。
- IoT product secret 属于凭据。量产构建中请勿把它留在提交的默认值里。

## 性能

| 项目 | 取值 |
| --- | --- |
| 上行音频 | 默认 G.711 A-law，8 kHz 单声道，64 kbit/s |
| 下行音频 | 与上行同编码、同采样率 |
| 采集与播放时钟 | 产品例程中 AFE 和 I2S 运行在 16 kHz PCM，转码到 8 kHz 后进入 RTC |
| 视频 | 默认关闭；启用时为 JPEG 或 H.264 |
| 传输 | 每台设备一个 RTC 会话，启动时先发起带签名的 IoT 配置请求 |
| 应用分区 | 产品例程为 6 MB |

## SoC 兼容性

在 ESP32-S3、ESP32-S31 和 ESP32-P4 上验证，开发板需提供 `audio_adc` 和 `audio_dac`。纯音频配置不要求相机或显示屏。RTC 和音频缓冲需要 PSRAM，OPUS 依赖 Kconfig 选项自动选择的 `AUDIO_ENCODER_OPUS_SUPPORT` 和 `AUDIO_DECODER_OPUS_SUPPORT`。

## FAQ

**为什么默认是 G.711 而不是 OPUS？**

G.711 A-law 把 RTC 链路固定在 64 kbit/s，编码开销极小，适合网络受限环境下的纯语音对话智能体。当宽带音质比 CPU 和带宽更重要时，可以改选 16 kHz 的 OPUS。

**采集是 16 kHz，流却写 8 kHz，到底哪个对？**

两个都对。AEC 参考信号需要 16 kHz，因此 AFE 和共用的 I2S 总线运行在 16 kHz PCM，adapter 再把上行编码降到 8 kHz 交给 G.711。不要把 ADC 时钟钉死在 8 kHz。

**`GetRTCConfig` 报签名错误。**

系统时钟没有设置成功。火山会对配置请求签名，因此要在 Wi-Fi 连上之后、创建服务之前完成 SNTP。

**本组件没封装的火山 SDK 函数怎么调用？**

用 `esp_llm_volc_backend_acquire()` 借出引擎，完成 SDK 调用后用 `esp_llm_volc_backend_release()` 归还。请在自己的文件中包含 SDK 头文件，公开头文件有意让该句柄保持不透明。

## 技术支持

如需技术支持，请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题报告和功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
