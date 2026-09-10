# Changelog

## v0.5.1

### Bug Fixes

- Keep CLI available if MCP UART cannot start, and use valid UART pins in example

## v0.5.0

### Features

- Initial version of `esp_rtsp_service`
  - Support RTSP server, pusher, and puller roles
  - Support UDP and interleaved TCP transport
  - Support media-service links, scheduler overrides, and MCP control tools
  - Optional `aud_frame_size` / `vid_frame_size` in setup (0 keeps 4 KB / 64 KB defaults)
