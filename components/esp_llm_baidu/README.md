# ESP LLM Baidu

- [中文版](./README_CN.md)

Baidu RTC backend adapter for [esp_llm_service](../esp_llm_service). It wraps the Baidu BRTC prebuilt SDK and implements the `esp_llm_adapter.h` contract, so an application drives either a Baidu chat agent or a peer-to-peer intercom through the generic `esp_llm_service_*` API and one typed create function.

The adapter owns the BRTC engine and the room session only. Capture and player objects stay with the application; the adapter consumes uplink frames handed to it by the service and publishes downlink audio and video back through the service downlink. It is the only adapter in this family that produces a cloud-generated video downlink.

## Features

- Two modes: agent, an AI chat agent engine, and intercom, a room-based peer-to-peer call
- Four agent scenes: voice chat, image understanding, image generation, and multi-modal
- Agent audio presets: OPUS 16 kHz encoded by the application and passed through, or PCM 16 kHz that the Baidu SDK encodes as G.722
- Vision uplink modes: snapshot on cloud request, or continuous frames, with a configurable stream expiry
- RTC video codec selectable between JPEG and H.264 for both uplink and downlink
- Video downlink of agent-generated images to an application video player
- Server voice interrupt with a configurable sensitivity level
- Text input, cancellation, and backend-native engine commands through the generic API
- Direct speech through `esp_llm_baidu_send_text_to_tts()` without a chat query
- FunctionCall replies through `esp_llm_baidu_send_function_call_result()`
- Query enrichment through `esp_llm_baidu_set_enhance_query()`
- Audio AI features attached to the application capture: AEC, NS, VAD, and WakeNet
- Escape hatch to borrow the raw engine and RTC client handles for unwrapped SDK calls

## Directory Structure

```
esp_llm_baidu/
├── include/
│   ├── esp_llm_baidu.h          Typed configuration, scene macros, and the public API
│   └── esp_llm_baidu_config.h   Mode, scene, and vision configuration structures
├── src/                         Adapter implementation over the BRTC SDK
├── components/                  Bundled BRTC prebuilt SDK
├── examples/
│   ├── baidu_rtc_agent/         In-component agent example
│   └── baidu_rtc_intercom/      In-component intercom example
├── test_apps/                   Offline create, stream info, and control-plane tests
├── Kconfig                      Mode, scene, audio, and video profile options
└── agent.md                     Adapter usage, scenes, and event guide
```

## Quick Start

The product example is [`adf_examples/ai_agent/esp_llm_baidu_agent_app`](../../adf_examples/ai_agent/esp_llm_baidu_agent_app). It keeps media ownership in the application and demonstrates all four scenes:

```bash
cd adf_examples/ai_agent/esp_llm_baidu_agent_app
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi plus Baidu agent credentials and scene
idf.py build flash monitor
```

The application sequence is Wi-Fi, then SNTP, then create and configure capture and player from `cfg.service.streams[]`, then fill `cfg.service.audio_ai_cfg.capture`, then `esp_llm_baidu_create()`, then link, subscribe, and start player, LLM, capture in that order.

The minimal adapter usage is:

```c
esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
cfg.agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
cfg.agent.app_id = CONFIG_BAIDU_APP_ID;
cfg.agent.user_id = CONFIG_BAIDU_USER_ID;
cfg.service.audio_ai_cfg.capture = capture;           /* application-owned */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* zero disables audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_baidu_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/baidu_rtc_agent` and `examples/baidu_rtc_intercom` are smaller in-component examples that exercise each mode without the full application media graph.

## Modes and Scenes

`ESP_LLM_BAIDU_MODE` selects the engine:

| Mode | Description |
| --- | --- |
| `ESP_LLM_BAIDU_MODE_AGENT` | Baidu chat agent engine with ASR, TTS, and vision scenes |
| `ESP_LLM_BAIDU_MODE_INTERCOM` | Peer-to-peer RTC intercom room with audio and video |

In agent mode, the scene decides which media streams the session needs. The scene configuration mirrors `esp_baidu_rtc_service`:

| Scene | Camera uplink | Video downlink |
| --- | --- | --- |
| Voice chat | No | No |
| Image understanding | Yes | No |
| Image generation | No | Yes |
| Multi-modal | Yes | Yes |

Enabling a scene sets the protocol requirement. The application still decides the device side by enabling the matching entries in `cfg.service.streams[]` and creating the camera and display, so a scene never creates media objects on its own.

## Configuration

Audio codec, sample rate, and video format come from `cfg.service.streams[]`, which the scene DEFAULT macros pre-fill. After create, `esp_llm_service_get_stream_info()` returns the stored formats.

Component Kconfig lives under `ESP LLM Baidu`:

| Option | Default |
| --- | --- |
| `ESP_LLM_BAIDU_MODE` | Agent; intercom also selectable |
| `ESP_LLM_BAIDU_AGENT_AUDIO_CODEC` | OPUS passthrough with application encode and decode; PCM with SDK-side G.722 also selectable |
| `ESP_LLM_BAIDU_AGENT_SCENE` | Voice chat |
| `ESP_LLM_BAIDU_AGENT_VISION_MODE` | Image, a snapshot on cloud request; stream also selectable |
| `ESP_LLM_BAIDU_VISION_EXPIRE_SEC` | 10 seconds |
| `ESP_LLM_BAIDU_VIDEO_CODEC` | JPEG; H.264 also selectable |
| `ESP_LLM_BAIDU_VOICE_INTERRUPT_ENABLE` | Enabled, with `ESP_LLM_BAIDU_VOICE_INTERRUPT_LEVEL` at 80 |
| `ESP_LLM_BAIDU_AGENT_AUTO_INTERRUPT_ON_WAKEUP` | Enabled |
| `ESP_LLM_BAIDU_AGENT_GATE_UPLINK_UNTIL_WAKEUP` | Disabled |
| `ESP_LLM_BAIDU_INTERCOM_USING_VIDEO` | Enabled in intercom mode |
| `ESP_LLM_BAIDU_INTERCOM_RECEIVING_VIDEO` | Enabled in intercom mode |

## Backend-Specific API

Most control goes through the generic `esp_llm_service_*` API. These extras cover BRTC features the generic API cannot express:

| API | Mode | Purpose |
| --- | --- | --- |
| `esp_llm_baidu_send_text_to_tts()` | Agent | Speak UTF-8 text without a chat query |
| `esp_llm_baidu_send_function_call_result()` | Agent | Answer a `FUNCTION_CALL` event |
| `esp_llm_baidu_set_enhance_query()` | Agent | Wrap every user query with extra context |
| `esp_llm_baidu_backend_acquire()` | Both | Borrow the raw engine and RTC client handles |
| `esp_llm_baidu_backend_release()` | Both | Return the borrowed handles |

`esp_llm_service_send_custom_data()` is implemented for both modes: agent sends the string as an engine event, using `[E]:` or `[SET]:` commands such as `AGENT_EVENT_UPDATE_SYSTEM_PROMPT`, while intercom broadcasts it to the room.

The borrowed handles are `void *` in the header so that `baidu_chat_agents_engine.h` does not leak names such as `SPEAKING` or `Region` into every application. Include the SDK header yourself and cast:

```c
esp_llm_baidu_backend_t backend;
if (esp_llm_baidu_backend_acquire(llm, &backend) == ESP_OK) {
    if (backend.mode == ESP_LLM_BAIDU_MODE_INTERCOM) {
        brtc_set_volume(backend.rtc_client, 8);
    }
    esp_llm_baidu_backend_release(llm);
}
```

## Notes

- The borrow holds the engine mutex and pins the adapter context. Keep it short, pair acquire and release on one thread, and call no other LLM API in between; a missing release deadlocks destroy.
- Link every uplink before `esp_service_start()`; linking a running service returns `ESP_ERR_INVALID_STATE`.
- Start the player before the LLM and the capture after it, so the AFE sidecar can attach. Stop in reverse.
- Downlink audio and generated images both travel through the service downlink path, so link a video player to receive them.
- A scene only states the protocol requirement. Enable the matching `cfg.service.streams[]` entries, otherwise the capability mask intersects them away.
- Camera JPEG dimensions are independent of the LCD size; the capture size must match a real esp-video sensor mode.
- The license key and application credentials are secrets. Keep them out of committed defaults in product builds.
- Agent audio follows `multimodal_assistant_app`: `AudioIncodecType` is the `send_audio` payload (`opus` or `pcm`); `rtc_ac` / `AudioOutcodecType` is `opus` (application OPUS, no SDK transcode) or `g722` (application PCM, G.722 inside the SDK).

## Performance

| Item | Value |
| --- | --- |
| Agent audio | OPUS 16 kHz mono passthrough by default; PCM 16 kHz with SDK-side G.722 also selectable |
| Video | JPEG by default, 320x240 at 3 fps from the stream macro; H.264 also selectable |
| Vision snapshot expiry | `ESP_LLM_BAIDU_VISION_EXPIRE_SEC`, default 10 seconds |
| Voice interrupt level | `ESP_LLM_BAIDU_VOICE_INTERRUPT_LEVEL`, default 80 of 100 |
| Transport | One BRTC room session per device |
| Application partition | 4 MB in the product example, plus 512 KB for the WakeNet model and 128 KB SPIFFS |

## SoC Compatibility

Supported targets are ESP32-S3, ESP32-S31, and ESP32-P4. Audio needs a board with `audio_adc` and `audio_dac`; the vision scenes additionally need `camera` and `display_lcd`. PSRAM is required.

`test_apps` covers create, destroy, stream info, and control-plane errors without Wi-Fi and without joining a Baidu room. Generate board headers first with the board that matches the chip, because `esp_llm_service` still compiles `esp_audio_capture_service`:

| Target | Board |
| --- | --- |
| ESP32-S3 | `esp32_s3_korvo_2_3` |
| ESP32-S31 | `esp32_s31_korvo_1` |
| ESP32-P4 | `esp32_p4_function_ev_board` |

```bash
cd test_apps
idf.py bmgr -b esp32_s3_korvo_2_3   # or esp32_s31_korvo_1 / esp32_p4_function_ev_board
idf.py build
```

## FAQ

**What does `esp_llm_service_get_capabilities()` return for this adapter?**

The adapter descriptor advertises a static superset covering audio, video, text, and vision. The instance value is that superset intersected with the streams enabled in `cfg.service.streams[]`, resolved once at create time. Agent and intercom differences are therefore expressed through the enabled streams and the typed configuration, not through a separate mask. Application links do not change it.

**A vision scene is selected but no image ever reaches the cloud.**

The scene states the protocol requirement; the device side is separate. Enable `ESP_LLM_MEDIA_STREAM_VIDEO_UP` in `cfg.service.streams[]`, create the camera, and link it. In image mode the adapter also waits for the cloud to request a snapshot, so frames sent before that request are dropped while `send_frame()` still returns `ESP_OK`.

**What is the difference between the local LCD preview and `VIDEO_DN`?**

The preview is a second capture stream the application draws locally and it never leaves the device. `VIDEO_DN` carries images generated by the agent in the cloud. In the multi-modal scene both end up on the same display but they are independent paths.

**How do the OPUS and PCM audio presets map onto the Baidu SDK?**

`AudioIncodecType` is the `send_audio` payload. `rtc_ac` and `AudioOutcodecType` name the wire codec.

OPUS is the default. The application encodes uplink and decodes downlink. The Baidu library does not transcode OPUS, so `send_audio` and `onAudioData` carry OPUS frames and `AudioIn` / `rtc_ac` / `AudioOut` are `opus`.

PCM means the application sends and plays PCM. The Baidu library encodes and decodes G.722 on the wire, so `AudioIn` is `pcm` and `rtc_ac` / `AudioOut` are `g722`.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
