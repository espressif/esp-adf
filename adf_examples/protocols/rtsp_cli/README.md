# RTSP CLI

- [中文版](./README_CN.md)
- Complex Example: ⭐⭐⭐

## Example Brief

This example provides RTSP server, push, and pull roles in one firmware and controls them through a serial CLI, allowing users to exercise common RTSP workflows and switch roles after stopping the active session without rebuilding or reflashing:

- **server** — let VLC or ffplay pull live camera video and microphone audio from `rtsp://<device-ip>:554/live`
- **push** — publish live camera video and microphone audio to a remote RTSP server such as MediaMTX (ANNOUNCE + RECORD)
- **pull** — play a remote RTSP stream on the board's LCD and speaker (DESCRIBE + SETUP + PLAY)

All roles use the same service model: the application links capture, playback, and RTSP services with `esp_media_service_link()` and controls their lifecycle through common APIs instead of implementing RTSP protocol handling and media data flow; the serial CLI also provides a command surface for scripts and agent-driven automation.

For a focused publisher, start with the [`RTSP Push`](../rtsp_push/README.md) example.

### Typical Use Cases

- IP cameras and video doorbells that a phone app or NVR pulls from
- Video surveillance nodes that publish to a central RTSP server
- Live monitoring displays that play a remote camera on a local LCD
- Media streaming clients for a factory or building automation network

### Runtime Flow

1. `esp_board_manager` initializes the camera, LCD, audio ADC, and audio DAC
2. `esp_wifi_service` connects the station
3. `esp_cli_service` starts the `rtsp>` console
4. **server / push**: `esp_video_capture_service` → `esp_media_service_link()` → `esp_rtsp_service` ROLE_SERVER or ROLE_SINK
5. **pull**: ROLE_SRC → `esp_media_service_link()` → `esp_video_player_service` → LCD and speaker

The consuming side of a link is always started first. On teardown the order is reversed: producers stop first and the consumer drains instead of blocking on an empty queue.

### File Structure

```
rtsp_cli/
├── main/
│   ├── app_main.c              Application entry: NVS, board, codecs, Wi-Fi, CLI
│   ├── rtsp_example.h          Board and Wi-Fi helpers
│   ├── rtsp_session.c/h        Three roles: create, setup, link, start, and stop
│   ├── rtsp_cli.c/h            `rtsp` and `wifi` command parsing
│   ├── rtsp_settings.h         Media and protocol defaults
│   ├── Kconfig.projbuild       Wi-Fi credentials and default peer URLs
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── sdkconfig.ci
├── partitions.csv
├── pytest_rtsp_cli.py
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Required

- ESP32-P4 Function EV Board (camera, MIPI LCD, audio codec)
- A PC on the same network for ffplay, VLC, or an RTSP server
- For chips without on-chip Wi-Fi, the board must provide available network connectivity

The camera and the LCD are both optional. Without a camera the send roles still work with `-v none`, and without a display the pull role plays audio only.

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3), release/v5.5 (>= v5.5.2), and IDF v6.1.

### Software Requirements

- To pull from the board (server role), install FFmpeg and use `ffplay`, or use VLC
- Configure Wi-Fi SSID and password in menuconfig
- To receive a push or serve a pull (push and pull roles), run an RTSP server on the PC. [MediaMTX](https://github.com/bluenviron/mediamtx) needs no configuration for this example:

```
./mediamtx
# RTSP is served on rtsp://<pc-ip>:8554/<any-path>
```

## Build and Flash

### Build Preparation

Before building, ensure the ESP-IDF environment is set up. If not, run in the ESP-IDF root directory:

```
./install.sh
. ./export.sh
```

Go to this example directory:

```
cd adf_examples/protocols/rtsp_cli
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) for the camera, LCD, audio codec, and other board peripherals. The [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) helper tool is recommended as the default entry point.

Install it in the activated ESP-IDF Python environment (only needed once per environment):

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # run this command when an update is requested
```

List the currently visible boards:

```bash
idf.py bmgr -l
```

Example output:

```text
ℹ️  Board Components:
  espressif/esp_boards:
    [1] esp32_c3_lyra
    [2] esp32_lyrat_4_3
    [3] esp32_lyrat_mini_1_1
    [4] esp32_p4_eye
    [5] esp32_p4_function_ev_board
    [6] esp32_s31_function_coreboard_1
    [7] esp32_s31_korvo_1
    [8] esp32_s3_box_3
    [9] esp32_s3_box_lite
    [10] esp32_s3_korvo_2_3
    [11] esp32_s3_lcd_ev_board
    [12] esp_vocat_1_0
    [13] esp_vocat_1_2
```

The example output above is based on the board list and ordering from `esp_boards` 0.5.2. Different `esp_boards` versions or custom board dependencies may change the list and indexes. Use the actual output of `idf.py bmgr -l` when selecting a board.

Select a board:

```bash
idf.py bmgr -b <board_index|board_name>
```

For example, to select `esp32_p4_function_ev_board`:

```bash
idf.py bmgr -b 5
# or
idf.py bmgr -b esp32_p4_function_ev_board
```

On first invocation of `idf.py bmgr`, the component is downloaded automatically based on the `espressif/esp_board_manager` dependency declared in `main/idf_component.yml`.

> [!NOTE]
> To switch to a different board supported by `esp_board_manager`, repeat the same steps with the new board name or index. Use `idf.py fullclean` before rebuilding if needed.
> The selected board should provide `camera`, `display_lcd`, `audio_adc` and `audio_dac` to exercise all three roles.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).
> For more information about `esp_board_manager`, see the [ESP Board Manager Getting Started Guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

### Project Configuration

Default options are in `sdkconfig.defaults` and `sdkconfig.defaults.<target>`. Media defaults live in `main/rtsp_settings.h`. You usually only need to set Wi-Fi:

```bash
idf.py menuconfig
```

Configure:

- **RTSP CLI Example Configuration** → **WiFi SSID**
- **RTSP CLI Example Configuration** → **WiFi Password**
- **RTSP CLI Example Configuration** → **Default push URL** / **Default pull URL** (used when `rtsp push` / `rtsp pull` are given no URL)

> For CI, use `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}` in `sdkconfig.ci`. Do not commit Wi-Fi passwords in `sdkconfig.defaults`; set the password locally in menuconfig.

Common tunables in `main/rtsp_settings.h`:

- `RTSP_VIDEO_WIDTH`, `RTSP_VIDEO_HEIGHT`, `RTSP_VIDEO_FPS`, `RTSP_VIDEO_CODEC`
- `RTSP_AUDIO_CODEC`, `RTSP_AUDIO_SAMPLE_RATE`, `RTSP_AUDIO_BITRATE`
- `RTSP_SERVER_PORT`, `RTSP_SERVER_PATH`, `RTSP_DEFAULT_TRANSPORT`
- `RTSP_RECV_AUDIO_CACHE`, `RTSP_RECV_VIDEO_CACHE`

Every media default can also be overridden per session from the command line.

Press `s` to save and `Esc` to exit menuconfig.

### Resource Optimization

The default configuration keeps all three RTSP roles and their codec choices so the CLI can switch among server, push, and pull; a fixed-function product can reduce firmware size in menuconfig:

- Open **Component config → ESP-RTSP Service** and use `CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT` for server, `CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT` for push, or `CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT` for pull.
- Open **Component config → Audio Codec Configuration** and **Component config → Video Codec Configuration** to toggle the `CONFIG_AUDIO_ENCODER_*_SUPPORT`, `CONFIG_AUDIO_DECODER_*_SUPPORT`, `CONFIG_VIDEO_ENCODER_*_SUPPORT`, and `CONFIG_VIDEO_DECODER_*_SUPPORT` options; server and push need encoders while pull needs decoders, so keep only the formats in use and one target-appropriate implementation for each video codec.

For example, a fixed MJPEG/AAC publisher normally keeps only the required `SERVER` or `SINK` role and the MJPEG/AAC encoders. Disabling a role or codec makes its CLI role or format unavailable; run `idf.py fullclean` before rebuilding after changing these options.

At runtime, `rtsp stop` destroys the active RTSP and capture or player services, while the board devices, codec registrations, and Wi-Fi remain initialized so another supported role can start without rebooting.

### Build and Flash Commands

```
idf.py build
idf.py -p PORT flash monitor
```

Exit the monitor with `Ctrl-]`. The CLI prompt is `rtsp>`; type `help` for commands.

## How to Use the Example

### Functionality and Usage

After startup, the example initializes peripherals, connects to Wi-Fi, and starts the CLI. If the credentials in menuconfig are wrong or missing, connect at runtime with `wifi <ssid> <password>`.

| Command | Description |
|---------|-------------|
| `rtsp server [url] [options]` | Serve the camera and microphone on the specified local RTSP URL |
| `rtsp push [url] [options]` | Publish the camera and microphone to a remote RTSP server |
| `rtsp pull [url] [options]` | Play a remote RTSP stream on the LCD and speaker |
| `rtsp stop` | Stop and destroy the active RTSP and capture or playback session |
| `rtsp info` | Print the current role, RTSP state, device IP and a ready-to-copy ffplay command |
| `wifi [ssid] [password]` | Connect to Wi-Fi; omit both arguments to reuse the menuconfig values |

Omitting `url` uses `rtsp://0.0.0.0:554/live` for server, and uses `RTSP_EXAMPLE_PUSH_URL` or `RTSP_EXAMPLE_PULL_URL` for push or pull.
When present, `url` must immediately follow the role name.
The server derives its listen port from this URL.

| Option | Roles | Description |
|--------|-------|-------------|
| `-v h264\|mjpeg\|none` | server, push, pull | Video codec, default `mjpeg`; `none` for an audio-only stream. On pull it is the depacketizer to expect |
| `-a aac\|g711a\|g711u\|none` | server, push | Audio codec, or `none` for a video-only stream |
| `--res <WxH>` | server, push | Capture resolution, for example `--res 640x480` |
| `--fps <n>` | server, push | Capture frame-rate |
| `--bitrate <bps>` | server, push | Video bitrate; omit it to derive one from the frame size |
| `--no-video` / `--no-audio` | pull | Ignore that track from the remote stream |
| `--cache <bytes>` | pull | Video receive cache size |

Selecting `g711a` or `g711u` pins the audio to 8 kHz mono, because RTP payload types 8 and 0 leave no other choice.

Automated tests (`pytest_rtsp_cli.py`) start and stop `rtsp server` after the CLI is ready.

**Serve the camera to a PC player:**

```
rtsp> rtsp server
```

```
ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
```

Other combinations:

```
rtsp> rtsp server -v h264 -a g711a --res 640x480 --fps 10
rtsp> rtsp server -v none  # audio-only stream
rtsp> rtsp server rtsp://0.0.0.0:8554/live -a none  # video-only stream on port 8554
```

**Push to a remote RTSP server.** Start MediaMTX on the PC, then:

```
rtsp> rtsp stop
rtsp> rtsp push rtsp://192.168.1.10:8554/live
```

```
ffplay -rtsp_transport udp rtsp://192.168.1.10:8554/live
```

**Pull and play a remote stream.** Point the board at an IP camera, MediaMTX, or a second board running `rtsp server`:

```
rtsp> rtsp stop
rtsp> rtsp pull rtsp://192.168.1.10:8554/live
```

Pull expects MJPEG by default, matching this example's server and push defaults; add `-v h264` when the source is H264. Use `--no-audio` or `--no-video` to ignore one of the tracks.

Only one role runs at a time. Run `rtsp stop` before starting a different role. When the peer has already torn the session down, the next `rtsp` command releases it automatically.

### Log Output

The startup sequence is numbered `[ 1 ]` to `[ 5 ]` and ends with `CLI ready`. During a session, every RTSP state transition is logged by name.

```text
I (1274) RTSP_EXAMPLE: === RTSP Example ===
I (1277) RTSP_EXAMPLE: [ 1 ] Initialize NVS and the media adapter
I (1284) RTSP_EXAMPLE: [ 2 ] Initialize board devices (camera / LCD / audio)
I (2801) RTSP_EXAMPLE: [ 3 ] Register audio and video codecs
I (2810) RTSP_EXAMPLE: [ 4 ] Connect to WiFi
I (2815) RTSP_EXAMPLE: Connecting to Wi-Fi SSID:my-ap
I (5120) RTSP_EXAMPLE: Wi-Fi connected
I (5125) RTSP_EXAMPLE: [ 5 ] Start CLI
I (5130) RTSP_EXAMPLE: CLI ready
I (5133) RTSP_EXAMPLE: Type 'help' to list all commands

rtsp> rtsp server
I (20130) RTSP_SESSION: server session running on rtsp://0.0.0.0:554/live (udp, video: mjpeg, audio: aac)
I (20138) RTSP_SESSION: Pull it with: ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
I (31502) RTSP_SESSION: OPTIONS (state: options)
I (31510) RTSP_SESSION: DESCRIBE (state: describe)
I (31530) RTSP_SESSION: SETUP (state: setup)
I (31544) RTSP_SESSION: PLAY (state: play)

rtsp> rtsp info
device ip  : 192.168.1.23
session    : server
rtsp state : play
url        : rtsp://0.0.0.0:554/live
transport  : udp
video      : mjpeg 640x480@10fps
audio      : aac 16000 Hz 1 ch
remote pull: ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
```

## Troubleshooting

- `rtsp server failed: ESP_ERR_NOT_FOUND` or `V4L2_SRC: Fail to open device`: the camera is not ready. Check the cable and board selection, or publish audio only with `-v none`.
- ffplay reports `Connection refused`: the device has no IP yet, or the port is wrong. Use the address printed by `rtsp info`.
- ffplay connects but shows no picture: confirm the player is on the same LAN, and that a firewall is not dropping the RTP UDP ports.
- `A server session is running`: run `rtsp stop` first. Only one role can run at a time.
- Multicast, RTSP over TLS, and authentication are not supported. The server role does not accept remote `ANNOUNCE` / `RECORD`; to push from one board to another, use the push role against a PC RTSP server.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
