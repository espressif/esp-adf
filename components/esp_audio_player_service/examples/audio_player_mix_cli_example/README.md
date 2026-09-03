# Audio Player Mix CLI Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates concurrent mixing of three input paths on one
`esp_audio_player_service` instance, controlled from a serial CLI
(`esp_cli_service`). Stream 0 is a URL playlist, not a single file.

| Stream | Role | Priority | Input path | Default media |
|--------|------|----------|------------|---------------|
| 0 `url` | Background music | `BACKGROUND` | URL + playlist (`set_playlist` + `play_index`) | `test.mp3` / `.aac` / `.wav` / `.opus` |
| 1 `link` | Notify overlay | `NOTIFY` | Link (`esp_media_service_link`) | dummy SRC, pattern AAC |
| 2 `feed` | Urgent announcement | `URGENT` | Feed (`set_track` + `write_frame`) | `/sdcard/test_8000hz_16bit_2ch_10000ms.pcm` |

Mix and preemption are fixed at boot: URL / LINK use `COEXIST` (duck), FEED uses
`EXCLUSIVE`. When FEED plays, URL freezes via `PAUSE` and LINK yields via `DROP`;
both restore when FEED ends. Join or leave streams from the console without
rebuilding. Playlist commands (`list` / `play` / `next` / `prev` / `mode`) act
only on stream 0. Refer to `link_source.c` for the link path. The dummy SRC
loops until `stop link`.

### Typical Scenarios

Multi-stream audio products that mix BGM, a notify-priority overlay, and urgent
announcements on one DAC, with priority-based ducking or exclusive preemption,
plus a local playlist for the BGM stream. This example uses dummy SRC on the
notify slot; a product can replace it with TTS or another SRC.

## Environment Setup

### Hardware Required

- Default board: **ESP32-S3** with **audio DAC**, **SD card** and PSRAM (via `esp_board_manager`)
- FAT-formatted microSD card

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

Place these under the SD mount root (for example `/sdcard/`):

| File | Description |
|------|-------------|
| `test.mp3` | Playlist index 0 |
| `test.aac` | Playlist index 1 |
| `test.wav` | Playlist index 2 |
| `test.opus` | Playlist index 3 |
| `test_8000hz_16bit_2ch_10000ms.pcm` | Raw PCM for `start feed`: 16-bit LE, 8 kHz stereo (~10 s); format in `main/settings.h` |

A missing PCM file only fails `start feed`; URL playlist and link still run.

Generate the PCM file on a PC (example):

```bash
edge-tts --text "Hello from the audio player mix CLI" --write-media test.mp3
ffmpeg -i test.mp3 -ar 8000 -ac 2 -t 10 -f s16le test_8000hz_16bit_2ch_10000ms.pcm
```

File names, playlist URLs and PCM format can be changed in `main/settings.h`.

## Build and Flash

### Build Preparation

Before building, ensure the ESP-IDF environment is set up:

```bash
./install.sh
. ./export.sh
```

Go to this example:

```bash
cd $ADF_PATH/components/esp_audio_player_service/examples/audio_player_mix_cli_example
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager).
Install the helper once in the activated IDF Python environment:

```bash
pip install esp-bmgr-assist
```

List and select a board:

```bash
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### Build and Flash

```bash
idf.py build
idf.py -p PORT flash monitor
```

## How to Use the Example

### Functionality and Usage

After boot the example runs `help` so every registered command is listed.
The prompt is `mix>`:

| Command | Description |
|---------|-------------|
| `start url` | Join stream 0 and play the current playlist item |
| `start link` | Join stream 1 (dummy SRC, looping pattern AAC) |
| `start feed` | Join stream 2 (feed / PCM) |
| `stop url` / `stop link` / `stop feed` | Leave the stream and release producers |
| `list` / `list <start>` | List URL playlist items from `start` (`*` = current) |
| `play` / `play <index>` | Play current URL item (resume if paused), or a zero-based index |
| `next` / `prev` | Next / previous URL playlist item |
| `mode <none\|one\|all\|shuffle>` | URL playlist repeat mode |
| `pause` / `resume` | Pause / resume stream 0 only |
| `status` | Print roles, playback state and current URL track |
| `help` | List console commands |

Typical session:

```text
mix> start url
mix> list
mix> play 1
mix> next
mix> start link
mix> start feed
mix> status
mix> stop feed
mix> stop url
```

Priority order is URL < LINK < FEED. Starting LINK ducks URL (`COEXIST`).
Starting FEED exclusively preempts URL / LINK (URL pauses and resumes; LINK
drops frames).

### Repeat modes (`mode`)

| Mode | Behavior |
|------|----------|
| `none` | Stop at list ends; no wrap on next/prev or auto-advance |
| `one` | Repeat the current item |
| `all` | Loop the whole list (default in this example) |
| `shuffle` | Pick a random other item on next/prev / auto-advance |

## Troubleshooting

- No sound: confirm DAC init logs and that the playlist files / PCM exist on the SD card.
- `start feed` fails to open PCM: check the path and format in `settings.h`.
- `play` / `next` fails: the current playlist URL is missing from the card.
- CLI does not appear: check USB-Serial-JTAG / UART console settings for your board.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports: [ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
