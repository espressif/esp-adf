# ESP LLM Tongyi Agent Guide

Use this when implementing or reviewing an application on `esp_llm_tongyi`.

The shared LLM contract (ownership, streams, start order, AFE, events) is in
[`../esp_llm_service/agent.md`](../esp_llm_service/agent.md). This file covers
the Tongyi backend only.

## What It Is

`esp_llm_tongyi` is the DashScope WebSocket adapter for `esp_llm_service`.
It wraps `components/esp_tongyi` and exposes `esp_llm_tongyi_create()`.

Official platform docs:

- [DashScope multimodal WebSocket](https://help.aliyun.com/zh/model-studio/websocket-for-multimodal-dialogue)
- [DashScope API key](https://help.aliyun.com/zh/model-studio/get-api-key)

Default endpoint: `wss://dashscope.aliyuncs.com/api-ws/v1/inference`.

Product example: `adf_examples/ai_agent/esp_llm_tongyi_agent`.
In-component chat: `examples/tongyi_chat`.

## Ownership

| Owner | Owns |
|---|---|
| Application | Capture, player, board, Wi-Fi, SNTP, `task_id`, one-shot JPEG |
| `esp_llm_service` | Streams, uplink tasks, downlink provider, AFE attach |
| `esp_llm_tongyi` | `esp_tongyi` session, Start payload, image `UpdateInfo` / `RequestToRespond` |

The adapter never creates capture or player. It does not emit `WAKEUP` or `VAD`;
those come from service `audio_ai`.

## Credentials

Required on `esp_llm_tongyi_cfg_t`:

| Field | Source |
|---|---|
| `api_key` | DashScope console |
| `workspace_id` | Tongyi workspace |
| `app_id` | Tongyi application |
| `task_id` | Optional. The example generates one with `esp_tongyi_generate_uuid()` |

TLS needs a valid clock. Sync SNTP after Wi-Fi before `esp_service_start()`.

`connect_timeout_ms` is the wait for connected / `Started`. Component Kconfig
under `ESP Tongyi` also sets WebSocket buffer, send, and image limits.

## Streams

`ESP_LLM_TONGYI_CFG_DEFAULT()` enables `AUDIO_UP` and `AUDIO_DN` (Opus, 16 kHz).
`VIDEO_UP` is present but disabled. `VIDEO_DN` is not supported.

| Stream | Supported | Notes |
|---|---|---|
| `AUDIO_UP` | Yes | Linked capture. Format comes from `cfg.service.streams[]` |
| `AUDIO_DN` | Yes | TTS Opus through the service downlink provider |
| `VIDEO_UP` | No | Rejected if enabled |
| `VIDEO_DN` | No | Not advertised |

Vision is **one-shot**, not a linked camera stream. Push a JPEG with
`esp_llm_service_send_frame()`.

Capabilities: `AUDIO_INPUT`, `AUDIO_OUTPUT`, `CANCEL_RESPONSE`, `VISION`.
`TEXT_INPUT` and `CUSTOM_DATA` are not advertised (`send_text` /
`send_custom_data` are NULL).

## Application Sequence

```c
esp_llm_tongyi_cfg_t cfg = ESP_LLM_TONGYI_CFG_DEFAULT();
cfg.api_key = CONFIG_TONGYI_API_KEY;
cfg.workspace_id = CONFIG_TONGYI_WORKSPACE_ID;
cfg.app_id = CONFIG_TONGYI_APP_ID;
cfg.task_id = task_id;
cfg.service.audio_ai_cfg.capture = capture;
cfg.service.audio_ai_cfg.feature_mask = ai_features;

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_tongyi_create(&cfg, &llm));
/* link AUDIO_UP / AUDIO_DN, subscribe, then start player → LLM → capture */
```

Start order and AFE rules are the same as `esp_llm_service`.

## Vision

Tongyi does not use RTC video. Capture one JPEG locally (the example caches
the last complete frame from the camera and shows LCD preview by writing MJPEG
to `esp_video_render`). Then:

```c
esp_media_frame_t image = {
    .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    .data = jpeg_buf,
    .size = jpeg_len,
    .user_data = (void *)prompt,   /* NULL = UpdateInfo only */
};
esp_llm_service_send_frame(llm, &image);
```

- Non-NULL `user_data` string → `RequestToRespond` with that prompt
- NULL `user_data` → `UpdateInfo` (session image only)

Vision apps that route to LiveAI / `video_chat` also need Start
`upstream.type = AudioAndVideo`. The adapter then sends
`voicechat_video_channel` connect (探索视界). Without that the cloud
replies “请先进入探索视界” and ignores the JPEG.

`frame->data` is valid only during `send_frame()`. Do not also link `VIDEO_UP`.

## Events

Subscribe with `esp_service_event_subscribe()`. Common IDs:

| `common_id` | Typical `backend_event_name` | Payload |
|---|---|---|
| `CONNECTED` | `CONNECTED` | none |
| `DISCONNECTED` | `DISCONNECTED` | none |
| `MEDIA_READY` | `SESSION_READY` | session is Started |
| `TEXT` | `SPEECH_CONTENT` / `RESPONDING_CONTENT` | user ASR or assistant text |
| `ERROR` | `ERROR` | JSON from the platform |
| `FINISHED` | — | chat turn complete |
| `WAKEUP` / `VAD` | — | from AFE, not Tongyi |

Raw backend names (`ESP_LLM_EVENT_NONE`):

| Name | Payload |
|---|---|
| `DIALOG_STATE` | `esp_tongyi_dialog_state_t` (`Idle` / `Listening` / `Thinking` / `Responding`) |
| `RESPONDING_STARTED` / `RESPONDING_ENDED` | none |

Payload pointers are valid only during the handler.

## Dialog And Wake-Up

- Direct dialog: keep `AUDIO_UP` enabled; server VAD plus local AFE VAD
- Wake word: set `ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN`, call
  `esp_llm_service_set_wakeup_cancel_response(llm, true)`, and hold uplink
  closed with `esp_llm_service_enable_uplink_track(..., AUDIO, false)` until
  `ESP_LLM_EVENT_WAKEUP` state is 1

`esp_llm_service_cancel_response()` stops the current TTS turn.

## Review Checklist

- App owns capture / player; adapter only sees borrowed capture for AFE
- `api_key`, `workspace_id`, `app_id` are non-empty; SNTP ran before start
- `VIDEO_UP` stays disabled; images go through `send_frame(VIDEO)`
- JPEG `user_data` is a prompt or NULL; buffer is not retained after send
- Event handlers do not keep payload pointers
- Control-plane calls stay on one thread
