# Changelog

## v0.5.0

### Features

- Initial version of `esp_audio_capture_service`
- Board-aware audio recorder built on `esp_capture_service`, discovering the audio ADC through `esp_board_manager`
- Automatic selection of codec-device source or AI audio front-end source when AI features are requested
- Up to two output streams with independent codecs, enable state, and muxers
- Fixed source sample-rate pinning for deterministic multi-stream negotiation
- Reuses `esp_capture_service` runtime features: multi-stream output, storage muxers, manual record, frame pull, and service linking
- Optional AI front-end features: AEC, NS, VAD, WakeNet, DOA, plus callbacks and PCM dump
- Optional MCP tool support for remote setup and control
- Example `audio_record` demonstrating streaming, storage, dual-stream, and AI audio cases
