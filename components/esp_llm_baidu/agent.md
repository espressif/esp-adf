# ESP LLM Baidu Agent Guide

Use this when implementing or reviewing an application on `esp_llm_baidu`.

The shared LLM contract is in
[`../esp_llm_service/agent.md`](../esp_llm_service/agent.md). This file covers
the Baidu RTC backend only.

## What It Is

`esp_llm_baidu` wraps the prebuilt BRTC SDK (`components/brtc`) as an
`esp_llm_service` adapter. It exposes `esp_llm_baidu_create()`.

Official platform docs:

- [Baidu RTC](https://cloud.baidu.com/doc/RTC/index.html)
- [RTC best practices](https://cloud.baidu.com/doc/RTC/s/Fmj75h7uj)
- SDK notes in `components/brtc/BRTC_SDK_NOTES.md`

Supported chips: **ESP32-S3**, **ESP32-S31**, **ESP32-P4**.

Product example: `adf_examples/ai_agent/esp_llm_baidu_agent_app`.
In-component: `examples/baidu_rtc_agent` and `examples/baidu_rtc_intercom`.

Only **one** Baidu adapter instance may exist. BRTC C callbacks have no user
context; a second `esp_llm_baidu_create()` returns `ESP_ERR_INVALID_STATE`.

## Modes

| Mode | Use |
|---|---|
| `ESP_LLM_BAIDU_MODE_AGENT` | Chat-agent engine (ASR / TTS / vision / function call) |
| `ESP_LLM_BAIDU_MODE_INTERCOM` | P2P RTC room |

The product example is agent mode. Intercom fills `cfg.intercom` instead of
`cfg.agent`.

## Ownership

| Owner | Owns |
|---|---|
| Application | Capture, player, board, Wi-Fi, SPIFFS prompt, FunctionCall handling |
| `esp_llm_service` | Streams, uplink tasks, downlink provider, AFE attach |
| `esp_llm_baidu` | BRTC engine, join / reconnect, vision snapshot request, TTS speak |

The adapter never creates capture or player. `WAKEUP` / `VAD` come from
service `audio_ai`, not BRTC.

## Agent Credentials

Required on `cfg.agent.account`:

| Field | Source |
|---|---|
| `app_id` | Baidu RTC / agent console |
| `user_id` | Device or user identifier |
| `server_url` | Agent platform URL |
| `workflow` | Workflow name (default `VoiceChat`) |
| `instance_id` | Agent instance ID (`> 0`) |
| `license_key` | Optional; empty string is allowed |

Optional engine fields: region, greeting, voice-interrupt, reconnect delay /
attempts, `wakeup_interrupt_cmd`.

## Scenes And Streams

`ESP_LLM_BAIDU_CFG_AGENT_DEFAULT()` enables audio only (PCM or OPUS @ 16 kHz).
Turn video on per scene:

| Scene | `VIDEO_UP` | `VIDEO_DN` | Typical use |
|---|---|---|---|
| `VOICE_CHAT` | off | off | Talk only |
| `IMAGE_UNDERSTANDING` | on | off | Camera JPEG to the agent |
| `IMAGE_GENERATION` | off | on | Generated image on the LCD player |
| `MULTI_MODAL` | on | on | Both, plus optional local preview |

Vision uplink mode (`cfg.vision.vision_mode`):

- `IMAGE` — snapshot when the engine asks (`VISION_IMAGE_REQUEST`)
- `STREAM` — continuous camera frames on `VIDEO_UP`

Camera JPEG size is independent of the LCD. Fill
`cfg.service.streams[VIDEO_UP].info.info.video` before create.

Static capabilities include audio, text, vision, function-call, video in/out.
`esp_llm_service_get_capabilities()` intersects that mask with enabled streams.

## Agent Audio

`AudioIncodecType` is the `send_audio` payload. `rtc_ac` and
`AudioOutcodecType` name the wire codec.

| Preset | App payload (`send_audio` / `onAudioData`) | `AudioIn` | SDK `rtc_ac` / `AudioOut` | Baidu lib |
|---|---|---|---|---|
| OPUS (default) | OPUS @ 16 kHz | `opus` | `opus` | No transcode (passthrough) |
| PCM | PCM @ 16 kHz | `pcm` | `g722` | Encodes and decodes G.722 |

`ESP_LLM_BAIDU_AGENT_AUDIO_CODEC` selects the preset. The application still
owns capture and player; the adapter does not open the codec device.

## Extra API

All three are agent-mode only; intercom returns `ESP_ERR_NOT_SUPPORTED`. They are
control-plane calls and must not overlap with other control calls.

- `esp_llm_baidu_send_text_to_tts(service, text)` speaks UTF-8 through the agent
  TTS path. It is **not** a chat query. Use it for local prompts such as volume
  confirmations.
- `esp_llm_baidu_send_function_call_result(service, id, result)` answers a
  `FUNCTION_CALL` event. Pass `NULL` for `id` to answer the most recent call; the
  backend raises them one turn at a time. Pass an explicit id when the answer is
  produced asynchronously.
- `esp_llm_baidu_set_enhance_query(service, type, pre, post)` wraps every user
  query with extra context. `ESP_LLM_BAIDU_ENHANCE_QUERY_NONE` clears it.

`esp_llm_service_send_custom_data()` is wired for both modes: agent forwards the
string as an engine event (`[E]:` / `[SET]:` commands), intercom broadcasts it to
the room.

## Escape Hatch

`esp_llm_baidu_backend_acquire()` / `..._release()` lend the raw
`BaiduChatAgentEngine *` and RTC client handles for SDK calls this component does
not wrap. Handles are `void *` in the header; include
`baidu_chat_agents_engine.h` / `baidu_rtc_client.h` and cast.

The borrow holds the engine mutex and pins the adapter context:

- Pair acquire / release on one thread with only short work in between
- Do not start, stop, or destroy the service while holding it
- Do not call any other `esp_llm_baidu_*` / `esp_llm_service_*` API while holding it
- Do not nest borrows, and do not store the handles past release
- A missing release deadlocks destroy

## Application Sequence

```c
esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
cfg.agent.account.app_id = CONFIG_BAIDU_APP_ID;
cfg.agent.account.user_id = CONFIG_BAIDU_USER_ID;
cfg.agent.account.server_url = CONFIG_BAIDU_SERVER_URL;
cfg.agent.account.workflow = CONFIG_BAIDU_WORKFLOW;
cfg.agent.account.instance_id = CONFIG_BAIDU_INSTANCE_ID;
cfg.agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
cfg.service.audio_ai_cfg.capture = capture;
cfg.service.audio_ai_cfg.feature_mask = ai_features;

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_baidu_create(&cfg, &llm));
/* link enabled streams, subscribe, then start player → LLM → capture */
```

When the LCD player is used, TTS audio and generated video share one player.

## Events

| `common_id` | Typical `backend_event_name` | Payload |
|---|---|---|
| `CONNECTED` | `CALL_STATE` / `CONNECTION_STATE` | join / connected |
| `DISCONNECTED` | `CONNECTION_STATE` | engine down |
| `MEDIA_READY` | `MEDIA_SETUP` | media path ready |
| `TEXT` | `ASR` / `AGENT_SUBTITLE` | user or assistant text |
| `ERROR` | `ERROR` / `CALL_STATE` / `RECONNECT_FAILED` | text |
| `WAKEUP` / `VAD` | — | from AFE |

Raw backend names (`ESP_LLM_EVENT_NONE`):

| Name | Meaning |
|---|---|
| `FUNCTION_CALL` | JSON; application executes the tool, then answers with `esp_llm_baidu_send_function_call_result()` |
| `AUDIO_PLAYER_START` / `AUDIO_PLAYER_STOP` | URL mix start / stop |
| `SPEAKING` | assistant speaking flag |
| `VISION_IMAGE_REQUEST` / `VISION_IMAGE_ACK` | snapshot request / ack |
| `MEDIA_GENERATE_RESULT` / `MEDIA_GENERATE_ACK` | generated image |
| `LICENSE` | license check result |
| `AGENT_EVENT` | other engine JSON |

The product example handles `FUNCTION_CALL` `adjust_volume` and
`AUDIO_PLAYER_*` URL mix. Payload pointers are valid only during the handler.

## Wake-Up

Typical product flow (see the example):

1. Enable WakeNet in `feature_mask`
2. On `ESP_LLM_EVENT_WAKEUP` start: `keep_awake(true)`, `cancel_response()`,
   play a local ding, stop any music mix
3. After the ding finishes: `cancel_response()` again (sends the listen
   command) and `keep_awake(false)`

`cfg.agent.wakeup_interrupt_cmd` defaults to `[B]:[BEGIN]:0`. Do not send
BEGIN before the ding, or the 2000 ms skip eats the prompt.

## Review Checklist

- Only one Baidu instance exists
- Account fields are complete; `instance_id > 0`
- Scene stream bits match camera / LCD hardware
- `VISION` snapshot vs stream matches `cfg.vision.vision_mode`
- FunctionCall and URL mix stay in the application
- TTS confirmations use `esp_llm_baidu_send_text_to_tts()`, not `send_text`
- Event handlers do not keep payload pointers
- Backend borrows are paired on one thread and hold no lifecycle calls
