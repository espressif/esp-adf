# ESP LLM Volc Agent

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example turns a development board into a Volcengine voice device. It authenticates through Volc IoT, joins an RTC room, and holds a duplex voice conversation: you speak, the agent answers, and the reply is spoken through the board speaker. It is an audio-only example, so no camera or display is involved.

Official documentation: [Volcengine RTC](https://www.volcengine.com/docs/6348).

### Features

Every capability below is turned on in `idf.py menuconfig`:

| Capability | What it does | Configured by | Default |
| --- | --- | --- | --- |
| Voice conversation | Duplex speech with the Volc conversational agent over RTC | Always built in | On |
| Always-listening turns | The Volc server decides when your question has ended | `Enable WakeNet wake-up` left off | On |
| Wake word | Keeps the microphone closed until the wake word, then cancels the current reply | `Enable WakeNet wake-up` | Off |
| Echo cancellation | Removes the speaker signal from the microphone so the agent does not hear itself | `Enable acoustic echo cancellation` | On |
| Noise suppression | Cleans the uplink signal | `Enable noise suppression` | On |
| Voice activity detection | Detects when speech starts and stops | `Enable voice activity detection` | On |
| Low-bitrate voice link | Keeps the RTC audio at 64 kbit/s with G.711A | `ESP LLM Volc` RTC audio codec | G.711A |
| Direct text to speech | Speaks a string without asking the model, through `esp_llm_volc_send_text_to_tts()` | Always built in | On |

### Typical Scenarios

- Voice assistant speaker that reaches the model over an RTC room instead of a WebSocket
- Low-bitrate voice link where G.711A keeps the uplink at 64 kbit/s
- Wake-word gated conversation that keeps the microphone closed until WakeNet fires

### Media Capabilities

| Item | Support in this example |
| --- | --- |
| Audio uplink | G.711A, 8 kHz, mono |
| Audio downlink | G.711A, 8 kHz, mono |
| Video uplink | Disabled by the application |
| Video downlink | Disabled by the application |
| Camera and LCD | Not used |
| Audio front end | AEC and NS always on; VAD via `VOLC_AUDIO_VAD`, on by default |
| Wake word | WakeNet, off by default, enabled by `VOLC_AUDIO_WN` |
| Uplink gating | With WakeNet on, the audio uplink stays closed until the wake word |
| Player streams | Stream 0 carries Volc TTS; stream 1 is free for a URL or prompt tone |

The RTC streams are G.711A at 8 kHz, selected by `CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711A` in `sdkconfig.defaults`. The AFE and the shared I2S bus still run at 16 kHz PCM, and the adapter encodes down to 8 kHz for the uplink. The player is opened at 16 kHz with two channels so it keeps the same I2S clock as the AEC reference.

This example is audio only. The video streams stay disabled even when the component Kconfig would publish them, so no camera or display device is required.

### File Structure

```
esp_llm_volc_agent/
├── main/
│   ├── app_main.c            Platform init, Wi-Fi, SNTP, and application lifecycle
│   ├── app_agent.c           Volc configuration, AFE mask, and LLM event handling
│   ├── app_agent.h           Agent create / start / destroy API
│   ├── Kconfig.projbuild     Application menuconfig options
│   └── idf_component.yml
├── components/
│   └── volc_app_media/       Board devices, audio capture, and audio player
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

- Volc IoT credentials: instance ID, product key, product secret, and device name
- A Volcengine conversational bot ID
- A Wi-Fi network that reaches the Volc RTC endpoints and an NTP server

`Kconfig.projbuild` and `sdkconfig.defaults` ship with an internal lab account and a lab Wi-Fi SSID so the example builds without edits. Replace every credential with your own before using the example outside that lab network.

## Build and Flash

### Build Preparation

Make sure ESP-IDF is configured before building this example. If it is already set up, enter the project directory directly. Otherwise, run the following scripts in the ESP-IDF root directory. See the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/) for the target chip you use.

```
./install.sh
. ./export.sh
```

- Enter the example directory:

```
cd adf_examples/ai_agent/esp_llm_volc_agent
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

Set the following under `Volc LLM agent application`:

- `Volc IoT instance ID`, `Volc IoT product key`, `Volc IoT product secret`, and `Volc IoT device name`: the device identity used to sign the RTC configuration request
- `Volc bot ID`: the conversational bot the device talks to
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

1. After boot the application connects to Wi-Fi and waits for the SNTP sync to complete
2. The log line `Volc agent is ready` means the device has joined the room and is ready to talk
3. With WakeNet disabled, speak at any time; the Volc server decides when a query ends
4. With WakeNet enabled, say the wake word first. The microphone opens on wake-up and the reply in progress is cancelled so a new turn can start
5. Replies arrive as TTS on player stream 0 and play through the board audio output
6. Conversation state changes are published as the backend event `CONV_STATUS`; connection changes, VAD transitions, and errors appear as `Agent event:` log lines
7. `esp_llm_service_send_text()` asks the model a question, while `esp_llm_volc_send_text_to_tts()` speaks a string without a model round trip
8. Set `Run time before clean shutdown` to a non-zero value to watch the complete teardown sequence instead of running forever

## Troubleshooting

### The application stops before joining the room

Check the log for the failing step. An empty Wi-Fi SSID reports `Start network failed: Wi-Fi SSID is empty`, and missing account fields are rejected before the RTC session is created.

### Signature rejected or `GetRTCConfig` fails

The log line `SNTP sync timed out; Volc GetRTCConfig signature will be rejected` means the wall clock was never set. RTC request signing fails with a stale clock, so confirm the network reaches `pool.ntp.org` or `cn.pool.ntp.org`.

### Board device names are missing

Build or startup errors about unknown device handles mean the board metadata was not generated for the selected chip. Rerun `idf.py bmgr -b <board>` with the board that matches the target.

### The RTC session never connects

Verify the IoT instance ID, product key, product secret, and device name against the Volc console, and confirm that the bot ID is valid for that instance.

### Playback is distorted or runs at the wrong speed

The board DAC and the AEC capture path share one I2S bus, so the player is opened at 16 kHz with two channels even though the G.711A stream is 8 kHz mono. Keep that relationship intact when changing the audio configuration.

## Technical Support

- Forum: [esp32.com](https://esp32.com/viewforum.php?f=20)
- Issues: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
