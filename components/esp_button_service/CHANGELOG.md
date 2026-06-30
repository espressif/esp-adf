# Changelog

## v0.5.1

### Changes

- Remove unnecessary sdkconfig for board manager

## v0.5.0~1

### Changes

- Added Component Registry badge to README.md and README_CN.md

## v0.5.0

### Features

- Initial version of `esp_button_service`
- `esp_service_t` subclass that wraps `esp_board_manager` button devices
- Typed button-event forwarding (press, release, single-click, long-press, etc.) via the service event hub
- Per-service event hub subscription (`esp_service_event_subscribe`); per-button `iot_button` callbacks with automatic unregister on service stop
