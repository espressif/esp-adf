# SIP CLI

- [中文版](./README_CN.md)
- Complex Example: ⭐⭐⭐

## Example Brief

This example turns the board into a SIP endpoint controlled through a serial CLI: it registers with a PBX, places and answers calls, and carries audio (and optionally video) in both directions at the same time. A console command drives every step, so the whole call flow can be exercised without reflashing.

Unlike RTSP or RTMP, a SIP call is full duplex, and `esp_sip_service` reports `ESP_MEDIA_ROLE_SRC_SINK` to match: one instance serves two media streams, an uplink that consumes the capture service and a downlink that feeds the player service.

```
esp_video_capture_service ──link(0 → uplink)──> esp_sip_service ──link(downlink → 0)──> esp_video_player_service
   microphone + camera                          SIP + RTP + SRTP                          speaker + LCD
```

Besides the media path, the example also exercises the signaling extras a real deployment needs: out-of-band DTMF, text SIP MESSAGE, and SDES-SRTP negotiation.

### Typical Use Cases

- Video door phones and intercoms that call an indoor unit or a softphone
- Audio intercom stations on a building or factory PBX
- Elevator and emergency call boxes that dial a fixed extension
- Devices that report status to an operator through SIP MESSAGE

### Runtime Flow

1. `esp_board_manager` initializes the audio ADC, audio DAC, and optionally camera and LCD
2. `esp_wifi_service` connects the station
3. `esp_cli_service` starts the `sip>` console
4. `sip start` creates the three services, links them in both directions, and registers with the server
5. `sip call <user>` or an incoming INVITE establishes the call; media starts flowing on both streams
6. `sip bye` ends the call while the session stays online for the next one

Registering is not the same as streaming: `sip start` only brings the device online. Media exists only for the duration of a call, so the downlink tracks are created when the call is answered and released when it ends.

The consuming side of a link is started first. On teardown the order is reversed: capture stops before the SIP service, and the SIP service before the player, so each consumer drains instead of blocking on an empty queue.

### File Structure

```
sip_cli/
├── main/
│   ├── app_main.c              Application entry: NVS, board, codecs, Wi-Fi, CLI
│   ├── sip_example.h           Board and Wi-Fi helpers
│   ├── sip_session.c/h         Create, setup, link, start, and the call actions
│   ├── sip_cli.c/h             `sip` and `wifi` command parsing
│   ├── sip_settings.h          Account and media defaults
│   ├── Kconfig.projbuild       Wi-Fi credentials and the SIP account
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.ci
├── partitions.csv
├── pytest_sip_cli.py
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Required

- A board with a microphone and a speaker, for example ESP32-S3-Korvo-2 or the ESP32-P4 Function EV Board
- A SIP server on the same network, or a second board for a P2P call
- For chips without on-chip Wi-Fi, the board must provide available network connectivity

The camera and the LCD are optional; audio-only calls need neither. Video calls need a camera on this side and a peer that accepts the video offer.

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3), release/v5.5 (>= v5.5.2), and IDF v6.1.

### Software Requirements

- A SIP registrar. [Asterisk](https://www.asterisk.org/) and [FreeSWITCH](https://signalwire.com/freeswitch) both work; for a quick test, a softphone such as [Linphone](https://www.linphone.org/) or [MicroSIP](https://www.microsip.org/) can register to the same PBX and be called by extension
- Two extensions on that PBX: one for the board, one for the softphone
- Configure Wi-Fi and the SIP account in menuconfig

No server is needed for a P2P call: two boards, or a board and a softphone in direct-IP mode, can invite each other by address.

## Build and Flash

### Build Preparation

Before building, ensure the ESP-IDF environment is set up. If not, run in the ESP-IDF root directory:

```
./install.sh
. ./export.sh
```

Go to this example directory:

```
cd adf_examples/protocols/sip_cli
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) for the audio codec, camera, LCD, and other board peripherals. The [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) helper tool is recommended as the default entry point.

Install it in the activated ESP-IDF Python environment (only needed once per environment):

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # run this command when an update is requested
```

List the currently visible boards:

```bash
idf.py bmgr -l
```

Select a board:

```bash
idf.py bmgr -b <board_index|board_name>
```

For example:

```bash
idf.py bmgr -b esp32_s3_korvo_2_3
```

On first invocation of `idf.py bmgr`, the component is downloaded automatically based on the `espressif/esp_board_manager` dependency declared in `main/idf_component.yml`.

> [!NOTE]
> To switch to a different board supported by `esp_board_manager`, repeat the same steps with the new board name or index. Use `idf.py fullclean` before rebuilding if needed.
> The selected board must provide `audio_adc` and `audio_dac`; `camera` and `display_lcd` are only needed for video calls.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).
> For more information about `esp_board_manager`, see the [ESP Board Manager Getting Started Guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

### Project Configuration

Default options are in `sdkconfig.defaults` and `sdkconfig.defaults.<target>`. Media defaults live in `main/sip_settings.h`. You usually only need to set Wi-Fi and the account:

```bash
idf.py menuconfig
```

Configure:

- **SIP Example Configuration** → **WiFi SSID** / **WiFi Password**
- **SIP Example Configuration** → **SIP user** / **SIP password** / **SIP server** / **SIP server port**
- **SIP Example Configuration** → **Default peer** (called by `sip call` with no argument)
- **SIP Example Configuration** → **SIP transport** (`udp`, `tcp` or `tls`)

> For CI, use `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}` in `sdkconfig.ci`. Do not commit Wi-Fi or SIP passwords in `sdkconfig.defaults`; set them locally in menuconfig.

Common tunables in `main/sip_settings.h`:

- `SIP_AUDIO_CODEC`, `SIP_AUDIO_SAMPLE_RATE`, `SIP_AUDIO_BITRATE`
- `SIP_VIDEO_CODEC`, `SIP_VIDEO_WIDTH`, `SIP_VIDEO_HEIGHT`, `SIP_VIDEO_FPS`
- `SIP_DEFAULT_SRTP_MODE`, `SIP_DTMF_VOLUME`, `SIP_DTMF_DURATION_MS`
- `SIP_RECV_AUDIO_CACHE`, `SIP_RECV_VIDEO_CACHE`, `SIP_MIC_GAIN`

Every account and media default can also be overridden per session from the command line.

Press `s` to save and `Esc` to exit menuconfig.

### Build and Flash Commands

```
idf.py build
idf.py -p PORT flash monitor
```

Exit the monitor with `Ctrl-]`. The CLI prompt is `sip>`; type `help` for commands.

## How to Use the Example

### Functionality and Usage

After startup, the example initializes peripherals, connects to Wi-Fi, and starts the CLI. If the credentials in menuconfig are wrong or missing, connect at runtime with `wifi <ssid> <password>`.

| Command | Description |
|---------|-------------|
| `sip start [options]` | Build the capture → SIP → player chain and register with the server |
| `sip stop` | Hang up, tear the chain down, and release the microphone, speaker and network resources |
| `sip call [user]` | Place a call; omitting the user dials the menuconfig default peer |
| `sip answer` | Answer the ringing call |
| `sip bye` | Hang up the active call, or cancel the one being placed |
| `sip dtmf <digit>` | Send one out-of-band DTMF digit: `0`-`9`, `*`, `#` or `A`-`D` |
| `sip msg [-t <uri>] <text...>` | Send a text SIP MESSAGE, by default through the configured server |
| `sip auto <on\|off>` | Answer incoming calls automatically instead of waiting for `sip answer` |
| `sip info` | Print the account, registration, call, SRTP and media state |
| `wifi [ssid] [password]` | Connect to Wi-Fi; omit both arguments to reuse the menuconfig values |

Options for `sip start`:

| Option | Description |
|--------|-------------|
| `-u <user>` | SIP user or extension, default from menuconfig |
| `-w <password>` | SIP password; `-` for a server that does not authenticate |
| `-s <server[:port]>` | Registrar address, or the peer address in P2P mode |
| `-t udp\|tcp\|tls` | Signaling transport, default `udp` |
| `-a g711a\|g711u\|opus\|none` | Audio codec, default `g711a`; `none` for a video-only call |
| `-v h264\|mjpeg\|none` | Video codec, default `none` for an audio-only call |
| `--p2p` | Skip registration and invite the peer directly |
| `--srtp off\|prefer\|required` | SDES-SRTP negotiation, default `off` |
| `--port <local>` | Fixed local SIP port; ignored in P2P mode, where the port from `-s` is used |
| `--res <WxH>`, `--fps <n>`, `--bitrate <bps>` | Video capture settings |

Selecting `g711a` or `g711u` pins the audio to 8 kHz mono, because RTP payload types 8 and 0 leave no other choice. `opus` runs at 16 kHz.

The microphone and the speaker share one I2S clock on these boards, and a call runs both at once, so the speaker is opened at the codec's rate rather than the usual 48 kHz. Changing `-a` therefore changes the playback clock as well.

**Register and place a call.** With the account set in menuconfig:

```
sip> sip start
sip> sip call 1002
```

The softphone registered as `1002` rings. Once it picks up, audio flows in both directions.

**Answer an incoming call.** Dial the board's extension from the softphone, then:

```
sip> sip answer
```

Or leave it to answer on its own, which is what an intercom usually wants:

```
sip> sip auto on
```

**Send DTMF and text during a call:**

```
sip> sip dtmf 5
sip> sip msg "door opened"
```

**Encrypt the media.** `prefer` falls back to plain RTP when the peer offers no crypto line, while `required` rejects the call instead:

```
sip> sip stop
sip> sip start --srtp required
```

**Video call.** Needs a camera on the board and a peer that accepts the offer:

```
sip> sip stop
sip> sip start -v h264 --res 640x480 --fps 15
```

**P2P call between two boards.** No registrar is involved; each side points at the other. In P2P mode the protocol stack listens on the port given in `-s`, so both boards must agree on one port:

```
# board A, at 192.168.1.20
sip> sip start --p2p -u 1001 -s 192.168.1.21:5060

# board B, at 192.168.1.21
sip> sip start --p2p -u 1002 -s 192.168.1.20:5060
sip> sip auto on

# then, on board A
sip> sip call 1002
```

One session runs at a time. Run `sip stop` before starting with different options.

Automated tests (`pytest_sip_cli.py`) start a P2P session, which is the only mode that needs no registrar, then check the call commands and stop it.

### Log Output

The startup sequence is numbered `[ 1 ]` to `[ 5 ]` and ends with `CLI ready`. During a session, every SIP event is logged by name.

```text
I (1274) SIP_EXAMPLE: === SIP Example ===
I (1277) SIP_EXAMPLE: [ 1 ] Initialize NVS and the media adapter
I (1284) SIP_EXAMPLE: [ 2 ] Initialize board devices (audio / camera / LCD)
I (2801) SIP_EXAMPLE: [ 3 ] Register audio and video codecs
I (2810) SIP_EXAMPLE: [ 4 ] Connect to WiFi
I (2815) SIP_EXAMPLE: Connecting to Wi-Fi SSID:my-ap (attempt 1/3)
I (5120) SIP_EXAMPLE: Wi-Fi connected
I (5125) SIP_EXAMPLE: [ 5 ] Start CLI
I (5130) SIP_EXAMPLE: CLI ready
I (5147) SIP_EXAMPLE: Type 'help' to list all commands

sip> sip start
I (20130) SIP_SESSION: SIP session online as 1001@192.168.1.10:5060 over udp (audio: g711a, video: none, srtp: off)
I (20320) SIP_SESSION: SIP_REGISTERED: ready to place and receive calls

sip> sip call 1002
I (31502) SIP_SESSION: SIP_CALLING 1002
I (33110) SIP_SESSION: SIP_CALL_ANSWERED: talking to 1002
I (33125) SIP_SESSION: SIP_AUDIO_SESSION_BEGIN

sip> sip info
device ip   : 192.168.1.23
session     : online
account     : 1001@192.168.1.10:5060 (udp)
registered  : yes
call        : in call with 1002
auto answer : off
srtp        : inactive (off)
audio       : g711a 8000 Hz 1 ch
video       : disabled

sip> sip bye
I (60210) SIP_SESSION: SIP_HANGUP: BYE
```

## Troubleshooting

- No `SIP_REGISTERED` after `sip start`: check the user, password, server address and port with `sip info`, and confirm the PBX accepts the extension. A wrong password usually shows up as a `SIP_ERROR` with a reject reason.
- `sip start failed: ESP_ERR_NOT_FOUND`: video was requested but the camera is not ready. Check the cable and board selection, or start without `-v`.
- `sip call failed: ESP_ERR_INVALID_STATE`: either no session is online, or a call is already ringing or connected. `sip info` shows which.
- Call connects but there is no audio: NAT is the usual cause. Keep both ends on the same subnet, or enable `use_public_addr` and `send_options` in the service setup so the RTP hole stays open.
- `sip start --srtp required` gets rejected: the peer offered no crypto line. Use `prefer` to allow the fallback to plain RTP.
- Choppy audio: `sdkconfig.defaults` already deepens the lwIP receive queues; raising `SIP_RECV_AUDIO_CACHE` helps when the network is bursty.
- Video is negotiated but nothing is displayed: the board has no LCD, which is logged as a warning at startup. The call still carries audio.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
