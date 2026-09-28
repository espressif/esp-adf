# ESP LLM Coze Agent Guide

Use this when implementing or reviewing an application on `esp_llm_coze`.

The shared LLM contract is in
[`../esp_llm_service/agent.md`](../esp_llm_service/agent.md). This file covers
the Coze backend only.

## What It Is

`esp_llm_coze` is the Coze streaming-chat adapter for `esp_llm_service`.
It wraps `esp_coze` and exposes `esp_llm_coze_create()`.

Official platform docs:

- [Streaming chat overview](https://www.coze.cn/open/docs/developer_guides/streaming_chat_overview)
- [Audio / speech input](https://www.coze.cn/open/docs/developer_guides/audio_speech_recognize)
- [Personal access token](https://www.coze.cn/open/docs/developer_guides/pat)

Default URLs:

- WebSocket: `wss://ws.coze.cn/v1/chat`
- Conversation create: `https://api.coze.cn/v1/conversation/create`

Override with `ws_base_url` and `conv_create_base_url` for `coze.com` or a
private endpoint.

Product example: `adf_examples/ai_agent/esp_llm_coze_agent`.
In-component chat: `examples/coze_chat`.

## Ownership

| Owner | Owns |
|---|---|
| Application | Capture, player, board, Wi-Fi, bot ID / token |
| `esp_llm_service` | Streams, uplink tasks, downlink provider, AFE attach |
| `esp_llm_coze` | Coze WebSocket session, conversation create, TTS downlink |

The adapter never creates capture or player. It does not emit `WAKEUP` or `VAD`.
Server speech start / stop is a raw event named `SPEECH_STARTED` or
`SPEECH_STOPPED`. Local AFE VAD is still published by the service when
`audio_ai` includes VAD.

## Credentials

Required:

| Field | Source |
|---|---|
| `bot_id` | Coze bot console |
| `access_token` | Personal access token (PAT) |

Optional: `user_id`, `voice_id`, `ws_base_url`, `conv_create_base_url`,
`mode`, `vad_mode`, downlink frame size / rate limit / speech rate,
`websocket_connect_timeout`, `enable_subtitle`.

String fields are copied at create. Do not keep secrets in
`sdkconfig.defaults`.

## Streams

`ESP_LLM_COZE_CFG_DEFAULT()` enables `AUDIO_UP` and `AUDIO_DN`. Codec comes
from component Kconfig (Opus / G.711A / G.711U / PCM). G.711 uses 8 kHz;
Opus / PCM use 16 kHz.

| Stream | Supported | Notes |
|---|---|---|
| `AUDIO_UP` | Yes | Linked capture |
| `AUDIO_DN` | Yes | TTS through the service downlink provider |
| `VIDEO_UP` / `VIDEO_DN` | No | Create rejects video streams |

Capabilities: `TEXT_INPUT`, `AUDIO_INPUT`, `TEXT_OUTPUT`, `AUDIO_OUTPUT`,
`CANCEL_RESPONSE`, `CUSTOM_DATA`. No `VISION`.

## Application Sequence

```c
esp_llm_coze_cfg_t cfg = ESP_LLM_COZE_CFG_DEFAULT();
cfg.bot_id = CONFIG_COZE_BOT_ID;
cfg.access_token = CONFIG_COZE_ACCESS_TOKEN;
cfg.service.audio_ai_cfg.capture = capture;
cfg.service.audio_ai_cfg.feature_mask = ai_features;

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_coze_create(&cfg, &llm));
/* link AUDIO_UP / AUDIO_DN, subscribe, then start player → LLM → capture */
```

`mode`:

- `ESP_LLM_COZE_MODE_SPEECH_INTERRUPT` — voice barge-in (default)
- `ESP_LLM_COZE_MODE_NORMAL` — button / hold-to-talk

`vad_mode` is **server** silence detection (`FAST` or `NORMAL`), not AFE VAD.

## Send And Cancel

While RUNNING, on the control thread:

- `esp_llm_service_send_text()` — text query
- `esp_llm_service_send_frame()` — extra audio push; do not also use it on a
  linked `AUDIO_UP` path
- `esp_llm_service_send_custom_data()` — forwarded as Coze customer data
- `esp_llm_service_cancel_response()` — stop current TTS

## Events

| `common_id` | Typical `backend_event_name` | Payload |
|---|---|---|
| `CONNECTED` | `CONNECTED` | WebSocket up |
| `MEDIA_READY` | `MEDIA_READY` | session can exchange media |
| `DISCONNECTED` | `DISCONNECTED` | socket down |
| `TEXT` | `SUBTITLE` | assistant subtitle when `enable_subtitle` is true |
| `ERROR` | `ERROR` | JSON |
| `FINISHED` | `CHAT_COMPLETED` | turn complete |
| `WAKEUP` | — | local AFE only |

Raw backend names:

| Name | Meaning |
|---|---|
| `SPEECH_STARTED` / `SPEECH_STOPPED` | server speech, payload `int` 1 / 0 |
| `CHAT_CREATE` | conversation created |
| `CUSTOMER_DATA` | JSON from the bot |
| `COZE_EVENT` / `WS_EVENT` | other SDK events |
| `AUDIO_DATA` | downlink audio tap (playback still uses the provider) |

Payload pointers are valid only during the handler.

## Wake-Up

Enable `ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN` in `feature_mask`. Then:

1. `esp_llm_service_set_wakeup_cancel_response(llm, true)`
2. `esp_llm_service_enable_uplink_track(llm, AUDIO, false)` until wake-up
3. On `ESP_LLM_EVENT_WAKEUP` state 1, enable the uplink

Uplink send is not tied to wake-up unless the application holds it closed.

## Review Checklist

- `bot_id` and `access_token` are non-empty
- Video streams stay disabled
- Downlink is the provider link (plus optional `downlink_cb` tap)
- Server VAD events are `int`; do not confuse them with AFE VAD
- Event handlers do not keep payload pointers
- Control-plane calls stay on one thread
