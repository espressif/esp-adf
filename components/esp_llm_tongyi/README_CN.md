# ESP LLM Tongyi

- [English](./README.md)

[esp_llm_service](../esp_llm_service) 的通义 backend adapter。它封装 `components/esp_tongyi` 中随附的 DashScope 多模态 WebSocket SDK 并实现 `esp_llm_adapter.h` 契约，应用通过通用的 `esp_llm_service_*` API 和一个 typed create 函数即可驱动通义语音或视觉会话。

Adapter 只拥有 DashScope 会话。Capture 和 player 仍归应用所有；adapter 消费服务交给它的上行帧，并把 TTS 帧经服务下行通路发布回去。

## 功能特性

- 基于 TLS WebSocket 的 DashScope 多模态对话，支持双工音频
- 上行音频：OPUS，采样率取自 `ESP_LLM_TONGYI_SAMPLE_RATE`，默认 16 kHz 单声道、24000 bit/s
- 下行音频：同采样率 OPUS，默认 32 kbit/s、60 ms 帧
- upstream 类型可在纯音频与音频加视频之间选择，供视觉应用使用
- 一次性图像输入：把 JPEG 作为 VIDEO `esp_media_frame_t` 经 `esp_llm_service_send_frame()` 推送
- 图像意图逐帧决定：`frame.user_data` 为提示词字符串时请求回答，为 NULL 时只更新会话图像
- 通过 `esp_llm_service_send_text()` 输入文本，通过 `esp_llm_service_cancel_response()` 取消回答
- 对话状态和朗读文本以 `esp_llm_service` 事件转发
- Audio AI 功能挂载到应用 capture 上：AEC、NS、VAD 和 WakeNet
- 不使用 RTC 视频轨道；`VIDEO_UP` 保持关闭，也不需要 `video_up` 槽位

## 目录结构

```
esp_llm_tongyi/
├── include/esp_llm_tongyi.h       Typed 配置、DEFAULT 宏和 create 函数
├── src/esp_llm_tongyi_adapter.c   基于 esp_tongyi 的 adapter 实现
├── components/esp_tongyi/         DashScope WebSocket SDK
├── examples/tongyi_chat/          组件内对话示例
├── test_apps/                     离线的 create、流信息和控制面测试
├── Kconfig                        音频档位选项
└── agent.md                       Adapter 实现、事件与视觉指引
```

## 快速开始

产品例程是 [`adf_examples/ai_agent/esp_llm_tongyi_agent`](../../adf_examples/ai_agent/esp_llm_tongyi_agent)。它把媒体所有权留在应用侧，并演示了对话和视觉两条流程：

```bash
cd adf_examples/ai_agent/esp_llm_tongyi_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi 以及 DashScope API key、workspace ID 和应用 ID
idf.py build flash monitor
```

应用的调用顺序是：Wi-Fi，然后做 SNTP（TLS 握手需要有效时钟），接着用 `cfg.service.streams[]` 创建并配置 capture 和 player，填写 `cfg.service.audio_ai_cfg.capture`，调用 `esp_llm_tongyi_create()`，再依次 link、订阅事件，并按 player、LLM、capture 的顺序启动。

adapter 的最简用法如下：

```c
esp_llm_tongyi_cfg_t cfg = ESP_LLM_TONGYI_CFG_DEFAULT();
cfg.api_key = CONFIG_TONGYI_API_KEY;
cfg.workspace_id = CONFIG_TONGYI_WORKSPACE_ID;
cfg.app_id = CONFIG_TONGYI_APP_ID;
cfg.task_id = task_id;
cfg.service.audio_ai_cfg.capture = capture;           /* 由应用持有 */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* 为零时关闭 audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_tongyi_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/tongyi_chat` 是组件内的小型示例，不搭建完整应用媒体图即可验证 adapter。

## 视觉

通义不走 RTC 视频流。视觉是一次性的：应用把单张 JPEG 作为 VIDEO `esp_media_frame_t` 推送，因此不需要启用 `video_up` 槽位。adapter 读取 `frame.user_data` 判断意图：非 NULL 字符串是回答提示词，产生 `RequestToRespond`；NULL 则发送 `UpdateInfo`，只刷新会话图像而不请求回答。

```c
esp_media_frame_t image = {
    .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    .data = jpeg_buf,
    .size = jpeg_len,
};

/* 就这张图作答 */
image.user_data = (void *)"这是什么？";
esp_llm_service_send_frame(llm, &image);

/* 只更新会话图像 */
image.user_data = NULL;
esp_llm_service_send_frame(llm, &image);
```

视觉应用还需要把 `cfg.upstream.type` 设为 `ESP_TONGYI_UPSTREAM_TYPE_AUDIO_VIDEO`，并使用支持图像的 DashScope 应用。纯语音应用 ID 配合多模态提示词时会返回 `images must not be empty`。

## 配置

`esp_llm_tongyi_create()` 要求 `api_key`、`workspace_id` 和 `app_id`。`esp_llm_tongyi_cfg_t` 的其余字段由 `ESP_LLM_TONGYI_CFG_DEFAULT()` 填充：

| 字段 | 用途 |
| --- | --- |
| `api_key` | DashScope API key，必填 |
| `workspace_id` | DashScope workspace，形如 `llm-`，必填 |
| `app_id` | DashScope 应用；纯语音或多模态 `mm_` |
| `task_id` | 上报给 DashScope 的会话任务标识 |
| `upstream.type` | 纯音频，或供视觉应用使用的音频加视频 |
| `service` | 共享的 `esp_llm_service_cfg_t`：流、audio AI、下行回调和名称 |

组件 Kconfig 位于 `ESP LLM Tongyi` 菜单下：

| 选项 | 默认值 |
| --- | --- |
| `ESP_LLM_TONGYI_SAMPLE_RATE` | 16000 Hz，上下行流共用 |
| `ESP_LLM_TONGYI_FRAME_SIZE_MS` | 下行 Opus 帧长 60 ms |
| `ESP_LLM_TONGYI_BIT_RATE_KBPS` | 下行 32 kbit/s |
| `ESP_LLM_TONGYI_CAPTURE_UPLINK_BITRATE` | 上行 24000 bit/s |

## 注意事项

- Wi-Fi 连上后要用 SNTP 同步时钟。时钟不准会导致 TLS 握手失败。
- DEFAULT 宏默认关闭 `VIDEO_UP`，并应保持关闭；视觉走 `send_frame()`，而不是 link 视频 provider。
- 所有上行必须在 `esp_service_start()` 之前 link；对运行中的服务 link 会返回 `ESP_ERR_INVALID_STATE`。
- player 要在 LLM 之前启动，capture 要在其之后启动，AFE sidecar 才能挂载。停止顺序相反。
- 下行 TTS 走服务下行通路，因此必须把 player link 到 `AUDIO_DN` 才能听到声音。
- 传给 `send_frame()` 的 JPEG 缓冲区必须在调用期间保持有效；adapter 不接管其所有权。
- API key 属于凭据。量产构建中请勿把它留在提交的默认值里。

## 性能

| 项目 | 取值 |
| --- | --- |
| 上行音频 | OPUS，16 kHz 单声道，24000 bit/s |
| 下行音频 | OPUS，16 kHz 单声道，32 kbit/s，60 ms 帧 |
| 视觉 | 每次 `send_frame()` 一张 JPEG；没有连续视频轨道 |
| 传输 | 每个会话一条 TLS WebSocket 连接 |
| TLS 缓冲 | 产品例程把 `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` 提高到 16384，并要求 PSRAM |
| 应用分区 | 产品例程为 6 MB |

## SoC 兼容性

在 ESP32-S3、ESP32-S31 和 ESP32-P4 上验证，开发板需提供 `audio_adc` 和 `audio_dac`。视觉功能还需要板上带相机和显示屏；ESP32-S3 没有硬件 JPEG 解码器，预览解码在软件中完成。TLS 和音频缓冲需要 PSRAM。

`test_apps` 在不连 Wi-Fi 的情况下覆盖 create、destroy、流信息和控制面错误。由于 `esp_llm_service` 仍会编译 `esp_audio_capture_service`，构建前请先用 `idf.py bmgr -b <board>` 生成板级头文件。

## FAQ

**为什么视觉模式下 `VIDEO_UP` 也是关闭的？**

通义在对话通道上接收离散图像，而不是连续的 RTC 视频轨道。保持 `VIDEO_UP` 关闭可以避免创建一条无帧可转发的上行消费任务，图像由 `send_frame()` 直接送达。

**如何区分"就图提问"和"静默更新图像"？**

靠 `frame.user_data`。非 NULL 的提示词字符串产生 `RequestToRespond`，模型会作答；NULL 产生 `UpdateInfo`，只刷新已存储的图像。

**模型返回 `images must not be empty`。**

使用了多模态 `mm_` 应用 ID 却从未发送图像。请用 `send_frame()` 发送一张 JPEG，或者为纯音频产品改配纯语音应用 ID。

**adapter 能直接接收 RGB 帧而不是 JPEG 吗？**

不能。对话通道传输的是已编码的 JPEG，因此 RGB 传感器输出需要先编码，例如用 `esp_jpeg_enc`。

## 技术支持

如需技术支持，请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题报告和功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
