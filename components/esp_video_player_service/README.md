# ESP Video Player Service

- [![Component Registry](https://components.espressif.com/components/espressif/esp_video_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_video_player_service)
- [中文版](./README_CN.md)

`esp_video_player_service` is a board-aware **video (A/V) player** built on top of [`esp_player_service`](../esp_player_service/README.md). It looks up the board LCD and the board audio codec, builds a video render for the display, attaches the audio output context, and plays audio, video or synchronized A/V content on a board with a screen.

The returned handle is a standard `esp_player_service_t *`. After creation you control it with the normal `esp_player_service` / `esp_media_service` / `esp_service` APIs (start, stop, link, transport, playlist), and release it with `esp_player_service_destroy()`. The audio outlet can be named in this component's `apply_setup()`, or configured beforehand with [`esp_audio_player_service`](../esp_audio_player_service/README.md)'s `apply_setup()`, which then wins. A screen product creates this component only, never a second audio instance for the same speaker.

## Features

- Video playback: plays the video track of a container on the board LCD, or on a display the application builds itself
- Synchronized A/V playback: the audio and the video of one session play as one stream, with a selectable clock source
- Configurable LCD output rate, 30 frames per second by default, independent of the source frame rate
- Track selection: reports the tracks of the current source and plays the one you pick
- Three input paths per stream, from the parent: a URL (`file:///`, HTTP(S), HLS `.m3u8`), frames fed by the application, or a media link from a peer source service
- Transport control per stream, from the parent: play, pause, resume, stop, millisecond seek, playback speed, duration, position, state, and playback events
- Multi-stream mixing with three static priorities (background, notify, urgent) and coexist-ducking or exclusive preemption, configured per stream; see [Mix and Preemption](../esp_player_service/README.md#mix-and-preemption)
- Optional MCP (`CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE`): a complete flash-resident schema of playback tools

## Typical Scenarios

- Local or HTTP video playback on a screen product
- A movie on one stream with TTS or a prompt tone mixed in on another stream
- Smart displays, doorbells and cameras alternating between video clips, prompts and background music
- Cascade with any SRC service (capture, extractor, RTSP / RTMP / SIP, and so on): the peer's A/V reaches one stream of this sink over a media link, video on the LCD and audio on the DAC, without the app forwarding frames

## Architecture

One instance owns one video render. Concurrent playback is a **stream** on that instance, and streams are equal: whether a session is a movie or a TTS prompt is decided by its tracks, not by a stream type. Two movies share the same video render.

`create()` attaches the board DAC, through the audio subclass, and the video context. LCD lookup happens in `apply_setup()`, not in create.

Two `create()` calls for one speaker would build two mixers, which is why a screen is added to an existing audio instance with `esp_video_player_service_attach()` on that handle instead.

### Headers

| Header | Contents |
| --- | --- |
| `esp_video_player_service.h` | `create` / `attach` (identity + `render_fps`) and container track APIs (with `stream`) |
| `esp_video_player_service_setup.h` | LCD / DAC fallback `apply_setup`, plus `set_render()` |
| `esp_video_player_service_mcp.h` | Complete MCP schema (playback + tracks), `CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE` |

Playback, mix, playlist, feed and link stay on `esp_player_service_*` and take a `stream` argument. Scheduler task names are in the parent `esp_player_scheduler.h` (included by `esp_player_service.h`).

## Quick Start

```c
#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service_playback.h"
#include "esp_service.h"
#include "esp_video_dec_default.h"
#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());
ESP_ERROR_CHECK(esp_video_dec_register_default());

esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_video_player_service_create(&cfg, &p));

esp_video_player_service_setup_t lcd = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
ESP_ERROR_CHECK(esp_video_player_service_apply_setup(p, &lcd));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/movie.mp4"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));
/* Stream 1: TTS / prompt on the same mixer */
```

## Video Output

A service has exactly one video render. Two ways to install it, both immediate, and **the later call replaces the earlier one**:

1. `apply_setup(.display_dev_name)` — built from an already initialized board LCD; the service owns it and destroys it
2. `esp_video_player_service_set_render(handle)` — built by the caller, who keeps ownership; the service never destroys it

Clearing `display_dev_name` to NULL, or `set_render(NULL)`, installs nothing and disables the video path. A caller-built render needs `set_render()` only; no apply_setup is required afterwards.

Audio when the audio-subclass cache is empty: the board DAC named by `audio_dev_name`. A prior `esp_audio_player_service_apply_setup()` cache wins.

NULL means the same thing in both name fields: leave that device out. The board default names live in `ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT()`, with no hidden fallback in the implementation.

Ending up with no audio outlet is allowed: streams play as video only and audio tracks are dropped. Adding an outlet later needs another `apply_setup()`, which discards any player built before it, so the URL and the feed track declarations must be issued again. The mirror case, an outlet without a video render, is audio only. With neither, `set_url()` and `write_frame()` report `ESP_ERR_INVALID_STATE`.

## Configuration

`CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE` (needs `CONFIG_ESP_MCP_ENABLE`, off by default) compiles the MCP handler and its flash-resident schema. The audio and video paths of the underlying engine are gated by `esp_player`'s `CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO`, reused here instead of a second switch. Device names, `render_fps` and the mixer settings are runtime configuration, not Kconfig.

## Points Of Attention

- Initialize the LCD (and the DAC) with `esp_board_manager_init_device_by_name()` first. This service does not init or deinit panels.
- Register the global extractor / audio decoder / video decoder tables before play. This service does not call `register_default` or `unregister_default`.
- `render_fps` is fixed at create / attach, so changing the LCD output rate needs a new create or attach.
- Mix / preemption (`set_mix_cfg` after `apply_setup`), transport, and the per-path start / stop contract belong to the parent: `esp_player_service_stop()` does not end an input, and a linked stream must be unlinked before it can switch to a URL or a feed. See [`esp_player_service`](../esp_player_service/README.md#mix-and-preemption).

## Example

- `examples/video_player_mix_cli_example` — movie URL(+playlist), ES or dummy-src link on stream 0, TTS on stream 1

## MCP Tools

With `CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE=y` the component exposes these tools under the `esp_video_player_service_` prefix:

- Transport: `play`, `pause`, `resume`, `stop`, `seek`, `set_speed`, `set_url`
- Playlist: `playlist_next`, `playlist_prev`, `playlist_play_index`, `set_repeat_mode`
- Volume: `set_volume`, `get_volume`, `set_output_volume`, `get_output_volume`
- Mixing and status: `set_mix_cfg`, `get_preempt_state`, `get_status`
- Container tracks: `get_track_info`, `enable_track`

Register them with a service manager through `esp_video_player_service_mcp_schema_get()` and `esp_video_player_service_tool_invoke()`. Media link and unlink stay in `esp_media_service` MCP, and frames never travel over MCP.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
