# ESP LLM Service Agent Guide

Use this when implementing or reviewing applications and adapters on `esp_llm_service`.

## What It Is

`esp_llm_service` is one AI session. The application owns capture and player. The service owns the backend adapter, four media streams, events, and optional AFE attach.

Core files:

- `include/esp_llm_service.h` — create/destroy, send, cancel, capabilities
- `include/esp_llm_adapter.h` — adapter descriptor, ops, stream IDs, capability bits
- `include/esp_llm_common.h` — events, audio-AI policy, uplink task names
- `include/esp_llm_service_mcp.h` — optional MCP tools
- `private_include/esp_llm_adapter_utils.h` — adapter-only helpers: PTS derivation and `esp_llm_adapter_ctx_get()`

Backend-specific public APIs (for example `esp_llm_volc_send_text_to_tts()`)
resolve their adapter context with `esp_llm_adapter_ctx_get(service, name, &ctx)`.
It rejects a mismatched adapter name, so one backend's API cannot run against
another backend's service. State checks and locking stay with the adapter.

Lifecycle uses `esp_service_start()` / `esp_service_stop()` on `ESP_SERVICE_BASE(llm)`. Do not invent LLM-specific start/stop APIs.

Verify the contract with `test_apps` (unity + test-only mock). Product backends live in `esp_llm_baidu`, `esp_llm_volc`, `esp_llm_coze`, `esp_llm_tongyi`.

## Ownership

| Owner | Owns | Does not |
|---|---|---|
| Application | Capture, player, board, Wi-Fi, link/unlink, start/stop of media | Adapter SDK |
| `esp_llm_service` | Adapter context, four streams, uplink tasks, downlink provider, event publish | Capture/player objects |
| Adapter | Vendor SDK session | Capture/player, AFE, WAKEUP/VAD |

`audio_ai_cfg.capture` is borrowed. LLM attaches AFE on start and detaches on stop. It never creates, links, starts, stops, unlinks, or destroys capture/player.

## Application Sequence

1. Fill typed `cfg.service.streams[]` (DEFAULT macros pre-fill formats) and create capture/player from those `info` fields
2. Fill remaining `cfg.service` fields (name, optional `audio_ai_cfg`, optional downlink callbacks)
3. `esp_llm_<backend>_create(&cfg, &llm)`
4. Optionally `esp_llm_service_get_stream_info()` after create
5. `esp_media_service_link()` for the streams you use
6. `esp_service_event_subscribe()`
7. Start player, then LLM, then capture
8. `send_text` / `send_frame` / `send_custom_data` / `cancel_response` as needed
9. Stop capture, then LLM, then player
10. Unlink, `esp_llm_service_destroy(&llm)`, destroy media

```c
esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
const esp_media_track_info_t *audio_up =
    &cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_UP].info;
const esp_media_track_info_t *audio_dn =
    &cfg.service.streams[ESP_LLM_MEDIA_STREAM_AUDIO_DN].info;

/* App creates capture/player from those track formats. */
cfg.service.audio_ai_cfg.capture = capture;
cfg.service.audio_ai_cfg.feature_mask = ESP_AUDIO_CAPTURE_SERVICE_AI_FEATURE_WN;

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));
ESP_ERROR_CHECK(esp_llm_service_set_wakeup_cancel_response(llm, true));

ESP_ERROR_CHECK(esp_media_service_link(
    ESP_SERVICE_BASE(capture), APP_CAPTURE_STREAM,
    ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_UP));
ESP_ERROR_CHECK(esp_media_service_link(
    ESP_SERVICE_BASE(llm), ESP_LLM_MEDIA_STREAM_AUDIO_DN,
    ESP_SERVICE_BASE(player), APP_PLAYER_STREAM));

ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(player)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(llm)));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(capture)));
```

Stop is the reverse of start. `audio_ai_cfg` requires capture stopped before LLM so AFE can detach.

## Media Streams

`esp_media_service_link()` uses these LLM stream IDs:

| ID | Direction | LLM role |
|---|---|---|
| `ESP_LLM_MEDIA_STREAM_AUDIO_UP` | capture → LLM | sink |
| `ESP_LLM_MEDIA_STREAM_VIDEO_UP` | camera → LLM | sink |
| `ESP_LLM_MEDIA_STREAM_AUDIO_DN` | LLM → player | source |
| `ESP_LLM_MEDIA_STREAM_VIDEO_DN` | LLM → display | source |

A stream exists only if it is enabled in `cfg.streams[]` **and** the adapter advertises the matching capability. Enabling a stream the adapter cannot serve returns `ESP_ERR_NOT_SUPPORTED` at create. `esp_llm_service_get_capabilities()` returns this intersection, not the static descriptor mask. After create, `esp_llm_service_get_stream_info()` returns the stored format or `ESP_ERR_NOT_FOUND` if the stream is disabled.

Link after create and before start. Unlink after stop and before destroy. Linking an uplink while RUNNING or ERROR returns `ESP_ERR_INVALID_STATE`.

## Uplink

On LLM start, each enabled and linked uplink runs its own consume task: `acquire_frame` → adapter `send_frame` → `release_frame`.

- Do not also call `esp_llm_service_send_frame()` for the same streaming path.
- `send_frame()` is for application-pushed audio/video or one-shot vision.
- `ESP_LLM_CAP_VISION` accepts a VIDEO frame without `video_up`. `ESP_LLM_CAP_VIDEO_INPUT` is streamed video.
- IMAGE-mode backends may return `ESP_OK` while dropping frames until they request a snapshot.
- `frame->data` and `frame->user_data` are valid only during `send_frame()`. PTS is not required on this uplink path.
- `esp_llm_service_enable_uplink_track()` gates audio or video send without stopping capture. It is thread-safe, may be called from a wake-up handler, and persists across stop/start.

## Downlink

Pick one or both:

- **Provider (default):** LLM exports one shared dual-track provider. App links `AUDIO_DN` (or `VIDEO_DN` if audio is unused) once to `esp_video_player_service`.
- **Callback tap:** set `cfg.service.downlink_cb`. It observes every downlink frame (`frame->type` is AUDIO or VIDEO) and does not replace the provider. Link a player as well if the application still wants playback. Frame is valid only during the callback; copy if it must outlive the call.

Adapters must fill `frame->pts` (`esp_llm_pts_from_pcm()` / `esp_llm_pts_from_video_fps()`). `ESP_LLM_PTS_ABSENT` is `-1`; 0 ms is a valid start. If DTS is not independent, set `dts` to PTS or `ESP_LLM_PTS_ABSENT`. Call `esp_llm_service_set_downlink_cache_size()` before link if needed; it is rejected after the provider is exported.

`ESP_MEDIA_FRAME_FLAG_EOS` is forwarded. `ESP_MEDIA_FRAME_FLAG_KEY` marks a new TTS utterance or video stream (reset the PTS cursor).

## Audio AI

Zero-initialize `audio_ai_cfg` to disable it.

When enabled:

- `capture` is required and must not be RUNNING at LLM start (attach fails with `ESP_ERR_INVALID_STATE`)
- Start LLM before capture; stop capture before LLM
- WAKEUP / VAD / DOA come from AFE callbacks in the service, not from the adapter
- `esp_llm_service_set_wakeup_cancel_response()` calls `esp_llm_service_cancel_response()` on wake-up start; `feature_mask` must include WakeNet
- Uplink send is not tied to wake-up. Hold it closed with `esp_llm_service_enable_uplink_track()` if the product needs that
- Button / hold-to-talk: `esp_llm_service_trigger_wakeup()`, `esp_llm_service_keep_awake()`, `esp_llm_service_trigger_sleep()` after both LLM and capture are running
- `ESP_LLM_EVENT_WAKEUP` payload is `esp_llm_wakeup_info_t` (`state` 1 start / 0 end)
- `ESP_LLM_EVENT_VAD` payload is `int` (AFE VAD state)
- DOA is published as backend-raw `"DOA"` with a `float` payload

Only one RUNNING instance should hold AFE on a shared capture.

## Events

Subscribe with `esp_service_event_subscribe()`. Hub payload is `esp_llm_event_t`.

- Common IDs: WAKEUP (`esp_llm_wakeup_info_t`), VAD (`int`), CONNECTED, DISCONNECTED, MEDIA_READY, TEXT, ERROR, FINISHED
- Raw backend events: `common_id = ESP_LLM_EVENT_NONE`, hub ID `ESP_LLM_EVENT_BACKEND_RAW`, name in `backend_event_name`
- Downlink audio/video is not an event
- `payload` and string fields are valid only during the handler
- Handlers run on the event-hub context; return quickly

## Send And Cancel

Call only while the service is RUNNING, from the application control thread (`send_*` / `cancel_response` / trigger_*). Empty `send_text()` and `send_custom_data()` strings return `ESP_ERR_INVALID_ARG`. `enable_uplink_track` may also run from the event-hub context.

- `esp_llm_service_send_text()`
- `esp_llm_service_send_frame()`
- `esp_llm_service_send_custom_data()` — forwarded verbatim
- `esp_llm_service_cancel_response()` — stop current TTS/generation, keep the session
- `esp_llm_service_trigger_wakeup()` / `keep_awake()` / `trigger_sleep()` — button and hold-to-talk

Unsupported capability returns `ESP_ERR_NOT_SUPPORTED`.

## Threading

- Control-plane APIs (`create`, `destroy`, `send_*`, `cancel_response`, getters, trigger_*) are single-threaded and must not overlap
- `esp_llm_service_enable_uplink_track()` is thread-safe and may be called from an event-hub handler
- Uplink tasks and audio-AI callbacks may call adapter `send_frame` / `cancel_response` concurrently; adapters must be thread-safe on the data plane
- `esp_service_stop()` before destroy
- MCP start/stop/send tools are debug-only and must not race the control thread

## Adapter Contract

Expose a static `esp_llm_adapter_desc_t` and a typed wrapper such as `esp_llm_volc_create()`.

Descriptor:

- `api_version` — adapter ABI for this service (for example `"1.0"`), not the vendor SDK version
- `name` — unique backend ID
- `capabilities` — static bitmask (`esp_llm_capability_t`), OR of supported features. `esp_llm_service_get_capabilities()` intersects this with `cfg.streams[]` at create and does not follow runtime gates
- `ops` — `create` / `start` / `stop` / `destroy` required; send/cancel optional and NULL if unsupported. `create()` must validate `cfg.streams[]` and reject unsupported codec/rate/channel with `ESP_ERR_NOT_SUPPORTED`

Ops rules:

- Typed cfg is valid only during `create()`; copy what you need
- `stop()` must silence callbacks before it returns
- Do not emit `ESP_LLM_EVENT_WAKEUP` or `ESP_LLM_EVENT_VAD`; those are service `audio_ai`

## Multiple Instances

One `esp_llm_service_t` is one agent. Multiple instances may exist. Shared mic/speaker: one capture/player, borrow the same handles.

Switching agents:

```text
stop capture → stop LLM-A → destroy LLM-A → create LLM-B → link → start player → start LLM-B → start capture
```

Two instances must not attach AFE to the same capture at the same time.

## Review Checklist

- App creates, links, starts, stops, unlinks, and destroys capture/player
- Stream formats come from `cfg.service.streams[]`, not from an adapter getter
- Start order is player → LLM → capture; stop is capture → LLM → player
- No duplicate `send_frame()` on a linked uplink
- Downlink is either provider link or callback, not both
- `audio_ai_cfg.capture` is set only when AFE is needed; capture is not RUNNING at LLM start
- Event handlers do not retain `payload` pointers
- Control-plane calls stay on one thread
- Adapter `stop()` silences callbacks; cfg is copied at `create()`
- Adapter does not forge WAKEUP/VAD
- Destroy failure keeps the handle for retry
