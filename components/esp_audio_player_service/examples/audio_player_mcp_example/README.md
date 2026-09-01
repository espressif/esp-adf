# Audio Player Service MCP Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example shows how to expose `esp_audio_player_service` as **MCP tools** over
**HTTP** (`POST /mcp`):

1. Create and start one audio player service (board DAC + optional SD card)
2. Bring up Wi-Fi SoftAP (default) or STA
3. Register MCP tools via `esp_service_manager`
4. Start `esp_service_mcp_server` with HTTP transport
5. Idle while a PC client / LLM bridge drives playback remotely

It does **not** demonstrate mix/preemption (see `audio_player_mix_cli_example` for that).

### Typical Scenarios

Remote or LLM-driven audio control over HTTP MCP (SoftAP demo network or STA on
an existing Wi-Fi).

## Environment Setup

### Hardware Required

- Default board: **ESP32-S3** with **audio DAC** and native Wi-Fi (via `esp_board_manager`); add **SD card** for remote `set_url` / `play`
- PC / phone that can join the board SoftAP (no USB-UART adapter needed for MCP)

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- Python `requests` for the host smoke-test script
- Optional: `/sdcard/test.mp3` for remote play

## Build and Flash

### Build Preparation

```bash
./install.sh
. ./export.sh
cd $ADF_PATH/components/esp_audio_player_service/examples/audio_player_mcp_example
pip install esp-bmgr-assist
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### Project Configuration

Default SoftAP needs no secrets. For STA:

```bash
idf.py menuconfig
# Audio Player MCP Example → Wi-Fi mode → Station
# Set SSID / password (do not put real secrets in sdkconfig.defaults)
```

| Kconfig | Default in this example |
| --- | --- |
| `ESP_MCP_ENABLE` | y |
| `AUDIO_PLAYER_SERVICE_MCP_ENABLE` | y |
| `ESP_MCP_TRANSPORT_HTTP` | y |
| `EXAMPLE_WIFI_MODE_SOFTAP` | y |
| `EXAMPLE_WIFI_SSID` | `esp-audio-mcp` |
| `EXAMPLE_MCP_HTTP_PORT` | 8080 |
| `EXAMPLE_MCP_HTTP_URI` | `/mcp` |

On chips without native Wi-Fi, the matching `sdkconfig.defaults.<target>` may
enable `esp_hosted` / `esp_wifi_remote`.

### Build and Flash Commands

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## How to Use the Example

### Functionality and Usage

Success logs include:

```text
Audio player MCP tools registered with service manager
MCP HTTP transport started: http://192.168.4.1:8080/mcp
Ready for MCP tools/list and tools/call over HTTP
```

#### MCP HTTP smoke test (SoftAP)

1. Connect the PC Wi-Fi to SSID `esp-audio-mcp` (open / no password)
2. Run:

```bash
pip install requests
python3 scripts/test_audio_player_mcp_http.py 192.168.4.1
```

The script checks `tools/list`, volume, `get_status`, then `set_url` + `play`
for `file:///sdcard/test.mp3` (needs that file on the SD card). Skip with `--no-play`.

For STA, use the STA IP printed in the log:

```bash
python3 scripts/test_audio_player_mcp_http.py <device-ip>
```

#### Manual play (optional)

Place `/sdcard/test.mp3` (or change `EXAMPLE_MCP_DEMO_MP3_FILENAME`), then call
tools such as:

- `esp_audio_player_service_set_url` with `{"url":"file:///sdcard/test.mp3"}`
- `esp_audio_player_service_play`
- `esp_audio_player_service_set_output_volume` / `esp_audio_player_service_set_volume`

### Log Output

```text
I (xxx) mcp_ex: Audio player MCP tools registered with service manager
I (xxx) mcp_ex: MCP HTTP transport started: http://192.168.4.1:8080/mcp
I (xxx) mcp_ex: Ready for MCP tools/list and tools/call over HTTP
```

## Troubleshooting

- Cannot join SoftAP: confirm SSID `esp-audio-mcp` and that SoftAP mode is selected.
- `set_url` / `play` fails: place `test.mp3` under `/sdcard/` or use `--no-play`.
- Chip has no native Wi-Fi: enable the hosted stack in the matching `sdkconfig.defaults.<target>`.

## Related

- Component MCP API: `esp_audio_player_service_mcp.h`
- Tool schema: `../../src/mcp/esp_audio_player_service_mcp.json`
- Mix demo: `../audio_player_mix_cli_example`
- Host LLM bridge: `esp_service/tools/mcp_llm_bridge`

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports: [ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
