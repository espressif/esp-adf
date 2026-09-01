# ESP Player Service

- [![Component Registry](https://components.espressif.com/components/espressif/esp_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_player_service)
- [中文版](./README_CN.md)

`esp_player_service` is the media **SINK** playback service in ESP-ADF. One instance owns the playback outputs of one board and plays audio, video or synchronized A/V content on them. Concurrent playback (BGM, TTS, a movie) is a **stream** on that instance.

The board-aware players [`esp_audio_player_service`](../esp_audio_player_service/README.md) and [`esp_video_player_service`](../esp_video_player_service/README.md) are built on top of this component. Use one of them on a board product; use this component directly when the PCM outlet is not a board DAC.

## Features

- Three mutually exclusive inputs per stream: a URL (`file:///`, HTTP(S), HLS `.m3u8`), frames fed by the application and **copied** into the player, or a **zero-copy** media link from a peer source service; see [Stream Inputs](#stream-inputs)
- Transport control per stream: play, pause, resume, stop, millisecond seek, playback speed, duration, position and state
- Audio-only, video-only or A/V per stream, following the outputs actually installed: a speaker mixer and a video render, of which at least one is required
- Multi-stream mixing with a configurable stream count: per-stream volume plus one device output volume
- Three static priorities (background, notify, urgent) with coexist-ducking or exclusive preemption, configured per stream; see [Mix and Preemption](#mix-and-preemption)
- Playlist per stream with next, previous, jump to index, repeat modes and auto-advance
- A/V in one session: at most one audio and one video track per stream, with a selectable sync clock source and a configurable extractor pool and prebuffer
- Playback events delivered to one callback per service, covering playback finished, buffering, playlist advance and errors
- Optional ID3 parse for bare MP3 URLs
- Optional video output, installed by the video subclass: one display per instance, and streams stay equal
- Audio and video paths are separately removable at build time (`CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO`)

This component has no MCP. Product MCP schemas live in the audio/video subclasses (`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE` / `CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE`).

## Typical Scenarios

- Products whose PCM outlet is not a board DAC: a custom writer or an application-owned codec handle
- Background music, TTS and prompt tones that must coexist or preempt each other on one speaker
- Cascade with any SRC service (capture, extractor, RTSP / RTMP / SIP, and so on): the peer's audio or A/V reaches one stream of this sink over a media link and plays without the app forwarding frames
- Board products, indirectly: they create one of the two subclasses, which return this parent handle

## Architecture

Board DAC and LCD lookup live in the subclasses [`esp_audio_player_service`](../esp_audio_player_service/README.md) and [`esp_video_player_service`](../esp_video_player_service/README.md). Both `create()` APIs return this parent handle. Destroy with `esp_player_service_destroy()`.

Lifecycle is unified through `esp_service`: `ESP_SERVICE_BASE(player)` for start / stop. Stop and destroy wait until media-link bridge tasks join. The role becomes `ESP_MEDIA_ROLE_SINK` after a successful `apply_setup`.

```text
            esp_service  (lifecycle: init / start / stop / deinit)
                  ^
            esp_media_service  (role, set_provider, link)
                  ^
            esp_player_service  (this component, ROLE_SINK)
                  |
   +--------------+----------------------------------+
   |  [DAC mixer (esp_audio_render)] [LCD render]    |
   |  +-- stream 0: this session A and/or V          |
   |  +-- stream 1: typically TTS / BGM (audio)      |
   +-------------------------------------------------+
```

Streams are equal. This session is A, V, or A+V from the URL extractor, feed `set_track`, or link SRC tracks, clipped to whichever outputs are installed. One LCD per instance; two movies share that video render.

Board products should call `esp_audio_player_service_create()` or `esp_video_player_service_create()` instead of the parent `create()` so DAC / LCD attach happens automatically. Do not create an audio instance and a video instance on the same speaker. Add a display with `esp_video_player_service_attach()`.

Pure audio firmware does not link `esp_video_render`. The video subclass registers GMF video elements into the pool. Global decoder tables stay with the application.

### Headers

| Header | Contents |
| --- | --- |
| `esp_player_service.h` | `create` / `destroy` / `set_deinit_cb`; advanced handle snapshots `get_info` / `get_stream_info` |
| `esp_player_scheduler.h` | Task-name macros and data-flow for `esp_service_scheduler_set_cb()` |
| `esp_player_service_setup.h` | Output `apply_setup` (PCM writer / format) |
| `esp_player_service_playback.h` | Per-stream URL / feed / volume / mix / playlist / ID3 / buffer / A/V sync |

## Quick Start

```c
#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service.h"
#include "esp_player_service_playback.h"
#include "esp_player_service_setup.h"
#include "esp_service.h"

ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());

esp_player_service_cfg_t cfg = ESP_PLAYER_SERVICE_CFG_DEFAULT();
cfg.max_stream_num = 2;
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_player_service_create(&cfg, &p));

esp_player_service_setup_t setup = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
setup.out_writer = my_pcm_writer;   /* or codec_dev; board DAC is the audio subclass */
ESP_ERROR_CHECK(esp_player_service_apply_setup(p, &setup));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));

esp_service_stop(ESP_SERVICE_BASE(p));
esp_player_service_destroy(p);
```

## Stream Inputs

Only one input is active per stream.

| Path | App calls | Data | Typical use |
| --- | --- | --- | --- |
| URL | `set_url` + `play` | Player pulls | `file:///`, HTTP(S), HLS, playlist |
| Feed | `set_track` + `write_frame` | **Copied** into the player | App thread pushes PCM / ES (TTS, custom parser) |
| Link | `esp_media_service_link` or `set_provider` | **Zero-copy** | Peer SRC (e.g. capture) or `esp_media_track_mngr`; **no** `set_track` |

Each path has its own start and stop, shown per path below. `esp_player_service_play()` / `_pause()` / `_resume()` / `_stop()` are player transport on one stream (thin `esp_player_*` wrappers): they never create or tear down a producer. Stopping the whole instance is a different call: `esp_service_stop()` / `esp_player_service_destroy()`.

Do **not** call `play()` on the feed or link path: `play()` requires a URL, feed starts on the first `write_frame`, and a linked stream starts from its bridge. Each stream is limited to one audio and one video track; extra audio (TTS) uses another stream. PCM needs `sample_rate` / `channel` / `bits`; ADTS AAC and MP3 need only `codec`. For A/V, call `set_track` once per type and route with `frame->type`. `write_frame` returns `ESP_ERR_INVALID_STATE` while a provider is linked. For audio-only next: `esp_player_service_stop()` then `set_track` audio once (drops video); this session's `av_mask` is A. `set_url` restores the output A/V mask. A successful `esp_player_service_stop()` clears the remembered input kind, so PAUSE vs DROP is re-checked for the next path.

**URL:**

```c
/* Start */
ESP_ERROR_CHECK(esp_player_service_set_url(p, stream, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, stream));

/* Stop this input */
ESP_ERROR_CHECK(esp_player_service_stop(p, stream));
```

**Feed:**

```c
/* Start: declare the track once, then the first write_frame begins the session */
esp_media_track_info_t track = {
    .id = 1,
    .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    .info.audio = {
        .codec = ESP_FOURCC_PCM,  /* PCM: also set sample_rate / bits / channel */
        .sample_rate = 8000,
        .bits_per_sample = 16,
        .channel = 2,
    },
};
ESP_ERROR_CHECK(esp_player_service_set_track(p, stream, &track));

esp_media_frame_t frame = {
    .track_id = 1,
    .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    .data = pcm,
    .size = pcm_bytes,
    .pts = pts_ms,
};
ESP_ERROR_CHECK(esp_player_service_write_frame(p, stream, &frame));
/* Last frame: ESP_MEDIA_FRAME_FLAG_EOS */

/* Stop this input: the app stops its writer. stop() ends the current session
   right away; writing again starts a new session with the same tracks (call
   set_track again only to change the track set). */
ESP_ERROR_CHECK(esp_player_service_stop(p, stream));
```

**Link:** Use `link` when you have a SRC service. Use `set_provider` when you already hold an `esp_media_provider_t` (for example from a track manager). The peer writes with `esp_media_track_write_frame()`, not the player's `write_frame` / `set_track`. Linking a new SRC replaces the track set (AV then audio-only is fine). Re-linking the same SRC does not need tracks redeclared.

```c
/* Start: the bridge task is armed here, or by esp_service_start() if the
   service is not running yet */
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(src), src_stream,
                                       ESP_SERVICE_BASE(p), play_stream));
/* Or: ESP_ERROR_CHECK(esp_media_service_set_provider(
 *         ESP_SERVICE_BASE(p), play_stream, &provider)); */

/* Stop this input: unlink aborts the provider and joins the bridge task.
   esp_player_service_stop() alone only stops the player, so the bridge keeps
   pulling and the next SRC frame starts playback again. Unlink before reusing
   the stream for URL or feed. */
ESP_ERROR_CHECK(esp_media_service_unlink(ESP_SERVICE_BASE(src), src_stream,
                                         ESP_SERVICE_BASE(p), play_stream));
/* Or: ESP_ERROR_CHECK(esp_media_service_set_provider(
 *         ESP_SERVICE_BASE(p), play_stream, NULL)); */
```

On teardown the SRC should `esp_media_track_write_abort()`. On the sink side `unlink` / `set_provider(NULL)` and `esp_service_stop()` / `destroy` abort the provider and join the bridge task; `esp_player_service_stop()` does not, because it is player transport only.

See `audio_player_mix_cli_example` for all three paths on one mixer.

## Mix and Preemption

Call `esp_player_service_set_mix_cfg()` once per stream after `apply_setup` and before that stream starts playing. Only streams with a mix config join arbitration. The first successful call also starts the service thread that runs it.

Each stream has a static `priority`: `ESP_PLAYER_PRIO_BACKGROUND`, `ESP_PLAYER_PRIO_NOTIFY` or `ESP_PLAYER_PRIO_URGENT`. While a higher-priority stream is preparing, playing or paused, it suppresses lower streams according to **its** `preempt_mode`:

| `preempt_mode` | Effect on lower streams |
| --- | --- |
| `ESP_PLAYER_PREEMPT_COEXIST` | Duck to `duck_gain` and keep playing |
| `ESP_PLAYER_PREEMPT_EXCLUSIVE` | Yield completely (pause or drop; see `on_preempt`) |

`on_preempt` is the **victim** policy when a higher EXCLUSIVE stream takes the floor. It must match that stream's input path:

| `on_preempt` | Allowed input | Behaviour |
| --- | --- | --- |
| `ESP_PLAYER_ON_PREEMPT_PAUSE` | URL only | Freeze the timeline and resume afterwards |
| `ESP_PLAYER_ON_PREEMPT_DROP` | Feed or link only | Discard output while suppressed |

`PAUSE` on feed/link, or `DROP` on URL, returns `ESP_ERR_INVALID_ARG` once both the mix config and the source are known. `active_gain` must be greater than 0 and not less than `duck_gain`. If the mixer is already running and cannot take a new gain, `set_mix_cfg` returns `ESP_ERR_INVALID_STATE`. Query the current suppression with `esp_player_service_get_preempt_state()`.

BGM plus TTS, same pattern as `audio_player_mix_cli_example`:

```c
esp_player_mix_cfg_t bgm = {
    .active_gain = 1.0f,
    .duck_gain = 0.2f,
    .transition_ms = 80,
    .priority = ESP_PLAYER_PRIO_BACKGROUND,
    .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
    .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,  /* URL */
};
ESP_ERROR_CHECK(esp_player_service_set_mix_cfg(p, 0, &bgm));

esp_player_mix_cfg_t tts = {
    .active_gain = 1.0f,
    .duck_gain = 0.2f,
    .transition_ms = 80,
    .priority = ESP_PLAYER_PRIO_NOTIFY,
    .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
    .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,  /* feed or link */
};
ESP_ERROR_CHECK(esp_player_service_set_mix_cfg(p, 1, &tts));
```

An alarm-style feed that must silence every other stream uses `ESP_PLAYER_PRIO_URGENT`, `ESP_PLAYER_PREEMPT_EXCLUSIVE` and `ESP_PLAYER_ON_PREEMPT_DROP`.

## Points Of Attention

- The application owns the global extractor / decoder tables. Call `esp_extractor_register_default()` and `esp_audio_dec_register_default()` before play, and `esp_video_dec_register_default()` on video products. This service never registers or unregisters them.
- `esp_player_service_apply_setup()` discards any player built against the previous output, so the URL and the feed track declarations must be issued again after it.
- `esp_player_service_stop()` is player transport only and does not end an input. Each path has its own stop, described in [Stream Inputs](#stream-inputs).

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
