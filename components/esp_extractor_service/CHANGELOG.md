# Changelog

## v0.5.0

### Features

- Initial version of `esp_extractor_service`
  - High-level media source that demuxes local files, HTTP(S)/HLS, or in-memory buffers into elementary audio/video frames
  - Link to any media sink (`esp_media_service_link`) or pull frames through `get_provider`
  - Optional file IO, HTTP IO, HLS, and MCP tools
  - Example `extractor_service` with board-manager SD setup, console cases, and UART MCP checks
