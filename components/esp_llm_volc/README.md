# ESP LLM Volc

- [中文版](./README_CN.md)

Volcengine RTC backend adapter for [esp_llm_service](../esp_llm_service). It wraps the `volc_conv_ai` SDK and implements the `esp_llm_adapter.h` contract, so an application drives a Volc conversational agent through the generic `esp_llm_service_*` API and one typed create function.

The adapter owns the Volc IoT authentication and the RTC session only. Capture and player objects stay with the application; the adapter consumes uplink frames handed to it by the service and publishes TTS frames back through the service downlink. The public header keeps the Volc SDK private, so applications never include `volc_conv_ai.h`.

## Features

- Volcengine conversational AI over RTC, with device identity signed through Volc IoT
- RTC audio codecs: G.711 A-law 8 kHz by default, plus G.711 mu-law 8 kHz and OPUS 16 kHz
- RTC video publish and subscribe, off by default, with JPEG or H.264 as the frame codec
- Text input through `esp_llm_service_send_text()` for a model round trip
- Direct speech through `esp_llm_volc_send_text_to_tts()` without a model round trip
- Backend-native RTS messages through `esp_llm_service_send_custom_data()`
- Cancellation through `esp_llm_service_cancel_response()`
- Conversation status published as the backend event `CONV_STATUS`
- Audio AI features attached to the application capture: AEC, NS, VAD, and WakeNet
- Escape hatch to borrow the raw engine handle for SDK features this component does not wrap

## Directory Structure

```
esp_llm_volc/
├── include/esp_llm_volc.h   Typed configuration, DEFAULT macro, and the public API
├── src/                     Adapter implementation over volc_conv_ai
├── examples/volc_chat/      In-component chat example
├── Kconfig                  Audio profile and video RTC profile options
└── agent.md                 Adapter implementation, IoT credentials, and events
```

## Quick Start

The product example is [`adf_examples/ai_agent/esp_llm_volc_agent`](../../adf_examples/ai_agent/esp_llm_volc_agent). It keeps media ownership in the application and is the reference for wiring this adapter:

```bash
cd adf_examples/ai_agent/esp_llm_volc_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi plus Volc IoT credentials and bot ID
idf.py build flash monitor
```

The application sequence is Wi-Fi, then SNTP because RTC request signing rejects a stale clock, then create and configure capture and player from `cfg.service.streams[]`, then fill `cfg.service.audio_ai_cfg.capture`, then `esp_llm_volc_create()`, then link, subscribe, and start player, LLM, capture in that order.

The minimal adapter usage is:

```c
esp_llm_volc_cfg_t cfg = ESP_LLM_VOLC_CFG_DEFAULT();
cfg.iot.instance_id = CONFIG_VOLC_IOT_INSTANCE_ID;
cfg.iot.product_key = CONFIG_VOLC_IOT_PRODUCT_KEY;
cfg.iot.product_secret = CONFIG_VOLC_IOT_PRODUCT_SECRET;
cfg.iot.device_name = CONFIG_VOLC_IOT_DEVICE_NAME;
cfg.bot_id = CONFIG_VOLC_BOT_ID;
cfg.service.audio_ai_cfg.capture = capture;           /* application-owned */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* zero disables audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_volc_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/volc_chat` is a smaller in-component example that exercises the adapter without the full application media graph.

## Configuration

`esp_llm_volc_create()` requires the IoT `instance_id`, `product_key`, `product_secret`, and `device_name`, plus a `bot_id`. Audio codec, sample rate, and bitrate come from `cfg.service.streams[]`, which the DEFAULT macro pre-fills from the Kconfig audio profile. Disable `VIDEO_UP` and `VIDEO_DN` unless the application owns a camera and a display, because nothing would link those streams.

Component Kconfig lives under `ESP LLM Volc`:

| Option | Default |
| --- | --- |
| `ESP_LLM_VOLC_AUDIO_CODEC` | G.711 A-law 8 kHz; G.711 mu-law and OPUS 16 kHz also selectable |
| `ESP_LLM_VOLC_VIDEO_PUBLISH` | Disabled; publishes an RTC video track when video uplink is enabled |
| `ESP_LLM_VOLC_VIDEO_SUBSCRIBE` | Disabled; subscribes to remote RTC video |
| `ESP_LLM_VOLC_VIDEO_CODEC` | JPEG; H.264 also selectable |

## Backend-Specific API

Most control goes through the generic `esp_llm_service_*` API. These extras cover Volc features the generic API cannot express:

| API | Purpose |
| --- | --- |
| `esp_llm_volc_send_text_to_tts()` | Speak UTF-8 text as-is, with no model round trip |
| `esp_llm_volc_backend_acquire()` | Borrow the raw engine handle for unwrapped SDK calls |
| `esp_llm_volc_backend_release()` | Return the borrowed handle |

`esp_llm_service_send_text()` asks the model a question, while `esp_llm_volc_send_text_to_tts()` only drives TTS. `esp_llm_service_send_custom_data()` sends a backend-native RTS message.

The borrow holds the engine lock and pins the adapter context. Keep it short, pair acquire and release on one thread, and call no other LLM API in between; a missing release deadlocks destroy.

## Notes

- Sync the clock with SNTP after Wi-Fi. RTC configuration requests are signed and a stale clock is rejected.
- Video is off by default in both the Kconfig profile and the product example. Turn the streams on only when a camera and display exist.
- Link every uplink before `esp_service_start()`; linking a running service returns `ESP_ERR_INVALID_STATE`.
- Start the player before the LLM and the capture after it, so the AFE sidecar can attach. Stop in reverse.
- Downlink TTS is written through the service downlink path, so a player must be linked to `AUDIO_DN` for audio to be heard.
- Always pair `esp_llm_volc_backend_acquire()` with `esp_llm_volc_backend_release()` on the same thread.
- The IoT product secret is a credential. Keep it out of committed defaults in product builds.

## Performance

| Item | Value |
| --- | --- |
| Uplink audio | G.711 A-law, 8 kHz mono, 64 kbit/s by default |
| Downlink audio | Same codec and rate as the uplink |
| Capture and playback clock | The product example runs AFE and I2S at 16 kHz PCM and transcodes to 8 kHz for RTC |
| Video | Disabled by default; JPEG or H.264 when enabled |
| Transport | One RTC session per device, with a signed IoT configuration request at startup |
| Application partition | 6 MB in the product example |

## SoC Compatibility

Validated on ESP32-S3, ESP32-S31, and ESP32-P4 with a board that provides `audio_adc` and `audio_dac`. The audio-only configuration has no camera or display requirement. PSRAM is required for the RTC and audio buffers, and OPUS needs the `AUDIO_ENCODER_OPUS_SUPPORT` and `AUDIO_DECODER_OPUS_SUPPORT` options that the Kconfig choice selects.

## FAQ

**Why is G.711 the default instead of OPUS?**

G.711 A-law keeps the RTC link at a fixed 64 kbit/s with negligible encode cost, which suits a voice-only conversational agent on constrained networks. OPUS at 16 kHz is selectable when wideband quality matters more than CPU and bandwidth.

**The capture runs at 16 kHz but the stream says 8 kHz. Which is right?**

Both. The AFE and the shared I2S bus run at 16 kHz PCM because the AEC reference needs that clock, and the adapter encodes down to 8 kHz for the G.711 uplink. Do not pin the ADC clock to 8 kHz.

**`GetRTCConfig` fails with a signature error.**

The wall clock was never set. Volc signs the configuration request, so run SNTP after Wi-Fi connects and before creating the service.

**How do I call a Volc SDK function this component does not wrap?**

Borrow the engine with `esp_llm_volc_backend_acquire()`, make the SDK call, and return it with `esp_llm_volc_backend_release()`. Include the SDK header in your own file, because the public header deliberately keeps the handle opaque.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
