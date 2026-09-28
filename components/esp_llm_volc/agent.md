# ESP LLM Volc Agent Guide

Use this when implementing or reviewing an application on `esp_llm_volc`.

The shared LLM contract is in
[`../esp_llm_service/agent.md`](../esp_llm_service/agent.md). This file covers
the Volcengine RTC backend only.

## What It Is

`esp_llm_volc` wraps Volc `volc_conv_ai` (RTC engine) as an `esp_llm_service`
adapter. It exposes `esp_llm_volc_create()`.

Official platform docs:

- [Volcengine RTC](https://www.volcengine.com/docs/6348)
- Embedded kit notes under `src/volc_conv_ai/README.md`

The product example (`adf_examples/ai_agent/esp_llm_volc_agent`) is
**audio-only RTC**. It disables `VIDEO_UP` and `VIDEO_DN`. The adapter itself
can advertise video when those streams are enabled.

In-component chat: `examples/volc_chat`.

## Ownership

| Owner | Owns |
|---|---|
| Application | Capture, player, board, Wi-Fi, SNTP, IoT credentials |
| `esp_llm_service` | Streams, uplink tasks, downlink provider, AFE attach |
| `esp_llm_volc` | RTC engine, IoT signing, room join |

The adapter never creates capture or player. `WAKEUP` / `VAD` come from
service `audio_ai`.

## Credentials

Required on `cfg.iot`:

| Field | Source |
|---|---|
| `instance_id` | Volc IoT instance |
| `product_key` | IoT product key |
| `product_secret` | IoT product secret |
| `device_name` | Device name |
| `bot_id` | Conversational bot ID |

RTC `GetRTCConfig` is signed with the device clock. Sync SNTP after Wi-Fi
before `esp_service_start()`, or the join fails with a signature error.

The transport is always RTC; there is no mode field.
`cfg.engine.config_json_raw` is optional; the adapter builds JSON from IoT
fields when it is NULL.

The public header does not include `volc_conv_ai.h`. Applications never need
the Volc SDK; the adapter keeps it private.

## Streams

`ESP_LLM_VOLC_CFG_DEFAULT()` enables audio and, depending on component
Kconfig, video publish / subscribe. The product example turns video off
after the default:

```c
esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = false;
cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = false;
```

Audio codec comes from Kconfig (Opus / G.711A / G.711U / PCM). G.711 is
8 kHz on the wire; the example keeps the I2S / AFE clock at 16 kHz and
encodes down.

| Stream | Adapter | Product example |
|---|---|---|
| `AUDIO_UP` / `AUDIO_DN` | Yes | Enabled |
| `VIDEO_UP` / `VIDEO_DN` | Yes if enabled | Disabled |

Capabilities: `AUDIO_INPUT`, `AUDIO_OUTPUT`, `VIDEO_INPUT`, `VIDEO_OUTPUT`,
`TEXT_INPUT`, `CUSTOM_DATA`, `CANCEL_RESPONSE`.

`esp_llm_service_send_text()` sends a chat query the model answers.
`esp_llm_service_send_custom_data()` sends a backend-native RTS message.

## Application Sequence

```c
esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
cfg.iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
cfg.iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
cfg.iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
cfg.iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
cfg.iot.bot_id = CONFIG_VOLC_BOT_ID;
cfg.service.audio_ai_cfg.capture = capture;
cfg.service.audio_ai_cfg.feature_mask = ai_features;
cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_UP].enabled = false;
cfg.service.streams[ESP_LLM_MEDIA_STREAM_VIDEO_DN].enabled = false;

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));
/* link AUDIO_UP / AUDIO_DN, subscribe, then start player → LLM → capture */
```

## Events

| `common_id` | Typical `backend_event_name` | Payload |
|---|---|---|
| `CONNECTED` | `CONNECTED` | RTC joined |
| `MEDIA_READY` | `MEDIA_READY` | media path ready |
| `DISCONNECTED` | `DISCONNECTED` | room left |
| `TEXT` | `TEXT` | subtitle / transcript when present |
| `ERROR` | `ERROR` | text |
| `FINISHED` | — | answer finished |
| `WAKEUP` / `VAD` | — | from AFE |

Raw backend names:

| Name | Payload |
|---|---|
| `CONV_STATUS` | status name text: `Listening` / `Thinking` / `Answering` / `Interrupted` / `AnswerFinish` |
| `MESSAGE` | JSON from the engine |

`CONV_STATUS` carries the name as text, not the SDK enum, so applications do
not need `volc_conv_ai.h`. `FINISHED` follows `Interrupted` and `AnswerFinish`.

Payload pointers are valid only during the handler.

## Backend-Specific API

| Function | Wraps | Notes |
|---|---|---|
| `esp_llm_volc_send_text_to_tts()` | `volc_send_text_to_agent(TTS)` | Speaks text as-is; not a chat query |

## Escape Hatch

`esp_llm_volc_backend_acquire()` / `esp_llm_volc_backend_release()` borrow the
raw `volc_engine_t` for SDK features this component does not wrap. The borrow
holds the engine mutex, so the pair must run on one thread with only short work
between them, and nothing else may touch the service meanwhile. A missing
release deadlocks destroy. Prefer the wrappers and the generic
`esp_llm_service_*` APIs.

## Wake-Up

Enable `ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN`. Then hold uplink closed
until `ESP_LLM_EVENT_WAKEUP` state is 1, and call
`esp_llm_service_set_wakeup_cancel_response(llm, true)` so a new wake-up
stops the current TTS.

`esp_llm_service_cancel_response()` maps to the RTC interrupt path.

## Review Checklist

- All five IoT fields and `bot_id` are non-empty
- SNTP completed before LLM start
- Product audio-only builds keep both video streams disabled
- G.711 builds keep the I2S / AFE clock at 16 kHz
- Event handlers do not keep payload pointers
- Control-plane calls stay on one thread
- Every `backend_acquire()` has a matching `backend_release()` on the same thread
- Application code does not include `volc_conv_ai.h`
