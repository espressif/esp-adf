# ESP LLM Coze Agent

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example implements Coze **WebSocket streaming chat** on **esp_llm_service**: the session and media path go through the `esp_llm_coze` adapter, with direct talk, wake word, and barge-in style interaction.

- Connects to a Coze bot over WebSocket for duplex voice.
- **Audio only** — no camera or LCD; the Coze adapter does not enable video streams.
- Apply for an **Access token** and **BOT ID** per Coze docs and fill them in menuconfig.

Official documentation: [Coze API overview](https://www.coze.cn/docs/developer_guides/coze_api_overview).

### Features

Every capability below is turned on in `idf.py menuconfig`:

| Capability | What it does | Configured by | Default |
| --- | --- | --- | --- |
| Voice conversation | Duplex speech with the Coze bot | Always built in | On |
| Always-listening turns | The Coze server decides when your question has ended | `Enable WakeNet wake-up` left off | On |
| Wake word | Keeps the microphone closed until the wake word, then cancels the current reply | `Enable WakeNet wake-up` | Off |
| Echo cancellation and NS | AEC and NS are always enabled | Built in | On |
| Voice activity detection | Detects when speech starts and stops | `Enable voice activity detection` | On |
| Custom TTS voice | Picks the voice the bot answers with | `Coze voice ID` | Bot default |
| Subtitles in the log | Prints the recognized question and the spoken reply | Always built in | On |

### Typical Scenarios

- Voice assistant speaker driven by a Coze bot
- Always-listening conversation where the server decides when a query ends
- Wake-word gated conversation that keeps the microphone closed until WakeNet fires

### Media Capabilities

| Item | Support in this example |
| --- | --- |
| Audio uplink | OPUS, 16 kHz, mono |
| Audio downlink | OPUS, 16 kHz, mono |
| Video uplink | Not supported |
| Video downlink | Not supported |
| Camera and LCD | Not used |
| Audio front end | AEC and NS always on; VAD via `COZE_AUDIO_VAD`, on by default |
| Wake word | WakeNet (`COZE_AUDIO_WN`): uplink gated until wake word, then barge-in |
| Uplink gating | With WakeNet on, the audio uplink stays closed until the wake word |
| Player streams | Stream 0 carries Coze TTS; stream 1 is free for a URL or prompt tone |

This example is audio only. The Coze adapter rejects video streams, so `VIDEO_UP` and `VIDEO_DN` are never enabled and no camera or display device is required.

### File Structure

```
esp_llm_coze_agent/
├── main/
│   ├── app_main.c            Platform init, Wi-Fi, and application lifecycle
│   ├── app_agent.c           Coze configuration, AFE mask, and LLM event handling
│   ├── app_agent.h           Agent create / start / destroy API
│   ├── Kconfig.projbuild     Application menuconfig options
│   └── idf_component.yml
├── components/
│   └── coze_app_media/       Board devices, audio capture, and audio player
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Requirements

- A board that provides `audio_adc` and `audio_dac` devices
- Speaker or headphones connected to the board audio output
- PSRAM, and 16 MB flash on ESP32-S3 and ESP32-S31 or 8 MB flash on ESP32-P4

| Target | Board |
| --- | --- |
| `esp32s3` | `esp32_s3_korvo_2_3` |
| `esp32s31` | `esp32_s31_korvo_1` |
| `esp32p4` | `esp32_p4_function_ev_board` |

The firmware uses a custom partition table with a 6 MB `factory` application partition.

### Supported IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- A bot ID and an access token issued by the Coze open platform
- A Wi-Fi network that can reach `ws.coze.cn` and `api.coze.cn`

## Build and Flash

### Build Preparation

Make sure ESP-IDF is configured before building this example. If it is already set up, enter the project directory directly. Otherwise, run the following scripts in the ESP-IDF root directory. See the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/) for the target chip you use.

```
./install.sh
. ./export.sh
```

- Enter the example directory:

```
cd adf_examples/ai_agent/esp_llm_coze_agent
```

- Select the chip, then the board:

```bash
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
```

Run `idf.py set-target` before `idf.py bmgr`, because it loads `sdkconfig.defaults` together with the matching `sdkconfig.defaults.<chip>` file. This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager); `idf.py bmgr -l` lists the boards that are currently visible, and `idf.py bmgr -b <board_index|board_name>` selects one.

### Project Configuration

```bash
idf.py menuconfig
```

Set the following under `Coze LLM agent application`:

- `Coze bot ID` and `Coze access token`: required; the application refuses to start when either is empty
- `Coze WebSocket URL` and `Coze conversation-create URL`: preset to the Coze public endpoints
- `Coze user ID` and `Coze voice ID`: leave empty to keep the adapter defaults
- `Audio front-end features`: AEC and NS are built in; VAD enabled; `Enable WakeNet wake-up` disabled
- `Audio output volume`: 0 through 100, default 80
- `Wi-Fi`: SSID, password, and connection timeout in milliseconds
- `Run time before clean shutdown`: 0 runs forever; a non-zero value exercises the full stop, unlink, and destroy path

Credentials are compiled into the firmware, so do not commit real values. The Wi-Fi profile is persisted to NVS by `esp_wifi_service_request_connect()`.

### Build and Flash

- Build the example

```
idf.py build
```

- Flash and monitor (replace PORT with your serial port):

```
idf.py -p PORT flash monitor
```

- Press `Ctrl-]` to exit the monitor

## How to Use the Example

### Features and Usage

1. After boot the application connects to Wi-Fi, then creates and starts the Coze session
2. The log line `Coze agent is ready` means the device is connected and ready to talk
3. With WakeNet disabled, speak at any time; the Coze server VAD decides when a query ends
4. With WakeNet enabled, say the wake word first. The microphone opens on wake-up and the reply in progress is cancelled so a new turn can start
5. Replies arrive as TTS on player stream 0 and play through the board audio output
6. Subtitles, VAD transitions, connection changes, and chat completion appear as `Agent event:` log lines
7. Set `Run time before clean shutdown` to a non-zero value to watch the complete teardown sequence instead of running forever

## Troubleshooting

### The application stops before connecting

`Validate configuration failed: Coze bot ID or access token is empty` means the account fields were left blank. Fill both under `Coze LLM agent application` and rebuild. `Start network failed: Wi-Fi SSID is empty` means the Wi-Fi SSID is still unset.

### Board device names are missing

Build or startup errors about unknown device handles mean the board metadata was not generated for the selected chip. Rerun `idf.py bmgr -b <board>` with the board that matches the target.

### The WebSocket connection fails

Confirm that the network reaches `ws.coze.cn`, that the access token has not expired, and that the bot is published. TLS also needs enough heap; this example keeps `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` at 10240 and requires PSRAM.

### Playback is distorted or runs at the wrong speed

The board DAC and the AEC capture path share one I2S bus, so the player is opened with two channels even though the stream is mono. Keep the player and capture sample rates aligned when changing the audio configuration.

## Technical Support

- Forum: [esp32.com](https://esp32.com/viewforum.php?f=20)
- Issues: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
