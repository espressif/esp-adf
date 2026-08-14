# ESP Audio Player Service

- [![Component Registry](https://components.espressif.com/components/espressif/esp_audio_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_audio_player_service)
- [中文版](./README_CN.md)

`esp_audio_player_service` is a board-aware **audio player** built on top of [`esp_player_service`](../esp_player_service/README.md). It looks up the board DAC through `esp_board_manager`, attaches it to the mixer, and lets several sources (background music, TTS, a prompt tone) play on that one outlet as separate streams.

The returned handle is a standard `esp_player_service_t *`. After creation you control it with the normal `esp_player_service` / `esp_media_service` / `esp_service` APIs (start, stop, link, transport, playlist), and release it with `esp_player_service_destroy()`. Adding a display later does not need a second instance: call [`esp_video_player_service`](../esp_video_player_service/README.md)'s `attach()` on this same handle.

## Features

- Board DAC output discovered through `esp_board_manager`; a later setup that names no device keeps the current outlet
- Mixer sample format, process period and initial DAC volume set at setup
- Three input paths per stream, from the parent: a URL (`file:///`, HTTP(S), HLS `.m3u8`), PCM or encoded frames fed by the application, or a media link from a peer source service
- Transport control per stream, from the parent: play, pause, resume, stop, millisecond seek, playback speed, duration, position, state, and playback events
- Multi-stream mixing with three static priorities (background, notify, urgent) and coexist-ducking or exclusive preemption, configured per stream; see [Mix and Preemption](../esp_player_service/README.md#mix-and-preemption)
- Per-stream volume plus one device output volume for the DAC
- Playlist per stream with next, previous, jump to index, repeat modes and auto-advance
- Optional ID3 parse for bare MP3 URLs
- Optional MCP (`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE`): a complete flash-resident schema of playback tools

## Typical Scenarios

- Music players, sound effects, prompt tones and TTS playback
- Voice assistants where a TTS answer must duck or preempt background music
- Cascade with any SRC service (capture, extractor, RTSP / RTMP / SIP, and so on): the peer's audio reaches one stream of this sink over a media link and plays on the board DAC without the app forwarding frames
- Speakers, and other screen-free products that need several audio sources on one DAC

## Architecture

The service is a thin board-aware layer: it only adds the board DAC lookup, and everything else is the parent. A custom PCM writer or a codec handle owned by the application stays on the parent `esp_player_service_apply_setup()`.

Two instances on one speaker would each build their own mixer, which is why a display is added with `esp_video_player_service_attach()` on the existing handle instead of a second `create()`.

### Headers

| Header | Contents |
| --- | --- |
| `esp_audio_player_service.h` | `create` (returns parent handle) |
| `esp_audio_player_service_setup.h` | Board DAC `apply_setup` |
| `esp_audio_player_service_mcp.h` | Complete MCP schema (playback + ID3), `CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE` |

Playback, mix, playlist, ID3 and events live in `esp_player_service_playback.h`. Scheduler task names are in the parent `esp_player_scheduler.h` (included by `esp_player_service.h`).

## Quick Start

```c
#include "esp_audio_dec_default.h"
#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service_playback.h"
#include "esp_service.h"

ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());

esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
cfg.max_stream_num = 2;
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_audio_player_service_create(&cfg, &p));

esp_audio_player_service_setup_t dac = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
dac.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
ESP_ERROR_CHECK(esp_audio_player_service_apply_setup(p, &dac));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));

esp_service_stop(ESP_SERVICE_BASE(p));
esp_player_service_destroy(p);
```

## Configuration

`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE` (needs `CONFIG_ESP_MCP_ENABLE`, off by default) compiles the MCP handler and its flash-resident schema. The audio and video paths of the underlying engine are gated by `esp_player`'s `CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO`, reused here instead of a second switch. Mixer format, process period, DAC volume and stream count are runtime settings, not Kconfig.

## Points Of Attention

- Initialize board devices with `esp_board_manager` before calling these APIs.
- Register the global extractor and audio decoder tables before play. This service does not call `register_default`.
- `esp_audio_player_service_apply_setup()` discards any player built before it, so the URL or the feed track declarations must be issued again.
- Mix / preemption (`set_mix_cfg` after `apply_setup`), transport, and the per-path start / stop contract belong to the parent: `esp_player_service_stop()` does not end an input, and a linked stream must be unlinked before it can switch to a URL or a feed. See [`esp_player_service`](../esp_player_service/README.md#mix-and-preemption).

## Example

- `examples/audio_player_mix_cli_example` — three-stream mix (URL playlist / link / feed)
- `examples/audio_player_mcp_example` — HTTP MCP

## MCP Tools

With `CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE=y` the component exposes these tools under the `esp_audio_player_service_` prefix:

- Transport: `play`, `pause`, `resume`, `stop`, `seek`, `set_speed`, `set_url`
- Playlist: `playlist_next`, `playlist_prev`, `playlist_play_index`, `set_repeat_mode`
- Volume: `set_volume`, `get_volume`, `set_output_volume`, `get_output_volume`
- Mixing and status: `set_mix_cfg`, `get_preempt_state`, `get_status`
- ID3: `enable_id3_parse`, `get_id3_info`

Register them with a service manager through `esp_audio_player_service_mcp_schema_get()` and `esp_audio_player_service_tool_invoke()`. Media link and unlink stay in `esp_media_service` MCP, and frames never travel over MCP.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
