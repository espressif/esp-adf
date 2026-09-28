# ESP LLM Tongyi

- [中文版](./README_CN.md)

Tongyi backend adapter for [esp_llm_service](../esp_llm_service). It wraps the DashScope multimodal WebSocket SDK bundled in `components/esp_tongyi` and implements the `esp_llm_adapter.h` contract, so an application drives a Tongyi voice or vision session through the generic `esp_llm_service_*` API and one typed create function.

The adapter owns the DashScope session only. Capture and player objects stay with the application; the adapter consumes uplink frames handed to it by the service and publishes TTS frames back through the service downlink.

## Features

- DashScope multimodal dialog over a TLS WebSocket, with duplex audio
- Uplink audio: OPUS at `ESP_LLM_TONGYI_SAMPLE_RATE`, default 16 kHz mono at 24000 bit/s
- Downlink audio: OPUS at the same rate, default 32 kbit/s with 60 ms frames
- Upstream type selectable between audio only and audio plus video for vision applications
- One-shot image input: push a JPEG as a VIDEO `esp_media_frame_t` through `esp_llm_service_send_frame()`
- Image intent chosen per frame: a prompt string in `frame.user_data` requests a reply, NULL updates the session image only
- Text input through `esp_llm_service_send_text()` and cancellation through `esp_llm_service_cancel_response()`
- Dialog state and spoken response text forwarded as `esp_llm_service` events
- Audio AI features attached to the application capture: AEC, NS, VAD, and WakeNet
- No RTC video track; `VIDEO_UP` stays disabled and no `video_up` slot is required

## Directory Structure

```
esp_llm_tongyi/
├── include/esp_llm_tongyi.h       Typed configuration, DEFAULT macro, and create function
├── src/esp_llm_tongyi_adapter.c   Adapter implementation over esp_tongyi
├── components/esp_tongyi/         DashScope WebSocket SDK
├── examples/tongyi_chat/          In-component chat example
├── test_apps/                     Offline create, stream info, and control-plane tests
├── Kconfig                        Audio profile options
└── agent.md                       Adapter implementation, events, and vision guide
```

## Quick Start

The product example is [`adf_examples/ai_agent/esp_llm_tongyi_agent`](../../adf_examples/ai_agent/esp_llm_tongyi_agent). It keeps media ownership in the application and demonstrates both the dialog and the vision flows:

```bash
cd adf_examples/ai_agent/esp_llm_tongyi_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi plus DashScope API key, workspace ID, and application ID
idf.py build flash monitor
```

The application sequence is Wi-Fi, then SNTP because the TLS handshake needs a valid clock, then create and configure capture and player from `cfg.service.streams[]`, then fill `cfg.service.audio_ai_cfg.capture`, then `esp_llm_tongyi_create()`, then link, subscribe, and start player, LLM, capture in that order.

The minimal adapter usage is:

```c
esp_llm_tongyi_cfg_t cfg = ESP_LLM_TONGYI_CFG_DEFAULT();
cfg.api_key = CONFIG_TONGYI_API_KEY;
cfg.workspace_id = CONFIG_TONGYI_WORKSPACE_ID;
cfg.app_id = CONFIG_TONGYI_APP_ID;
cfg.task_id = task_id;
cfg.service.audio_ai_cfg.capture = capture;           /* application-owned */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* zero disables audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_tongyi_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/tongyi_chat` is a smaller in-component example that exercises the adapter without the full application media graph.

## Vision

Tongyi does not stream RTC video. Vision is one-shot: the application pushes a single JPEG as a VIDEO `esp_media_frame_t`, so no `video_up` slot has to be enabled. The adapter reads `frame.user_data` to decide the intent. A non-NULL string is the respond prompt and produces `RequestToRespond`; NULL sends `UpdateInfo`, which refreshes the session image without asking for a reply.

```c
esp_media_frame_t image = {
    .type = ESP_MEDIA_TRACK_TYPE_VIDEO,
    .data = jpeg_buf,
    .size = jpeg_len,
};

/* Respond with this image */
image.user_data = (void *)"what is this?";
esp_llm_service_send_frame(llm, &image);

/* Update the session image only */
image.user_data = NULL;
esp_llm_service_send_frame(llm, &image);
```

Vision applications also need `cfg.upstream.type` set to `ESP_TONGYI_UPSTREAM_TYPE_AUDIO_VIDEO`, and a DashScope application that accepts images. A voice-only application ID answers `images must not be empty` when it is used with a multimodal prompt.

## Configuration

`esp_llm_tongyi_create()` requires `api_key`, `workspace_id`, and `app_id`. The remaining fields of `esp_llm_tongyi_cfg_t` are filled by `ESP_LLM_TONGYI_CFG_DEFAULT()`:

| Field | Purpose |
| --- | --- |
| `api_key` | DashScope API key; required |
| `workspace_id` | DashScope workspace, in the `llm-` form; required |
| `app_id` | DashScope application; voice-only or multimodal `mm_` |
| `task_id` | Session task identifier reported to DashScope |
| `upstream.type` | Audio only, or audio plus video for vision applications |
| `service` | Shared `esp_llm_service_cfg_t`: streams, audio AI, downlink callback, and name |

Component Kconfig lives under `ESP LLM Tongyi`:

| Option | Default |
| --- | --- |
| `ESP_LLM_TONGYI_SAMPLE_RATE` | 16000 Hz, used by both uplink and downlink streams |
| `ESP_LLM_TONGYI_FRAME_SIZE_MS` | 60 ms downstream Opus frame |
| `ESP_LLM_TONGYI_BIT_RATE_KBPS` | 32 kbit/s downstream |
| `ESP_LLM_TONGYI_CAPTURE_UPLINK_BITRATE` | 24000 bit/s uplink |

## Notes

- Sync the clock with SNTP after Wi-Fi. The TLS handshake fails with a stale clock.
- `VIDEO_UP` is disabled by the DEFAULT macro and should stay that way; vision uses `send_frame()` instead of a linked video provider.
- Link every uplink before `esp_service_start()`; linking a running service returns `ESP_ERR_INVALID_STATE`.
- Start the player before the LLM and the capture after it, so the AFE sidecar can attach. Stop in reverse.
- Downlink TTS is written through the service downlink path, so a player must be linked to `AUDIO_DN` for audio to be heard.
- The JPEG buffer passed to `send_frame()` must stay valid for the duration of the call; the adapter does not take ownership.
- The API key is a credential. Keep it out of committed defaults in product builds.

## Performance

| Item | Value |
| --- | --- |
| Uplink audio | OPUS, 16 kHz mono, 24000 bit/s |
| Downlink audio | OPUS, 16 kHz mono, 32 kbit/s, 60 ms frames |
| Vision | One JPEG per `send_frame()` call; no continuous video track |
| Transport | One TLS WebSocket connection per session |
| TLS buffer | The product example raises `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` to 16384 and requires PSRAM |
| Application partition | 6 MB in the product example |

## SoC Compatibility

Validated on ESP32-S3, ESP32-S31, and ESP32-P4 with a board that provides `audio_adc` and `audio_dac`. Vision additionally needs a camera and a display on the board, and on ESP32-S3 the JPEG preview decode is done in software because the chip has no hardware JPEG decoder. PSRAM is required for the TLS and audio buffers.

`test_apps` covers create, destroy, stream info, and control-plane errors without Wi-Fi. Generate board headers with `idf.py bmgr -b <board>` before building it, because `esp_llm_service` still compiles `esp_audio_capture_service`.

## FAQ

**Why is `VIDEO_UP` disabled even in vision mode?**

Tongyi expects discrete images on the dialog channel rather than a continuous RTC video track. Keeping `VIDEO_UP` disabled avoids creating an uplink consume task that would have nothing to forward, and `send_frame()` delivers the image directly.

**How is a question about the image distinguished from a silent image update?**

By `frame.user_data`. A non-NULL prompt string produces `RequestToRespond` and the model answers; NULL produces `UpdateInfo` and only refreshes the stored image.

**The model answers `images must not be empty`.**

A multimodal `mm_` application ID was used without ever sending an image. Either send a JPEG with `send_frame()`, or configure a voice-only application ID for audio-only products.

**Can the adapter accept RGB frames instead of JPEG?**

No. The dialog channel carries encoded JPEG, so an RGB sensor output has to be encoded first, for example with `esp_jpeg_enc`.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
