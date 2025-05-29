# Changelog

## v0.6.0

### Added

- Support for Opus encoding, and G711A/U encoding and decoding.
- JWT authentication support for secure communication.
- API to set custom user parameters.
- Optional `wss` URL and `conversition id URL` connection configuration in the project settings.
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
