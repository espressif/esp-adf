# RTMP CLI

- [中文版](./README_CN.md)
- Complex Example: ⭐⭐⭐

## Example Brief

This example provides five RTMP modes in one firmware and controls them through a serial CLI, allowing users to publish, receive, relay, and loop back streams, then switch modes after stopping the active session without rebuilding or reflashing:

- **push** — publish live camera video and microphone audio to MediaMTX, nginx-rtmp, YouTube, Twitch, or another RTMP server
- **pull** — play a remote RTMP stream on the board's LCD and speaker
- **server** — receive a stream published by `ffmpeg` and relay it to multiple players
- **live** — let network players pull the board's live camera and microphone stream directly from the board
- **loopback** — run server, push, and pull together to exercise the complete RTMP path on the board without a PC

The **server** mode only relays media, so **live** combines server and push to expose the board's camera stream, while **loopback** also adds pull; three independent slots allow these services to run together.

All modes use the same service model: the application creates capture, playback, and RTMP services as needed, links media producers to consumers with `esp_media_service_link()`, and controls their lifecycle through common APIs instead of implementing RTMP protocol handling and media data flow; the serial CLI also provides a command surface for scripts and agent-driven automation.

### Typical Use Cases

- Live-streaming a camera to a CDN ingest endpoint such as YouTube or Twitch
- Video doorbells and surveillance nodes that publish to a central media server
- An all-in-one streaming box that players pull from directly, with no server to operate
- Network broadcast displays that play a remote RTMP stream on a local LCD

### Runtime Flow

1. `esp_board_manager` initializes the camera, LCD, audio ADC, and audio DAC
2. `esp_wifi_service` connects the station
3. `esp_cli_service` starts the `rtmp>` console
4. **push**: `esp_video_capture_service` → `esp_media_service_link()` → `esp_rtmp_service` ROLE_SINK
5. **server**: `esp_rtmp_service` ROLE_SERVER relays only; there is no media link
6. **pull**: ROLE_SRC → `esp_media_service_link()` → `esp_video_player_service` → LCD and speaker

The consuming side of a link is always started first. On teardown the order is reversed inside each slot, and the client slots are released before the server.

### File Structure

```
rtmp_cli/
├── main/
│   ├── app_main.c              Application entry: NVS, board, codecs, Wi-Fi, CLI
│   ├── rtmp_example.h          Board and Wi-Fi helpers
│   ├── rtmp_session.c/h        Three slots: create, setup, link, start, and stop
│   ├── rtmp_cli.c/h            `rtmp` and `wifi` command parsing
│   ├── rtmp_settings.h         Media and protocol defaults
│   ├── Kconfig.projbuild       Wi-Fi credentials and default peer URLs
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── sdkconfig.ci
├── partitions.csv
├── pytest_rtmp_cli.py
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Required

- ESP32-P4 Function EV Board (camera, MIPI LCD, audio codec)
- A PC on the same network for ffmpeg, ffplay, or an RTMP server
- For chips without on-chip Wi-Fi, the board must provide available network connectivity

The camera and the LCD are both optional. Without a camera the push slot still works with `-v none`, and without a display the pull slot plays audio only. `rtmp loopback -v none` needs neither.

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3), release/v5.5 (>= v5.5.2), and IDF v6.1.

### Software Requirements

- Install FFmpeg, which provides both `ffmpeg` and `ffplay`
- Configure Wi-Fi SSID and password in menuconfig
- To play what the board serves (`server`, `live`): `ffplay rtmp://<device-ip>:1935/live/stream`
- To publish into the board (`server`): `ffmpeg` with `-f flv rtmp://<device-ip>:1935/live/stream`
- To receive a push or serve a pull (`push`, `pull`), run an RTMP server on the PC. [MediaMTX](https://github.com/bluenviron/mediamtx) needs no configuration for this example:

```
./mediamtx
# RTMP is served on rtmp://<pc-ip>:1935/<path>
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
cd adf_examples/protocols/rtmp_cli
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
> The selected board should provide `camera`, `display_lcd`, `audio_adc` and `audio_dac` to exercise every slot.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).
> For more information about `esp_board_manager`, see the [ESP Board Manager Getting Started Guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

### Project Configuration

Default options are in `sdkconfig.defaults` and `sdkconfig.defaults.<target>`. Media defaults live in `main/rtmp_settings.h`. You usually only need to set Wi-Fi:

```bash
idf.py menuconfig
```

Configure:

- **RTMP CLI Example Configuration** → **WiFi SSID**
- **RTMP CLI Example Configuration** → **WiFi Password**
- **RTMP CLI Example Configuration** → **Default push URL** / **Default pull URL** (used when `rtmp push` / `rtmp pull` are given no URL)

> For CI, use `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}` in `sdkconfig.ci`. Do not commit Wi-Fi passwords in `sdkconfig.defaults`; set the password locally in menuconfig.

Common tunables in `main/rtmp_settings.h`:

- `RTMP_VIDEO_WIDTH`, `RTMP_VIDEO_HEIGHT`, `RTMP_VIDEO_FPS`, `RTMP_VIDEO_CODEC`
- `RTMP_AUDIO_CODEC`, `RTMP_AUDIO_SAMPLE_RATE`, `RTMP_AUDIO_BITRATE`
- `RTMP_SERVER_PORT`, `RTMP_SERVER_APP`, `RTMP_SERVER_STREAM`, `RTMP_SERVER_MAX_CLIENTS`
- `RTMP_CHUNK_SIZE`, `RTMP_RECV_AUDIO_CACHE`, `RTMP_RECV_VIDEO_CACHE`

Every media default can also be overridden per session from the command line.

Press `s` to save and `Esc` to exit menuconfig.

### Resource Optimization

The default configuration keeps every RTMP role and codec choice used by the five CLI modes; a fixed-function product can reduce firmware size in menuconfig:

- Open **Component config → ESP-RTMP Service** and use `CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT` for push, `CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT` for pull, and `CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT` for server; live needs `SERVER` and `SINK`, while loopback needs all three roles.
- Open **Component config → Audio Codec Configuration** and **Component config → Video Codec Configuration** to toggle the `CONFIG_AUDIO_ENCODER_*_SUPPORT`, `CONFIG_AUDIO_DECODER_*_SUPPORT`, `CONFIG_VIDEO_ENCODER_*_SUPPORT`, and `CONFIG_VIDEO_DECODER_*_SUPPORT` options; push and live need encoders, pull needs decoders, a relay-only server needs neither, and loopback needs both, so keep only the formats in use and one target-appropriate implementation for each video codec.

For example, a fixed H264/AAC publisher normally keeps only the RTMP `SINK` role and the H264/AAC encoders. Disabling a role or codec makes its CLI mode or format unavailable; run `idf.py fullclean` before rebuilding after changing these options.

At runtime, `rtmp stop` destroys the active RTMP, capture, and player services, while the board devices, codec registrations, and Wi-Fi remain initialized so another supported mode can start without rebooting.

### Build and Flash Commands

```
idf.py build
idf.py -p PORT flash monitor
```

Exit the monitor with `Ctrl-]`. The CLI prompt is `rtmp>`; type `help` for commands.

## How to Use the Example

### Functionality and Usage

After startup, the example initializes peripherals, connects to Wi-Fi, and starts the CLI. If the credentials in menuconfig are wrong or missing, connect at runtime with `wifi <ssid> <password>`.

| Command | Description |
|---------|-------------|
| `rtmp server [options]` | Start the relay server on `rtmp://<device-ip>:<port>/<app>` |
| `rtmp push [url] [options]` | Publish the camera and microphone to an RTMP URL |
| `rtmp pull [url] [options]` | Play an RTMP stream on the LCD and speaker |
| `rtmp live [options]` | Server plus publisher, so players pull the board's camera from the board |
| `rtmp loopback [options]` | Server plus publisher plus player, the whole chain on the board alone |
| `rtmp stop` | Stop and destroy every active RTMP, capture, and playback slot |
| `rtmp info` | Print the active slots, their URLs, and the server's client and session table |
| `wifi [ssid] [password]` | Connect to Wi-Fi; omit both arguments to reuse the menuconfig values |

Omitting the URL on `rtmp push` and `rtmp pull` uses `RTMP_EXAMPLE_PUSH_URL` and `RTMP_EXAMPLE_PULL_URL`.

| Option | Slots | Description |
|--------|-------|-------------|
| `-p <port>` | server, live, loopback | Listen port, default `1935` |
| `--app <name>` | server, live, loopback | RTMP application name, default `live` |
| `--stream <name>` | server, live, loopback | Stream name, default `stream` |
| `--max-clients <n>` | server, live, loopback | Server client limit, default `4` |
| `-v h264\|mjpeg\|none` | push, live, loopback | Video codec; `h264` on push/live, `mjpeg` on loopback; `none` for audio-only |
| `-a aac\|pcm\|g711a\|g711u\|none` | push, live, loopback | Audio codec, default `aac`; `none` for a video-only stream |
| `--res <WxH>` | push, live, loopback | Capture resolution, for example `--res 1280x720` |
| `--fps <n>` | push, live, loopback | Capture frame-rate |
| `--bitrate <bps>` | push, live, loopback | Video bitrate; omit it to derive one from the frame size |
| `--no-video` / `--no-audio` | push, live, loopback | Shorthand for `-v none` / `-a none` |
| `--chunk <bytes>` | all | RTMP chunk size, default `4096` |
| `--cache <bytes>` | pull | Video receive cache size |
| `--insecure` | push, pull | Accept an `rtmps://` server without verifying its certificate |

Selecting `g711a` or `g711u` pins the audio to 8 kHz mono, because G.711 is defined only at that rate.

Automated tests (`pytest_rtmp_cli.py`) run `rtmp loopback -v none` after the CLI is ready and expect `PREPARING -> PLAYING`.

**Serve the camera to a PC player** with `rtmp live`, then on the PC:

```
rtmp> rtmp live
```

```
ffplay rtmp://192.168.1.23:1935/live/stream
```

Other combinations:

```
rtmp> rtmp live -v h264 --res 1280x720 --fps 15 --bitrate 2000000
rtmp> rtmp live -v none                   # audio-only stream
rtmp> rtmp live -a none -p 8935           # video-only stream on port 8935
rtmp> rtmp live --app app1 --stream cam0  # rtmp://<device-ip>:1935/app1/cam0
```

**Publish to a remote RTMP server.** Start MediaMTX on the PC, then:

```
rtmp> rtmp stop
rtmp> rtmp push rtmp://192.168.1.10:1935/live/stream
```

```
ffplay rtmp://192.168.1.10:1935/live/stream
```

A CDN ingest endpoint works the same way; the stream key is the last path element:

```
rtmp> rtmp push rtmp://a.rtmp.youtube.com/live2/<your-stream-key>
```

For `rtmps://` endpoints the certificate is checked against the IDF certificate bundle. Add `--insecure` to accept a self-signed test server:

```
rtmp> rtmp push rtmps://192.168.1.10:1936/live/stream --insecure
```

**Play a remote stream:**

```
rtmp> rtmp stop
rtmp> rtmp pull rtmp://192.168.1.10:1935/live/stream
```

Codecs come from the stream metadata, so no codec option is needed on this side. Use `--cache` to widen the receive buffer on a lossy network.

**Act as the relay server only.** The board carries no media of its own. Publish from the PC:

```
ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=15 -f lavfi -i sine -c:v libx264 -preset ultrafast -tune zerolatency -c:a aac -f flv rtmp://192.168.1.23:1935/live/stream
```

```
ffplay rtmp://192.168.1.23:1935/live/stream
```

**Run the whole chain on the board** with `rtmp loopback`. Camera frames travel capture → SINK → local server → SRC → LCD. `rtmp loopback -v none` runs on a board with no camera and no display.

`rtmp loopback` defaults to MJPEG so the on-board LCD can keep up: ESP32-P4 decodes JPEG in hardware but H264 in software. `rtmp live` and `rtmp push` stay on H264 so ffplay can read them. Pass `-v h264` on loopback only when a PC will also pull the same stream.

The three slots are independent. Each start restarts only its own slot, so a publisher can be repointed without dropping a running server:

```
rtmp> rtmp server
rtmp> rtmp push rtmp://127.0.0.1:1935/live/stream    # equivalent to 'rtmp live'
rtmp> rtmp push rtmp://192.168.1.10:1935/live/stream # repoints, server keeps running
rtmp> rtmp stop                                      # releases everything
```

Only one instance of each role exists at a time. When a peer has already closed the connection, `rtmp info` marks the slot `(peer left)` and the next start on that slot releases it automatically.

### Log Output

The startup sequence is numbered `[ 1 ]` to `[ 5 ]` and ends with `CLI ready`. During a session every RTMP event is logged with the slot that raised it:

- `push: PEER_CLOSED` or `pull: PEER_CLOSED` — the remote side dropped the connection
- `server: SERVER_CLIENT_CONNECTED` — a client reached the relay server
- `server: SERVER_PULLER_STARTED` / `SERVER_PULLER_STOPPED` — raised only when an in-process local pusher is registered through `esp_rtmp_server_monitor_puller()`. Clients that connect over a socket, including this example's publisher at `127.0.0.1`, do not raise them. Use `SERVER_CLIENT_CONNECTED` and `rtmp info` instead

The pull slot starts the player first, then the source. Audio-only streams such as `rtmp loopback -v none` go straight to `PREPARING -> PLAYING`. An A/V stream may also log `PLAYER_SERVICE: Stream 0 took a late video track, restarting the feed session` when the video header arrives after audio; that is expected. A short burst of `H264_DEC` errors at that moment means the player joined mid-GOP.

These two fragments are from one `rtmp loopback -v none` run on ESP32-P4 Function EV. The first is the command starting the relay. The second is the player leaving `PREPARING` after audio frames arrived. The `ffmpeg ...` in the first fragment is the literal string the firmware prints.

```text
rtmp loopback -v none
I (45589) ADF_EVENT_HUB: Create 'server': domain registered
I (45589) ESP_SERVICE: [server] Initialized
I (45590) ESP_SERVICE: [server] Started
I (45591) RTMP_SESSION: Local RTMP server listening on port 1935, app 'live', up to 4 clients
I (45591) RTMP_SESSION: Publish into it with: ffmpeg ... -f flv rtmp://192.168.3.101:1935/live/stream
I (45592) RTMP_SESSION: Play from it with:    ffplay rtmp://192.168.3.101:1935/live/stream
```

```text
I (47574) RTMP_SESSION: Playing rtmp://127.0.0.1:1935/live/stream
I (47574) RTMP_SESSION: Loopback running: camera -> SINK -> local server -> SRC -> LCD
rtmp>  
rtmp>  I (47584) RTMP_SERVER: Add client 0x48263ab4 count 2
I (47584) RTMP_SERVER: Start close client 0x4827b79c
I (47613) ESP_PLAYER: Set av_mask: 3
I (47613) ESP_PLAYER: Set sync_mode: 1
I (47613) ESP_PLAYER: Set av_mask: 1
I (47614) ESP_PLAYER: set dec cfg, type: 541278529, line: 129
I (47614) ESP_PLAYER_STATE: Handling cmd: PREPARE in state: IDLE
I (47614) ESP_PLAYER_STATE: State transition: IDLE -> PREPARING
I (47615) ESP_PLAYER_STATE: Entering PREPARING state
I (47616) ESP_PLAYER_STATE: Audio decoder started, waiting for ready event
W (47617) ESP_GMF_ASMP_DEC: Not enough memory for out, need:4096, old: 1024, new: 4096
I (47619) ESP_PLAYER_STATE: Handling cmd: REPORT_AUDIO_INFO in state: PREPARING
I (47624) ESP_PLAYER_AUDIO_RENDER: Audio render opened successfully
I (47625) ESP_PLAYER_STATE: Handling cmd: PLAYING in state: PREPARING
I (47625) ESP_PLAYER_STATE: State transition: PREPARING -> PLAYING
I (47625) ESP_PLAYER_STATE: Entering PLAYING state, old_state: PREPARING
```

`PREPARING -> PLAYING` is the proof that data crossed capture, publisher, server and player. `rtmp stop` ends with `All RTMP slots released`.

## Troubleshooting

- `rtmp push failed: ESP_ERR_NOT_FOUND` or `V4L2_SRC: Fail to open device`: the camera is not ready. Check the cable and board selection, or publish audio only with `-v none`.
- `rtmp push` fails to connect: the client URL must include a stream name, as in `rtmp://host:port/app/stream`; a server URL stops at the application: `rtmp://host:port/app`.
- ffplay reports `Connection refused`: no server is listening. Run `rtmp server` or `rtmp live`, and use the address printed by `rtmp info`.
- ffplay connects but shows no picture, or reports an unknown codec: the stream is MJPEG. Restart with the default `-v h264`. MJPEG over RTMP is an Espressif extension; third-party players cannot decode it.
- `rtmp push rtmps://...` fails in the handshake: retry with `--insecure` for a self-signed test server.
- The server role has no authentication. RTMPS is available to the client roles only.

The example enables `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY` so `--insecure` can skip certificate checks; use only in demo environments.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
