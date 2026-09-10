# Changelog

## v0.5.0~1

### Changes

- Enlarged the `audio_player_mcp_example` factory partition to 3072K
- Updated `audio_player_mix_cli_example` and `audio_player_mcp_example` build instructions to use the target selected by ESP Board Manager

## v0.5.0

### Features

- Initial version of `esp_audio_player_service`
- Board-aware audio player built on `esp_player_service`, discovering the DAC through `esp_board_manager`
- Output selection: custom PCM writer, codec device, or board DAC
- Reuses `esp_player_service` runtime features: multi-stream mix, playlist, URL / feed / link, volume, and ID3
- Optional MCP tool support for remote playback and ID3
- Examples `audio_player_mix_cli_example` (URL playlist, link, and feed) and `audio_player_mcp_example`
