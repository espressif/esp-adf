# ESP LLM Coze

- [中文版](./README_CN.md)

Coze backend adapter for [esp_llm_service](../esp_llm_service). It wraps [`esp_coze`](../esp_coze) and implements the `esp_llm_adapter.h` contract, so an application drives a Coze voice chat through the generic `esp_llm_service_*` API and one typed create function.

The adapter owns the Coze WebSocket session only. Capture and player objects stay with the application; the adapter consumes uplink frames handed to it by the service and publishes TTS frames back through the service downlink.

## Features

- Coze streaming chat over WebSocket, with conversation creation through the Coze REST endpoint
- Uplink and downlink audio codecs: OPUS 16 kHz, PCM 16 kHz, G.711 A-law 8 kHz, G.711 mu-law 8 kHz
- Downlink Opus bitrate and frame size configurable, default 16000 bit/s and 60 ms
- Server VAD silence detection in fast or normal mode
- Text input through `esp_llm_service_send_text()` and cancellation through `esp_llm_service_cancel_response()`
- Backend-native messages through `esp_llm_service_send_custom_data()`
- Subtitles, server VAD, errors, and chat completion forwarded as `esp_llm_service` events
- Audio AI features attached to the application capture: AEC, NS, VAD, and WakeNet
- Wake-word policies: hold the uplink closed until the wake word, and auto interrupt on wake-up
- Video streams are rejected; the adapter advertises audio and text capabilities only

## Directory Structure

```
esp_llm_coze/
├── include/esp_llm_coze.h   Typed configuration, DEFAULT macro, and esp_llm_coze_create()
├── src/                     Adapter implementation over esp_coze
├── examples/coze_chat/      In-component chat example
├── Kconfig                  Audio profile and wake-word policy options
└── agent.md                 Adapter implementation and review guide
```

## Quick Start

The product example is [`adf_examples/ai_agent/esp_llm_coze_agent`](../../adf_examples/ai_agent/esp_llm_coze_agent). It keeps media ownership in the application and is the reference for wiring this adapter:

```bash
cd adf_examples/ai_agent/esp_llm_coze_agent
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
idf.py menuconfig   # Wi-Fi plus Coze bot ID and access token
idf.py build flash monitor
```

The application sequence is Wi-Fi, then create and configure capture and player from `cfg.service.streams[]`, then fill `cfg.service.audio_ai_cfg.capture`, then `esp_llm_coze_create()`, then link, subscribe, and start player, LLM, capture in that order.

The minimal adapter usage is:

```c
esp_llm_coze_cfg_t cfg = ESP_LLM_COZE_CFG_DEFAULT();
cfg.bot_id = CONFIG_COZE_BOT_ID;
cfg.access_token = CONFIG_COZE_ACCESS_TOKEN;
cfg.service.audio_ai_cfg.capture = capture;           /* application-owned */
cfg.service.audio_ai_cfg.feature_mask = ai_features;  /* zero disables audio AI */

esp_llm_service_t *llm = NULL;
ESP_ERROR_CHECK(esp_llm_coze_create(&cfg, &llm));
esp_service_start(ESP_SERVICE_BASE(llm));
```

`examples/coze_chat` is a smaller in-component example that exercises the adapter without the full application media graph.

## Configuration

`esp_llm_coze_create()` requires `bot_id` and `access_token` and rejects the call when either is empty. The remaining fields of `esp_llm_coze_cfg_t` are filled by `ESP_LLM_COZE_CFG_DEFAULT()`:

| Field | Purpose |
| --- | --- |
| `bot_id` | Coze bot to talk to; required |
| `access_token` | Coze access token; required |
| `ws_base_url` | WebSocket endpoint; NULL keeps the default |
| `conv_create_base_url` | Conversation-create REST endpoint; NULL keeps the default |
| `user_id` | User identity reported to Coze; NULL keeps the adapter default |
| `voice_id` | TTS voice; NULL keeps the bot default |
| `service` | Shared `esp_llm_service_cfg_t`: streams, audio AI, downlink callback, and name |

Audio codec, sample rate, and bitrate come from `cfg.service.streams[]`, which the DEFAULT macro pre-fills from the Kconfig audio profile. After create, `esp_llm_service_get_stream_info()` returns the stored formats.

Component Kconfig lives under `ESP LLM Coze`:

| Option | Default |
| --- | --- |
| `ESP_LLM_COZE_AUDIO_CODEC` | OPUS 16 kHz; PCM, G.711 A-law, and G.711 mu-law also selectable |
| `ESP_LLM_COZE_DOWNLINK_BITRATE` | 16000 bit/s, used when the codec is OPUS |
| `ESP_LLM_COZE_DOWNLINK_FRAME_SIZE_MS` | 60 ms |
| `ESP_LLM_COZE_VAD_MODE` | Fast silence detection |
| `ESP_LLM_COZE_GATE_UPLINK_UNTIL_WAKEUP` | Disabled |
| `ESP_LLM_COZE_AUTO_INTERRUPT_ON_WAKEUP` | Disabled |

## Notes

- Video streams are not supported. Enabling `VIDEO_UP` or `VIDEO_DN` in `cfg.service.streams[]` makes create fail with `ESP_ERR_NOT_SUPPORTED`.
- Link every uplink before `esp_service_start()`; linking a running service returns `ESP_ERR_INVALID_STATE`.
- Start the player before the LLM and the capture after it, so the AFE sidecar can attach. Stop in reverse.
- WebSocket connect publishes `ESP_LLM_EVENT_CONNECTED` and then `ESP_LLM_EVENT_MEDIA_READY`. Wait for media-ready before treating the session as usable.
- Downlink TTS is written through the service downlink path, so a player must be linked to `AUDIO_DN` for audio to be heard.
- Empty strings passed to `esp_llm_service_send_text()` or `send_custom_data()` return `ESP_ERR_INVALID_ARG`.
- The access token is a credential. Keep it out of committed defaults in product builds.

## Performance

| Item | Value |
| --- | --- |
| Uplink audio | OPUS 16 kHz mono by default; PCM or G.711 selectable |
| Downlink audio | Same codec as the uplink, 16000 bit/s and 60 ms frames for OPUS |
| Transport | One TLS WebSocket connection per session |
| TLS buffer | The product example raises `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` to 10240 and requires PSRAM |
| Application partition | 6 MB in the product example |

## SoC Compatibility

Validated on ESP32-S3, ESP32-S31, and ESP32-P4 with a board that provides `audio_adc` and `audio_dac`. The adapter has no camera or display requirement. PSRAM is required for the TLS and audio buffers, and OPUS encode and decode need the `AUDIO_ENCODER_OPUS_SUPPORT` and `AUDIO_DECODER_OPUS_SUPPORT` options that the Kconfig choice selects.

## FAQ

**Can this adapter send images to the bot?**

No. The adapter advertises audio and text only and rejects video streams at create time. Use [esp_llm_tongyi](../esp_llm_tongyi) for one-shot image input or [esp_llm_baidu](../esp_llm_baidu) for camera uplink.

**The session connects but nothing is heard.**

Downlink TTS travels through the service downlink provider. Link a player to `ESP_LLM_MEDIA_STREAM_AUDIO_DN`, or set `cfg.service.downlink_cb` and render the frames yourself.

**How does the wake word stop the microphone from streaming?**

The adapter does not gate the uplink on its own. The application calls `esp_llm_service_enable_uplink_track(llm, ESP_MEDIA_TRACK_TYPE_AUDIO, false)` after create and reopens it from the wake-up event; `ESP_LLM_COZE_GATE_UPLINK_UNTIL_WAKEUP` documents that policy for the examples.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
