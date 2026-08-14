# ESP Video Player Service Agent Guide

Use this when implementing or reviewing `esp_video_player_service`. Read `../esp_player_service/agent.md` and `../esp_audio_player_service/agent.md`.

## What It Is

Board LCD subclass of `esp_player_service`. `create()` does parent create → audio attach → video attach and returns `esp_player_service_t *`. Destroy with `esp_player_service_destroy()`.

`esp_video_player_service_attach(p, cfg)` adds a display to an existing audio parent; only `render_fps` is read from `cfg`, the pool comes from the parent. Never `audio_create` + `video_create` on the same speaker.

The application inits the panel with `esp_board_manager`. The service does not init/deinit LCD.

The application registers `esp_video_dec_register_default()` (and extractor / audio decoder). This component only adds GMF video elements to the pool.

Core files:

- `include/esp_video_player_service.h`: create / attach (identity + `render_fps`) / container tracks (with `stream`)
- `include/esp_video_player_service_setup.h`: LCD `display_dev_name` / optional `audio_dev_name` / `set_render()`
- `include/esp_video_player_service_mcp.h`: complete MCP schema (playback + track tools)
- `src/esp_video_player_service.c` / `_setup.c` / `_ctrl.c` / `src/mcp/`

One video render per service, two immediate install points, later call wins: `apply_setup(.display_dev_name)` builds one from the board LCD and owns it; `set_render()` installs a caller-owned one. NULL `display_dev_name` / NULL render means video off. `apply_setup` also fills parent audio from the audio-subclass cache, else the board DAC named by `audio_dev_name`. Board default names live in `ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT()`; NULL in either name field means "leave that device out", with no hidden fallback in the implementation. Playback is `esp_player_service_*`. Domain APIs return `ESP_ERR_NOT_SUPPORTED` without a video render.

One LCD per instance. Overlay/z-order uses `get_info()->video_render`.

`apply_setup` always uses `esp_video_render_get_lcd_backend()`. Do not add the LVGL backend there: it needs a live `lv_disp` (void* in `esp_video_render_lvgl_cfg_t`) owned by the application after `lvgl_port` init, and pulling LVGL into this component would fight an app that already has UI on the same panel. Current path for "video in a widget": the app creates the render with `esp_video_render_get_lvgl_backend()` (Kconfig `ESP_VIDEO_RENDER_LVGL_BACKEND_SUPPORT`) and installs it via `set_render()`. A first-class LVGL apply_setup (e.g. an `lv_disp` field) is deferred.

Example: `video_player_mix_cli_example` (stream 0 movie URL/playlist, ES or dummy-src link; stream 1 TTS).

When publishing to the Component Registry, strip monorepo `override_path`.
