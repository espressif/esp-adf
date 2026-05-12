# ESP_COZE

[中文](./README.md)

## Introduction

[ESP_COZE](https://www.coze.cn/home) is an open-source component for the Coze platform on ESP-IDF. It talks to Coze over WebSocket with low latency and a small footprint, suited to ESP32-class devices.

**Three entry points, all independent:** you can use **full streaming chat**, **text-to-speech only**, or **speech recognition only** in parallel. Each API owns its own connection and lifecycle (`esp_coze_chat_*`, `esp_coze_tts_*`, `esp_coze_asr_*`); they share types and defaults via [`esp_coze_common.h`](./include/esp_coze_common.h). Typical uses: voice assistants and voice Q&A where you need chat plus optional dedicated TTS/ASR pipes on separate sessions.

| API | Header | Role |
| --- | ------ | ---- |
| Chat (voice + text streaming) | [`esp_coze_chat.h`](./include/esp_coze_chat.h) | Full duplex: mic uplink, subtitles, downlink audio/events. |
| TTS | [`esp_coze_tts.h`](./include/esp_coze_tts.h) | Push text; receive decoded audio in a callback. |
| ASR | [`esp_coze_asr.h`](./include/esp_coze_asr.h) | Stream uplink audio; receive transcripts in events. |

HTTP helpers for OAuth/JWT live in [`esp_coze_jwt.h`](./include/esp_coze_jwt.h) and [`esp_coze_http.h`](./include/esp_coze_http.h).

## Kconfig (`ESP COZE Chat`)

Component-wide settings live under **`menuconfig → ESP COZE Chat`** (see [`Kconfig`](./Kconfig) in this directory). Most apply at `esp_coze_chat_init()` and override matching fields in `esp_coze_chat_config_t` where noted; **`ESP_COZE_WS_BUFFER_SIZE`** and **WebSocket task affinity** (`task_core_id_set` / `task_core_id`) are applied only via Kconfig (not fields on `esp_coze_chat_config_t`).

| Option | Role |
| ------ | ---- |
| `ESP_COZE_WS_TASK_PRIO` | WebSocket worker task priority (`task_prio`). |
| `ESP_COZE_WS_TASK_STACK` | Worker stack size in bytes; **0** keeps the websocket client library default. |
| `ESP_COZE_WS_TASK_NAME` | Optional non-empty string overrides the worker task name. |
| `ESP_COZE_WS_TASK_CORE_ID_SET` | If enabled, pin the worker to **`ESP_COZE_WS_TASK_CORE_ID`** (`task_core_id_set` / `task_core_id`). |
| `ESP_COZE_WS_TASK_CORE_ID` | Core **0** or **1** when pinning is enabled (ignored otherwise). |
| `ESP_COZE_WS_BUFFER_SIZE` | `esp_websocket_client_config_t.buffer_size`; range 1 KiB-128 KiB; default 20480. |
| `ESP_COZE_WS_RECONNECT_TIMEOUT_MS` | `reconnect_timeout_ms` for link drops. |
| `ESP_COZE_WS_NETWORK_TIMEOUT_MS` | Network I/O timeout for sends/recvs. |
| `ESP_COZE_WS_CONNECT_TIMEOUT_MS` | Max wait after start until the session reports connected (`websocket_connect_timeout`). |
| `ESP_COZE_WS_AUDIO_SEND_TIMEOUT_MS` | Timeout for `esp_websocket_client_send_text()` on uplink `input_audio_buffer.append` (default 20000 ms). |
| `ESP_COZE_DEBUG_TRACE_DATA` | Log uplink/downlink JSON text at INFO; for audio-related events only byte lengths are logged to avoid huge base64 dumps. |

Credentials, bot ID, and OAuth/PAT fields are **not** defined here; they belong to your application Kconfig (see the **`coze_ws_app`** example).

## Authentication

Coze requires valid credentials for HTTP/WebSocket access; see [**Authentication**](https://www.coze.cn/open/docs/developer_guides/authentication). Pass the bearer token via `esp_coze_chat_config_t.access_token` when calling `esp_coze_chat_init()`.

Helpers (`esp_coze_jwt.h`, `esp_coze_http.h`) support JWT signing and the OAuth token POST. For **PAT vs OAuth**, embedding `private_key.pem`, and **menuconfig** fields, see [`adf_examples/ai_agent/coze_ws_app/README.md`](../../adf_examples/ai_agent/coze_ws_app/README.md) and `main/Kconfig.projbuild` in that example.

## Protocol

Server-to-device WebSocket frames are JSON messages whose `event_type` field is defined in the Coze documentation: [**Streaming chat - downlink events**](https://www.coze.cn/open/docs/developer_guides/streaming_chat_downlink_event).

This component handles a subset (for example `conversation.audio.delta`, `conversation.message.delta`, `conversation.chat.completed`, `input_audio_buffer.speech_started` / `speech_stopped`, `chat.created`, `error`). Any other `event_type` is forwarded unchanged as `ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA` so applications can parse new protocol additions without waiting for an SDK update.

Uplink message shapes follow the same platform streaming chat guides; see also `esp_coze_chat_protocol.c` (`chat.update`, `input_audio_buffer.append`, and so on).

## Adding the Component to Your Project

Use the component manager to add `esp_coze` as a project dependency. The component will be downloaded and integrated into your project automatically during the build:

```bash
idf.py add-dependency "espressif/esp_coze=*"
```

## How to Use

1. **Create a bot.** Create a bot on the Coze platform. After creation, you will receive a unique `BOT_ID` used to identify the session target (chat, TTS, or ASR, depending on the API you use).
2. **Configure authorization.** Obtain a token per [Authentication](#authentication) and the **`coze_ws_app`** example / Coze docs (PAT or OAuth/JWT).
3. **Pick one or more APIs.** For full conversation, follow the chat example below. For TTS-only or ASR-only flows, use `esp_coze_tts_init` / `esp_coze_asr_init` and their `start`/`send` APIs as documented in the respective headers—these can run **at the same time** as chat if your product needs separate Coze sessions.
4. **Chat: configure downlink audio and event callbacks in the config (style aligned with esp_xiaozhi_chat), then initialize and start:**

```c
#include "esp_coze_chat.h"

static void on_downlink_audio(const uint8_t *data, int len, void *ctx)
{
    (void)ctx;
    /* Feed decoded downlink bytes into playback; valid only until this returns. */
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

    /* ... keep feeding microphone PCM with esp_coze_chat_send_audio_data() ... */

    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);
}
```

## Notes

- Chat, TTS, and ASR each use their own WebSocket client instance; running them together is supported when your application opens multiple sessions (watch memory and task priorities).
- Tune WebSocket buffers and timeouts from menuconfig using the **Kconfig (`ESP COZE Chat`)** section and table above.
- All string fields in `esp_coze_chat_config_t` are deep-copied during `esp_coze_chat_init`; the caller is free to modify or release the original buffers afterwards.
- Callbacks run **synchronously on the WebSocket client task**. Avoid blocking; copy into a queue and process from another task if needed.
- With `audio_callback` set, decoded downlink audio is delivered only through `(data, len)`. Without `audio_callback`, `event_callback` may receive `ESP_COZE_CHAT_EVENT_AUDIO_DATA` with `esp_coze_chat_audio_data_t *` valid until the callback returns.
- See [`coze_ws_app`](https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app) for a complete reference application that supports both button-trigger and wake-word interaction modes.

## TTS / ASR Minimal Examples

The snippets below are minimum sketches for TTS-only and ASR-only flows; field and event details live in the respective headers and in the Coze docs for [TTS events](https://docs.coze.cn/api/open/docs/developer_guides/tts_event) and [ASR events](https://docs.coze.cn/api/open/docs/developer_guides/asr_event).

### TTS

```c
#include "esp_coze_tts.h"

static void on_audio(const uint8_t *pcm, int len, void *ctx) {
    (void)ctx;
    /* Forward pcm to the playback pipeline */
}

esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
cfg.bot_id          = COZE_BOT_ID;
cfg.access_token    = COZE_ACCESS_TOKEN;
cfg.audio_callback  = on_audio;

esp_coze_tts_handle_t tts = NULL;
ESP_ERROR_CHECK(esp_coze_tts_init(&cfg, &tts));
ESP_ERROR_CHECK(esp_coze_tts_start(tts));
ESP_ERROR_CHECK(esp_coze_tts_send_text(tts, "Hello"));
/* ... wait for ESP_COZE_TTS_EVENT_CHAT_COMPLETED events ... */
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
/* keep streaming with esp_coze_asr_send_audio(asr, frame, frame_len)... */
esp_coze_asr_send_audio_complete(asr);
esp_coze_asr_stop(asr);
esp_coze_asr_deinit(asr);
```
