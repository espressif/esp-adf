# Changelog

## v1.0.0

### Added

- **New: TTS-only API** (`include/esp_coze_tts.h`): `esp_coze_tts_*`; `esp_coze_tts_send_text()`; decoded PCM in `audio_callback`; lifecycle and events in `event_callback` (see header).
- **New: ASR-only API** (`include/esp_coze_asr.h`): `esp_coze_asr_*`; `esp_coze_asr_send_audio*()`, `esp_coze_asr_send_audio_complete()`, `esp_coze_asr_clear_buffer`; transcripts via `event_callback` (`ESP_COZE_ASR_EVENT_TRANSCRIPT_UPDATE`, `ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED`, etc.).
- **`include/esp_coze_common.h`**: shared types/macros for the new facades (`COZE_DEFAULT_WS_BASE_URL`, `esp_coze_audio_type_t`, `ESP_COZE_AUDIO_FOURCC_TO_INT`); TTS/ASR need only this plus `esp_err.h`, not `esp_coze_chat.h`. Chat keeps `esp_coze_chat_audio_type_t` / `ESP_COZE_CHAT_AUDIO_TYPE_*` as aliases.
- **`include/esp_coze_chat.h`**: `esp_coze_chat_init` / `start` / `stop` / `deinit`, `esp_coze_chat_send_audio_*`, etc.; `esp_coze_chat_config_t` adds `downlink_bitrate`, `downlink_frame_size_ms`, `downlink_limit_period`, `downlink_max_frame_num`, `downlink_speech_rate`; credentials and callbacks per Doxygen.
- **Kconfig** (`menuconfig` → **ESP COZE Chat**): `ESP_COZE_WS_TASK_PRIO`, `ESP_COZE_WS_TASK_STACK`, `ESP_COZE_WS_TASK_NAME`, core pinning, `ESP_COZE_WS_BUFFER_SIZE`, reconnect/network/connect/audio-send timeouts, `ESP_COZE_DEBUG_TRACE_DATA` (see `Kconfig` in component).
- **Dependency**: `espressif/esp_websocket_client` ≥ 1.7.0.

### Breaking changes

- **`esp_coze_chat.h` / chat API**: `esp_event` replaced by `audio_callback` (downlink PCM) and `event_callback`. Removed register/unregister, chat event loop, and `ESP_COZE_CHAT_EVENTS`. If `audio_callback` is set, PCM is not delivered through `event_callback`. Callbacks run on the WebSocket task; keep them short.
- **Downlink audio**: without `audio_callback`, use `ESP_COZE_CHAT_EVENT_AUDIO_DATA` in `event_callback` (`esp_coze_chat_audio_data_t *`, valid only for that call).
- **`esp_coze_chat_audio_type_t`** (`esp_coze_chat.h`): values are FOURCCs (aligned with `esp_audio_type_t` in **esp_audio_codec**), not 0–3.
- **Headers**: `esp_coze_utils.h` removed; use `esp_coze_jwt.h` and `esp_coze_http.h` instead of the old `http_client_post` path.

## v0.6.0

### Added

- Support for Opus encoding, and G711A/U encoding and decoding.
- JWT authentication support for secure communication.
- API to set custom user parameters.
- Optional `wss` URL and conversation-id URL connection configuration in the project settings.
- Support websocket event callback.
- New API to configure voice ID.
- APIs to set and retrieve the conversation ID.

### Changed

- Renamed `http_client_request.h` to `esp_coze_utils.h` for better clarity and consistency.
- Added `http_client_post.c` file.

## v0.5.1~1

- Enhanced coze_ws_app example
    - Fixed Kconfig dependency configuration
    - Improved documentation in README
    - Upgraded button component version

## v0.5.1

- Fix ESP-IDF version dependency issue

## v0.5.0

- Initial version of `esp_coze`
