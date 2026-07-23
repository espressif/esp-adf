# Changelog

## v0.5.0

### Features

- Initial version of `esp_video_capture_service`
- Board-aware video (A/V) recorder built on `esp_capture_service`, discovering camera and audio devices through `esp_board_manager`
- V4L2 camera source with an attached audio capture context for A/V or AI audio on the same handle
- Up to two output streams with independent video / audio codecs, enable state, and muxers
- Optional source-side text overlay (camera type and / or date-time), including shared overlay for multi-stream capture
- Optional full-speed decode before re-encode on the source path (for compressed camera formats such as UVC MJPEG / H.264)
- Reuses `esp_capture_service` runtime features: multi-stream output, storage muxers, manual record, frame pull, one-shot, and service linking
- Optional MCP tool support for remote setup and control
- Example `video_capture` demonstrating video-only, A/V stream, storage, dual-stream, overlay, and AI audio cases
