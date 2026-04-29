# ESP_COZE

[English](./README_EN.md)

## 简介

[ESP_COZE](https://www.coze.cn/home) 是基于 Coze 平台、面向 ESP-IDF 的开源组件，通过 WebSocket 与 Coze 通信，时延低、体量轻，适合 ESP32 类设备。

**三条入口，彼此独立、可并行使用：** 可选用**完整流式对话**、**仅文本转语音（TTS）**或**仅语音识别（ASR）**。每种 API 各自维护连接与生命周期（`esp_coze_chat_*`、`esp_coze_tts_*`、`esp_coze_asr_*`）；公共类型与默认宏见 [`esp_coze_common.h`](./include/esp_coze_common.h)。典型场景：语音助手、智能语音问答，以及在对话之外再开独立 TTS/ASR 会话。

| API | 头文件 | 作用 |
| --- | ------ | ---- |
| Chat（语音 + 文本流式） | [`esp_coze_chat.h`](./include/esp_coze_chat.h) | 全双工：麦克风上行、字幕、下行音频与事件。 |
| TTS | [`esp_coze_tts.h`](./include/esp_coze_tts.h) | 提交文本，在回调中接收解码后的音频。 |
| ASR | [`esp_coze_asr.h`](./include/esp_coze_asr.h) | 上行音频流，在事件中接收识别结果。 |

OAuth / JWT 相关辅助接口见 [`esp_coze_jwt.h`](./include/esp_coze_jwt.h)、[`esp_coze_http.h`](./include/esp_coze_http.h)。

## Kconfig（`ESP COZE Chat`）

组件级配置位于 **`menuconfig → ESP COZE Chat`**（见当前目录下的 [`Kconfig`](./Kconfig)）。大多数配置会在 `esp_coze_chat_init()` 时生效，并在说明处覆盖 `esp_coze_chat_config_t` 中的对应字段；**`ESP_COZE_WS_BUFFER_SIZE`** 和 **WebSocket 任务绑核配置**（`task_core_id_set` / `task_core_id`）仅可通过 Kconfig 配置，不属于 `esp_coze_chat_config_t` 字段。

| 选项 | 作用 |
| ------ | ---- |
| `ESP_COZE_WS_TASK_PRIO` | WebSocket 工作任务优先级（`task_prio`）。 |
| `ESP_COZE_WS_TASK_STACK` | 工作任务栈大小，单位字节；为 **0** 时沿用 websocket client 库默认值。 |
| `ESP_COZE_WS_TASK_NAME` | 可选的非空字符串，用于覆盖工作任务名称。 |
| `ESP_COZE_WS_TASK_CORE_ID_SET` | 启用后，将工作任务绑定到 **`ESP_COZE_WS_TASK_CORE_ID`**（对应 `task_core_id_set` / `task_core_id`）。 |
| `ESP_COZE_WS_TASK_CORE_ID` | 启用绑核时可选 **0** 或 **1**；未启用时忽略。 |
| `ESP_COZE_WS_BUFFER_SIZE` | `esp_websocket_client_config_t.buffer_size`；范围 1 KiB–128 KiB；默认 20480。 |
| `ESP_COZE_WS_RECONNECT_TIMEOUT_MS` | 链路断开后的 `reconnect_timeout_ms`。 |
| `ESP_COZE_WS_NETWORK_TIMEOUT_MS` | 网络发送/接收超时时间。 |
| `ESP_COZE_WS_CONNECT_TIMEOUT_MS` | 启动后等待连接成功上报的最长时间（`websocket_connect_timeout`）。 |
| `ESP_COZE_WS_AUDIO_SEND_TIMEOUT_MS` | 上行 `input_audio_buffer.append` 调用 `esp_websocket_client_send_text()` 的超时时间（默认 20000 ms）。 |
| `ESP_COZE_DEBUG_TRACE_DATA` | 以 INFO 级别打印上下行 JSON 文本；音频相关事件仅打印字节长度，避免大量 base64 日志。 |

这里不包含凭证、Bot ID、OAuth/PAT 等业务配置，它们应放在应用侧 Kconfig 中（可参考 **`coze_ws_app`** 示例）。

## 鉴权

Coze 的 HTTP / WebSocket 访问都需要有效凭证，详见 [**Authentication**](https://www.coze.cn/open/docs/developer_guides/authentication)。调用 `esp_coze_chat_init()` 时，请通过 `esp_coze_chat_config_t.access_token` 传入 bearer token。

辅助接口（`esp_coze_jwt.h`、`esp_coze_http.h`）支持 JWT 签名和 OAuth token POST。关于 **PAT 与 OAuth 的区别**、`private_key.pem` 的嵌入方式，以及 **menuconfig** 字段配置，可参考示例说明 [`adf_examples/ai_agent/coze_ws_app/README_CN.md`](../../adf_examples/ai_agent/coze_ws_app/README_CN.md) 和该示例中的 `main/Kconfig.projbuild`。

## 协议

服务端发送到设备侧的 WebSocket 帧为 JSON 消息，其中 `event_type` 字段定义见 Coze 文档：[**Streaming chat — downlink events**](https://www.coze.cn/open/docs/developer_guides/streaming_chat_downlink_event)。

当前组件处理其中一部分事件（例如 `conversation.audio.delta`、`conversation.message.delta`、`conversation.chat.completed`、`input_audio_buffer.speech_started` / `speech_stopped`、`chat.created`、`error`）。其他未识别的 `event_type` 会原样透传为 `ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA`，方便应用在无需等待 SDK 更新的情况下自行解析新增协议字段。

上行消息格式遵循同一套流式对话协议，可进一步参考 `esp_coze_chat_protocol.c` 中的实现（如 `chat.update`、`input_audio_buffer.append` 等）。

## 添加到工程

使用组件管理器将 `esp_coze` 添加为工程依赖。构建时，组件会自动下载并集成到项目中：

```bash
idf.py add-dependency "espressif/esp_coze=*"
```

## 使用方法

1. **创建 Bot。** 在 Coze 平台创建一个 bot。创建完成后会获得唯一的 `BOT_ID`，用于标识会话目标（具体使用 Chat / TTS / ASR 中的哪一种 API）。
2. **配置鉴权。** 按 [鉴权](#鉴权) 章节以及 **`coze_ws_app`** 示例 / Coze 官方文档准备 token（PAT 或 OAuth/JWT）。
3. **选择一种或多种 API。** 需要完整对话时，使用下面 Chat 示例；若只要 TTS 或只要 ASR，请按各自头文件使用 `esp_coze_tts_init` / `esp_coze_asr_init` 及对应的 `start`、`send` 等接口——在业务需要多个 Coze 会话时，可与 Chat **同时**运行。
4. **Chat：在配置里设置下行音频与事件回调（风格对齐 esp_xiaozhi_chat），然后初始化并启动：**

```c
#include "esp_coze_chat.h"

static void on_downlink_audio(const uint8_t *data, int len, void *ctx)
{
    (void)ctx;
    /* 将解码后的下行音频送入播放链路；仅在回调返回前有效。 */
}

static void on_chat_event(esp_coze_chat_event_t event, void *event_data, void *ctx)
{
    (void)ctx;
    switch (event) {
    case ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT:
        printf("subtitle: %s\n", (const char *)event_data);
        break;
    case ESP_COZE_CHAT_EVENT_CHAT_ERROR:
        printf("error: %s\n", (const char *)event_data);
        break;
    default:
        break;
    }
}

void app_demo(void)
{
    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id           = COZE_BOT_ID;
    cfg.access_token     = COZE_ACCESS_TOKEN;
    cfg.enable_subtitle  = true;
    cfg.audio_callback   = on_downlink_audio;
    cfg.event_callback   = on_chat_event;

    esp_coze_chat_handle_t chat = NULL;
    ESP_ERROR_CHECK(esp_coze_chat_init(&cfg, &chat));
    ESP_ERROR_CHECK(esp_coze_chat_start(chat));

    /* ... 使用 esp_coze_chat_send_audio_data() 持续送入麦克风 PCM ... */

    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);
}
```

## 说明

- Chat、TTS、ASR 各自使用独立的 WebSocket 客户端实例；在应用中并行开启多路会话时，请注意内存占用与任务优先级。
- 可通过上文 **Kconfig（`ESP COZE Chat`）** 章节中的配置项，在 menuconfig 中调节 WebSocket buffer 和超时时间。
- `esp_coze_chat_config_t` 中的所有字符串字段都会在 `esp_coze_chat_init()` 中进行深拷贝，初始化完成后，调用方可自由修改或释放原始缓冲区。
- 回调在 **WebSocket 客户端任务** 上下文中同步触发；不要在回调里长时间阻塞，可把数据拷入队列再在其它任务里处理。
- 若配置了 `audio_callback`，下行解码音频只通过 `(data, len)` 交给该回调。若未配置 `audio_callback` 但配置了 `event_callback`，则仍可通过 `ESP_COZE_CHAT_EVENT_AUDIO_DATA` 收到 `esp_coze_chat_audio_data_t *`（仅在回调返回前有效）。
- 完整参考应用见 [`coze_ws_app`](https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app)，支持按键触发和唤醒词等交互模式。

## TTS / ASR 最小示例

以下为仅 TTS、仅 ASR 的精简示例；字段与事件详见对应头文件及 [TTS 事件](https://docs.coze.cn/api/open/docs/developer_guides/tts_event)、[ASR 事件](https://docs.coze.cn/api/open/docs/developer_guides/asr_event)。

### TTS

```c
#include "esp_coze_tts.h"

static void on_audio(const uint8_t *pcm, int len, void *ctx) {
    (void)ctx;
    /* 把 pcm 喂给播放器 */
}

esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
cfg.bot_id          = COZE_BOT_ID;
cfg.access_token    = COZE_ACCESS_TOKEN;
cfg.audio_callback  = on_audio;

esp_coze_tts_handle_t tts = NULL;
ESP_ERROR_CHECK(esp_coze_tts_init(&cfg, &tts));
ESP_ERROR_CHECK(esp_coze_tts_start(tts));
ESP_ERROR_CHECK(esp_coze_tts_send_text(tts, "你好"));
/* ... 等若干 ESP_COZE_TTS_EVENT_CHAT_COMPLETED 后 ... */
esp_coze_tts_stop(tts);
esp_coze_tts_deinit(tts);
```

### ASR

```c
#include "esp_coze_asr.h"

static void on_asr(esp_coze_asr_event_t event, void *event_data, void *ctx) {
    (void)ctx;
    if (event == ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED) {
        printf("ASR final: %s\n", (const char *)event_data);
    }
}

esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
cfg.bot_id          = COZE_BOT_ID;
cfg.access_token    = COZE_ACCESS_TOKEN;
cfg.event_callback  = on_asr;

esp_coze_asr_handle_t asr = NULL;
ESP_ERROR_CHECK(esp_coze_asr_init(&cfg, &asr));
ESP_ERROR_CHECK(esp_coze_asr_start(asr));
/* 持续 esp_coze_asr_send_audio(asr, frame, frame_len)... */
esp_coze_asr_send_audio_complete(asr);
esp_coze_asr_stop(asr);
esp_coze_asr_deinit(asr);
```
