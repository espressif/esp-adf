# ESP Player Service Agent Guide

Use this when implementing or reviewing `esp_player_service`. Read `../esp_media_service/agent.md` first: this service is a SINK.

## What It Is

Parent playback engine: one mixer (one speaker) and optional one LCD on the same handle. Concurrent playback (BGM, TTS, a movie) is a **stream**. Subclasses only attach board DAC / LCD. ID3 parse for bare MP3 URLs is a parent playback API. Container track catalog lives on the video subclass.

Public type is only `esp_player_service_t`. Audio / video `create()` return it. Destroy with `esp_player_service_destroy()`. Lifecycle is `ESP_SERVICE_BASE(p)`.

Core files:

- `include/esp_player_service.h`: create / destroy / deinit_cb / get_info + get_stream_info (advanced; both zero-alloc, handles owned by the service)
- `include/esp_player_scheduler.h`: defer / mixer / reader / player task names and flow
- `include/esp_player_service_setup.h`: audio writer/codec + PCM format + `apply_setup`. DAC volume, demux buffer, and A/V `sync_mode` use playback setters, not this struct.
- `include/internal/esp_player_service_subclass.h`: `get_pool` / `set_video_render` for subclasses; not part of the app-facing API
- `include/esp_player_service_playback.h`: control APIs take `stream` except service-level `set_output_volume`
- `src/core/esp_player_service.c`: lifecycle, SINK ops, lazy `esp_player`, feed/link
- `src/core/esp_player_service_setup.c` / `_ctrl.c` / `_mix.c` / `_playlist.c`
- `private_inc/player_out.h` + `src/out/`: the audio output domain

No MCP in this component. Product MCP lives in the audio/video subclasses.

## Layering

`src/core/` must stay render-agnostic: no `esp_audio_render` include, no render type in its signatures. Everything touching the mixer, the codec device, or per-slot gain lives behind `player_out.h` in `src/out/`, and the core sees only `player_out_handle_t` and `void *`. Keep new render work on that side of the line.

`src/out/` has two implementations and CMake builds exactly one: `player_out_audio.c` (default) and `player_out_stub.c` for a silent video build. The stub keeps the same contract, so adding a `player_out_*` function means adding it to both. Core asks about the capability through `PLAYER_OUT_AUDIO_SUPPORTED`, never the Kconfig symbol.

The per-stream A/V mask is a runtime property, not a build-time one: it comes from `player_out_audio_has_sink()` and the installed video render, so a deferred audio output yields a video-only stream instead of an open failure at first playback. Whoever changes the sink must also drop any player built against the old mask.

This component owns no Kconfig: the switch is esp_player's `CONFIG_ESP_PLAYER_ENABLE_AUDIO`, deliberately reused instead of a second option. A separate one would allow a mixer feeding an engine built without audio, where `esp_player_set_av_mask(ESP_PLAYER_MASK_AUDIO)` returns `ERR_NOT_SUPPORT` and every stream fails. The same symbol gates `esp_audio_render`, `esp_codec_dev` and `gmf_audio` in `idf_component.yml` through `matches: - if: $CONFIG{...}`, leaving `gmf_core` as the only unconditional private dependency, so keep every audio-library include inside `src/out/`. The default GMF pool is built by the core (the video subclass registers into the same pool) but its audio elements come from `player_out_audio_register_elements()`, which is why core needs no gmf_audio.

Do not public-depend on `esp_video_render`. The video render arrives as `void *` through `esp_player_service_set_video_render()`, separate from `apply_setup()` so an audio-only setup cannot drop the display. Do not reintroduce `acquire_render_stream` as a cross-service protocol.

The application owns global extractor / decoder tables (`register_default`). The default GMF pool only installs elements. Do not register or unregister those tables from this component.

## Invariants

- One speaker = one mixer = one parent instance
- One LCD per instance; streams are equal
- Stopping one stream must not tear down the mixer or another stream
- `play` / `pause` / `resume` / `stop` are transport only (thin `esp_player_*` wrappers). Input lifecycle is per path: URL is `set_url` + `play` / `stop`, feed is `set_track` + first `write_frame` and the app stopping its writer, link is `link` / `set_provider` and `unlink` / `set_provider(NULL)`. Do not move bridge teardown or SRC abort into `stop`
- `esp_player_service_stop()` clears the slot input kind; the next `set_url` / `set_track` can pick a different path (PAUSE vs DROP is re-checked). The next `set_track` starts a new feed track set; re-feed without it keeps the previous set. Feed `av_mask` follows declared tracks; `set_url` restores the output mask. Link copies tracks from the SRC on `set_provider` / `link` (no app `set_track`)
- A linked stream keeps its bridge across `stop`, so the next SRC frame restarts the player through `write_audio_frame()` / `ensure_av_feed_session()`. This is why switching a linked stream to URL / feed requires `unlink` first
- Mixer stream on a slot may be NULL (no codec/writer yet)
- `write_frame` is invalid while a provider is linked (`provider.ops != NULL`)
- Playlist auto-advance and mix preemption run on the `ps_defer` task, never `set_url`+`play` on the player event thread. Its name is deliberately not `ctrl`: `*_ctrl.c` is the transport API surface. The task is scheduler-tunable like every other service thread, and `ps_stop_runtime()` quiesces it before touching players, so deferred work cannot outlive a slot. Do not move this work onto `esp_timer`: its task priority and stack are global and its callback cannot be joined
- `ps_ensure_defer()` builds that task on demand from `set_playlist` (non-NULL) and `set_mix_cfg`, the only two calls that can make deferred work reachable: auto-advance needs `slot->playlist`, and `slot_holds_floor()` requires `mix.cfg_set`. A plain single-stream player never pays for it. Adding a third source of deferred work means adding the `ensure` call that arms it
- Player events are forwarded by name (one enum entry per engine event); the service invents no aggregate event. `TRACK_CHANGED` is the only service-raised event. Unmapped engine events map to `NONE` and are dropped
- `esp_service_stop` / `destroy` and `set_provider(NULL)` (so also `unlink`) wait until provider bridge tasks join (same as `esp_player_stop`). Do not treat join failure as a leakable `ESP_ERR_TIMEOUT`
