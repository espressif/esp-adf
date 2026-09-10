# Changelog

## v0.5.0~1

### Changes

- Updated `video_player_mix_cli_example` build instructions to use the target selected by ESP Board Manager

## v0.5.0

### Features

- Initial version of `esp_video_player_service`
- Board-aware A/V player built on `esp_player_service`, discovering LCD and DAC through `esp_board_manager`
- `create()` returns one parent handle with audio attach and video context; `attach()` adds a display to an existing player
- Board LCD or caller-owned `esp_video_render`
- Container track APIs: `get_track_num` / `get_track_info` / `enable_track`
- Reuses `esp_player_service` runtime features: multi-stream mix, playlist, URL / feed / link, and volume
- Optional MCP tool support for remote playback and container tracks
- Example `video_player_mix_cli_example` demonstrating movie + TTS mix (URL / playlist or elementary-stream feed)
