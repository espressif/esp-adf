# Changelog

## v0.5.0~1

### Changes

- Updated depended components `gmf_core` to ~1.1

## v0.5.0

### Features

- Initial version of `esp_muxer_service`
  - High-level media sink that muxes linked elementary audio/video frames into a container
  - Storage-only, streaming-only, or both output modes (TS / MP4 / FLV / WAV / CAF / OGG / AVI)
  - Link from any media source (`esp_media_service_link`); optional MCP tools
  - Example `muxer_service` with dummy AAC / H264+AAC cases, board-manager SD setup, and UART MCP checks
