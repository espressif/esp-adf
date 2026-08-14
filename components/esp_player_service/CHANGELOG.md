# Changelog

## v0.5.0

### Features

- Initial version of `esp_player_service`
- Media sink service (`ESP_MEDIA_ROLE_SINK`) for multi-stream playback through `esp_media_service` / `esp_service`
- One mixer per instance, with optional caller-owned video render
- Per-stream URL, feed (`write_frame`), and media-link (zero-copy) inputs
- Per-stream playlist with auto-advance and repeat modes
- Concurrent mix with priority-based preemption (COEXIST / EXCLUSIVE)
- Playback, volume, mix, playlist, and ID3 APIs take a stream id
