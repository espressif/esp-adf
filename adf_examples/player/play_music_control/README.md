
# CLI multi-source music player

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates a multi-source music player controlled through a serial CLI. It adds local audio files from a microSD card, online audio URLs, and prompt tones stored in flash to one playlist, and lets users control playback, pause, track change, volume, and play mode from the serial console.

Audio under the **`/sdcard`** mount point and its first-level subdirectories is scanned into a playlist. Playlist metadata can be cached under `/sdcard/__playlist/` for faster startup on the next boot.

Supported formats include MP3, WAV, FLAC, AAC, M4A, TS, AMRNB, AMRWB, and other common types.

### Typical Use Cases

- CLI-driven multi-source music playback and playlist management
- Mixed local SD card tracks and HTTP/HTTPS streaming
- Offline prompt tones (LittleFS) plus online audio on embedded devices

## Environment Setup

### Hardware Required

- microSD card (FAT) with audio files
- Audio DAC and speaker (or headphones)
- Wi-Fi with Internet access when using HTTP test URLs
- For chips without on-chip Wi-Fi, the board must provide available network connectivity

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- Place supported audio files under **`/sdcard`** or its first-level subdirectories on the microSD card
- Configure Wi-Fi SSID and password in menuconfig to reach the public HTTPS test URLs built into the example

### Prompt Tone Resources

Prompt tones are stored in a **LittleFS** flash partition (`storage`), mounted at runtime as `/littlefs`, and played via `file://littlefs/...` URIs.

The `tone/` directory contains four test files: `alarm.mp3`, `dingdong.mp3`, `haode.mp3`, and `new_message.mp3`. At build time, `littlefs_create_partition_image()` packs them into `storage.bin`, which is flashed together with the app. To replace tones, update the mp3 files under `tone/` and rebuild/flash:

```
adf_examples/player/play_music_control/tone/
├── alarm.mp3
├── dingdong.mp3
├── haode.mp3
└── new_message.mp3
```

> Total tone size must fit within the `storage` partition size in `partitions.csv` (1 MB by default).

## Build and Flash

### Build Preparation

Before building, ensure the ESP-IDF environment is set up. If not, run in the ESP-IDF root directory:

```
./install.sh
. ./export.sh
```

Go to this example directory:

```
cd adf_examples/player/play_music_control
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) for the SD card, audio output, and other board peripherals. Install [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) to select the development board with the commands below:

```bash
pip install esp-bmgr-assist
```

List supported boards:

```bash
idf.py bmgr -l
```

Select a board (generates `components/gen_bmgr_codes/` and sets the target; **do not commit this folder to Git**):

```bash
idf.py bmgr -b esp32_p4_function_ev
```

> To use another board, run `idf.py bmgr -b <board_name>` again. For custom boards, see the [ESP Board Manager customization guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/docs/how_to_customize_board.md).

### Project Configuration

Default options are in `sdkconfig.defaults` and `sdkconfig.defaults.<target>` (GMF IO, Simple Player resample, custom partition table, etc.). You usually only need to set Wi-Fi:

```bash
idf.py menuconfig
```

Configure:

- **Example Configuration** → **WiFi SSID**
- **Example Configuration** → **WiFi Password**

> For CI, use `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}` in `sdkconfig.ci`. Do not commit Wi-Fi passwords in `sdkconfig.defaults`; set the password locally in menuconfig.

Default configuration checks (usually already set in defaults):

- **Audio Simple Player**: resample / channel / bit-width targets
- **GMF IO**: File / HTTP readers, Codec Dev TX

This example uses a custom partition table `partitions.csv` (`factory` ~2.5 MB + `storage` LittleFS 1 MB). Ensure flash size matches `sdkconfig.defaults` (4 MB by default).

Press `s` to save and `Esc` to exit menuconfig.

### Build and Flash Commands

```
idf.py build
idf.py -p PORT flash monitor
```

`idf.py flash` writes both the application and the `storage` LittleFS image (`build/storage.bin`). Exit the monitor with `Ctrl-]`. The CLI prompt is `gmf>`; type `help` for commands.

## How to Use the Example

### Functionality and Usage

After startup, the example initializes peripherals, mounts LittleFS, loads or scans the playlist, connects to Wi-Fi (for HTTP tracks), and starts the CLI. Commands:

| Command      | Description                                      |
|--------------|--------------------------------------------------|
| `scan`       | Scan `/sdcard` and its first-level subdirectories (including `audio`) |
| `list`       | List all tracks and URLs in the playlist         |
| `play [N]`   | Play current or specified song (N = index)       |
| `pause`      | Pause the current song                           |
| `resume`     | Resume the paused song                           |
| `stop`       | Stop the current song                            |
| `next`       | Play the next song                               |
| `prev`       | Play the previous song                           |
| `get_vol`    | Get current volume level                         |
| `set_vol V`  | Set volume level (0-100)                         |
| `mute`       | Mute the audio output                            |
| `unmute`     | Unmute the audio output                          |
| `status`     | Show the current player status                   |
| `mode [M]`   | Get or set play mode when a song ends (see below)|

### Play Mode

When the current song finishes, behavior depends on **play mode**:

- **stop** (default): Stop after the current song; no auto-next.
- **single**: Single-song loop — replay the same song when it ends.
- **list**: List loop — play the next song when the current ends; wrap to the first at the end of the list.

Use `mode` with no argument to show the current mode; use `mode stop`, `mode single`, or `mode list` to set it. `status` also shows the current mode.

Automated tests (`pytest_play_music_control.py`) run `play 1` after the CLI is ready and expect a `Song finished` log; Internet access is required for the built-in HTTPS test URL.

### Log Output

On startup, the example loads the playlist (from `/sdcard/__playlist/` cache if present, otherwise scans **`/sdcard`**), adds HTTP and LittleFS tone URLs, connects to Wi-Fi, then enters the CLI. Key log lines on first run (timestamps, file counts, and Wi-Fi details depend on your SD card and network):

```c
I (1327) main_task: Calling app_main()
I (1328) PLAY_MUSIC_CONTROL: === Play Music Control (multi-source) ===
I (1328) PLAY_MUSIC_CONTROL: [ 1 ] Setup peripheral for audio codec and SD card
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1411) PLAY_MUSIC_CONTROL: [ 2 ] Mount LittleFS tone partition
I (1419) PLAY_MUSIC_CONTROL: Partition size: total: 1048576, used: 90112
I (1419) PLAY_MUSIC_CONTROL: [ 3 ] Initialize simple player
I (1420) PLAY_MUSIC_CONTROL: [ 4 ] Initialize CLI music player and register commands
I (1462) CLI_MUSIC_CONTROL: Loaded 23 entries from playlist JSON (skip import)
I (1463) CLI_MUSIC_CONTROL: Music library ready! Use 'list' to see all files.
I (1463) CLI_MUSIC_CONTROL: ================================================
I (1463) CLI_MUSIC_CONTROL:      ESP-GMF Music Player - CLI Control
I (1463) CLI_MUSIC_CONTROL: ================================================
I (1463) CLI_MUSIC_CONTROL: Available commands:
I (1463) CLI_MUSIC_CONTROL:   scan      - Scan for music files in SD card
I (1463) CLI_MUSIC_CONTROL:   list      - List all music files and URLs
I (1464) CLI_MUSIC_CONTROL:   play [N]  - Start playing current or specified song (N=file index)
I (1464) CLI_MUSIC_CONTROL:   pause     - Pause current song
I (1464) CLI_MUSIC_CONTROL:   resume    - Resume paused song
I (1464) CLI_MUSIC_CONTROL:   stop      - Stop current song
I (1464) CLI_MUSIC_CONTROL:   next      - Play next song
I (1464) CLI_MUSIC_CONTROL:   prev      - Play previous song
I (1464) CLI_MUSIC_CONTROL:   get_vol   - Get current volume level
I (1465) CLI_MUSIC_CONTROL:   set_vol   - Set volume level (0-100)
I (1465) CLI_MUSIC_CONTROL:   mute      - Mute audio output
I (1465) CLI_MUSIC_CONTROL:   unmute    - Unmute audio output
I (1465) CLI_MUSIC_CONTROL:   status    - Show current player status
I (1465) CLI_MUSIC_CONTROL:   mode [stop|single|list] - Get/set play mode (stop/single loop/list loop)
I (1465) CLI_MUSIC_CONTROL:   help      - Show help information
I (1465) CLI_MUSIC_CONTROL:   restart   - Restart system
I (1465) CLI_MUSIC_CONTROL:   free      - Show free memory
I (1466) CLI_MUSIC_CONTROL: ================================================
I (1466) CLI_MUSIC_CONTROL: Found 23 music files. Use 'list' to view them.
I (1466) CLI_MUSIC_CONTROL: Use 'play' to start playing the first song.
I (1466) CLI_MUSIC_CONTROL: ================================================

I (1467) CLI_MUSIC_CONTROL: Audio muted (volume remains: 80%)
I (1467) CLI_MUSIC_CONTROL: Adding 3 source URLs to media DB and playlist...
I (1506) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
I (1544) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac
I (1581) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.wav
I (1606) CLI_MUSIC_CONTROL: Successfully added 3/3 source URLs (playlist count=23)
I (1606) CLI_MUSIC_CONTROL: Adding 4 source URLs to media DB and playlist...
I (1642) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/alarm.mp3
I (1680) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/dingdong.mp3
I (1720) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/haode.mp3
I (1757) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/new_message.mp3
I (1778) CLI_MUSIC_CONTROL: Successfully added 4/4 source URLs (playlist count=23)
I (1778) PLAY_MUSIC_CONTROL: [ 5 ] Connect to WiFi
I (9756) PLAY_MUSIC_CONTROL: WiFi connected

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
I (9802) PLAY_MUSIC_CONTROL: CLI ready (type 'help' for commands)
gmf>
gmf>  list
I (11859) CLI_MUSIC_CONTROL: === Music File List (Total: 23) ===
I (11859) CLI_MUSIC_CONTROL: * [1] [URL]  https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
I (11860) CLI_MUSIC_CONTROL:   [2] [FILE] sdcard/esp_gmf_rec001.aac
I (11861) CLI_MUSIC_CONTROL:   [3] [FILE] sdcard/audio/Solitary_brave.mp3
I (11861) CLI_MUSIC_CONTROL:   [4] [FILE] sdcard/audio/boot.mp3
I (11861) CLI_MUSIC_CONTROL:   [5] [FILE] sdcard/audio/esp_gmf_rec001.aac
I (11862) CLI_MUSIC_CONTROL:   [6] [FILE] sdcard/audio/ff-16b-2c-44100hz.aac
I (11862) CLI_MUSIC_CONTROL:   [7] [FILE] sdcard/audio/ff-16b-2c-44100hz.flac
I (11863) CLI_MUSIC_CONTROL:   [8] [FILE] sdcard/audio/ff-16b-2c-44100hz.mp3
I (11863) CLI_MUSIC_CONTROL:   [9] [FILE] sdcard/audio/ff-16b-2c-44100hz.wav
I (11864) CLI_MUSIC_CONTROL:   [10] [FILE] sdcard/audio/music.mp3
I (11864) CLI_MUSIC_CONTROL:   [11] [FILE] sdcard/audio/test.aac
I (11865) CLI_MUSIC_CONTROL:   [12] [FILE] sdcard/audio/test.flac
I (11865) CLI_MUSIC_CONTROL:   [13] [FILE] sdcard/audio/test.mp3
I (11865) CLI_MUSIC_CONTROL:   [14] [FILE] sdcard/audio/test.wav
I (11866) CLI_MUSIC_CONTROL:   [15] [FILE] sdcard/test.wav
I (11866) CLI_MUSIC_CONTROL:   [16] [FILE] sdcard/test.aac
I (11867) CLI_MUSIC_CONTROL:   [17] [FILE] sdcard/test.flac
I (11867) CLI_MUSIC_CONTROL:   [18] [URL]  https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac
I (11868) CLI_MUSIC_CONTROL:   [19] [URL]  https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.wav
I (11868) CLI_MUSIC_CONTROL:   [20] [FILE] littlefs/alarm.mp3
I (11869) CLI_MUSIC_CONTROL:   [21] [FILE] littlefs/dingdong.mp3
I (11869) CLI_MUSIC_CONTROL:   [22] [FILE] littlefs/haode.mp3
I (11869) CLI_MUSIC_CONTROL:   [23] [FILE] littlefs/new_message.mp3
I (11870) CLI_MUSIC_CONTROL: =============================

gmf>  set_vol 50
I (16335) CLI_MUSIC_CONTROL: Volume set to 50%
gmf>  play
I (18404) CLI_MUSIC_CONTROL: Audio unmuted (volume: 50%)
I (18404) CLI_MUSIC_CONTROL: Playing song [1/23]: https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
gmf>  I (23253) PLAY_MUSIC_CONTROL: Music info - rate:48000, channels:2, bits:16
I (23254) PLAY_MUSIC_CONTROL: Player state: ESP_AUD_SIMPLE_PLAYER_RUNNING
gmf>  next
I (27422) PLAY_MUSIC_CONTROL: Player state: ESP_AUD_SIMPLE_PLAYER_STOPPED
I (27423) CLI_MUSIC_CONTROL: Playing song [2/23]: file://sdcard/esp_gmf_rec001.aac
```

## Troubleshooting

- If no music is found, ensure the microSD card is mounted and supported files are under **`/sdcard/audio`** (or another first-level subdirectory of **`/sdcard`**), or run `scan` again.
- If LittleFS tones fail to play, verify mp3 files exist under `tone/` and that you rebuilt and flashed (including `storage.bin`); startup logs should show `Mount LittleFS tone partition` and partition usage.
- If HTTP tracks fail, check **Example Configuration** Wi-Fi SSID/password, confirm `WiFi connected` appears in the startup log, and verify the device got an IP address.
- If playback fails, verify the file is not corrupted, the format is supported, and the DAC initialized correctly.
- The example enables `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY` by default for the built-in HTTPS test URLs; use only in demo environments.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
