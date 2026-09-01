# ESP Audio Player Service Agent Guide

Use this when implementing or reviewing `esp_audio_player_service`. Read `../esp_player_service/agent.md` first: this is a thin board-DAC subclass.

## What It Is

Board DAC lookup on a parent `esp_player_service_t`. `create()` attaches and returns the parent handle. Destroy with `esp_player_service_destroy()`.

Core files:

- `include/esp_audio_player_service.h`: create
- `include/internal/esp_audio_player_service_priv.h`: attach / detach / select_codec / setup cache
- `include/esp_audio_player_service_setup.h`: board DAC (`dev_name`, sample, period) → parent `apply_setup`. NULL `dev_name` keeps the cached outlet; custom writer stays on the parent setup. DAC volume is `esp_player_service_set_output_volume()`.
- `include/esp_audio_player_service_mcp.h`: complete MCP schema (playback + ID3)
- `src/esp_audio_player_service.c` / `_setup.c` / `src/mcp/`

Do not reintroduce a second handle type, `acquire_render_stream`, or a private mixer. Playback APIs are `esp_player_service_*` with a `stream` argument.

`cfg.name` is the esp_service / scheduler name. `setup.dev_name` is the board DAC key.

The application registers extractor / audio decoder tables before play. This subclass does not call `register_default`.

Examples: `audio_player_mix_cli_example` (URL playlist + dummy-src link + feed) and `audio_player_mcp_example`.

When publishing to the Component Registry, strip monorepo `override_path`.
