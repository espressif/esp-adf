# Video Player Mix CLI Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates one `esp_video_player_service` handle mixing a film
and TTS on the same DAC. `create()` returns `esp_player_service_t *`. Playback
is driven from a serial CLI (`esp_cli_service`).

Stream 0 plays the film. Movie URL (playlist), elementary-stream feed,
and media link are mutually exclusive on that slot. Stream 1 TTS can mix with
any of them.

| Stream | Role | Priority | Input | Default media |
|--------|------|----------|-------|---------------|
| 0 `movie` | Film | `BACKGROUND` | URL + playlist | `test.mp4` / `test1.mp4` / `test.mp3` |
| 0 `es` | Film | `BACKGROUND` | Feed (`set_track` + `write_frame`) | `/sdcard/feed.aac` + `feed.mjpeg` |
| 0 `link` | Film | `BACKGROUND` | Link (`esp_media_service_link`) | dummy SRC, bouncing-ball H264 + AAC |
| 1 `tts` | Prompt | `NOTIFY` | Feed PCM | `/sdcard/test_8000hz_16bit_2ch_10000ms.pcm` |

`start movie` while ES or link is running stops that source first. `start es` /
`start link` while another stream-0 source is running stops it first. Playlist
commands (`list` is always available; `play` / `next` / `seek` / `pause`)
require movie mode: they fail with `INVALID_STATE` during ES or link. Refer to
the feed contract in `feed_source.c`, not the ADTS/JPEG parsers. Refer to
`link_source.c` for the link path.

Do not create a second audio player service for TTS. Both streams live on this
handle. The movie ducks when TTS plays (`COEXIST`).

Before initialising the LCD the example raises the board frame buffer count to 2.
`esp_video_render` draws into panel buffers only when the panel owns more than
one; a single buffer forces a per-frame copy.

### Typical Scenarios

Smart displays that play a film, an application-owned elementary stream, or a
linked SRC, and speak prompts on the same speaker.

## Environment Setup

### Hardware Required

- Default board: **ESP32-S3** with **320x240 LCD**, **audio DAC**, **SD card** and PSRAM (via `esp_board_manager`)
- FAT-formatted microSD card with the files above at the mount root

If the board brings up no display, the example still starts. Movie audio and TTS
still mix; ES feeds audio only when the stream has no video output.

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

Place these under the SD mount root (for example `/sdcard/`):

| File | Description |
|------|-------------|
| `test.mp4` | Playlist index 0 |
| `test1.mp4` | Playlist index 1 |
| `test.mp3` | Playlist index 2 (audio-only on purpose) |
| `feed.aac` | AAC ADTS elementary stream for `start es` |
| `feed.mjpeg` | MJPEG elementary stream for `start es` |
| `test_8000hz_16bit_2ch_10000ms.pcm` | Raw PCM: 16-bit LE, 8 kHz stereo (~10 s) |

A missing ES file only fails `start es`; movie URL, link and TTS still run.

Generate PCM:

```bash
edge-tts --text "Hello from the video player mix CLI" --write-media tts.mp3
ffmpeg -i tts.mp3 -ar 8000 -ac 2 -t 10 -f s16le test_8000hz_16bit_2ch_10000ms.pcm
```

Generate elementary streams from any clip. Refer to the `set_track` / `write_frame` /
PTS / EOS contract, not the file parsers:

```bash
ffmpeg -i input.mp4 -vn -c:a aac -b:a 96k -ar 44100 -ac 2 -f adts feed.aac
ffmpeg -i input.mp4 -an -c:v mjpeg -q:v 6 -r 15 -pix_fmt yuvj420p \
       -vf scale=320:240 -f mjpeg feed.mjpeg
```

`-vf scale=` must match the LCD (ESP32-S3 SPI panels in this example are 320x240).
`-r` must match `EXAMPLE_VIDEO_FPS` in `main/settings.h`. File names can be
changed in that header.

## Build and Flash

### Build Preparation

```bash
./install.sh
. ./export.sh
cd $ADF_PATH/components/esp_video_player_service/examples/video_player_mix_cli_example
pip install esp-bmgr-assist
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### Build and Flash Commands

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## How to Use the Example

### Functionality and Usage

After boot the example runs `help` so every registered command is listed.
Prompt is `vpm>`:

| Command | Description |
|---------|-------------|
| `start movie` | Play stream 0 from the URL playlist |
| `start es` | Feed AAC + MJPEG on stream 0 (stops movie or link first) |
| `start link` | Link dummy SRC (bouncing-ball H264 + AAC) to stream 0 (stops movie or ES first) |
| `start tts` | Feed PCM on stream 1 (works with movie, ES or link) |
| `stop movie` / `stop es` / `stop link` | Stop stream 0 (any of the three inputs) |
| `stop tts` | Stop stream 1 |
| `list` / `list <start>` | List movie playlist items from `start` (`*` = current) |
| `play` / `play <index>` | Play current movie item, or a zero-based index |
| `next` / `prev` | Next / previous movie playlist item |
| `seek <ms>` | Seek within the current movie item |
| `mode <none\|one\|all\|shuffle>` | Movie playlist repeat mode |
| `pause` / `resume` | Pause / resume movie URL playback |
| `status` | Print roles, playback state, playlist, ES counters or link |
| `help` | List console commands |

Typical session:

```text
vpm> start movie
vpm> list
vpm> play 1
vpm> start tts
vpm> status
vpm> stop tts
vpm> start es
vpm> stop es
vpm> start link
vpm> stop link
```

### Repeat modes (`mode`)

| Mode | Behavior |
|------|----------|
| `none` | Stop at list ends; no wrap on next/prev or auto-advance |
| `one` | Repeat the current item |
| `all` | Loop the whole list (default in this example) |
| `shuffle` | Pick a random other item on next/prev / auto-advance |

## Troubleshooting

- `start movie failed: ESP_ERR_NOT_FOUND` — place `test.mp4` under `/sdcard/`.
- `start es failed` — place `feed.aac` / `feed.mjpeg` under `/sdcard/`, or stay on movie / link.
- `play` / `next` prints `stream 0 is not in movie mode` — `stop es` or `stop link`, then use movie commands.
- `start tts failed` / `Failed to open PCM` — place the PCM file under `/sdcard/`.
- Audio plays but the screen stays dark — the board brought up no display; the
  startup log prints `No display came up`. Check the board description for an LCD.
- Tearing plus `dma2d_configure_color_space_conversion` errors — the panel came
  up with a single frame buffer. Check for `Using 2 LCD frame buffers` at boot.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports: [ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
