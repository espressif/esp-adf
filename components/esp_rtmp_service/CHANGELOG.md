# Changelog

## Unreleased

### Changes

- Free example service handles only after successful media deinitialization
- Update example cleanup to retain failed teardown handles for retry

## v0.5.1

### Bug Fixes

- Keep CLI available if MCP UART cannot start, and use valid UART pins in example

## v0.5.0

### Features

- Initial version of `esp_rtmp_service`
  - Support RTMP / RTMPS server, pusher, and puller roles
  - Support media-service links, scheduler overrides, and MCP control tools
