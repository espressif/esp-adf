# Changelog

## Unreleased

### Changes

- Preserve the instance on any base teardown failure for retry; free only after successful cleanup
- Accept esp_capture's already-stopped result so configured, never-started captures can be destroyed
- Retain failed user deinit callbacks and their contexts until cleanup succeeds

## v0.5.1~1

### Changes

- Updated depended components `esp_capture` to ~1.1

## v0.5.1

### Bug Fixes

- Fixed capture service failing to restart after stop when used through a linked media provider

## v0.5.0

### Features

- Initial version of `esp_capture_service`
- Media source service (`ESP_MEDIA_ROLE_SRC`) wrapping the `esp_capture` engine through `esp_media_service` / `esp_service`
- Declarative setup bundle for sources, streams, tracks, and muxers with atomic apply
- Multi-stream capture with independent codecs and run-state per stream
- Up to three tracks per stream: audio, video, and muxer
- Pull-based frame access and provider-based linking to other media services
- Lazy-binding storage muxer (MP4 / TS / FLV / WAV / CAF / OGG) with manual or auto record
- Runtime stream / track enable, one-shot capture, and escape hatches to native `esp_capture` / sink handles
- Optional shared video overlay and full-speed decode on the source path (when enabled in `esp_capture`)

